// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <crypto/aes_cipher.h>
#include <limits>
#include <loader/loader.h>

#include "ctr_encrypted_backing.h"
#include "bktr_ctr_ex.h"
#include "region_backing.h"
#include "partition_filesystem.h"
#include "nca.h"
#include "rom_filesystem.h"
#include "directory.h"

namespace skyline::vfs {
    using namespace loader;

    NCA::NCA(std::shared_ptr<vfs::Backing> pBacking, std::shared_ptr<crypto::KeyStore> pKeyStore, bool pUseKeyArea, bool headerOnly) : backing(std::move(pBacking)), keyStore(std::move(pKeyStore)), useKeyArea(pUseKeyArea) {
        header = backing->Read<NcaHeader>();

        if (header.magic != util::MakeMagic<u32>("NCA3")) {
            if (!keyStore->headerKey)
                throw loader_exception(LoaderResult::MissingHeaderKey);

            crypto::AesCipher cipher(*keyStore->headerKey, MBEDTLS_CIPHER_AES_128_XTS);

            cipher.XtsDecrypt({reinterpret_cast<u8 *>(&header), sizeof(NcaHeader)}, 0, 0x200);

            // Check if decryption was successful
            if (header.magic != util::MakeMagic<u32>("NCA3"))
                throw loader_exception(LoaderResult::ParsingError);
            encrypted = true;
        }

        contentType = header.contentType;
        rightsIdEmpty = header.rightsId == crypto::KeyStore::Key128{};

        // Firmware title discovery needs only the encrypted NCA header.
        // Avoid decrypting ExeFS/RomFS for hundreds of unrelated NCAs:
        // newer key generations may otherwise abort a valid title scan.
        if (headerOnly)
            return;

        for (size_t i{}; i < header.sectionHeaders.size(); i++) {
            auto &sectionHeader{header.sectionHeaders.at(i)};
            auto &sectionEntry{header.fsEntries.at(i)};

            if (sectionHeader.fsType == NcaSectionFsType::PFS0 && sectionHeader.hashType == NcaSectionHashType::HierarchicalSha256)
                ReadPfs0(sectionHeader, sectionEntry);
            else if (sectionHeader.fsType == NcaSectionFsType::RomFs && sectionHeader.hashType == NcaSectionHashType::HierarchicalIntegrity)
                ReadRomFs(sectionHeader, sectionEntry);
        }
    }

    void NCA::ReadPfs0(const NcaSectionHeader &sectionHeader, const NcaFsEntry &entry) {
        size_t offset{static_cast<size_t>(entry.startOffset) * constant::MediaUnitSize + sectionHeader.sha256HashInfo.pfs0Offset};
        size_t size{constant::MediaUnitSize * static_cast<size_t>(entry.endOffset - entry.startOffset)};

        auto pfs{std::make_shared<PartitionFileSystem>(CreateBacking(sectionHeader, std::make_shared<RegionBacking>(backing, offset, size), offset))};

        if (contentType == NcaContentType::Program) {
            // An ExeFS must always contain an NPDM and a main NSO, whereas the logo section will always contain a logo and a startup movie
            if (pfs->FileExists("main") && pfs->FileExists("main.npdm"))
                exeFs = std::move(pfs);
            else if (pfs->FileExists("NintendoLogo.png") && pfs->FileExists("StartupMovie.gif"))
                logo = std::move(pfs);
        } else if (contentType == NcaContentType::Meta) {
            cnmt = std::move(pfs);
        }
    }

    void NCA::ReadRomFs(const NcaSectionHeader &sectionHeader, const NcaFsEntry &entry) {
        if (sectionHeader.encryptionType == NcaSectionEncryptionType::BKTR) {
            // BKTR isn't ordinary AES-CTR over a contiguous RomFS. The
            // relocation table refers to both base+patch RomFS, and patch
            // subsections have their own CTR values. Exposing an ordinary
            // CtrEncryptedBacking here served corrupt data as valid RomFS.
            hasBktrRomFs = true;
            const auto &patch{sectionHeader.patchInfo};
            constexpr size_t MaxRelocationTable{32 * 1024 * 1024};
            const auto sectionStart{static_cast<u64>(entry.startOffset) * constant::MediaUnitSize};
            const auto sectionSize{static_cast<u64>(entry.endOffset - entry.startOffset) * constant::MediaUnitSize};
            if (entry.endOffset < entry.startOffset || patch.indirectSize < bktr::BucketSize ||
                patch.indirectSize > MaxRelocationTable ||
                patch.indirectOffset > sectionSize ||
                patch.indirectSize > sectionSize - patch.indirectOffset ||
                sectionStart > backing->size ||
                patch.indirectOffset > backing->size - sectionStart ||
                patch.indirectSize > backing->size - sectionStart - patch.indirectOffset) {
                LOGW("Invalid BKTR relocation table bounds in NCA");
                return;
            }
            try {
                const auto offset{static_cast<size_t>(sectionStart + patch.indirectOffset)};
                const auto bytes{static_cast<size_t>(patch.indirectSize)};
                auto region{std::make_shared<RegionBacking>(backing, offset, bytes)};
                auto decrypted{CreateBacking(sectionHeader, region, offset)};
                if (!decrypted)
                    return;
                std::vector<u8> buffer(bytes);
                decrypted->Read(span(buffer));
                bktrRelocations = bktr::RelocationTable::Parse(
                    std::span<const u8>(buffer.data(), buffer.size()));
                if (bktrRelocations) {
                    LOGD("Parsed BKTR relocation index with {} entries", bktrRelocations->EntryCount());
                }

                if (patch.subsectionSize >= bktr::BucketSize &&
                    patch.subsectionSize <= MaxRelocationTable &&
                    patch.subsectionOffset <= sectionSize &&
                    patch.subsectionSize <= sectionSize - patch.subsectionOffset &&
                    patch.subsectionOffset <= backing->size - sectionStart &&
                    patch.subsectionSize <= backing->size - sectionStart - patch.subsectionOffset) {
                    const auto subOffset{static_cast<size_t>(sectionStart + patch.subsectionOffset)};
                    const auto subSize{static_cast<size_t>(patch.subsectionSize)};
                    auto subRegion{std::make_shared<RegionBacking>(backing, subOffset, subSize)};
                    auto subBacking{CreateBacking(sectionHeader, subRegion, subOffset)};
                    if (subBacking) {
                        std::vector<u8> subsectionBytes(subSize);
                        subBacking->Read(span(subsectionBytes));
                        bktrSubsections = bktr::SubsectionTable::Parse(
                            std::span<const u8>(subsectionBytes.data(), subsectionBytes.size()));
                        if (bktrSubsections) {
                            LOGD("Parsed BKTR subsection table with {} CTR-Ex counters", bktrSubsections->EntryCount());
                            // Keep the *physical* patch encrypted backing
                            // separate from romFs. Only an independently
                            // verified base + relocation overlay can produce
                            // a usable guest-visible updated RomFS.
                            if (bktrRelocations && sectionSize <= backing->size - sectionStart &&
                                sectionSize <= std::numeric_limits<size_t>::max()) {
                                auto patchSection{std::make_shared<RegionBacking>(
                                    backing, static_cast<size_t>(sectionStart),
                                    static_cast<size_t>(sectionSize))};
                                if (!encrypted) {
                                    // Some homebrew/tool-generated NCAs have
                                    // already decrypted sections: don't
                                    // encrypt/decrypt them a second time.
                                    bktrPhysicalSection = std::move(patchSection);
                                } else {
                                    const auto key{!(rightsIdEmpty || useKeyArea)
                                        ? GetTitleKey() : GetKeyAreaKey(sectionHeader.encryptionType)};
                                    std::array<u8, 0x10> initialIv{};
                                    const u32 secure{util::SwapEndianness(sectionHeader.secureValue)};
                                    const u32 generation{util::SwapEndianness(sectionHeader.generation)};
                                    std::memcpy(initialIv.data(), &secure, 4);
                                    std::memcpy(initialIv.data() + 4, &generation, 4);
                                    bktrPhysicalSection = std::make_shared<bktr::BktrCtrExBacking>(
                                        std::move(patchSection),
                                        std::make_shared<const bktr::SubsectionTable>(*bktrSubsections),
                                        key, initialIv, sectionStart);
                                }
                                LOGD("BKTR physical section ready; virtual/base RomFS merge not yet active");
                            }
                        }
                    }
                }
            } catch (const std::exception &e) {
                LOGW("Unable to read BKTR relocation metadata: {}", e.what());
            }
            // Do not set romFs yet: successful BKTR relocation parsing alone
            // is insufficient without subsection decryption and base merge.
            return;
        }

        size_t offset{static_cast<size_t>(entry.startOffset) * constant::MediaUnitSize + sectionHeader.integrityHashInfo.levels.back().offset};
        size_t size{sectionHeader.integrityHashInfo.levels.back().size};

        romFs = CreateBacking(sectionHeader, std::make_shared<RegionBacking>(backing, offset, size), offset);
    }

    std::shared_ptr<Backing> NCA::CreateBacking(const NcaSectionHeader &sectionHeader, std::shared_ptr<Backing> rawBacking, size_t offset) {
        if (!encrypted)
            return rawBacking;

        switch (sectionHeader.encryptionType) {
            case NcaSectionEncryptionType::None:
                return rawBacking;
            case NcaSectionEncryptionType::CTR:
            case NcaSectionEncryptionType::BKTR: {
                auto key{!(rightsIdEmpty || useKeyArea) ? GetTitleKey() : GetKeyAreaKey(sectionHeader.encryptionType)};

                std::array<u8, 0x10> ctr{};
                u32 secureValueLE{util::SwapEndianness(sectionHeader.secureValue)};
                u32 generationLE{util::SwapEndianness(sectionHeader.generation)};
                std::memcpy(ctr.data(), &secureValueLE, 4);
                std::memcpy(ctr.data() + 4, &generationLE, 4);

                return std::make_shared<CtrEncryptedBacking>(ctr, key, std::move(rawBacking), offset);
            }
            default:
                return nullptr;
        }
    }

    u8 NCA::GetKeyGeneration() {
        u8 legacyGen{static_cast<u8>(header.legacyKeyGenerationType)};
        u8 gen{static_cast<u8>(header.keyGenerationType)};
        gen = std::max<u8>(legacyGen, gen);
        return gen > 0 ? gen - 1 : gen;
    }

    crypto::KeyStore::Key128 NCA::GetTitleKey() {
        u8 keyGeneration{GetKeyGeneration()};

        auto titleKey{keyStore->GetTitleKey(header.rightsId)};
        if (keyGeneration >= keyStore->titleKek.size())
            throw loader_exception(LoaderResult::MissingTitleKek);
        auto &titleKek{keyStore->titleKek[keyGeneration]};

        if (!titleKey)
            throw loader_exception(LoaderResult::MissingTitleKey);
        if (!titleKek)
            throw loader_exception(LoaderResult::MissingTitleKek);

        crypto::AesCipher cipher(*titleKek, MBEDTLS_CIPHER_AES_128_ECB);
        cipher.Decrypt(*titleKey);
        return *titleKey;
    }

    crypto::KeyStore::Key128 NCA::GetKeyAreaKey(NCA::NcaSectionEncryptionType type) {
        auto keyArea{[this, &type](crypto::KeyStore::IndexedKeys128 &keys) {
            u8 keyGeneration{GetKeyGeneration()};

            if (keyGeneration >= keys.size())
                throw loader_exception(LoaderResult::MissingKeyArea);
            auto &keyArea{keys[keyGeneration]};

            if (!keyArea)
                throw loader_exception(LoaderResult::MissingKeyArea);

            size_t keyAreaIndex;
            switch (type) {
                case NcaSectionEncryptionType::XTS:
                    keyAreaIndex = 0;
                    break;
                case NcaSectionEncryptionType::CTR:
                case NcaSectionEncryptionType::BKTR:
                    keyAreaIndex = 2;
                    break;
                default:
                    throw exception("Unsupported NcaSectionEncryptionType");
            }

            crypto::KeyStore::Key128 decryptedKeyArea;
            crypto::AesCipher cipher(*keyArea, MBEDTLS_CIPHER_AES_128_ECB);
            cipher.Decrypt(decryptedKeyArea.data(), header.encryptedKeyArea[keyAreaIndex].data(), decryptedKeyArea.size());
            return decryptedKeyArea;
        }};

        switch (header.keyAreaEncryptionKeyType) {
            case NcaKeyAreaEncryptionKeyType::Application:
                return keyArea(keyStore->areaKeyApplication);
            case NcaKeyAreaEncryptionKeyType::Ocean:
                return keyArea(keyStore->areaKeyOcean);
            case NcaKeyAreaEncryptionKeyType::System:
                return keyArea(keyStore->areaKeySystem);
        }
    }
}

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "gpu.h"
#include "nce.h"
#include "nce/guest.h"
#include "kernel/types/KProcess.h"
#include <fcntl.h>
#include <unistd.h>
#include <filesystem>
#include <system_error>
#include <vfs/content_meta.h>
#include <vfs/nca.h>
#include <vfs/partition_filesystem.h>
#include <vfs/ticket.h>
#include "vfs/os_backing.h"
#include "loader/nro.h"
#include "loader/nso.h"
#include "loader/nca.h"
#include "loader/nsp.h"
#include "loader/xci.h"
#include "os.h"

namespace skyline::kernel {
    /**
     * Inspect app-private, user-imported DLC NSPs for this launch only.
     *
     * A DLC gets registered only if a decrypted CNMT matches the running
     * program and a referenced Data NCA exposes an actual RomFS. A staged
     * file, guessed name or CNMT alone is not considered mounted.
     *
     * No signature/content SHA verification is claimed; files are local.
     * Keep a strict cap so a malformed folder cannot block startup.
     */
    static void MountLocalAddOns(OS &os, const std::shared_ptr<crypto::KeyStore> &keyStore,
                                 u64 appId) {
        auto &registry{os.mountedAddOns};
        registry.SetApplication(appId);
        if (!appId || !keyStore)
            return;
        const auto directory{os.privateAppFilesPath +
            fmt::format("synolsis-content/{:016X}/dlc/", appId)};
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error))
            return;

        size_t packages{};
        for (const auto &entry : std::filesystem::directory_iterator(
                 directory, std::filesystem::directory_options::skip_permission_denied, error)) {
            if (error || packages >= 128)
                break;
            if (entry.is_symlink(error) || !entry.is_regular_file(error) ||
                entry.path().extension() != ".nsp")
                continue;
            ++packages;
            try {
                const auto path{entry.path().string()};
                int fd{open(path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW)};
                if (fd < 0)
                    continue;
                std::shared_ptr<vfs::Backing> archive;
                try {
                    archive = std::make_shared<vfs::OsBacking>(fd, true);
                } catch (...) {
                    close(fd);
                    throw;
                }
                auto nsp{std::make_shared<vfs::PartitionFileSystem>(archive)};
                const auto files{nsp->OpenDirectory("", {false, true})->Read()};
                if (files.size() > 4096)
                    continue;

                // The same user-supplied tickets used by NspLoader and the
                // Java metadata inspector may be needed for title-rights NCAs.
                for (const auto &file : files) {
                    if (!file.name.ends_with(".tik"))
                        continue;
                    try {
                        vfs::Ticket ticket{nsp->OpenFile(file.name)};
                        crypto::KeyStore::Key128 key{};
                        std::copy_n(ticket.titleKeyBlock.begin(), key.size(), key.begin());
                        keyStore->PopulateTitleKey(ticket.rightsId, key);
                    } catch (const std::exception &) {
                        // Invalid tickets cannot be used for mounting.
                    }
                }

                for (const auto &file : files) {
                    if (!file.name.ends_with(".cnmt.nca"))
                        continue;
                    try {
                        vfs::NCA meta{nsp->OpenFile(file.name), keyStore};
                        if (meta.contentType != vfs::NcaContentType::Meta || !meta.cnmt)
                            continue;
                        const auto metaFiles{meta.cnmt->OpenDirectory("", {false, true})};
                        if (!metaFiles)
                            continue;
                        for (const auto &cnmtFile : metaFiles->Read()) {
                            if (!cnmtFile.name.ends_with(".cnmt"))
                                continue;
                            auto content{meta.cnmt->OpenFile(cnmtFile.name)};
                            if (content->size < 0x20 || content->size > 4 * 1024 * 1024)
                                continue;
                            std::vector<u8> decoded(content->size);
                            content->Read(span(decoded));
                            const auto manifest{vfs::content_meta::ParseManifest(
                                std::span<const u8>(decoded.data(), decoded.size()))};
                            if (!manifest ||
                                manifest->summary.type != vfs::content_meta::Type::AddOnContent ||
                                !vfs::content_meta::BelongsToApplication(*manifest, appId))
                                continue;
                            const auto titleId{manifest->summary.titleId};
                            for (const auto &part : manifest->contents) {
                                // CNMT type 2 = Data. Never register metadata,
                                // unrelated program NCAs or placeholders.
                                if (part.type != 2)
                                    continue;
                                const auto name{part.NcaFilename()};
                                if (!nsp->FileExists(name))
                                    continue;
                                try {
                                    vfs::NCA nca{nsp->OpenFile(name), keyStore};
                                    if (nca.contentType != vfs::NcaContentType::Data ||
                                        nca.header.programId != titleId || !nca.romFs)
                                        continue;
                                    if (registry.Add(titleId, nca.romFs)) {
                                        LOGINF("Mounted local DLC 0x{:016X} for 0x{:016X}", titleId, appId);
                                    }
                                    break;
                                } catch (const std::exception &) {
                                    // Unreadable AOC data never becomes visible.
                                }
                            }
                        }
                    } catch (const std::exception &) {
                        // Missing keys, incomplete metadata or corrupt NCA.
                    }
                }
            } catch (const std::exception &e) {
                LOGW("Skipping invalid imported DLC '{}': {}", entry.path().string(), e.what());
            }
        }
    }

    OS::OS(
        std::shared_ptr<JvmManager> &jvmManager,
        std::shared_ptr<Settings> &settings,
        std::string publicAppFilesPath,
        std::string privateAppFilesPath,
        std::string nativeLibraryPath,
        std::string deviceTimeZone,
        std::shared_ptr<vfs::FileSystem> assetFileSystem)
        : nativeLibraryPath(std::move(nativeLibraryPath)),
          publicAppFilesPath(std::move(publicAppFilesPath)),
          privateAppFilesPath(std::move(privateAppFilesPath)),
          deviceTimeZone(std::move(deviceTimeZone)),
          assetFileSystem(std::move(assetFileSystem)),
          state(this, jvmManager, settings),
          serviceManager(state) {}

    void OS::Execute(int romFd, loader::RomFormat romType) {
        auto romFile{std::make_shared<vfs::OsBacking>(romFd)};
        auto keyStore{std::make_shared<crypto::KeyStore>(privateAppFilesPath + "keys/")};

        state.loader = [&]() -> std::shared_ptr<loader::Loader> {
            switch (romType) {
                case loader::RomFormat::NRO:
                    return std::make_shared<loader::NroLoader>(std::move(romFile));
                case loader::RomFormat::NSO:
                    return std::make_shared<loader::NsoLoader>(std::move(romFile));
                case loader::RomFormat::NCA:
                    return std::make_shared<loader::NcaLoader>(std::move(romFile), std::move(keyStore));
                case loader::RomFormat::NSP:
                    return std::make_shared<loader::NspLoader>(romFile, keyStore);
                case loader::RomFormat::XCI:
                    return std::make_shared<loader::XciLoader>(romFile, keyStore);
                default:
                    throw exception("Unsupported ROM extension.");
            }
        }();

        // Probe user-imported AOC content before the guest starts. The mount
        // registry is per OS/process; false positives must not alter base game
        // loading. Other RomFormats can still use NACP to identify their game.
        if (state.loader->nacp) {
            MountLocalAddOns(*this, keyStore, state.loader->nacp->nacpContents.saveDataOwnerId);
        }

        state.gpu->Initialise();

        auto &process{state.process};
        process = std::make_shared<kernel::type::KProcess>(state);

        auto entry{state.loader->LoadProcessData(process, state)};
        auto &nacp{state.loader->nacp};
        if (nacp) {
            std::string name{nacp->GetApplicationName(language::ApplicationLanguage::AmericanEnglish)}, publisher{nacp->GetApplicationPublisher(language::ApplicationLanguage::AmericanEnglish)};
            if (name.empty())
                name = nacp->GetApplicationName(nacp->GetFirstSupportedTitleLanguage());
            if (publisher.empty())
                publisher = nacp->GetApplicationPublisher(nacp->GetFirstSupportedTitleLanguage());
            LOGINF(R"(Starting "{}" ({}) v{} by "{}")", name, nacp->GetSaveDataOwnerId(), nacp->GetApplicationVersion(), publisher);
        }

        process->InitializeHeapTls();
        auto thread{process->CreateThread(entry)};
        if (thread) {
            LOGI("Starting main HOS thread");
            thread->Start(true);
            process->Kill(true, true, true);
        }
    }
}

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "skyline/crypto/key_store.h"
#include "skyline/vfs/nca.h"
#include <algorithm>
#include "skyline/vfs/ticket.h"
#include "skyline/vfs/partition_filesystem.h"
#include "skyline/vfs/content_meta.h"
#include "skyline/vfs/os_backing.h"
#include "skyline/vfs/os_filesystem.h"
#include "skyline/loader/nro.h"
#include "skyline/loader/nso.h"
#include "skyline/loader/nca.h"
#include "skyline/loader/xci.h"
#include "skyline/loader/nsp.h"
#include "skyline/jvm.h"

extern "C" JNIEXPORT jint JNICALL Java_org_stratoemu_strato_loader_RomFile_populate(JNIEnv *env, jobject thiz, jint jformat, jint fd, jstring appFilesPathJstring, jint systemLanguage) {
    skyline::signal::ScopedStackBlocker stackBlocker;

    skyline::loader::RomFormat format{static_cast<skyline::loader::RomFormat>(jformat)};

    auto keyStore{std::make_shared<skyline::crypto::KeyStore>(skyline::JniString(env, appFilesPathJstring))};
    std::unique_ptr<skyline::loader::Loader> loader;
    try {
        auto backing{std::make_shared<skyline::vfs::OsBacking>(fd)};

        switch (format) {
            case skyline::loader::RomFormat::NRO:
                loader = std::make_unique<skyline::loader::NroLoader>(backing);
                break;
            case skyline::loader::RomFormat::NSO:
                loader = std::make_unique<skyline::loader::NsoLoader>(backing);
                break;
            case skyline::loader::RomFormat::NCA:
                loader = std::make_unique<skyline::loader::NcaLoader>(backing, keyStore);
                break;
            case skyline::loader::RomFormat::XCI:
                loader = std::make_unique<skyline::loader::XciLoader>(backing, keyStore);
                break;
            case skyline::loader::RomFormat::NSP:
                loader = std::make_unique<skyline::loader::NspLoader>(backing, keyStore);
                break;
            default:
                return static_cast<jint>(skyline::loader::LoaderResult::ParsingError);
        }
    } catch (const skyline::loader::loader_exception &e) {
        return static_cast<jint>(e.error);
    } catch (const std::exception &e) {
        return static_cast<jint>(skyline::loader::LoaderResult::ParsingError);
    }

    jclass clazz{env->GetObjectClass(thiz)};
    jfieldID applicationNameField{env->GetFieldID(clazz, "applicationName", "Ljava/lang/String;")};
    jfieldID applicationTitleIdField{env->GetFieldID(clazz, "applicationTitleId", "Ljava/lang/String;")};
    jfieldID applicationAuthorField{env->GetFieldID(clazz, "applicationAuthor", "Ljava/lang/String;")};
    jfieldID rawIconField{env->GetFieldID(clazz, "rawIcon", "[B")};
    jfieldID applicationVersionField{env->GetFieldID(clazz, "applicationVersion", "Ljava/lang/String;")};

    if (loader->nacp) {
        auto language{skyline::language::GetApplicationLanguage(static_cast<skyline::language::SystemLanguage>(systemLanguage))};
        if (((1 << static_cast<skyline::u32>(language)) & loader->nacp->supportedTitleLanguages) == 0)
            language = loader->nacp->GetFirstSupportedTitleLanguage();

        env->SetObjectField(thiz, applicationNameField, env->NewStringUTF(loader->nacp->GetApplicationName(language).c_str()));
        env->SetObjectField(thiz, applicationVersionField, env->NewStringUTF(loader->nacp->GetApplicationVersion().c_str()));
        env->SetObjectField(thiz, applicationTitleIdField, env->NewStringUTF(loader->nacp->GetSaveDataOwnerId().c_str()));
        env->SetObjectField(thiz, applicationAuthorField, env->NewStringUTF(loader->nacp->GetApplicationPublisher(language).c_str()));

        auto icon{loader->GetIcon(language)};
        jbyteArray iconByteArray{env->NewByteArray(static_cast<jsize>(icon.size()))};
        env->SetByteArrayRegion(iconByteArray, 0, static_cast<jsize>(icon.size()), reinterpret_cast<const jbyte *>(icon.data()));
        env->SetObjectField(thiz, rawIconField, iconByteArray);
    }

    return static_cast<jint>(skyline::loader::LoaderResult::Success);
}

extern "C" JNIEXPORT jstring Java_org_stratoemu_strato_preference_FirmwareImportPreference_fetchFirmwareVersion(JNIEnv *env, jobject thiz, jstring systemArchivesPathJstring, jstring keysPathJstring) {
    struct SystemVersion {
        skyline::u8 major;
        skyline::u8 minor;
        skyline::u8 micro;
        skyline::u8 _pad0_;
        skyline::u8 revisionMajor;
        skyline::u8 revisionMinor;
        skyline::u8 _pad1_[2];
        skyline::u8 platformString[0x20];
        skyline::u8 versionHash[0x40];
        skyline::u8 displayVersion[0x18];
        skyline::u8 displayTitle[0x80];
    };

    constexpr skyline::u64 systemVersionProgramId{0x0100000000000809};

    // Version detection must not crash when an unrelated NCA uses missing keys.
    try {
        auto systemArchivesFileSystem{std::make_shared<skyline::vfs::OsFileSystem>(skyline::JniString(env, systemArchivesPathJstring))};
        auto systemArchives{systemArchivesFileSystem->OpenDirectory("")};
        auto keyStore{std::make_shared<skyline::crypto::KeyStore>(skyline::JniString(env, keysPathJstring))};
        bool missingKeys{};

        for (const auto &entry : systemArchives->Read()) {
            try {
                std::shared_ptr<skyline::vfs::Backing> backing{systemArchivesFileSystem->OpenFile(entry.name)};
                auto nca{skyline::vfs::NCA(backing, keyStore)};
                if (nca.header.programId == systemVersionProgramId && nca.romFs != nullptr) {
                    auto controlRomFs{std::make_shared<skyline::vfs::RomFileSystem>(nca.romFs)};
                    auto file{controlRomFs->OpenFile("file")};
                    SystemVersion systemVersion{};
                    file->Read<SystemVersion>(systemVersion);
                    systemVersion.displayVersion[sizeof(systemVersion.displayVersion) - 1] = 0;
                    return env->NewStringUTF(reinterpret_cast<char *>(systemVersion.displayVersion));
                }
            } catch (const skyline::loader::loader_exception &e) {
                if (e.error == skyline::loader::LoaderResult::MissingHeaderKey ||
                    e.error == skyline::loader::LoaderResult::MissingTitleKey ||
                    e.error == skyline::loader::LoaderResult::MissingTitleKek ||
                    e.error == skyline::loader::LoaderResult::MissingKeyArea)
                    missingKeys = true;
            } catch (const std::exception &) {
                // A malformed unrelated NCA must not stop the SystemVersion scan.
            }
        }
        return env->NewStringUTF(missingKeys ? "-1" : "");
    } catch (const std::exception &) {
        return env->NewStringUTF("");
    }
}

std::vector<skyline::u8> decodeBfttfFont(const std::shared_ptr<skyline::vfs::Backing> bfttfFile){
    constexpr skyline::u32 fontKey{0x06186249};
    constexpr skyline::u32 BFTTFMagic{0x18029a7f};

    if (!bfttfFile || bfttfFile->size < 8 || (bfttfFile->size - 8) % 4 != 0)
        return {};
    auto firstBytes{bfttfFile->Read<skyline::u32>()};
    auto firstBytesXor{firstBytes ^ fontKey};

    if (firstBytesXor == BFTTFMagic) {
        constexpr size_t initialOffset{8};
        std::vector<skyline::u8> font(bfttfFile->size - initialOffset);

        for (size_t offset = initialOffset; offset < bfttfFile->size; offset += 4) {
            skyline::u32 decodedData{bfttfFile->Read<skyline::u32>(offset) ^ fontKey};

            font[offset - 8] = static_cast<skyline::u8>(decodedData >> 0);
            font[offset - 7] = static_cast<skyline::u8>(decodedData >> 8);
            font[offset - 6] = static_cast<skyline::u8>(decodedData >> 16);
            font[offset - 5] = static_cast<skyline::u8>(decodedData >> 24);
        }

        return font;
    }
    return {};
}

extern "C" JNIEXPORT void Java_org_stratoemu_strato_preference_FirmwareImportPreference_extractFonts(JNIEnv *env, jobject thiz, jstring systemArchivesPathJstring, jstring keysPathJstring, jstring fontsPath) {
    // Fonts are stored in the following NCAs
    // 0x0100000000000810 -> "FontNintendoExtended"
    // 0x0100000000000811 -> "FontStandard"
    // 0x0100000000000812 -> "FontKorean"
    // 0x0100000000000813 -> "FontChineseTraditional"
    // 0x0100000000000814 -> "FontChineseSimplified"

    constexpr skyline::u64 firstFontProgramId{0x0100000000000810};
    constexpr skyline::u64 lastFontProgramId{0x0100000000000814};

    const std::map<std::string, std::string> sharedFontFilenameDictionary = {
        {"nintendo_ext_003.bfttf", "FontNintendoExtended"},
        {"nintendo_ext2_003.bfttf", "FontNintendoExtended2"},
        {"nintendo_udsg-r_std_003.bfttf", "FontStandard"},
        {"nintendo_udsg-r_ko_003.bfttf", "FontKorean"},
        {"nintendo_udjxh-db_zh-tw_003.bfttf", "FontChineseTraditional"},
        {"nintendo_udsg-r_org_zh-cn_003.bfttf", "FontChineseSimplified"},
        {"nintendo_udsg-r_ext_zh-cn_003.bfttf", "FontExtendedChineseSimplified"}
    };

    auto fontsFileSystem{std::make_shared<skyline::vfs::OsFileSystem>(skyline::JniString(env, fontsPath))};
    auto systemArchivesFileSystem{std::make_shared<skyline::vfs::OsFileSystem>(skyline::JniString(env, systemArchivesPathJstring))};
    auto systemArchives{systemArchivesFileSystem->OpenDirectory("")};
    auto keyStore{std::make_shared<skyline::crypto::KeyStore>(skyline::JniString(env, keysPathJstring))};

    for (const auto &entry : systemArchives->Read()) {
        try {
            std::shared_ptr<skyline::vfs::Backing> backing{systemArchivesFileSystem->OpenFile(entry.name)};
            auto nca{skyline::vfs::NCA(backing, keyStore)};

            if (nca.header.programId >= firstFontProgramId && nca.header.programId <= lastFontProgramId && nca.romFs != nullptr) {
                auto controlRomFs{std::make_shared<skyline::vfs::RomFileSystem>(nca.romFs)};

                for (auto fileEntry = controlRomFs->fileMap.begin(); fileEntry != controlRomFs->fileMap.end(); fileEntry++) {
                    auto fileName{fileEntry->first};
                    auto bfttfFile{controlRomFs->OpenFile(fileName)};

                    auto decodedFont{decodeBfttfFont(bfttfFile)};
                    if (decodedFont.empty())
                        continue;

                    const auto fontIt{sharedFontFilenameDictionary.find(fileName)};
                    if (fontIt == sharedFontFilenameDictionary.end())
                        continue; // Firmware may contain additional font names.
                    auto ttfFileName{fontIt->second + ".ttf"};
                    if (fontsFileSystem->FileExists(ttfFileName))
                        fontsFileSystem->DeleteFile(ttfFileName);

                    fontsFileSystem->CreateFile(ttfFileName, decodedFont.size());
                    std::shared_ptr<skyline::vfs::Backing> ttfFile{fontsFileSystem->OpenFile(ttfFileName, {true, true, false})};

                    ttfFile->Write(decodedFont);
                }
            }
        } catch (const std::exception &) {
            // A single unreadable/unrecognized NCA must not crash firmware import.
        }
    }
}


/**
 * Inspects a staged NSP's own decrypted CNMT metadata through JNI, without
 * requiring it to contain a bootable base application. Regular NspLoader
 * intentionally rejects standalone updates/DLCs lacking ExeFS and control.
 *
 * Returns an array of "type:titleId:applicationId:version" strings, or null
 * when the file/keys cannot be inspected. This DOES NOT mount or authenticate
 * updates, import title keys, validate SHA256 records or register AOC.
 *
 * The Java-owned fd stays open: OsBacking(fd) defaults to closable=false.
 */
extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_stratoemu_strato_data_GameContentStore_readNativeContentMetadata(
    JNIEnv *env, jobject /*thiz*/, jint fd, jstring keysPath) {
    skyline::signal::ScopedStackBlocker stackBlocker;
    try {
        if (fd < 0 || keysPath == nullptr)
            return nullptr;

        auto store{std::make_shared<skyline::crypto::KeyStore>(
            skyline::JniString(env, keysPath))};
        auto package{std::make_shared<skyline::vfs::PartitionFileSystem>(
            std::make_shared<skyline::vfs::OsBacking>(fd))};
        auto directory{package->OpenDirectory("", {false, true})};
        if (!directory)
            return nullptr;
        const auto entries{directory->Read()};
        if (entries.size() > 4096)
            return nullptr;

        // Allow user-supplied NSP tickets, including updates with title-key
        // rights IDs, to be resolved using the existing Strato KeyStore.
        for (const auto &entry : entries) {
            if (!entry.name.ends_with(".tik"))
                continue;
            try {
                skyline::vfs::Ticket ticket{package->OpenFile(entry.name)};
                skyline::crypto::KeyStore::Key128 titleKey{};
                std::copy_n(ticket.titleKeyBlock.begin(), titleKey.size(), titleKey.begin());
                store->PopulateTitleKey(ticket.rightsId, titleKey);
            } catch (const std::exception &) {
                // A malformed extra ticket must not invalidate unrelated
                // valid metadata; the relevant NCA decrypt will still fail.
            }
        }

        std::vector<std::string> identities;
        for (const auto &entry : entries) {
            // Metadata NCAs in standard NSPs use the .cnmt.nca suffix.
            // Skip big program/data NCAs to keep imports responsive on 6 GB.
            if (!entry.name.ends_with(".cnmt.nca"))
                continue;
            try {
                skyline::vfs::NCA nca{package->OpenFile(entry.name), store};
                if (nca.contentType != skyline::vfs::NcaContentType::Meta || !nca.cnmt)
                    continue;
                const auto files{nca.cnmt->OpenDirectory("", {false, true})};
                if (!files)
                    continue;
                for (const auto &file : files->Read()) {
                    if (!file.name.ends_with(".cnmt"))
                        continue;
                    const auto backing{nca.cnmt->OpenFile(file.name)};
                    if (backing->size < 0x20 || backing->size > (4 * 1024 * 1024))
                        continue;
                    std::vector<skyline::u8> bytes(backing->size);
                    backing->Read(skyline::span(bytes));
                    const auto manifest{skyline::vfs::content_meta::ParseManifest(
                        std::span<const skyline::u8>(bytes.data(), bytes.size()))};
                    if (!manifest)
                        continue;
                    const auto &header{manifest->summary};
                    identities.emplace_back(fmt::format("{:02X}:{:016X}:{:016X}:{}",
                        static_cast<skyline::u32>(header.type), header.titleId,
                        header.applicationId, header.titleVersion));
                    if (identities.size() > 64)
                        return nullptr;
                }
            } catch (const std::exception &) {
                // Missing keys, bad ticket, malformed NCA or unsupported
                // metadata remains UNKNOWN on Android, not "verified".
            }
        }

        jclass stringClass{env->FindClass("java/lang/String")};
        if (!stringClass)
            return nullptr;
        jobjectArray output{env->NewObjectArray(
            static_cast<jsize>(identities.size()), stringClass, nullptr)};
        env->DeleteLocalRef(stringClass);
        if (!output)
            return nullptr;
        for (jsize i{}; i < static_cast<jsize>(identities.size()); ++i) {
            jstring value{env->NewStringUTF(identities[i].c_str())};
            if (!value)
                return nullptr;
            env->SetObjectArrayElement(output, i, value);
            env->DeleteLocalRef(value);
            if (env->ExceptionCheck())
                return nullptr;
        }
        return output;
    } catch (const std::exception &) {
        return nullptr;
    }
}


/**
 * QLauncher is the system-program title 0100000000001000. A firmware NCA
 * must come from the user's own Switch; the emulator bundles none.
 * Validate encrypted headers and a bootable ExeFS rather than trusting the
 * document filename or a guessed title ID. This does not promise that every
 * Horizon service needed by QLauncher is implemented.
 */
extern "C" JNIEXPORT jboolean JNICALL
Java_org_stratoemu_strato_SynolsisSettingsHubActivity_validateQLauncherNca(
    JNIEnv *env, jobject /*this*/, jint fd, jstring keysPath) {
    skyline::signal::ScopedStackBlocker stackBlocker;
    if (fd < 0 || !keysPath)
        return JNI_FALSE;
    try {
        auto backing{std::make_shared<skyline::vfs::OsBacking>(fd)};
        auto keyStore{std::make_shared<skyline::crypto::KeyStore>(
            skyline::JniString(env, keysPath))};
        skyline::vfs::NCA nca{std::move(backing), std::move(keyStore)};
        constexpr skyline::u64 QLauncherProgramId{0x0100000000001000ULL};
        return nca.header.programId == QLauncherProgramId &&
               nca.contentType == skyline::vfs::NcaContentType::Program &&
               nca.exeFs != nullptr &&
               nca.exeFs->FileExists("rtld") &&
               nca.exeFs->FileExists("main") &&
               nca.exeFs->FileExists("main.npdm") ? JNI_TRUE : JNI_FALSE;
    } catch (const std::exception &) {
        return JNI_FALSE;
    }
}


/**
 * Discover the QLauncher Program NCA inside the user's *already imported*
 * firmware, using the same registered-content directory and KeyStore used
 * for firmware version and font scanning. No filename guessing and no
 * bundled firmware. Returns a basename only, so Kotlin must resolve it
 * under the registered firmware directory before opening it.
 *
 * The bootstrap remains experimental: Horizon home-menu services do not
 * become functional merely because an executable NCA was found.
 */
extern "C" JNIEXPORT jstring JNICALL
Java_org_stratoemu_strato_SynolsisSettingsHubActivity_findInstalledQLauncherNca(
    JNIEnv *env, jobject /*thiz*/, jstring firmwarePath, jstring keysPath) {
    skyline::signal::ScopedStackBlocker stackBlocker;
    if (!firmwarePath || !keysPath)
        return nullptr;

    constexpr skyline::u64 QLauncherProgramId{0x0100000000001000ULL};
    try {
        auto systemArchives{std::make_shared<skyline::vfs::OsFileSystem>(
            skyline::JniString(env, firmwarePath))};
        auto keyStore{std::make_shared<skyline::crypto::KeyStore>(
            skyline::JniString(env, keysPath))};
        auto directory{systemArchives->OpenDirectory("")};
        if (!directory)
            return nullptr;
        auto entries{directory->Read()};
        if (entries.size() > 8192)
            return nullptr;

        // Deterministic order if a firmware dump contains multiple revisions.
        std::sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return a.name < b.name;
        });
        for (const auto &entry : entries) {
            // Never return arbitrary paths, directory traversal or non-NCA
            // entries from an imported archive.
            if (!entry.name.ends_with(".nca") ||
                entry.name.find('/') != std::string::npos ||
                entry.name.find('\\') != std::string::npos ||
                entry.name == "." || entry.name == "..")
                continue;
            try {
                auto backing{systemArchives->OpenFile(entry.name)};
                // Firmware 22/23 dumps may contain many system NCAs with
                // encryption keys we cannot decode. Inspect the NCA header
                // first: only the matching Program needs ExeFS decryption.
                skyline::vfs::NCA header{backing, keyStore, false, true};
                if (header.header.programId != QLauncherProgramId ||
                    header.contentType != skyline::vfs::NcaContentType::Program)
                    continue;
                skyline::vfs::NCA program{backing, keyStore};
                if (!program.exeFs || !program.exeFs->FileExists("rtld") ||
                    !program.exeFs->FileExists("main") ||
                    !program.exeFs->FileExists("main.npdm"))
                    continue;
                return env->NewStringUTF(entry.name.c_str());
            } catch (const std::exception &) {
                // A missing key, bad NCA or unrelated firmware content must
                // not prevent checking the other firmware NCAs.
            }
        }
    } catch (const std::exception &) {
        // Firmware not imported or cannot be opened.
    }
    return nullptr;
}

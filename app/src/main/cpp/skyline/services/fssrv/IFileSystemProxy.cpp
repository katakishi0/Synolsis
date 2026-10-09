// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <os.h>
#include <vfs/os_filesystem.h>
#include <vfs/nca.h>
#include <loader/loader.h>
#include "results.h"
#include "IStorage.h"
#include "IMultiCommitManager.h"
#include "IFileSystemProxy.h"
#include "ISaveDataInfoReader.h"

namespace skyline::service::fssrv {
    IFileSystemProxy::IFileSystemProxy(const DeviceState &state, ServiceManager &manager) : BaseService(state, manager) {}

    Result IFileSystemProxy::SetCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        process = request.Pop<u64>();
        return {};
    }

    Result IFileSystemProxy::OpenSdCardFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(std::make_shared<IFileSystem>(std::make_shared<vfs::OsFileSystem>(state.os->publicAppFilesPath + "/switch/sdmc/"), state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::GetCacheStorageSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(0);
        response.Push<u64>(0);
        return {};
    }

    Result IFileSystemProxy::OpenSaveDataFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto spaceId{request.Pop<SaveDataSpaceId>()};
        auto attribute{request.Pop<SaveDataAttribute>()};

        if (attribute.programId == 0)
            attribute.programId = state.loader->nacp->nacpContents.saveDataOwnerId;

        std::string saveDataPath{[spaceId, &attribute]() {
            std::string spaceIdStr{[spaceId]() {
                switch (spaceId) {
                    case SaveDataSpaceId::System:
                        return "/nand/system";
                    case SaveDataSpaceId::User:
                        return "/nand/user";
                    case SaveDataSpaceId::Temporary:
                        return "/nand/temp";
                    default:
                        throw exception("Unsupported savedata ID: {}", spaceId);
                }
            }()};

            switch (attribute.type) {
                case SaveDataType::System:
                    return fmt::format("{}/save/{:016X}/{:016X}{:016X}/", spaceIdStr, attribute.saveDataId, attribute.userId.lower, attribute.userId.upper);
                case SaveDataType::Account:
                case SaveDataType::Device:
                    return fmt::format("{}/save/{:016X}/{:016X}{:016X}/{:016X}/", spaceIdStr, 0, attribute.userId.lower, attribute.userId.upper, attribute.programId);
                case SaveDataType::Temporary:
                    return fmt::format("{}/{:016X}/{:016X}{:016X}/{:016X}/", spaceIdStr, 0, attribute.userId.lower, attribute.userId.upper, attribute.programId);
                case SaveDataType::Cache:
                    return fmt::format("{}/save/cache/{:016X}/", spaceIdStr, attribute.programId);
                default:
                    throw exception("Unsupported savedata type: {}", attribute.type);
            }
        }()};

        manager.RegisterService(std::make_shared<IFileSystem>(std::make_shared<vfs::OsFileSystem>(state.os->publicAppFilesPath + "/switch" + saveDataPath), state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenReadOnlySaveDataFileSystem(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // Forward to OpenSaveDataFileSystem for now.
        // TODO: This should wrap the underlying filesystem with nn::fs::ReadOnlyFileSystem.
        return OpenSaveDataFileSystem(session, request, response);
    }

    Result IFileSystemProxy::OpenSaveDataInfoReader(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISaveDataInfoReader), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenSaveDataInfoReaderBySaveDataSpaceId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISaveDataInfoReader), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenSaveDataInfoReaderOnlyCacheStorage(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(ISaveDataInfoReader), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        if (!state.loader->romFs)
            return result::NoRomFsAvailable;

        manager.RegisterService(std::make_shared<IStorage>(state.loader->romFs, state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenDataStorageByDataId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto storageId{request.Pop<StorageId>()};
        request.Skip<std::array<u8, 7>>(); // 7-bytes padding
        auto dataId{request.Pop<u64>()};

        auto systemArchivesFileSystem{std::make_shared<vfs::OsFileSystem>(state.os->publicAppFilesPath + "/switch/nand/system/Contents/registered/")};
        auto systemArchives{systemArchivesFileSystem->OpenDirectory("")};

        // Firmware imports may contain extra directories, incomplete NCAs, or
        // archives encrypted with keys not yet available. A single unrelated
        // bad entry must not crash every request for a system RomFS archive.
        // Only create a KeyStore if there is a candidate file to inspect.
        if (systemArchives) {
            std::shared_ptr<skyline::crypto::KeyStore> keyStore;
            for (const auto &entry : systemArchives->Read()) {
                if (entry.type != vfs::Directory::EntryType::File ||
                    !entry.name.ends_with(".nca"))
                    continue;
                try {
                    if (!keyStore)
                        keyStore = std::make_shared<skyline::crypto::KeyStore>(state.os->privateAppFilesPath + "keys");
                    auto backing{systemArchivesFileSystem->OpenFile(entry.name)};
                    auto nca{vfs::NCA(backing, keyStore)};
                    if (nca.header.programId == dataId && nca.romFs) {
                        manager.RegisterService(std::make_shared<IStorage>(nca.romFs, state, manager), session, response);
                        return {};
                    }
                } catch (const std::exception &e) {
                    LOGW("Skipping unreadable firmware archive '{}': {}", entry.name, e.what());
                }
            }
        }

        // Missing system data is normal with partial firmware. Do not hand an
        // empty shared_ptr to IStorage: it would crash on the first GetSize/Read.
        const auto path{fmt::format("romfs/{:016X}", dataId)};
        auto backing{state.os->assetFileSystem->OpenFileUnchecked(path)};
        if (!backing)
            return result::EntityNotFound;

        manager.RegisterService(std::make_shared<IStorage>(std::move(backing), state, manager), session, response);
        return {};
    }

    Result IFileSystemProxy::OpenPatchDataStorageByCurrentProcess(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return result::EntityNotFound;
    }

    Result IFileSystemProxy::GetGlobalAccessLogMode(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u32>(0);
        return {};
    }

    Result IFileSystemProxy::OpenMultiCommitManager(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IMultiCommitManager), session, response);
        return {};
    }
}

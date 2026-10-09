// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "results.h"
#include "IStorage.h"

namespace skyline::service::fssrv {
    IStorage::IStorage(std::shared_ptr<vfs::Backing> backing, const DeviceState &state, ServiceManager &manager) : backing(std::move(backing)), BaseService(state, manager) {}

    Result IStorage::Read(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        auto offset{request.Pop<i64>()};
        auto size{request.Pop<i64>()};

        if (offset < 0) {
            LOGW("Trying to read a file with a negative offset");
            return result::InvalidOffset;
        }

        if (size < 0) {
            LOGW("Trying to read a file with a negative size");
            return result::InvalidSize;
        }

        // Guest IPC provides an explicit byte count. The old implementation
        // ignored it and always read the entire outgoing buffer, which could
        // read past the backing end or expose unwanted bytes. Limit the read
        // independently to request length, output buffer, and remaining file.
        if (request.outputBuf.empty())
            return result::InvalidSize;

        const auto output{request.outputBuf.at(0)};
        const auto position{static_cast<u64>(offset)};
        if (position > backing->size)
            return result::InvalidOffset;

        const auto bytes{std::min({
            static_cast<u64>(size),
            static_cast<u64>(output.size()),
            static_cast<u64>(backing->size - position)
        })};
        if (bytes != 0)
            backing->Read(output.subspan(0, static_cast<size_t>(bytes)), static_cast<size_t>(position));
        return {};
    }

    Result IStorage::GetSize(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        response.Push<u64>(backing->size);
        return {};
    }
}

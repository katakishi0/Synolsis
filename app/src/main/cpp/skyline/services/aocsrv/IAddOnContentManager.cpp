// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <kernel/types/KProcess.h>
#include <os.h>
#include <algorithm>
#include <cstring>
#include "IAddOnContentManager.h"
#include "IPurchaseEventManager.h"

namespace skyline::service::aocsrv {
    IAddOnContentManager::IAddOnContentManager(const DeviceState &state, ServiceManager &manager)
        : BaseService(state, manager),
          addOnContentListChangedEvent(std::make_shared<type::KEvent>(state, false)) {}

    Result IAddOnContentManager::CountAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // Count only readable, decrypted AOC RomFS images registered for
        // this running application. Staged-only NSP files do not qualify.
        const auto processId{request.Pop<u64>()}; // aoc:u command 2 carries an id/pid
        (void)processId;
        response.Push<u32>(static_cast<u32>(state.os->mountedAddOns.Count()));
        return {};
    }

    Result IAddOnContentManager::ListAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto startIndex{request.Pop<u32>()};
        const auto requestedCount{request.Pop<u32>()};
        const auto processId{request.Pop<u64>()}; // command 3 also accepts id/pid
        (void)processId;
        if (request.outputBuf.empty()) {
            response.Push<u32>(0);
            return {};
        }

        const auto output{request.outputBuf.at(0)};
        const auto capacity{output.size() / sizeof(u32)};
        const auto amount{std::min<std::size_t>(requestedCount, capacity)};
        const auto indexes{state.os->mountedAddOns.List(startIndex, amount)};
        for (std::size_t i{}; i < indexes.size(); ++i)
            std::memcpy(output.data() + i * sizeof(u32), &indexes[i], sizeof(u32));
        response.Push<u32>(static_cast<u32>(indexes.size()));
        return {};
    }

    Result IAddOnContentManager::GetAddOnContentBaseId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        const auto processId{request.Pop<u64>()};
        (void)processId;
        response.Push<u64>(MountedAddOnRegistry::BaseId(state.os->mountedAddOns.Application()));
        return {};
    }

    Result IAddOnContentManager::PrepareAddOnContent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        // Backings have already been opened at game launch. Do not pretend
        // an unknown/unmounted DLC was installed by a Prepare request.
        const auto index{request.Pop<u32>()};
        const auto processId{request.Pop<u64>()};
        (void)processId;
        if (!state.os->mountedAddOns.HasIndex(index))
            LOGW("Guest requested unavailable DLC index {}", index);
        return {};
    }

    Result IAddOnContentManager::GetAddOnContentListChangedEvent(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle{state.process->InsertItem(addOnContentListChangedEvent)};
        LOGD("Add On Content List Changed Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAddOnContentManager::GetAddOnContentListChangedEventWithProcessId(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        KHandle handle{state.process->InsertItem(addOnContentListChangedEvent)};
        LOGD("Add On Content List Changed Event Handle: 0x{:X}", handle);

        response.copyHandles.push_back(handle);
        return {};
    }

    Result IAddOnContentManager::CheckAddOnContentMountStatus(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        return {};
    }

    Result IAddOnContentManager::CreateEcPurchasedEventManager(type::KSession &session, ipc::IpcRequest &request, ipc::IpcResponse &response) {
        manager.RegisterService(SRVREG(IPurchaseEventManager), session, response);
        return {};
    }
}

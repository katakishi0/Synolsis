// SPDX-License-Identifier: MIT OR MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <soc.h>
#include <services/nvdrv/devices/deserialisation/deserialisation.h>
#include "host1x_channel.h"
#include <soc/host1x/video_command_probe.h>
#include <vector>

namespace skyline::service::nvdrv::device::nvhost {
    Host1xChannel::Host1xChannel(const DeviceState &state,
                                 Driver &driver,
                                 Core &core,
                                 const SessionContext &ctx,
                                 core::ChannelType channelType)
        : NvDevice(state, driver, core, ctx),
          channelType(channelType) {
        state.soc->host1x.channels[static_cast<size_t>(channelType)].Start();
    }

    PosixResult Host1xChannel::SetNvmapFd(In<FileDescriptor> fd) {
        LOGD("fd: {}", fd);
        return PosixResult::Success;
    }

    PosixResult Host1xChannel::Submit(span<SubmitCmdBuf> cmdBufs,
                                      span<SubmitReloc> relocs, span<u32> relocShifts,
                                      span<SubmitSyncpointIncr> syncpointIncrs, span<u32> fenceThresholds) {
        LOGD("numCmdBufs: {}, numRelocs: {}, numSyncpointIncrs: {}, numFenceThresholds: {}",
                            cmdBufs.size(), relocs.size(), syncpointIncrs.size(), fenceThresholds.size());

        if (fenceThresholds.size() > syncpointIncrs.size())
            return PosixResult::InvalidArgument;

        if (!relocs.empty())
            // The relocation path is not implemented; return a guest error
            // instead of terminating the emulator with a host exception.
            return PosixResult::InvalidArgument;

        std::scoped_lock lock(channelMutex);

        // Validate all gather ranges *before* advancing any guest syncpoints.
        // NVDEC/VIC command execution remains unimplemented; these checks
        // prevent malformed/unsupported submits from reading arbitrary RAM.
        for (const auto &cmdBuf : cmdBufs) {
            auto handleDesc{core.nvMap.GetHandle(cmdBuf.mem)};
            if (!handleDesc || !handleDesc->address ||
                (cmdBuf.offset & 3U) || cmdBuf.offset > handleDesc->size ||
                static_cast<u64>(cmdBuf.words) * sizeof(u32) >
                    handleDesc->size - cmdBuf.offset)
                return PosixResult::InvalidArgument;
        }
        for (const auto &incr : syncpointIncrs) {
            if (incr.syncpointId >= soc::host1x::SyncpointCount)
                return PosixResult::InvalidArgument;
        }

        // Execute ONLY the NVDEC/VIC THI method register subset before the
        // guest-visible syncpoint acknowledgement. Prior builds discarded
        // the entire video pushbuffer, leaving real H264/VIC implementations
        // unreachable. Running in-place avoids retaining freed guest spans.
        // Host1x syncpoint method 0 is deliberately not forwarded here:
        // nvhost already acknowledges the submitted count below.
        auto &videoChannel{state.soc->host1x.channels[static_cast<size_t>(channelType)]};
        soc::host1x::video_probe::Summary videoSummary{};
        try {
            std::vector<std::span<const u32>> gathers;
            if (cmdBufs.size() > 128)
                return PosixResult::InvalidArgument;
            gathers.reserve(cmdBufs.size());
            for (const auto &cmdBuf : cmdBufs) {
                const auto handleDesc{core.nvMap.GetHandle(cmdBuf.mem)};
                const auto gatherAddress{handleDesc->address + cmdBuf.offset};
                gathers.emplace_back(
                    reinterpret_cast<const u32 *>(gatherAddress), cmdBuf.words);
            }
            // The complete ioctl's buffers must be valid as one unit.
            // A truncated later gather must not partially decode the first.
            if (!videoChannel.DispatchVideoGathersAtomically(gathers, videoSummary))
                return PosixResult::InvalidArgument;
        } catch (const std::exception &e) {
            LOGW("Video guest pushbuffer dispatch rejected: {}", e.what());
            return PosixResult::InvalidArgument;
        }
        if (videoSummary.nvdecExecutions || videoSummary.vicExecutions) {
            static std::atomic<std::uint64_t> videoSubmitCount{};
            const auto serial{videoSubmitCount.fetch_add(1, std::memory_order_relaxed) + 1};
            if (serial <= 4 || (serial & (serial - 1)) == 0)
                LOGI("Synolsis video command dispatch #{}: NVDEC executions={} codec=0x{:X} VIC executions={}",
                     serial, videoSummary.nvdecExecutions, videoSummary.lastNvdecCodec,
                     videoSummary.vicExecutions);
        }
        if (videoSummary.truncatedOrUnsupported) {
            static std::atomic<std::uint64_t> skippedSubmitCount{};
            const auto serial{skippedSubmitCount.fetch_add(1, std::memory_order_relaxed) + 1};
            if (serial <= 4 || (serial & (serial - 1)) == 0)
                LOGW("Video pushbuffer contained truncated or unsupported Host1x packet (sample #{})", serial);
        }
        // This is still a compatibility acknowledgement, not a claim that
        // decoded frames reached the game. Accurate per-operation fences and
        // asynchronous video scheduling require further integration.
        for (size_t i{}; i < syncpointIncrs.size(); i++) {
            const auto &incr{syncpointIncrs[i]};
            u32 max{core.syncpointManager.IncrementSyncpointMaxExt(incr.syncpointId, incr.numIncrs)};
            for (size_t j{}; j < incr.numIncrs; j++)
                state.soc->host1x.syncpoints[incr.syncpointId].Increment();
            if (i < fenceThresholds.size())
                fenceThresholds[i] = max;
        }

        return PosixResult::Success;
    }

    PosixResult Host1xChannel::GetSyncpoint(In<u32> channelSyncpointIdx, Out<u32> syncpointId) {
        LOGD("channelSyncpointIdx: {}", channelSyncpointIdx);

        if (channelSyncpointIdx > 0)
            return PosixResult::InvalidArgument;

        u32 id{core::SyncpointManager::ChannelSyncpoints[static_cast<u32>(channelType)]};
        if (!id || id >= soc::host1x::SyncpointCount) {
            // Never hand an invalid syncpoint back to the guest; downstream
            // host1x code indexes a fixed-size array with this ID.
            return PosixResult::InvalidArgument;
        }

        LOGD("syncpointId: {}", id);
        syncpointId = id;
        return PosixResult::Success;
    }

    PosixResult Host1xChannel::GetWaitBase(In<core::ChannelType> pChannelType, Out<u32> waitBase) {
        LOGD("channelType: {}", static_cast<u32>(pChannelType));
        waitBase = 0;
        return PosixResult::Success;
    }

    PosixResult Host1xChannel::SetSubmitTimeout(In<u32> timeout) {
        LOGD("timeout: {}", timeout);
        return PosixResult::Success;
    }

    PosixResult Host1xChannel::MapBuffer(u8 compressed, span<BufferHandle> handles) {
        LOGD("compressed: {}", compressed);

        for (auto &bufferHandle : handles) {
            bufferHandle.address = core.nvMap.PinHandle(bufferHandle.handle);
            LOGD("handle: {}, address: 0x{:X}", bufferHandle.handle, bufferHandle.address);
        }

        return PosixResult::Success;
    }

    PosixResult Host1xChannel::UnmapBuffer(u8 compressed, span<BufferHandle> handles) {
        LOGD("compressed: {}", compressed);

        for (auto &bufferHandle : handles) {
            core.nvMap.UnpinHandle(bufferHandle.handle);
            LOGD("handle: {}", bufferHandle.handle);
        }

        return PosixResult::Success;
    }

// @fmt:off
#include <services/nvdrv/devices/deserialisation/macro_def.inc>
    static constexpr u32 Host1xChannelMagic{0x00};
    static constexpr u32 GpuChannelMagic{0x48}; //!< Used for SetNvmapFd which is needed in both GPU and host1x channels

    VARIABLE_IOCTL_HANDLER_FUNC(Host1xChannel,
        IOCTL_CASE_ARGS(IN,    SIZE(0x4), MAGIC(GpuChannelMagic),    FUNC(0x1),
                        SetNvmapFd,       ARGS(In<FileDescriptor>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x8), MAGIC(Host1xChannelMagic), FUNC(0x2),
                        GetSyncpoint,     ARGS(In<u32>, Out<u32>))
        IOCTL_CASE_ARGS(INOUT, SIZE(0x8), MAGIC(Host1xChannelMagic), FUNC(0x3),
                        GetWaitBase,      ARGS(In<core::ChannelType>, Out<u32>))
        IOCTL_CASE_ARGS(IN,    SIZE(0x4), MAGIC(Host1xChannelMagic), FUNC(0x7),
                        SetSubmitTimeout, ARGS(In<u32>))
    ,
        VARIABLE_IOCTL_CASE_ARGS(INOUT, MAGIC(Host1xChannelMagic), FUNC(0x1),
                                 Submit,      ARGS(Save<u32, 0>, Save<u32, 1>, Save<u32, 2>, Save<u32, 3>,
                                                   SlotSizeSpan<SubmitCmdBuf, 0>,
                                                   SlotSizeSpan<SubmitReloc, 1>, SlotSizeSpan<u32, 1>,
                                                   SlotSizeSpan<SubmitSyncpointIncr, 2>, SlotSizeSpan<u32, 3>))
        VARIABLE_IOCTL_CASE_ARGS(INOUT, MAGIC(Host1xChannelMagic), FUNC(0x9),
                                 MapBuffer,   ARGS(Save<u32, 0>, Pad<u32>, In<u8>, Pad<u8, 3>, SlotSizeSpan<BufferHandle, 0>))
        VARIABLE_IOCTL_CASE_ARGS(INOUT, MAGIC(Host1xChannelMagic), FUNC(0xA),
                                 UnmapBuffer, ARGS(Save<u32, 0>, Pad<u32>, In<u8>, Pad<u8, 3>, SlotSizeSpan<BufferHandle, 0>))
    )
#include <services/nvdrv/devices/deserialisation/macro_undef.inc>
// @fmt:on
}

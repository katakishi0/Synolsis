// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include "video_command_probe.h"

namespace skyline::soc::host1x::video_probe {
    // The guest can submit multiple nvhost gather buffers in one ioctl.
    // Checking just one buffer at a time can invoke NVDEC before a malformed
    // later buffer is discovered. Inspect ALL syntax on a private state copy
    // while no real decoder callbacks are visible, then commit in order.
    // This promises syntax-level atomicity, not transactional rollback of
    // hardware/MediaCodec failures once valid commands are executing.
    template<typename Sink>
    bool DispatchBatch(VideoCommandProbe &live,
                       std::span<const std::span<const std::uint32_t>> gathers,
                       Summary &out, Sink &&sink) {
        constexpr std::size_t MaxBuffers{128};
        constexpr std::size_t MaxWords{262144};
        if (gathers.size() > MaxBuffers) {
            out.truncatedOrUnsupported = true;
            return false;
        }

        std::size_t total{};
        for (const auto gather : gathers) {
            if (gather.size() > MaxWords - total) {
                out.truncatedOrUnsupported = true;
                return false;
            }
            total += gather.size();
        }

        auto staged{live};
        Summary stagingSummary{};
        for (const auto gather : gathers) {
            if (!staged.Inspect(gather, stagingSummary)) {
                out.truncatedOrUnsupported = true;
                return false;
            }
        }

        for (const auto gather : gathers)
            if (!live.Inspect(gather, out, sink)) {
                // Unreachable for identical immutable buffers after a
                // successful dry run; report failure rather than silently
                // advancing guest syncpoints if execution state diverged.
                out.truncatedOrUnsupported = true;
                return false;
            }
        return true;
    }
}

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include "frame_interpolation_planner.h"

namespace skyline::gpu::framegen {
    // A lightweight history of *actual guest frame submissions*. No VkImage
    // is retained here, so this tracker cannot pin GPU memory on 6 GiB phones.
    // Until image snapshots, synchronized descriptor sets and a compute
    // presenter exist, an observed candidate must NOT be presented as an
    // interpolated frame or counted as extra FPS.
    struct SubmittedFrame {
        std::uint64_t id{};
        std::int64_t hostNs{};
        std::uint32_t width{}, height{}, format{};
        std::uintptr_t sourceIdentity{};
    };

    class FrameHistory {
        std::optional<SubmittedFrame> previous;
        std::uint64_t acceptedPairs{};
        std::uint64_t rejectedPairs{};

      public:
        constexpr void Reset() { previous.reset(); }
        constexpr std::uint64_t AcceptedPairs() const { return acceptedPairs; }
        constexpr std::uint64_t RejectedPairs() const { return rejectedPairs; }

        // Triply buffered RGBA8: history A + history B + synthesized target.
        // Capped independently of the caller's Vulkan/Android memory budget.
        static constexpr bool FitsBudget(std::uint32_t width, std::uint32_t height,
                                         std::uint64_t maxBytes = 48ULL * 1024 * 1024) {
            if (!width || !height || width > 8192 || height > 8192)
                return false;
            const std::uint64_t need =
                static_cast<std::uint64_t>(width) * height * 4ULL * 3ULL;
            return need <= maxBytes;
        }

        constexpr std::optional<FramePlan> Observe(SubmittedFrame frame) {
            if (!frame.id || frame.hostNs <= 0 ||
                !frame.sourceIdentity || !frame.format ||
                !FitsBudget(frame.width, frame.height)) {
                ++rejectedPairs;
                Reset();
                return std::nullopt;
            }

            const auto old = previous;
            previous = frame;
            if (!old)
                return std::nullopt;

            // Frame history needs *two separate completed snapshots*. A
            // game can reuse a texture address for a new frame, but until
            // the GPU copies its old contents, such a pair is unsafe.
            if (old->id >= frame.id || old->sourceIdentity == frame.sourceIdentity ||
                old->width != frame.width || old->height != frame.height ||
                old->format != frame.format) {
                ++rejectedPairs;
                return std::nullopt;
            }

            const auto plan = Midpoint(old->hostNs, frame.hostNs);
            if (!plan) {
                ++rejectedPairs;
                return std::nullopt;
            }
            ++acceptedPairs;
            return plan;
        }
    };
}

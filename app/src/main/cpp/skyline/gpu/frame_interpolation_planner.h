// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>
#include <optional>

namespace skyline::gpu::framegen {
    // Pure planning for one *synthetic presentation frame*. Game simulation
    // FPS is NEVER incremented by this plan. The actual producer requires
    // previous/current Vulkan images and synchronized presentation.
    struct FramePlan {
        std::int64_t previousNs{};
        std::int64_t generatedNs{};
        std::int64_t currentNs{};
        float blendAlpha{};
    };

    constexpr std::optional<FramePlan> Midpoint(
        std::int64_t previousNs, std::int64_t currentNs,
        std::int64_t minFrameIntervalNs = 8'000'000,
        std::int64_t maxFrameIntervalNs = 60'000'000) {
        if (previousNs < 0 || currentNs <= previousNs || minFrameIntervalNs <= 0 ||
            maxFrameIntervalNs < minFrameIntervalNs)
            return std::nullopt;
        // Subtraction cannot overflow if timestamps are ordered and >=0.
        const auto difference = currentNs - previousNs;
        if (difference < minFrameIntervalNs || difference > maxFrameIntervalNs)
            return std::nullopt;
        const auto half = difference / 2;
        if (!half)
            return std::nullopt;
        return FramePlan{previousNs, previousNs + half, currentNs, 0.5f};
    }

    // A host renderer may request intermediates only after both consecutive
    // source images exist and their GPU fences have signaled. This is a gate,
    // not a fake "FPS hack" that duplicates frames unconditionally.
    constexpr bool CanSubmit(bool previousReady, bool currentReady,
                             bool outputAvailable, bool historyValid) {
        return previousReady && currentReady && outputAvailable && historyValid;
    }
}

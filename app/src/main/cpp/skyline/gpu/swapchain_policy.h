// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>

namespace skyline::gpu::swapchain_policy {
    // Surface capabilities expose bitmasks; a requested enum value must be a
    // SINGLE supported bit, not merely an arbitrary nonzero intersection.
    // This helper is Vulkan-independent so the policy can be unit-tested on
    // host CI without installing Vulkan headers.
    constexpr std::uint32_t ChooseSupportedFlag(std::uint32_t supported,
                                                 std::uint32_t preferred,
                                                 std::uint32_t fallback) {
        const auto isSingleBit{[](std::uint32_t v) {
            return v && !(v & (v - 1));
        }};
        if (isSingleBit(preferred) && (supported & preferred))
            return preferred;
        if (isSingleBit(fallback) && (supported & fallback))
            return fallback;
        // All VkSurfaceTransform and VkCompositeAlpha bits represent
        // independent options; choose a deterministic last-resort value.
        return supported & (~supported + 1u);
    }
}

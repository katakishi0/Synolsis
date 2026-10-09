// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once
#include <cstdint>

namespace skyline::gpu::surface_policy {
    // Vulkan swapchain preTransform/compositeAlpha must be members of the
    // capability bitmasks. Use identity where available because the Android
    // native-window transform is applied separately by the presentation path.
    constexpr std::uint32_t PickSupportedBit(std::uint32_t supported,
                                             std::uint32_t preferred,
                                             std::uint32_t fallback) {
        if (supported & preferred)
            return preferred;
        if (supported & fallback)
            return fallback;
        // Last resort: select the lowest advertised flag, never invent one.
        return supported & (~supported + 1U);
    }

    // VkSurfaceTransformFlagBitsKHR: IDENTITY = 0x1.
    constexpr std::uint32_t ChoosePreTransform(std::uint32_t supported,
                                               std::uint32_t current) {
        return PickSupportedBit(supported, 0x1U, current);
    }

    // VkCompositeAlphaFlagBitsKHR: OPAQUE = 0x1, INHERIT = 0x8.
    constexpr std::uint32_t ChooseCompositeAlpha(std::uint32_t supported) {
        return PickSupportedBit(supported, 0x8U, 0x1U);
    }
}

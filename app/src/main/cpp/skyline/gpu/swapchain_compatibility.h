// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>
#include <initializer_list>

namespace skyline::gpu::swapchain_compatibility {
    // Raw Vulkan KHR flag values. This small pure helper also runs under
    // desktop C++ CI without requiring the Android NDK or Vulkan loader.
    constexpr std::uint32_t ChooseCompositeAlphaBits(std::uint32_t supported) {
        // Opaque is the portable Android choice; INHERIT is not universally
        // advertised, despite being hardcoded in upstream Strato.
        for (std::uint32_t bit : {1U, 8U, 2U, 4U})
            if (supported & bit)
                return bit;
        return 0;
    }

    constexpr std::uint32_t ChoosePreTransformBits(
        std::uint32_t supported, std::uint32_t current) {
        // Keep the existing Android surface transform handling where possible.
        if (supported & 1U) // VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            return 1U;
        if (current && (supported & current) == current)
            return current;
        // Spec expects at least one supported transform. Return 0 for broken
        // drivers instead of passing an invalid flag into vkCreateSwapchainKHR.
        return supported & (0U - supported);
    }
}

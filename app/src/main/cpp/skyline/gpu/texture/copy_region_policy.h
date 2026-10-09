// SPDX-License-Identifier: MPL-2.0
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace skyline::gpu::texture::copy_region_policy {
    constexpr std::uint32_t Remaining{std::numeric_limits<std::uint32_t>::max()};

    struct Extent {
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t depth;
    };

    constexpr std::uint32_t MipDimension(std::uint32_t base, std::uint32_t mip) {
        return std::max(1u, mip >= 32 ? 0u : base >> mip);
    }

    constexpr Extent MipExtent(Extent original, std::uint32_t mip) {
        return {
            MipDimension(original.width, mip),
            MipDimension(original.height, mip),
            MipDimension(original.depth, mip),
        };
    }

    // Vulkan count may be VK_REMAINING_MIP_LEVELS or VK_REMAINING_ARRAY_LAYERS;
    // validate before subtracting to avoid underflow and out-of-bounds blits.
    constexpr bool ValidRange(std::uint32_t available, std::uint32_t base,
                              std::uint32_t count) {
        if (base >= available)
            return false;
        return count == Remaining || (count > 0 && count <= available - base);
    }

    constexpr std::uint32_t RangeEnd(std::uint32_t available, std::uint32_t base,
                                     std::uint32_t count) {
        return count == Remaining ? available : base + count;
    }

    constexpr std::uint32_t RangeCount(std::uint32_t available, std::uint32_t base,
                                       std::uint32_t count) {
        return RangeEnd(available, base, count) - base;
    }
}

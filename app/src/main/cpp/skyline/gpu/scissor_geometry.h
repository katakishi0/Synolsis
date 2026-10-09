// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>

namespace skyline::gpu::scissor_geometry {
    // Keep Maxwell rectangle calculations in signed 64-bit until they have
    // been intersected with actual render target bounds. Converting a
    // negative width to uint32_t produces extremely large Vulkan rectangles.
    struct Rect {
        std::int64_t left{}, top{}, right{}, bottom{};
        constexpr bool operator==(const Rect &) const = default;
    };

    constexpr Rect FromOriginSize(std::int64_t x, std::int64_t y,
                                  std::uint32_t width, std::uint32_t height) {
        return {x, y, x + width, y + height};
    }

    constexpr Rect FromBounds(std::int64_t left, std::int64_t top,
                              std::int64_t right, std::int64_t bottom) {
        return {left, top, std::max(left, right), std::max(top, bottom)};
    }

    constexpr Rect Intersect(Rect a, Rect b) {
        const auto left{std::max(a.left, b.left)};
        const auto top{std::max(a.top, b.top)};
        return {left, top,
                std::max(left, std::min(a.right, b.right)),
                std::max(top, std::min(a.bottom, b.bottom))};
    }

    constexpr Rect ClipToExtent(Rect rect, std::uint32_t width,
                                std::uint32_t height) {
        const auto left{std::clamp<std::int64_t>(rect.left, 0, width)};
        const auto top{std::clamp<std::int64_t>(rect.top, 0, height)};
        return {left, top,
                std::clamp<std::int64_t>(rect.right, left, width),
                std::clamp<std::int64_t>(rect.bottom, top, height)};
    }

    constexpr Rect ClipToPositive(Rect rect) {
        // Vulkan scissor offsets must be >= 0. Preserve the right/bottom
        // edges rather than shifting a negative rectangle into view.
        constexpr auto limit{std::numeric_limits<std::int32_t>::max()};
        const auto left{std::clamp<std::int64_t>(rect.left, 0, limit)};
        const auto top{std::clamp<std::int64_t>(rect.top, 0, limit)};
        return {left, top,
                std::clamp<std::int64_t>(rect.right, left, limit),
                std::clamp<std::int64_t>(rect.bottom, top, limit)};
    }

    struct VulkanScissor {
        std::int32_t x{}, y{};
        std::uint32_t width{}, height{};
        constexpr bool operator==(const VulkanScissor &) const = default;
    };

    constexpr VulkanScissor MakeScissor(Rect rect) {
        rect = ClipToPositive(rect);
        return {
            static_cast<std::int32_t>(rect.left),
            static_cast<std::int32_t>(rect.top),
            static_cast<std::uint32_t>(rect.right - rect.left),
            static_cast<std::uint32_t>(rect.bottom - rect.top)
        };
    }
}

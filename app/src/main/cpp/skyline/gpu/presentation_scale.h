// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <cstdint>
#include "render_scale_geometry.h"

namespace skyline::gpu::presentation_scale {
    using scale::Extent;

    struct Rect {
        std::int32_t left{}, top{}, right{}, bottom{};
        constexpr bool operator==(const Rect &) const = default;
    };

    // Only final host-present images are scaled here. Guest render targets,
    // emulated viewport/scissor and CPU-visible textures remain untouched.
    constexpr Extent ChooseExtent(Extent original, std::uint32_t percent,
                                  Extent surfaceMin, Extent surfaceMax,
                                  bool canBlit) {
        if (!canBlit || (percent != 50 && percent != 75) ||
            !original.width || !original.height)
            return original;
        const auto factor = percent == 50 ? scale::Factor::Half : scale::Factor::ThreeQuarters;
        const auto requested = scale::ScaleExtent(original, factor);
        if (requested.width < surfaceMin.width ||
            requested.height < surfaceMin.height ||
            requested.width > surfaceMax.width ||
            requested.height > surfaceMax.height)
            return original;
        return requested;
    }

    // Android crop coordinates refer to the *swapchain buffer*, so they must
    // follow the Vulkan blit output dimensions. Clamp invalid guest crops.
    constexpr Rect ScaleCrop(Rect input, Extent source, Extent destination) {
        if (!source.width || !source.height || !destination.width || !destination.height)
            return {};
        const auto coord = [](std::int32_t value, std::uint32_t src,
                              std::uint32_t dst, bool upper) -> std::int32_t {
            const auto clamped = static_cast<std::uint64_t>(
                std::clamp<std::int64_t>(value, 0, src));
            const auto scaled = clamped * dst + (upper ? src - 1 : 0);
            return static_cast<std::int32_t>(std::min<std::uint64_t>(
                scaled / src, dst));
        };
        const auto left = coord(input.left, source.width, destination.width, false);
        const auto top = coord(input.top, source.height, destination.height, false);
        return {left, top,
                std::max(left, coord(input.right, source.width, destination.width, true)),
                std::max(top, coord(input.bottom, source.height, destination.height, true))};
    }
}

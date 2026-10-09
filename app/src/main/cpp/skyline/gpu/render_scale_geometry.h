// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>

namespace skyline::gpu::scale {
    // Pure render-target geometry. This does not alter guest textures,
    // shader viewport state or Vulkan allocations until those paths are
    // integrated and tested together. Native (100%) remains the only live
    // renderer mode; do not expose the others as working UI settings.
    enum class Factor : std::uint32_t {
        Half = 50,
        ThreeQuarters = 75,
        Native = 100,
    };

    struct Extent {
        std::uint32_t width{};
        std::uint32_t height{};

        constexpr bool operator==(const Extent &) const = default;
    };

    constexpr std::uint32_t ScaleDimension(std::uint32_t dimension, Factor factor) {
        if (!dimension)
            return 0;

        // 64-bit intermediate avoids overflow for large/bad guest extents.
        // Ceil instead of truncate preserves non-zero odd-size render targets.
        const auto percent = static_cast<std::uint32_t>(factor);
        return static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(dimension) * percent + 99u) / 100u
        );
    }

    constexpr Extent ScaleExtent(Extent source, Factor factor) {
        return {ScaleDimension(source.width, factor),
                ScaleDimension(source.height, factor)};
    }
}

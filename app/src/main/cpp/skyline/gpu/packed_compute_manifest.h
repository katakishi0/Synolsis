// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string_view>

namespace skyline::gpu::packed_compute {
    enum class Kind : std::uint8_t { Easu, Rcas, Temporal };

    struct Asset {
        Kind kind;
        std::string_view path;
        bool usesPushConstants;
    };

    constexpr std::array<Asset, 3> Assets{{
        {Kind::Easu, "shaders/synolsis/easu.spv", false},
        {Kind::Rcas, "shaders/synolsis/rcas.spv", false},
        {Kind::Temporal, "shaders/synolsis/temporal_interpolate.spv", true},
    }};

    constexpr bool ValidSpirv(std::span<const std::uint32_t> words) {
        // header = magic, version, generator, ID bound, schema.
        // Driver/SPIRV-Tools performs full semantic validation at build time;
        // this cheap guard bounds runtime allocations and malformed assets.
        return words.size() >= 5 && words.size() <= 262144 &&
               words[0] == 0x07230203U &&
               (words[1] & 0xFF000000U) == 0U &&
               (words[1] & 0x0000FF00U) <= 0x00000600U &&
               words[3] > 0U && words[3] <= 1048576U &&
               words[4] == 0U;
    }
}

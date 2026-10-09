// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstdint>

namespace skyline::diagnostics {
    // Sample initial events and then powers of two. Keep extremely hot GPU
    // debug output useful without flooding Android storage or adding
    // formatting costs to hundreds of thousands of guest commands.
    constexpr bool ShouldSample(std::uint64_t sequence) {
        return sequence > 0 &&
            (sequence <= 8 || (sequence & (sequence - 1)) == 0);
    }
}

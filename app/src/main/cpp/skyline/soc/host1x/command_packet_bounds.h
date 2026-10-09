// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>

namespace skyline::soc::host1x::command_packet {
    // A header is already consumed when this is called. The requested
    // guest-controlled payload count must not exceed the gather's remaining
    // mapped word span.
    constexpr bool Fits(std::size_t remainingWords, std::size_t requiredWords) {
        return requiredWords <= remainingWords;
    }

    constexpr std::size_t MaskWords(std::uint32_t mask) {
        return std::popcount(mask);
    }
}

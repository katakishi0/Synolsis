// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace skyline::vfs::bktr {
    // BKTR physical AES-CTR: subsection IV replaces bytes [4,8), while
    // [8,16) holds the absolute NCA physical offset in 16-byte blocks.
    // The first four IV bytes originate from the section secure value.
    constexpr std::array<std::uint8_t, 16> Counter(
        std::array<std::uint8_t, 16> iv,
        std::uint32_t subsectionCtr,
        std::uint64_t absoluteByteOffset) {
        for (int i = 7; i >= 4; --i) {
            iv[static_cast<std::size_t>(i)] =
                static_cast<std::uint8_t>(subsectionCtr & 0xff);
            subsectionCtr >>= 8;
        }
        auto block = absoluteByteOffset >> 4;
        for (int i = 15; i >= 8; --i) {
            iv[static_cast<std::size_t>(i)] =
                static_cast<std::uint8_t>(block & 0xff);
            block >>= 8;
        }
        return iv;
    }
}

// SPDX-License-Identifier: MPL-2.0
#pragma once
#include <cstdint>
#include <span>
namespace skyline::gpu::packed_compute {
constexpr bool ValidSpirv(std::span<const std::uint32_t> words) {
    return words.size() >= 5 && words.size() <= 262144 && words[0] == 0x07230203U && words[3] != 0U && words[4] == 0U;
}
}

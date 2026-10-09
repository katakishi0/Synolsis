// SPDX-License-Identifier: MPL-2.0
// Host-only, deterministic AES-CTR-Ex IV tests. No firmware/keys required.
#include "../app/src/main/cpp/skyline/vfs/bktr_ctr_counter.h"
#include <array>
#include <cassert>
#include <cstdint>
using skyline::vfs::bktr::Counter;
constexpr std::array<std::uint8_t, 16> seed = {
    0x12,0x34,0x56,0x78,0xAA,0xBB,0xCC,0xDD,0,0,0,0,0,0,0,0
};
constexpr auto first = Counter(seed, 0xA1B2C3D4, 0x123456789ABCDE0ULL);
static_assert(first == std::array<std::uint8_t,16>{
    0x12,0x34,0x56,0x78,0xA1,0xB2,0xC3,0xD4,0x00,0x01,0x23,0x45,0x67,0x89,0xAB,0xCD});
static_assert(Counter(seed, 0, 0)[0] == 0x12);
static_assert(Counter(seed, 0, 0)[4] == 0);
static_assert(Counter(seed, 0, 16)[15] == 1);
static_assert(Counter(seed, 0x01020304, 0)[4] == 1);
static_assert(Counter(seed, 0x01020304, 0)[7] == 4);
int main() {
    for (std::uint64_t n=0; n < 100000; n += 37) {
        const auto iv=Counter(seed, 0x02030405, n*16);
        assert(iv[4]==2 && iv[5]==3 && iv[6]==4 && iv[7]==5);
        const std::uint64_t counter =
            (std::uint64_t(iv[12])<<24) | (std::uint64_t(iv[13])<<16) |
            (std::uint64_t(iv[14])<<8) | iv[15];
        assert(counter == n);
    }
}

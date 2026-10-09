// SPDX-License-Identifier: MPL-2.0
// Synthetic BKTR boundary checks, without game data or keys.
#include "../app/src/main/cpp/skyline/vfs/bktr_relocation.h"
#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    std::vector<std::uint8_t> bytes(0x8000);
    const auto put32 = [&](std::size_t at, std::uint32_t n) {
        for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<std::uint8_t>(n >> (i * 8));
    };
    const auto put64 = [&](std::size_t at, std::uint64_t n) {
        for (unsigned i = 0; i < 8; ++i) bytes[at + i] = static_cast<std::uint8_t>(n >> (i * 8));
    };
    put32(4, 1);
    put64(8, 0x2000);
    put64(0x10, 0);
    put32(0x4004, 2);
    put64(0x4008, 0x2000);
    put64(0x4010, 0);
    put32(0x401c, 5);
    put64(0x4020, 0x1000);
    put32(0x402c, 9);
    auto parsed = skyline::vfs::bktr::SubsectionTable::Parse(bytes);
    assert(parsed && parsed->EntryCount() == 2);
    auto first = parsed->Resolve(0xfff);
    auto second = parsed->Resolve(0x1000);
    assert(first && first->size == 1 && first->counter == 5);
    assert(second && second->size == 0x1000 && second->counter == 9);
    assert(!parsed->Resolve(0x2000));
}

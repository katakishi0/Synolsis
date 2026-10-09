// SPDX-License-Identifier: MPL-2.0
// Synthetic decrypted CNMT fixtures: no third-party ROM, firmware or keys.
#include "../app/src/main/cpp/skyline/vfs/content_meta.h"
#include <cassert>
#include <cstdint>
#include <vector>

using skyline::vfs::content_meta::BelongsToApplication;
using skyline::vfs::content_meta::ParseManifest;
using skyline::vfs::content_meta::Type;

static void Put16(std::vector<std::uint8_t> &b, std::size_t p, std::uint16_t v) {
    b.at(p) = static_cast<std::uint8_t>(v);
    b.at(p+1) = static_cast<std::uint8_t>(v >> 8);
}
static void Put64(std::vector<std::uint8_t> &b, std::size_t p, std::uint64_t v) {
    for (int i{}; i < 8; ++i) b.at(p+i) = static_cast<std::uint8_t>(v >> (8*i));
}
static std::vector<std::uint8_t> MakePatch() {
    constexpr std::size_t ext{0x18};
    constexpr std::size_t base{0x20 + ext};
    std::vector<std::uint8_t> b(base + 0x38*2 + 0x10 + 0x20);
    Put64(b, 0, 0x0100567800000800ULL);
    b.at(0xC) = 0x81;
    Put16(b, 0xE, ext);
    Put16(b, 0x10, 2);
    Put16(b, 0x12, 1);
    Put64(b, 0x20, 0x0100567800000000ULL);
    for (int idx{}; idx < 2; idx++) {
        const auto at = base + 0x38*idx;
        for (int i{}; i < 32; i++) b[at+i] = static_cast<std::uint8_t>(i+idx);
        for (int i{}; i < 16; i++) b[at+0x20+i] = static_cast<std::uint8_t>(i+1+idx);
        b[at+0x30] = static_cast<std::uint8_t>(0x5a + idx);
        b[at+0x31] = 1;
        b[at+0x32] = 2;
        b[at+0x33] = 3;
        b[at+0x34] = 4;
        b[at+0x35] = 5;
        b[at+0x36] = idx == 0 ? 1 : 3;
        b[at+0x37] = static_cast<std::uint8_t>(idx);
    }
    return b;
}
int main() {
    constexpr std::uint64_t App{0x0100567800000000ULL};
    const auto data = MakePatch();
    const auto result = ParseManifest(data);
    assert(result);
    assert(result->summary.type == Type::Patch);
    assert(BelongsToApplication(*result, App));
    assert(!BelongsToApplication(*result, App+1));
    assert(!BelongsToApplication(*result, 0));
    assert(result->contents.size() == 2);
    assert(result->contents[0].type == 1);
    assert(result->contents[1].type == 3);
    assert(result->contents[0].sha256[31] == 31);
    assert(result->contents[1].idOffset == 1);
    assert(result->contents[0].size == 0x05040302015aULL);
    assert(result->contents[0].NcaFilename() ==
        "0102030405060708090a0b0c0d0e0f10.nca");
    for (std::size_t len{}; len < 0x20 + 0x18 + 2*0x38 + 0x10; ++len) {
        assert(!ParseManifest(std::span<const std::uint8_t>(data.data(), len)));
    }
    auto invalid = data;
    invalid[0x20+0x18+0x38+0x20] = invalid[0x20+0x18+0x20];
    // A single duplicated byte does not constitute an entire duplicate ID.
    assert(ParseManifest(invalid));
    invalid = data;
    for (std::size_t i{}; i < 16; ++i)
        invalid[0x20+0x18+0x38+0x20+i] = invalid[0x20+0x18+0x20+i];
    assert(!ParseManifest(invalid)); // Duplicate content ID
    invalid = data;
    for (std::size_t i{}; i < 16; ++i)
        invalid[0x20+0x18+0x20+i] = 0;
    assert(!ParseManifest(invalid)); // Zero content ID
    invalid = data;
    for (std::size_t i{}; i < 6; ++i)
        invalid[0x20+0x18+0x30+i] = 0;
    assert(!ParseManifest(invalid)); // Zero-sized content entry
    invalid = data;
    Put16(invalid, 0x10, 2049);
    assert(!ParseManifest(invalid)); // Excessive allocation guard
    invalid = data;
    Put16(invalid, 0x10, 3);
    assert(!ParseManifest(invalid)); // Truncated entries
}

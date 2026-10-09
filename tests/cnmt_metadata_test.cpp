// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/vfs/content_meta.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>

using skyline::vfs::content_meta::Parse;
using skyline::vfs::content_meta::Type;

static void Put16(std::vector<std::uint8_t> &data, std::size_t off, std::uint16_t n) {
    data.at(off) = static_cast<std::uint8_t>(n);
    data.at(off + 1) = static_cast<std::uint8_t>(n >> 8);
}
static void Put32(std::vector<std::uint8_t> &data, std::size_t off, std::uint32_t n) {
    for (int i{}; i < 4; i++) data.at(off + i) = static_cast<std::uint8_t>(n >> (8 * i));
}
static void Put64(std::vector<std::uint8_t> &data, std::size_t off, std::uint64_t n) {
    for (int i{}; i < 8; i++) data.at(off + i) = static_cast<std::uint8_t>(n >> (8 * i));
}
static std::vector<std::uint8_t> Make(std::uint8_t type, int ext) {
    // One content-info and one content-meta entry, plus 32-byte digest.
    std::vector<std::uint8_t> data(0x20 + ext + 0x38 + 0x10 + 0x20, 0);
    Put64(data, 0, 0x0100ABCD00000800ULL);
    Put32(data, 8, 131072);
    data[0xC] = type;
    Put16(data, 0xE, static_cast<std::uint16_t>(ext));
    Put16(data, 0x10, 1);
    Put16(data, 0x12, 1);
    return data;
}
int main() {
    constexpr std::uint64_t Game{0x0100ABCD00000000ULL};
    {
        auto data = Make(0x80, 16);
        auto info = Parse(data);
        assert(info && info->type == Type::Application);
        assert(info->applicationId == info->titleId);
        assert(info->contentCount == 1);
        assert(info->titleVersion == 131072);
    }
    for (std::uint8_t kind : {std::uint8_t(0x81), std::uint8_t(0x82), std::uint8_t(0x83)}) {
        auto data = Make(kind, kind == 0x81 ? 24 : 16);
        Put64(data, 0x20, Game);
        auto info = Parse(data);
        assert(info && info->applicationId == Game);
        assert(info->type == static_cast<Type>(kind));
    }
    {
        auto data = Make(0x84, 32);
        Put64(data, 0x20, 0x0100ABCD00000802ULL);  // Data ID, not app ID
        Put64(data, 0x28, Game);
        auto info = Parse(data);
        assert(info && info->applicationId == Game);
    }
    {
        auto data = Make(0x81, 24);
        Put64(data, 0x20, Game);
        for (std::size_t len{}; len < 0x20; ++len)
            assert(!Parse(std::span<const std::uint8_t>(data.data(), len)));
        auto bad = data;
        bad[0xC] = 0x05; // System package is not application content.
        assert(!Parse(bad));
        bad = data;
        Put16(bad, 0xE, 65535);
        assert(!Parse(bad)); // Extended-header overflow
        bad = data;
        Put16(bad, 0x10, 65535);
        assert(!Parse(bad)); // Entry-count overflow
        bad = data;
        Put16(bad, 0x12, 65535);
        assert(!Parse(bad));
        bad = data;
        Put64(bad, 0x20, 0);
        assert(!Parse(bad)); // Patch with no base application ID
        bad = data;
        bad.resize(0x20 + 24 + 0x38 - 1);
        assert(!Parse(bad)); // Missing required content/meta entries
        bad = data;
        Put16(bad, 0xE, 4);
        assert(!Parse(bad)); // Too-short patch extended header
    }
    for (std::uint8_t type = 0; type < 255; ++type) {
        std::vector<std::uint8_t> allZero(0x20);
        allZero[0xC] = type;
        assert(!Parse(allZero)); // Never identify zero title as installed.
    }
}

// SPDX-License-Identifier: MPL-2.0
// Standalone, synthetic tests: no Switch firmware, ROM or keys.
#include "../app/src/main/cpp/skyline/vfs/bktr_relocation.h"
#include <cassert>
#include <cstdint>
#include <vector>

using skyline::vfs::bktr::RelocationTable;
using skyline::vfs::bktr::SubsectionTable;
static void Put32(std::vector<std::uint8_t> &b, std::size_t p, std::uint32_t v) {
    for (int i{}; i < 4; ++i) b[p+i] = std::uint8_t(v >> (8*i));
}
static void Put64(std::vector<std::uint8_t> &b, std::size_t p, std::uint64_t v) {
    for (int i{}; i < 8; ++i) b[p+i] = std::uint8_t(v >> (8*i));
}
static std::vector<std::uint8_t> Make() {
    std::vector<std::uint8_t> b(0x4000*2);
    Put32(b, 4, 1); // one relocation bucket
    Put64(b, 8, 0x4000); // virtual image size
    Put64(b, 0x10, 0);
    Put32(b, 0x4004, 2);
    Put64(b, 0x4008, 0x4000);
    // [0,0x1000): guest reads base at +0x200
    Put64(b, 0x4010, 0);
    Put64(b, 0x4018, 0x200);
    Put32(b, 0x4020, 0);
    // [0x1000,0x4000): guest reads patch at +0x600
    Put64(b, 0x4024, 0x1000);
    Put64(b, 0x402C, 0x600);
    Put32(b, 0x4034, 1);
    return b;
}
int main() {
    const auto source = Make();
    const auto parsed = RelocationTable::Parse(source);
    assert(parsed && parsed->EntryCount() == 2 && parsed->Size() == 0x4000);
    const auto base = parsed->Resolve(0x50, 0x3000, 0x4000);
    assert(base && !base->fromPatch && base->sourceOffset == 0x250 && base->size == 0xFB0);
    const auto patch = parsed->Resolve(0x1100, 0x3000, 0x4000);
    assert(patch && patch->fromPatch && patch->sourceOffset == 0x700);
    assert(!parsed->Resolve(0x4000, 0x3000, 0x4000));
    assert(!parsed->Resolve(0x1100, 0x3000, 0x650));
    {
        // The virtual image crosses the boundary between base/patch
        // backing at 0x1000. The merge must choose the correct source
        // without a whole-RomFS allocation.
        std::vector<std::uint8_t> baseBytes(0x3000, 0x42);
        std::vector<std::uint8_t> patchBytes(0x4000, 0x77);
        std::vector<std::uint8_t> output(0x60);
        assert(parsed->ReadDecrypted(baseBytes, patchBytes, output, 0xFE0));
        for (std::size_t i{}; i < 0x20; ++i)
            assert(output[i] == 0x42);
        for (std::size_t i{0x20}; i < output.size(); ++i)
            assert(output[i] == 0x77);
        assert(!parsed->ReadDecrypted(baseBytes, patchBytes, output, 0x3FE0));
        assert(!parsed->ReadDecrypted(baseBytes, patchBytes,
                                      std::span<std::uint8_t>(output), 0x5000));
    }
    const auto clip = parsed->Resolve(0x3FFC, 0x3000, 0x4000);
    assert(clip && clip->size == 4);
    {
        auto subsection = Make();
        // Reinterpret the synthetic bucket as AES-CTR-Ex subsections:
        // 0x00..0x0FFF uses counter 7; 0x1000..0x3FFF uses 9.
        Put32(subsection, 0x4020, 7);
        Put32(subsection, 0x4030, 9); // entry at 0x4020, counter at 0x402C+4
        // Make() puts relocation entries at 0x4010 and 0x4024.
        // Subsection entries are 0x10 bytes, second starts at 0x4020.
        Put64(subsection, 0x4020, 0x1000);
        Put32(subsection, 0x402C, 9);
        Put32(subsection, 0x4004, 2);
        Put64(subsection, 0x4008, 0x4000);
        Put64(subsection, 0x4010, 0);
        Put32(subsection, 0x401C, 7);
        auto parsedSections = SubsectionTable::Parse(subsection);
        assert(parsedSections && parsedSections->EntryCount() == 2);
        auto r0 = parsedSections->Resolve(0xFF0);
        auto r1 = parsedSections->Resolve(0x1200);
        assert(r0 && r0->counter == 7 && r0->size == 0x10);
        assert(r1 && r1->counter == 9 && r1->size == 0x2E00);
        assert(!parsedSections->Resolve(0x4000));
        subsection[0x4004] = 0;
        assert(!SubsectionTable::Parse(subsection));
    }
    for (std::size_t len{}; len < 0x4000*2; len += 73)
        assert(!RelocationTable::Parse(
            std::span<const std::uint8_t>(source.data(), len)));
    auto invalid = source;
    Put32(invalid, 4, 4097);
    assert(!RelocationTable::Parse(invalid));
    invalid = source; Put64(invalid, 0x4010, 1);
    assert(!RelocationTable::Parse(invalid)); // first relocation missing 0
    invalid = source; Put64(invalid, 0x4024, 0x5000);
    assert(!RelocationTable::Parse(invalid)); // entry outside virtual image
    invalid = source; Put32(invalid, 0x4034, 2);
    assert(!RelocationTable::Parse(invalid)); // invalid source discriminator
    invalid = source; Put64(invalid, 0x4008, 0);
    assert(!RelocationTable::Parse(invalid)); // zero bucket end
    invalid = source; Put32(invalid, 0x4004, 65535);
    assert(!RelocationTable::Parse(invalid)); // too many entries
}

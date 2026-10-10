// SPDX-License-Identifier: MPL-2.0
#include <cassert>
#include <array>
#include <cstdint>
#include <span>
#include <vector>
#include <algorithm>
#include "../app/src/main/cpp/skyline/soc/host1x/classes/nvdec_h264_packet_probe.h"

using namespace skyline::soc::host1x::nvdec_probe;

int main() {
    static_assert(!ExpandAddress(0).has_value());
    static_assert(ExpandAddress(0x20).value() == 0x2000);
    static_assert(!ExpandAddress(0x01000000).has_value());
    static_assert(!ValidAddressRange(0, 100));
    static_assert(!ValidAddressRange(0x1000, 0));
    static_assert(ValidAddressRange(0x1000, 512));
    static_assert(!ValidAddressRange(0xFFFFFF00, 0x1000));
    static_assert(!ValidAddressRange(0x1000, MaxAccessUnitBytes+1));
    static_assert(HasAnnexB(std::array<std::uint8_t, 4>{0, 0, 0, 1}));
    static_assert(HasAnnexB(std::array<std::uint8_t, 4>{0, 0, 1, 0x65}));
    static_assert(!HasAnnexB(std::array<std::uint8_t, 4>{1, 2, 3, 4}));

    RegisterState regs{};
    regs.Write(CodecMethod, H264CodecId);
    regs.Write(ControlMethod, 3);
    regs.Write(PictureInfoMethod, 0x10); // guest 0x1000
    regs.Write(BitstreamMethod, 0x20);  // guest 0x2000
    assert(regs.IsH264());

    std::vector<std::uint8_t> memory(0x4000);
    const std::uint32_t contextLengthAt{0x1000 + H264StreamLengthOffset};
    memory[contextLengthAt] = 100;
    memory[0x2000] = 0; memory[0x2001] = 0;
    memory[0x2002] = 0; memory[0x2003] = 1;
    auto read = [&](std::uint32_t guest, std::span<std::uint8_t> out) {
        if (guest > memory.size() || out.size() > memory.size() - guest)
            return false;
        std::copy(memory.begin()+guest, memory.begin()+guest+out.size(), out.begin());
        return true;
    };
    const auto packet = Inspect(regs, read);
    assert(packet && packet->size == 100 && packet->address == 0x2000);
    assert(packet->hasAnnexBPrefix);

    memory[0x2003] = 0x65;
    const auto another = Inspect(regs, read);
    assert(another && !another->hasAnnexBPrefix);
    memory[contextLengthAt] = 3;
    assert(!Inspect(regs, read));

    memory[contextLengthAt] = 0xff; memory[contextLengthAt+1] = 0xff;
    memory[contextLengthAt+2] = 0xff; memory[contextLengthAt+3] = 0xff;
    assert(!Inspect(regs, read)); // unbounded payload rejected
    memory[contextLengthAt] = 64; memory[contextLengthAt+1] = 0;
    memory[contextLengthAt+2] = 0; memory[contextLengthAt+3] = 0;
    assert(!Inspect(regs, [](std::uint32_t, std::span<std::uint8_t>) {
        return false; // SMMU page fault / invalid mapping
    }));
    regs.Write(BitstreamMethod, 0x01000000);
    assert(!Inspect(regs, read)); // out-of-range shifted address
    regs.Write(BitstreamMethod, 0x20);
    regs.Write(CodecMethod, 0x9); // VP9, do not mislabel H264
    assert(!regs.IsH264() && !Inspect(regs, read));
    regs.Write(CodecMethod, 0);
    regs.Write(ControlMethod, H264CodecId);
    assert(regs.IsH264()); // alternate codec selection register
    regs.Write(CodecMethod, H264CodecId);
    regs.Write(ControlMethod, 0x5);
    assert(!regs.IsH264()); // contradictory codec registers
}

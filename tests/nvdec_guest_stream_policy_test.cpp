// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#include "../app/src/main/cpp/skyline/soc/host1x/classes/nvdec_guest_stream.h"
#include <array>
#include <cassert>
using namespace skyline::soc::host1x::nvdec_guest;

constexpr bool RegisterValidation() {
    RegisterState state{};
    if (!state.Write(CodecMethod, 3) || TranslateCodec(state.Value(CodecMethod)) != Codec::H264)
        return false;
    if (!state.Write(PictureInfoMethod, 0x5000) || state.Address(PictureInfoMethod) != 0x500000ULL)
        return false;
    if (state.Write(RegisterCount, 2) || state.Value(RegisterCount) != 0)
        return false;
    return BoundedSmmuRange(0x5000, 1024) &&
           !BoundedSmmuRange(0, 1024) &&
           !BoundedSmmuRange(0xFFFFFF00ULL, 0x200) &&
           !BoundedSmmuRange(0x1000, MaximumPacketSize + 1ULL);
}
static_assert(RegisterValidation());
static_assert(TranslateCodec(0x9) == Codec::VP9,
              "V54 NieR codec=0x9 must remain VP9, never H264");
static_assert(TranslateCodec(0x3) == Codec::H264);

constexpr bool ContextAndAnnexB() {
    std::array<std::uint8_t, 0x6C> bytes{};
    bytes[0x48] = 0x40; // compressed stream length 64
    bytes[0x64] = 80; // 1280px width
    bytes[0x68] = 45; // 720px height
    bytes[0x60] = 1;  // progressive
    auto frame = InspectH264Context(bytes);
    if (!frame.valid || frame.width != 1280 || frame.height != 720 || frame.streamBytes != 64)
        return false;
    bytes[0x60] = 0;
    if (InspectH264Context(bytes).valid)
        return false;
    constexpr std::array<std::uint8_t, 16> configured{
        0,0,0,1,0x67,0x64,0,0,
        0,0,1,0x68,0xee,0,
        0x00,0x00
    };
    constexpr std::array<std::uint8_t, 8> noPps{0,0,1,0x67,0x64,0,0,0};
    return ContainsH264ParameterSets(configured) &&
           !ContainsH264ParameterSets(noPps);
}
static_assert(ContextAndAnnexB());
int main() {
    assert(RegisterValidation());
    assert(ContextAndAnnexB());
}

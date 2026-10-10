// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/classes/vic_yuv420_output.h"
#include <array>
#include <cassert>
using namespace skyline::soc::host1x::vic_output;

constexpr bool Layout() {
    const auto raw = (static_cast<std::uint64_t>(15) << 32) |
                     (static_cast<std::uint64_t>(15) << 46) | 0x44U;
    const auto surface = DecodeSurface(raw);
    if (!SupportedSurface(surface) || surface.width != 16 || surface.height != 16)
        return false;
    return !SupportedSurface(DecodeSurface(raw | (1ULL << 11)));
}
static_assert(Layout());

int main() {
    const Surface config{16,16,0,GuestYuv420};
    std::array<std::uint8_t, 384> planar{};
    for (int i=0;i<256;++i) planar[i]=static_cast<std::uint8_t>(i);
    for (int i=256;i<320;++i) planar[i]=20;
    for (int i=320;i<384;++i) planar[i]=30;
    Output output;
    assert(PackLinearNv12(planar,16,16,16,16,AndroidYuvPlanar,config,output));
    assert(output.luma.size()==256U*16);
    assert(output.chroma.size()==256U*8);
    assert(output.chroma[0]==20 && output.chroma[1]==30);
    assert(output.chroma[256]==20 && output.chroma[257]==30);
    assert(output.luma[0]==0 && output.luma[16]==0);
    assert(output.luma[256]==16);
    std::array<std::uint8_t,384> semi{};
    for(int i=0;i<256;++i)semi[i]=7;
    for(int i=256;i<384;++i)semi[i]=5;
    assert(PackLinearNv12(semi,16,16,16,16,AndroidYuvSemiplanar,config,output));
    assert(output.chroma[0]==5 && output.chroma[256]==5);
    assert(!PackLinearNv12(semi,16,16,14,16,AndroidYuvSemiplanar,config,output));
    assert(!PackLinearNv12(semi,16,16,16,16,0x7F420888,config,output));
    assert(!PackLinearNv12(std::span<const std::uint8_t>(semi.data(),10),16,16,16,16,
                           AndroidYuvSemiplanar,config,output));
}

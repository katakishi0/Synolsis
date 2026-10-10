// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/classes/nvdec_h264_csd.h"
#include <array>
#include <cassert>
using namespace skyline::soc::host1x;

int main() {
    std::array<std::uint8_t,0xB8> context{};
    context[0x48]=0x20; // H264 byte count 32
    context[0x64]=80; // 1280 in 16-pixel macroblocks
    context[0x68]=45; // 720
    context[0x60]=1;  // progressive
    context[0xB1]=0x10; // chroma_format_idc=1 at flag bit12
    const auto generated=h264_csd::Build(context);
    assert(generated.valid);
    assert(generated.packet.size()<256);
    assert(nvdec_guest::ContainsH264ParameterSets(generated.packet));
    assert(generated.packet[0]==0 && generated.packet[1]==0 &&
           generated.packet[2]==0 && generated.packet[3]==1);
    assert(generated.packet[4]==0x67);
    context[0x60]=0; // interlaced unsupported: refuse to guess
    assert(!h264_csd::Build(context).valid);
    context[0x60]=1;
    context[0xB1]=0; // non 4:2:0
    assert(!h264_csd::Build(context).valid);
}

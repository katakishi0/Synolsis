// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/classes/nvdec_h264_access_unit.h"
#include <array>
#include <cassert>
#include <vector>
using namespace skyline::soc::host1x::h264_access_unit;
static_assert(PlausibleNal(0x65) && !PlausibleNal(0xE5) && !PlausibleNal(0));
int main() {
    const std::array<std::uint8_t, 13> annex{
        0,0,0,1,0x67,0x64,0,0,1,0x65,0x88,0x99,0
    };
    auto a = Normalize(annex);
    assert(a && a.format == Format::AnnexB && a.nalCount == 2);
    assert(a.bytes == std::vector<std::uint8_t>(annex.begin(),annex.end()));

    const std::array<std::uint8_t, 13> length{
        0,0,0,2,0x67,0x64,0,0,0,3,0x65,0x88,0x99
    };
    auto b = Normalize(length);
    assert(b && b.format == Format::LengthPrefixed4 &&
           b.nalCount == 2 && b.containsPicture);
    const std::array<std::uint8_t, 13> expected{
        0,0,0,1,0x67,0x64,0,0,0,1,0x65,0x88,0x99
    };
    assert(b.bytes == std::vector<std::uint8_t>(expected.begin(),expected.end()));
    const std::array<std::uint8_t, 6> badLength{0,0,0,8,0x65,0x88};
    const std::array<std::uint8_t, 5> badNal{0,0,0,1,0xE5};
    const std::array<std::uint8_t, 5> onlySps{0,0,0,1,0x67};
    const std::array<std::uint8_t, 5> noNal{0,0,0,0,0x65};
    assert(!Normalize(badLength));
    assert(!Normalize(badNal));
    assert(!Normalize(onlySps));
    assert(!Normalize(noNal));
    const std::vector<std::uint8_t> huge(8U*1024U*1024U+1U,0);
    assert(!Normalize(huge));
    const std::array<std::uint8_t, 8> annex3{0,0,1,0x65,0x88,0,0,0};
    assert(Normalize(annex3).format == Format::AnnexB);
    const std::array<std::uint8_t, 7> truncated{0,0,0,2,0x65,0x88,9};
    assert(!Normalize(truncated));
}

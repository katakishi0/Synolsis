// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/classes/vic_yuv420_output.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <span>

using namespace skyline::soc::host1x::vic_output;

static_assert(IsPackedRgb(GuestRgba8) && IsPackedRgb(GuestBgra8) &&
              IsPackedRgb(GuestRgbx8) && !IsPackedRgb(GuestYuv420));
static_assert(YuvToRgb(16,128,128).r == 0);
static_assert(YuvToRgb(235,128,128).r == 255);
static_assert(YuvToRgb(235,128,128).g == 255);
static_assert(YuvToRgb(235,128,128).b == 255);
static_assert(SupportedSurface({16,16,0,GuestRgba8,0}));
static_assert(SupportedSurface({16,16,1,GuestBgra8,3}));
static_assert(!SupportedSurface({16,16,0,0x10,0}));

int main() {
    std::array<std::uint8_t,384> planar{};
    // Black frame, 8-bit planar Y + U + V.
    for (std::size_t i=0; i<256; ++i) planar[i] = 16;
    for (std::size_t i=256; i<384; ++i) planar[i] = 128;
    Output rgb;
    const Surface rgba{16,16,0,GuestRgba8,0};
    assert(PackLinearRgba(planar,16,16,16,16,AndroidYuvPlanar,rgba,rgb));
    assert(rgb.luma.size() == 256U * 16U && rgb.chroma.empty());
    assert(rgb.luma[0] == 0 && rgb.luma[1] == 0 &&
           rgb.luma[2] == 0 && rgb.luma[3] == 255);
    assert(rgb.luma[4 * 16] == 0); // Row padding remains zero.
    assert(rgb.luma[256 + 3] == 255); // Next line, alpha is valid.

    // White frame and non-grey red sample; RGB/BGRA must differ.
    for(std::size_t i=0;i<256;++i) planar[i]=235;
    assert(PackLinearRgba(planar,16,16,16,16,AndroidYuvPlanar,rgba,rgb));
    assert(rgb.luma[0] == 255 && rgb.luma[1] == 255 && rgb.luma[2] == 255);
    for(std::size_t i=0;i<256;++i) planar[i]=82;
    for(std::size_t i=256;i<320;++i) planar[i]=90;
    for(std::size_t i=320;i<384;++i) planar[i]=240;
    Output rgbaOut;
    const Surface bgra{16,16,0,GuestBgra8,0};
    Output bgraOut;
    assert(PackLinearRgba(planar,16,16,16,16,AndroidYuvPlanar,rgba,rgbaOut));
    assert(PackLinearRgba(planar,16,16,16,16,AndroidYuvPlanar,bgra,bgraOut));
    assert(rgbaOut.luma[0] == bgraOut.luma[2]);
    assert(rgbaOut.luma[2] == bgraOut.luma[0]);
    assert(rgbaOut.luma[0] >= 250 && rgbaOut.luma[1] < 15);
    assert(rgbaOut.luma[3] == 255 && bgraOut.luma[3] == 255);

    std::array<std::uint8_t,384> semi{};
    for(std::size_t i=0;i<256;++i) semi[i]=82;
    for(std::size_t i=256;i<384;i+=2) {semi[i]=90;semi[i+1]=240;}
    const Surface rgbx{16,16,0,GuestRgbx8,0};
    Output rgbxOut;
    assert(PackLinearRgba(semi,16,16,16,16,AndroidYuvSemiplanar,rgbx,rgbxOut));
    assert(rgbxOut.luma[0] == rgbaOut.luma[0] &&
           rgbxOut.luma[1] == rgbaOut.luma[1] &&
           rgbxOut.luma[2] == rgbaOut.luma[2]);

    // Reject unknown flexible YUV without pixel stride/plane layout,
    // incomplete buffers, odd strides and mismatched surfaces.
    const auto oldOutput = rgbxOut.luma;
    assert(!PackLinearRgba(semi,16,16,16,16,0x7F420888,rgbx,rgbxOut));
    assert(!PackLinearRgba(std::span<const std::uint8_t>(semi.data(),10),
                          16,16,16,16,AndroidYuvSemiplanar,rgbx,rgbxOut));
    assert(!PackLinearRgba(semi,16,16,15,16,AndroidYuvSemiplanar,rgbx,rgbxOut));
    assert(!PackLinearRgba(semi,16,16,16,16,AndroidYuvSemiplanar,
                           Surface{18,16,0,GuestRgbx8,0},rgbxOut));
    assert(!PackLinearRgba(semi,16,16,16,16,AndroidYuvSemiplanar,
                           Surface{16,16,0,GuestYuv420,0},rgbxOut));
    assert(rgbxOut.luma == oldOutput); // Failed conversion leaves output untouched.

    Output yuv;
    assert(PackLinearNv12(semi,16,16,16,16,AndroidYuvSemiplanar,
                          Surface{16,16,0,GuestYuv420,0},yuv));
    assert(!PackLinearNv12(semi,16,16,16,16,AndroidYuvSemiplanar,rgba,yuv));
}

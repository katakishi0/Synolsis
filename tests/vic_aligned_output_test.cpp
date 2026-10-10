// SPDX-License-Identifier: MPL-2.0
// V53: actual MediaCodec coded/visible overhangs must not cause VIC rejection.
#include "../app/src/main/cpp/skyline/soc/host1x/classes/vic_yuv420_output.h"
#include <cassert>
#include <cstdint>
#include <vector>
using namespace skyline::soc::host1x::vic_output;

static_assert(DecodedFrameCoversSurface({16,16,0,GuestYuv420,0},16,16));
static_assert(DecodedFrameCoversSurface({16,16,0,GuestYuv420,0},32,32));
static_assert(DecodedFrameCoversSurface({1920,1080,0,GuestRgba8,0},1920,1088));
static_assert(!DecodedFrameCoversSurface({16,16,0,GuestYuv420,0},33,16));
static_assert(!DecodedFrameCoversSurface({16,16,0,GuestBgra8,0},16,33));
static_assert(!DecodedFrameCoversSurface({32,16,0,GuestYuv420,0},16,16));
static_assert(!DecodedFrameCoversSurface({16,16,0,0x10,0},32,16));
static_assert(!DecodedFrameCoversSurface({0,16,0,GuestYuv420,0},16,16));

int main() {
    // 32x32 Android decoder allocation for a 16x16 guest VIC destination:
    // copy only the visible 16x16 region; keep its 256-byte guest row pitch.
    constexpr std::uint32_t stride = 32, slice = 32;
    constexpr Surface guest{16,16,0,GuestYuv420,0};
    std::vector<std::uint8_t> yuv(stride * slice + stride * slice / 2U, 128);
    for (std::uint32_t row = 0; row < slice; ++row)
        for (std::uint32_t col = 0; col < stride; ++col)
            yuv[row * stride + col] = static_cast<std::uint8_t>(16 + row * 2 + col);
    Output actual;
    assert(DecodedFrameCoversSurface(guest, 32, 24));
    assert(PackLinearNv12(yuv,guest.width,guest.height,stride,slice,
                          AndroidYuvSemiplanar,guest,actual));
    assert(actual.luma.size() == 256U * 16U);
    assert(actual.luma[0] == yuv[0]);
    assert(actual.luma[15] == yuv[15]);
    assert(actual.luma[255] == 0); // No leak from the padded source.
    assert(actual.luma[256] == yuv[stride]);
    assert(actual.luma[15U * 256U + 15U] == yuv[15U * stride + 15U]);

    constexpr Surface packed{16,16,0,GuestRgba8,0};
    Output rgb;
    assert(DecodedFrameCoversSurface(packed, 32, 24));
    assert(PackLinearRgba(yuv, packed.width,packed.height,stride,slice,
                          AndroidYuvSemiplanar,packed,rgb));
    assert(rgb.luma.size() == 256U * 16U);
    assert(rgb.chroma.empty());
    assert(rgb.luma[3] == 255 && rgb.luma[4U*15U+3U] == 255);

    // No invented padding or rescaling when guest requests more pixels than
    // source, and no accidental expansion beyond one H264 macroblock.
    assert(!DecodedFrameCoversSurface({32,16,0,GuestYuv420,0},16,16));
    assert(!DecodedFrameCoversSurface(guest, 40,16));
    assert(!DecodedFrameCoversSurface(guest,16,40));
}

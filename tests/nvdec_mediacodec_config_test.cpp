// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/classes/media_decoder_types.h"
#include <cassert>
using skyline::soc::host1x::AndroidMime;
using skyline::soc::host1x::VideoCodec;
using skyline::soc::host1x::ValidVideoExtent;
static_assert(AndroidMime(VideoCodec::H264) == "video/avc");
static_assert(AndroidMime(VideoCodec::VP8) == "video/x-vnd.on2.vp8");
static_assert(AndroidMime(VideoCodec::VP9) == "video/x-vnd.on2.vp9");
static_assert(AndroidMime(VideoCodec::H265) == "video/hevc");
static_assert(AndroidMime(VideoCodec::VC1).empty());
static_assert(ValidVideoExtent(1920,1080));
static_assert(ValidVideoExtent(1280,720));
static_assert(!ValidVideoExtent(0,1080));
static_assert(!ValidVideoExtent(7680,4320));
static_assert(!ValidVideoExtent(4096,4096));
int main() {
    for(unsigned w=16;w<4097;w+=127)
        for(unsigned h=16;h<4097;h+=127)
            assert(ValidVideoExtent(w,h) == (static_cast<unsigned long long>(w)*h<=4096ULL*2160ULL));
}

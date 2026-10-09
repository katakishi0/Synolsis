// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>
#include <string_view>

namespace skyline::soc::host1x {
    enum class VideoCodec : std::uint32_t {
        H264,
        VP8,
        VP9,
        H265,
        VC1
    };

    // Android MediaCodec does not guarantee hardware acceleration for every
    // supported format; actual codec selection is done by the OS.
    constexpr std::string_view AndroidMime(VideoCodec codec) {
        switch (codec) {
            case VideoCodec::H264: return "video/avc";
            case VideoCodec::VP8: return "video/x-vnd.on2.vp8";
            case VideoCodec::VP9: return "video/x-vnd.on2.vp9";
            case VideoCodec::H265: return "video/hevc";
            case VideoCodec::VC1: return {};
        }
        return {};
    }

    constexpr bool ValidVideoExtent(std::uint32_t width, std::uint32_t height) {
        return width >= 16 && height >= 16 &&
               width <= 4096 && height <= 4096 &&
               std::uint64_t(width) * height <= 4096ULL * 2160ULL;
    }
}

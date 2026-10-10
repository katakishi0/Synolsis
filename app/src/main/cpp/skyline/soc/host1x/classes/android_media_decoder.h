// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include "media_decoder_types.h"

namespace skyline::soc::host1x {
    // A real Android MediaCodec Access Unit decoder using CPU output buffers.
    //
    // This is an isolated decoder backend, not yet wired to guest NVDEC
    // register/method parsing, SMMU bitstream addresses, VIC colorspace
    // conversion or guest fence signal ordering. Do not claim video works
    // until the host1x producer passes real access units to this instance.
    class AndroidMediaDecoder final {
      public:
        struct Frame {
            std::vector<std::uint8_t> data; // codec-selected YUV/RGB layout
            std::int64_t ptsUs{};
            std::int32_t width{};
            std::int32_t height{};
            std::int32_t colorFormat{};
            std::int32_t rowStride{};
            std::int32_t sliceHeight{};
        };

      private:
        std::mutex mutex;
        AMediaCodec *codec{};
        std::int32_t width{};
        std::int32_t height{};
        std::int32_t colorFormat{};
        std::int32_t rowStride{};
        std::int32_t sliceHeight{};

        void StopLocked();

      public:
        AndroidMediaDecoder() = default;
        AndroidMediaDecoder(const AndroidMediaDecoder &) = delete;
        AndroidMediaDecoder &operator=(const AndroidMediaDecoder &) = delete;
        ~AndroidMediaDecoder();

        bool Start(VideoCodec format, std::uint32_t width, std::uint32_t height);
        void Stop();

        // input is an already extracted compressed access unit (NAL/VPx
        // packet). The 6-GB mobile profile forbids oversized packets.
        bool QueueAccessUnit(std::span<const std::uint8_t> input,
                             std::int64_t presentationTimeUs,
                             bool codecConfiguration = false);
        bool Drain(Frame &frame, std::int64_t waitUs = 0); // VIC: bounded <=12ms; NVDEC: nonblocking
    };
}

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <atomic>
#include <soc/smmu.h>
#include "android_media_decoder.h"
#include "nvdec_h264_packet_probe.h"

namespace skyline::soc::host1x {
    /**
     * @brief The NVDEC Host1x class implements hardware accelerated video decoding for the VP9/VP8/H264/VC1 codecs
     */
    class NvDecClass {
      private:
        std::function<void()> opDoneCallback;
        SMMU &smmu;
        nvdec_probe::RegisterState registers{};
        std::atomic<std::uint64_t> unhandledMethodCount{}; //!< Bounded diagnostics; not a fake decode counter
        std::atomic<u32> lastCodecId{};
        std::atomic<std::uint64_t> executeCount{};
        AndroidMediaDecoder mediaCodecBackend; //!< Only decoded access-unit pipeline; register parser still pending

      public:
        NvDecClass(std::function<void()> opDoneCallback, SMMU &smmu);

        void CallMethod(u32 method, u32 argument);

        // Real MediaCodec backend API: guest NVDEC register decoder and
        // SMMU bitstream extraction must call these with actual access units.
        bool ConfigureMediaDecoder(VideoCodec codec, u32 width, u32 height);
        bool SubmitAccessUnit(span<const u8> packet, i64 ptsUs, bool isConfiguration = false);
        bool TryGetDecodedFrame(AndroidMediaDecoder::Frame &frame);
    };
}

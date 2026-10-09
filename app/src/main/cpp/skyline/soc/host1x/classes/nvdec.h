// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include "android_media_decoder.h"

namespace skyline::soc::host1x {
    /**
     * @brief The NVDEC Host1x class implements hardware accelerated video decoding for the VP9/VP8/H264/VC1 codecs
     */
    class NvDecClass {
      private:
        std::function<void()> opDoneCallback;
        AndroidMediaDecoder mediaCodecBackend; //!< Only decoded access-unit pipeline; register parser still pending

      public:
        NvDecClass(std::function<void()> opDoneCallback);

        void CallMethod(u32 method, u32 argument);

        // Real MediaCodec backend API: guest NVDEC register decoder and
        // SMMU bitstream extraction must call these with actual access units.
        bool ConfigureMediaDecoder(VideoCodec codec, u32 width, u32 height);
        bool SubmitAccessUnit(span<const u8> packet, i64 ptsUs, bool isConfiguration = false);
        bool TryGetDecodedFrame(AndroidMediaDecoder::Frame &frame);
    };
}

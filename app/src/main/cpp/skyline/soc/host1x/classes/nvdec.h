// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common.h>
#include <atomic>
#include <optional>
#include "android_media_decoder.h"
#include "decoded_frame_queue.h"
#include "nvdec_guest_stream.h"

namespace skyline::soc::host1x {
    /**
     * @brief The NVDEC Host1x class implements hardware accelerated video decoding for the VP9/VP8/H264/VC1 codecs
     */
    class NvDecClass {
      private:
        std::function<void()> opDoneCallback;
        const DeviceState &state;
        nvdec_guest::RegisterState registers;
        u32 decoderWidth{}, decoderHeight{};
        std::uint64_t rejectedVideoPackets{};
        std::uint64_t decodedAccessUnits{};

        // Only processes guest frames with actual Annex-B codec headers.
        // VIC output/syncpoints are not wired yet: decoding is not proof
        // that cutscenes are visible on-screen.
        void ProcessGuestDecode();
        std::atomic<std::uint64_t> unhandledMethodCount{}; //!< Bounded diagnostics; not a fake decode counter
        std::atomic<u32> lastCodecId{};
        std::atomic<std::uint64_t> executeCount{};
        AndroidMediaDecoder mediaCodecBackend;
        video_frame_queue::BoundedFrames<AndroidMediaDecoder::Frame> readyFrames; //!< <=2 frames, <=48 MiB, VIC presentation order

      public:
        NvDecClass(std::function<void()> opDoneCallback, const DeviceState &state);

        void CallMethod(u32 method, u32 argument);

        // Real MediaCodec backend API: guest NVDEC register decoder and
        // SMMU bitstream extraction must call these with actual access units.
        bool ConfigureMediaDecoder(VideoCodec codec, u32 width, u32 height);
        bool SubmitAccessUnit(span<const u8> packet, i64 ptsUs, bool isConfiguration = false);
        bool TryGetDecodedFrame(AndroidMediaDecoder::Frame &frame, i64 waitUs = 0);
    };
}

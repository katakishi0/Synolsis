// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvdec.h"

namespace skyline::soc::host1x {
    NvDecClass::NvDecClass(std::function<void()> opDoneCallback)
        : opDoneCallback(std::move(opDoneCallback)) {}

    void NvDecClass::CallMethod(u32 method, u32 argument) {
        // Validate guest-provided register IDs; don't index beyond NVDEC state.
        if (!registerState.Write(method, argument)) {
            const auto count{unhandledMethodCount.fetch_add(1, std::memory_order_relaxed) + 1};
            if (count <= 4 || (count & (count - 1)) == 0)
                LOGW("Ignoring out-of-range NVDEC register method 0x{:X}", method);
            return;
        }
        // Do not signal fake decode completion: the guest NVDEC method
        // registers and their SMMU-addressed bitstream buffers have not yet
        // been translated into compressed access units for MediaCodec.
        // Citron/yuzu Host1x reference registers (GPL implementation was not copied):
        // 0x80 = codec selection; 0xC0 = decode execute. These observations
        // distinguish "no cutscene commands" from "decoder never wired".
        // They do NOT submit a fake frame or signal a successful decode.
        if (method == 0x80) {
            const auto previous{lastCodecId.exchange(argument, std::memory_order_relaxed)};
            if (previous != argument)
                LOGI("NVDEC guest codec selected: id=0x{:X} (decode not connected)", argument);
            return;
        }
        if (method == 0xC0) {
            const auto executions{executeCount.fetch_add(1, std::memory_order_relaxed) + 1};
            if (executions <= 4 || (executions & (executions - 1)) == 0)
                LOGW("NVDEC execute requested: count={} codec=0x{:X} frame={} pictureInfo={} bitstream={}; guest decode and VIC not connected",
                     executions, registerState.CodecId(), registerState.Raw(NvdecRegisterState::FrameNumber),
                     registerState.GuestPictureInfoAddress() != 0,
                     registerState.GuestBitstreamAddress() != 0);
            return;
        }
        const auto count{unhandledMethodCount.fetch_add(1, std::memory_order_relaxed) + 1};
        // Games can submit thousands of NVDEC registers per second. Logging
        // each register on Android severely harms frame pacing, even though
        // full bitstream decoding is not yet integrated.
        if (count <= 8 || (count & (count - 1)) == 0)
            LOGW("NVDEC registers not decoded: sample #{} method 0x{:X} argument 0x{:X}",
                 count, method, argument);
    }

    bool NvDecClass::ConfigureMediaDecoder(VideoCodec codec, u32 width, u32 height) {
        return mediaCodecBackend.Start(codec, width, height);
    }

    bool NvDecClass::SubmitAccessUnit(span<const u8> packet, i64 ptsUs, bool isConfiguration) {
        return mediaCodecBackend.QueueAccessUnit(
            std::span<const std::uint8_t>(packet.data(), packet.size()), ptsUs, isConfiguration);
    }

    bool NvDecClass::TryGetDecodedFrame(AndroidMediaDecoder::Frame &frame) {
        return mediaCodecBackend.Drain(frame);
    }
}

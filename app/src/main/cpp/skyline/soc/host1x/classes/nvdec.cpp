// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvdec.h"

namespace skyline::soc::host1x {
    NvDecClass::NvDecClass(std::function<void()> opDoneCallback)
        : opDoneCallback(std::move(opDoneCallback)) {}

    void NvDecClass::CallMethod(u32 method, u32 argument) {
        // Do not signal fake decode completion: the guest NVDEC method
        // registers and their SMMU-addressed bitstream buffers have not yet
        // been translated into compressed access units for MediaCodec.
        LOGW("Unhandled NVDEC class method 0x{:X} argument 0x{:X}", method, argument);
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

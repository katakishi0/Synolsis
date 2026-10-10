// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "nvdec.h"

namespace skyline::soc::host1x {
    NvDecClass::NvDecClass(std::function<void()> opDoneCallback, SMMU &smmu)
        : opDoneCallback(std::move(opDoneCallback)), smmu(smmu) {}

    void NvDecClass::CallMethod(u32 method, u32 argument) {
        // Do not signal fake decode completion: the guest NVDEC method
        // registers and their SMMU-addressed bitstream buffers have not yet
        // been translated into compressed access units for MediaCodec.
        registers.Write(method, argument);
        // Hardware method indices are documented independently in the
        // MPL-licensed nvdec_h264_packet_probe.h. No video is synthesized.
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
            // Read the guest H264 stream descriptor through Skyline's real
            // SMMU mappings. This is bounded and probes ONLY the four-byte
            // length and first four compressed bytes, not entire video frames.
            // It establishes whether the guest video source is reachable.
            auto packet{nvdec_probe::Inspect(registers, [this](u32 address,
                          std::span<std::uint8_t> output) -> bool {
                try {
                    smmu.Read(output.data(), address, static_cast<u32>(output.size()));
                    return true;
                } catch (const std::exception &) {
                    return false; // Guest pages may legitimately be unmapped.
                }
            })};
            if (executions <= 4 || (executions & (executions - 1)) == 0) {
                if (packet)
                    LOGI("NVDEC H264 guest packet: count={} size={} AnnexB={} (decode/VIC still pending)",
                         executions, packet->size, packet->hasAnnexBPrefix);
                else
                    LOGW("NVDEC execute requested: count={} codec=0x{:X}; H264 guest packet unavailable or invalid",
                         executions, lastCodecId.load(std::memory_order_relaxed));
            }
            return;
        }
        // Known state registers are tracked, not "unknown" spam.
        if (method == nvdec_probe::ControlMethod ||
            method == nvdec_probe::PictureInfoMethod ||
            method == nvdec_probe::BitstreamMethod)
            return;
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

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2026 Synolsis Emu contributors

#include "nvdec.h"
#include "nvdec_h264_csd.h"
#include <soc.h>
#include <algorithm>
#include <array>
#include <vector>

namespace skyline::soc::host1x {
    NvDecClass::NvDecClass(std::function<void()> callback, const DeviceState &deviceState)
        : opDoneCallback(std::move(callback)), state(deviceState) {}

    void NvDecClass::ProcessGuestDecode() {
        const auto reject{[this](const char *reason) {
            const auto n{++rejectedVideoPackets};
            if (n <= 5 || (n & (n - 1)) == 0)
                LOGW("NVDEC guest frame unavailable: reason={} count={}", reason, n);
        }};

        if (nvdec_guest::TranslateCodec(registers.Value(nvdec_guest::CodecMethod)) !=
            nvdec_guest::Codec::H264) {
            reject("codec-not-yet-supported");
            return;
        }
        if (!state.soc) {
            reject("missing-soc");
            return;
        }

        const auto pictureAddress{registers.Address(nvdec_guest::PictureInfoMethod)};
        constexpr std::size_t ContextBytes{0xB8};
        if (!nvdec_guest::BoundedSmmuRange(pictureAddress, ContextBytes)) {
            reject("picture-address-out-of-range");
            return;
        }

        // Reject unmapped guest memory *before* reading. This is not a fake
        // 0-byte "decoded" frame if the guest mapping is absent.
        const auto mapped{[this](u32 address, u32 size) {
            auto chunks{state.soc->smmu.TranslateRange(address, size)};
            std::size_t bytes{};
            for (const auto &chunk : chunks) {
                if (!chunk.data())
                    return false;
                bytes += chunk.size();
            }
            return bytes == size;
        }};

        try {
            if (!mapped(static_cast<u32>(pictureAddress), ContextBytes)) {
                reject("picture-unmapped");
                return;
            }
            std::array<u8, ContextBytes> context{};
            state.soc->smmu.Read(context.data(), static_cast<u32>(pictureAddress),
                                 static_cast<u32>(context.size()));
            const auto params{nvdec_guest::InspectH264Context(context)};
            if (!params.valid) {
                reject("invalid-h264-context");
                return;
            }

            const auto streamAddress{registers.Address(nvdec_guest::BitstreamMethod)};
            if (!nvdec_guest::BoundedSmmuRange(streamAddress, params.streamBytes) ||
                !mapped(static_cast<u32>(streamAddress), params.streamBytes)) {
                reject("bitstream-unmapped-or-too-large");
                return;
            }
            std::vector<u8> packet(params.streamBytes);
            state.soc->smmu.Read(packet.data(), static_cast<u32>(streamAddress),
                                 params.streamBytes);

            // Many Switch NVDEC streams contain only slice NAL units.
            // Build conservative SPS/PPS from the *actual guest picture info*
            // instead of trying to decode unconfigured H264. Only progressive
            // 8-bit 4:2:0 parameter sets supported at this stage.
            const bool hasHeaders{nvdec_guest::ContainsH264ParameterSets(packet)};
            if (!decoderWidth || !decoderHeight || decoderWidth != params.width ||
                decoderHeight != params.height) {
                auto config{hasHeaders ? h264_csd::HeaderPair{} :
                                       h264_csd::Build(context)};
                if (!hasHeaders && !config.valid) {
                    reject("unsupported-h264-picture-parameter-sets");
                    return;
                }
                if (!mediaCodecBackend.Start(VideoCodec::H264, params.width, params.height)) {
                    reject("mediacodec-configuration-failed");
                    return;
                }
                decoderWidth = params.width;
                decoderHeight = params.height;
                readyFrame.reset();
                if (!hasHeaders && !mediaCodecBackend.QueueAccessUnit(
                        std::span<const std::uint8_t>(config.packet.data(), config.packet.size()),
                        0, true)) {
                    decoderWidth = decoderHeight = 0;
                    reject("mediacodec-csd-queue-rejected");
                    return;
                }
            }

            // The queued bytes come from the guest via SMMU, never fabricated.
            // Guest video timestamps and VIC surface composition are still TODO.
            if (!mediaCodecBackend.QueueAccessUnit(
                    std::span<const std::uint8_t>(packet.data(), packet.size()),
                    static_cast<std::int64_t>(decodedAccessUnits) * 33333, false)) {
                reject("mediacodec-input-not-ready");
                return;
            }
            ++decodedAccessUnits;
            AndroidMediaDecoder::Frame frame;
            // Drain output to keep MediaCodec progressing while the VIC
            // consumer is not connected. Deliberately do NOT report video
            // as presented to the guest or signal fake completion.
            if (mediaCodecBackend.Drain(frame)) {
                if (decodedAccessUnits <= 4 || (decodedAccessUnits % 120) == 0)
                    LOGI("NVDEC decoded Android frame: {}x{} format={} bytes={} (waiting for VIC)",
                         frame.width, frame.height, frame.colorFormat, frame.data.size());
                readyFrame = std::move(frame); // bounded single-frame handoff
            }
        } catch (const std::exception &e) {
            reject("guest-memory-read-failed");
            LOGW("NVDEC guest stream reader rejected command: {}", e.what());
        }
    }

    void NvDecClass::CallMethod(u32 method, u32 argument) {
        if (!registers.Write(method, argument)) {
            const auto n{unhandledMethodCount.fetch_add(1, std::memory_order_relaxed) + 1};
            if (n <= 5 || (n & (n - 1)) == 0)
                LOGW("NVDEC register outside documented bank: 0x{:X}", method);
            return;
        }
        if (method == nvdec_guest::CodecMethod) {
            const auto previous{lastCodecId.exchange(argument, std::memory_order_relaxed)};
            if (previous != argument)
                LOGI("NVDEC hardware codec selected: id=0x{:X}", argument);
            return;
        }
        if (method == nvdec_guest::ExecuteMethod) {
            const auto n{executeCount.fetch_add(1, std::memory_order_relaxed) + 1};
            if (n <= 4 || (n & (n - 1)) == 0)
                LOGI("NVDEC execute count={} codec=0x{:X}", n, lastCodecId.load());
            ProcessGuestDecode();
        }
    }

    bool NvDecClass::ConfigureMediaDecoder(VideoCodec codec, u32 width, u32 height) {
        return mediaCodecBackend.Start(codec, width, height);
    }

    bool NvDecClass::SubmitAccessUnit(span<const u8> packet, i64 ptsUs, bool isConfiguration) {
        return mediaCodecBackend.QueueAccessUnit(
            std::span<const std::uint8_t>(packet.data(), packet.size()), ptsUs, isConfiguration);
    }

    bool NvDecClass::TryGetDecodedFrame(AndroidMediaDecoder::Frame &frame, i64 waitUs) {
        if (readyFrame) {
            frame = std::move(*readyFrame);
            readyFrame.reset();
            return true;
        }
        return mediaCodecBackend.Drain(frame, waitUs);
    }
}

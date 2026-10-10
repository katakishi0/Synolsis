// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2026 Synolsis Emu contributors
#include "vic.h"
#include <soc.h>
#include <gpu/texture/layout.h>
#include <utility>
#include <atomic>

namespace skyline::soc::host1x {
    VicClass::VicClass(std::function<void()> callback, const DeviceState &deviceState,
                       NvDecClass &producer)
        : opDoneCallback(std::move(callback)), state(deviceState), decoder(producer) {}

    void VicClass::Execute() {
        const auto reject{[this](const char *cause) {
            const auto n{skippedCount.fetch_add(1, std::memory_order_relaxed) + 1};
            if (n <= 5 || (n & (n - 1)) == 0)
                LOGW("VIC guest image not available: reason={} count={}", cause, n);
        }};
        if (!state.soc || !configAddress || !lumaAddress ||
            configAddress > 0xFFFFFFFFULL - 0x28ULL) {
            reject("missing-mapped-guest-surfaces");
            return;
        }
        const auto mapped{[this](std::uint64_t at, std::size_t length) {
            if (at < 0x100 || !length || length > 64ULL * 1024 * 1024 ||
                at >= (1ULL << 32) || length > (1ULL << 32) - at)
                return false;
            const auto chunks{state.soc->smmu.TranslateRange(
                static_cast<u32>(at), static_cast<u32>(length))};
            std::size_t bytes{};
            for (auto chunk : chunks) {
                if (!chunk.data())
                    return false;
                bytes += chunk.size();
            }
            return bytes == length;
        }};
        try {
            if (!mapped(configAddress + 0x20ULL, sizeof(std::uint64_t))) {
                reject("vic-config-unmapped");
                return;
            }
            const auto configuration{state.soc->smmu.Read<std::uint64_t>(
                static_cast<u32>(configAddress + 0x20ULL))};
            const auto surface{vic_output::DecodeSurface(configuration)};
            if (!vic_output::SupportedSurface(surface)) {
                reject("unsupported-vic-format-or-block-linear-surface");
                return;
            }
            if (surface.format == vic_output::GuestYuv420 && !chromaAddress) {
                reject("missing-vic-chroma-surface");
                return;
            }
            AndroidMediaDecoder::Frame frame{};
            // Give an asynchronously decoding Android codec a short chance to
            // deliver output; a zero-time poll always missed some cutscenes.
            // Never stall the nvhost submit indefinitely.
            if (!decoder.TryGetDecodedFrame(frame, 12'000)) {
                reject("nvdec-output-not-ready");
                return;
            }
            // A hardware decoder may expose 1920x1088 for a 1920x1080
            // picture. Previous exact-width/height checks rejected the
            // otherwise valid VIC output. Accept at most one macroblock of
            // *trailing* alignment and never stretch an undersized frame.
            if (frame.width <= 0 || frame.height <= 0 ||
                !vic_output::DecodedFrameCoversSurface(
                    surface, static_cast<u32>(frame.width),
                    static_cast<u32>(frame.height))) {
                reject("decoded-frame-vic-extent-mismatch");
                return;
            }
            if (frame.width != static_cast<std::int32_t>(surface.width) ||
                frame.height != static_cast<std::int32_t>(surface.height))
                LOGD("VIC trims aligned decoded frame {}x{} to guest {}x{}",
                     frame.width, frame.height, surface.width, surface.height);

            vic_output::Output output{};
            const auto data = std::span<const std::uint8_t>(
                frame.data.data(), frame.data.size());
            const bool rgb = vic_output::IsPackedRgb(surface.format);
            const bool staged = rgb
                ? vic_output::PackLinearRgba(
                    data, surface.width, surface.height,
                    static_cast<u32>(frame.rowStride),
                    static_cast<u32>(frame.sliceHeight), frame.colorFormat,
                    surface, output,
                    static_cast<u32>(frame.cropLeft),
                    static_cast<u32>(frame.cropTop))
                : vic_output::PackLinearNv12(
                    data, surface.width, surface.height,
                    static_cast<u32>(frame.rowStride),
                    static_cast<u32>(frame.sliceHeight), frame.colorFormat,
                    surface, output,
                    static_cast<u32>(frame.cropLeft),
                    static_cast<u32>(frame.cropTop));
            if (!staged) {
                reject("unsupported-android-color-plane-layout");
                return;
            }
            if (surface.blockKind != 0) {
                // Convert verified planar staging with Skyline's own
                // MPL-2.0 guest texture layout routine; packed RGB is
                // one 4-bytes-per-pixel guest surface, never a UV plane.
                const auto gobHeight{std::size_t{1} << surface.blockHeightLog2};
                const gpu::texture::Dimensions yDimensions{
                    surface.width, surface.height, 1};
                const gpu::texture::Dimensions uvDimensions{
                    surface.width, surface.height / 2U, 1};
                const auto lumaBytes{gpu::texture::GetBlockLinearLayerSize(
                    yDimensions, 1, 1, rgb ? 4 : 1, gobHeight, 1)};
                const auto chromaBytes{rgb ? 0 : gpu::texture::GetBlockLinearLayerSize(
                    uvDimensions, 1, 1, 1, gobHeight, 1)};
                if (!lumaBytes || lumaBytes > (rgb ? 64ULL : 32ULL) * 1024 * 1024 ||
                    chromaBytes > 16ULL * 1024 * 1024) {
                    reject("block-linear-image-too-large");
                    return;
                }
                const u32 sourcePitch{((surface.width * (rgb ? 4U : 1U)) + 255U) & ~255U};
                std::vector<u8> tiledY(lumaBytes, 0);
                gpu::texture::CopyPitchToBlockLinear(
                    yDimensions, 1, 1, rgb ? 4 : 1,
                    sourcePitch, gobHeight, 1,
                    output.luma.data(), tiledY.data());
                output.luma = std::move(tiledY);
                if (!rgb) {
                    std::vector<u8> tiledUv(chromaBytes, 0);
                    gpu::texture::CopyPitchToBlockLinear(
                        uvDimensions, 1, 1, 1,
                        sourcePitch, gobHeight, 1,
                        output.chroma.data(), tiledUv.data());
                    output.chroma = std::move(tiledUv);
                }
            }
            if (!mapped(lumaAddress, output.luma.size()) ||
                (!output.chroma.empty() && !mapped(chromaAddress, output.chroma.size()))) {
                reject("vic-output-surfaces-unmapped");
                return;
            }
            // Only after decoder output format and guest mappings have been
            // validated do we write actual video pixels to the guest surface.
            state.soc->smmu.Write(static_cast<u32>(lumaAddress), output.luma.data(),
                                  static_cast<u32>(output.luma.size()));
            if (!output.chroma.empty())
                state.soc->smmu.Write(static_cast<u32>(chromaAddress), output.chroma.data(),
                                      static_cast<u32>(output.chroma.size()));
            // Device-only proof point: distinguish decoded MediaCodec frames
            // from actual guest surface writes. Do not log addresses/pixels.
            static std::atomic<std::uint64_t> completedVideoSurfaces{};
            const auto completed{completedVideoSurfaces.fetch_add(1, std::memory_order_relaxed) + 1};
            if (completed <= 5 || (completed % 120) == 0)
                LOGI("Synolsis NVDEC/VIC guest frame write #{}: {}x{} fmt=0x{:X} tiled={} ptsUs={} decodedBytes={}",
                     completed, surface.width, surface.height, surface.format,
                     surface.blockKind != 0, frame.ptsUs, frame.data.size());
        } catch (const std::exception &e) {
            reject("vic-memory-or-format-error");
            LOGW("VIC frame bridge rejected guest buffers: {}", e.what());
        }
    }

    void VicClass::CallMethod(u32 method, u32 argument) {
        switch (method) {
            case vic_output::ConfigMethod:
                configAddress = static_cast<std::uint64_t>(argument) << 8;
                break;
            case vic_output::LumaMethod:
                lumaAddress = static_cast<std::uint64_t>(argument) << 8;
                break;
            case vic_output::ChromaMethod:
                chromaAddress = static_cast<std::uint64_t>(argument) << 8;
                break;
            case vic_output::ExecuteMethod:
                Execute();
                break;
            default:
                break;
        }
    }
}

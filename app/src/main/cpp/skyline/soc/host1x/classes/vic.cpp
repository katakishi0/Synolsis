// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2026 Synolsis Emu contributors
#include "vic.h"
#include <soc.h>
#include <gpu/texture/layout.h>
#include <utility>

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
        if (!state.soc || !configAddress || !lumaAddress || !chromaAddress ||
            configAddress > 0xFFFFFFFFULL - 0x28ULL) {
            reject("missing-mapped-guest-surfaces");
            return;
        }
        const auto mapped{[this](std::uint64_t at, std::size_t length) {
            if (at < 0x100 || !length || length > 32ULL * 1024 * 1024 ||
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
            AndroidMediaDecoder::Frame frame{};
            // Give an asynchronously decoding Android codec a short chance to
            // deliver output; a zero-time poll always missed some cutscenes.
            // Never stall the nvhost submit indefinitely.
            if (!decoder.TryGetDecodedFrame(frame, 12'000)) {
                reject("nvdec-output-not-ready");
                return;
            }
            vic_output::Output output{};
            if (!vic_output::PackLinearNv12(
                    std::span<const std::uint8_t>(frame.data.data(), frame.data.size()),
                    static_cast<u32>(frame.width), static_cast<u32>(frame.height),
                    static_cast<u32>(frame.rowStride),
                    static_cast<u32>(frame.sliceHeight), frame.colorFormat,
                    surface, output)) {
                reject("unsupported-android-yuv-layout");
                return;
            }
            if (surface.blockKind != 0) {
                // Convert the verified linear Android output using Skyline's
                // existing MPL-licensed Tegra block-linear texture swizzler.
                // This path preserves the guest's declared block-height,
                // rather than writing pitch-linear data into tiled memory.
                const auto gobHeight{std::size_t{1} << surface.blockHeightLog2};
                const gpu::texture::Dimensions yDimensions{
                    surface.width, surface.height, 1};
                const gpu::texture::Dimensions uvDimensions{
                    surface.width, surface.height / 2U, 1};
                const auto lumaBytes{gpu::texture::GetBlockLinearLayerSize(
                    yDimensions, 1, 1, 1, gobHeight, 1)};
                const auto chromaBytes{gpu::texture::GetBlockLinearLayerSize(
                    uvDimensions, 1, 1, 1, gobHeight, 1)};
                if (!lumaBytes || !chromaBytes ||
                    lumaBytes > 32ULL * 1024 * 1024 ||
                    chromaBytes > 16ULL * 1024 * 1024) {
                    reject("block-linear-image-too-large");
                    return;
                }
                const u32 sourcePitch{(surface.width + 255U) & ~255U};
                std::vector<u8> tiledY(lumaBytes, 0), tiledUv(chromaBytes, 0);
                gpu::texture::CopyPitchToBlockLinear(
                    yDimensions, 1, 1, 1, sourcePitch, gobHeight, 1,
                    output.luma.data(), tiledY.data());
                gpu::texture::CopyPitchToBlockLinear(
                    uvDimensions, 1, 1, 1, sourcePitch, gobHeight, 1,
                    output.chroma.data(), tiledUv.data());
                output.luma = std::move(tiledY);
                output.chroma = std::move(tiledUv);
            }
            if (!mapped(lumaAddress, output.luma.size()) ||
                !mapped(chromaAddress, output.chroma.size())) {
                reject("vic-output-surfaces-unmapped");
                return;
            }
            // Only after decoder output format and guest mappings have been
            // validated do we write actual video pixels to the guest surface.
            state.soc->smmu.Write(static_cast<u32>(lumaAddress), output.luma.data(),
                                  static_cast<u32>(output.luma.size()));
            state.soc->smmu.Write(static_cast<u32>(chromaAddress), output.chroma.data(),
                                  static_cast<u32>(output.chroma.size()));
            LOGD("VIC wrote decoded YUV420 frame to guest memory: {}x{} (blockKind={})",
                 surface.width, surface.height, surface.blockKind);
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

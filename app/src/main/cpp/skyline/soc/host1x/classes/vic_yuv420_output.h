// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Independent, bounded linear YUV420 conversion. No block-linear image
// emulation is attempted here; incorrectly swizzled output is worse than
// keeping an unsupported VIC mode disabled.
namespace skyline::soc::host1x::vic_output {
    constexpr std::uint32_t ExecuteMethod = 0xC0;
    constexpr std::uint32_t ConfigMethod = 0x1C2;
    constexpr std::uint32_t LumaMethod = 0x1C8;
    constexpr std::uint32_t ChromaMethod = 0x1C9;

    constexpr std::int32_t AndroidYuvPlanar = 0x13;
    constexpr std::int32_t AndroidYuvSemiplanar = 0x15;
    constexpr std::uint32_t GuestYuv420 = 0x44;

    struct Surface {
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t blockKind{};
        std::uint32_t format{};
    };

    constexpr Surface DecodeSurface(std::uint64_t config) {
        return {
            static_cast<std::uint32_t>(((config >> 32) & 0x3FFFU) + 1),
            static_cast<std::uint32_t>(((config >> 46) & 0x3FFFU) + 1),
            static_cast<std::uint32_t>((config >> 11) & 0xFU),
            static_cast<std::uint32_t>(config & 0x7FU)
        };
    }

    constexpr bool SupportedSurface(Surface surface) {
        return surface.format == GuestYuv420 && surface.blockKind == 0 &&
               surface.width >= 16 && surface.height >= 16 &&
               surface.width <= 4096 && surface.height <= 2160 &&
               !(surface.width & 1U) && !(surface.height & 1U);
    }

    struct Output {
        std::vector<std::uint8_t> luma;
        std::vector<std::uint8_t> chroma;
    };

    // Android CPU output buffers may be padded by sourceStride and
    // sliceHeight. Guest VIC linear YUV420 uses 256-byte-aligned pitch.
    inline bool PackLinearNv12(std::span<const std::uint8_t> source,
                               std::uint32_t width, std::uint32_t height,
                               std::uint32_t sourceStride,
                               std::uint32_t sliceHeight,
                               std::int32_t androidFormat,
                               Surface surface, Output &output) {
        if (!SupportedSurface(surface) || width != surface.width ||
            height != surface.height || sourceStride < width ||
            sliceHeight < height || (sourceStride & 1U) ||
            (sliceHeight & 1U) ||
            sourceStride > 8192 || sliceHeight > 4096 ||
            (androidFormat != AndroidYuvPlanar &&
             androidFormat != AndroidYuvSemiplanar))
            return false;
        const auto pitch = (static_cast<std::size_t>(width) + 255U) & ~std::size_t{255};
        const auto ySize = pitch * height;
        const auto uvSize = pitch * (height / 2U);
        if (ySize + uvSize > 32U * 1024U * 1024U)
            return false;

        const auto fullYBytes = static_cast<std::size_t>(sourceStride) * sliceHeight;
        const auto chromaRows = sliceHeight / 2U;
        const auto chromaStride = androidFormat == AndroidYuvPlanar ?
                                  sourceStride / 2U : sourceStride;
        const auto planeBytes = static_cast<std::size_t>(chromaStride) * chromaRows;
        const auto required = fullYBytes +
                              (androidFormat == AndroidYuvPlanar ? planeBytes * 2 : planeBytes);
        if (required > source.size())
            return false;
        Output staged;
        staged.luma.assign(ySize, 0);
        staged.chroma.assign(uvSize, 0);
        for (std::size_t row = 0; row < height; ++row)
            std::copy_n(source.data() + row * sourceStride, width,
                        staged.luma.data() + row * pitch);
        for (std::size_t row = 0; row < height / 2; ++row) {
            const auto *chromaBase = source.data() + fullYBytes;
            auto *dst = staged.chroma.data() + row * pitch;
            if (androidFormat == AndroidYuvSemiplanar) {
                std::copy_n(chromaBase + row * chromaStride, width, dst);
            } else {
                const auto *u = chromaBase + row * chromaStride;
                const auto *v = chromaBase + planeBytes + row * chromaStride;
                for (std::size_t col = 0; col < width / 2; ++col) {
                    dst[col * 2] = u[col];
                    dst[col * 2 + 1] = v[col];
                }
            }
        }
        output = std::move(staged);
        return true;
    }
}

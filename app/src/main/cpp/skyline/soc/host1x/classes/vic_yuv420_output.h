// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Independent, bounded YUV420 staging. The caller uses Skyline's own
// MPL-licensed guest texture swizzler for block-linear output.
namespace skyline::soc::host1x::vic_output {
    constexpr std::uint32_t ExecuteMethod = 0xC0;
    constexpr std::uint32_t ConfigMethod = 0x1C2;
    constexpr std::uint32_t LumaMethod = 0x1C8;
    constexpr std::uint32_t ChromaMethod = 0x1C9;

    constexpr std::int32_t AndroidYuvPlanar = 0x13;
    constexpr std::int32_t AndroidYuvSemiplanar = 0x15;
    constexpr std::uint32_t GuestYuv420 = 0x44;
    // Tegra VIC packed 32-bit pixel formats. Independent implementation;
    // numeric hardware interface constants are not GPL implementation code.
    constexpr std::uint32_t GuestRgba8 = 0x1FU;
    constexpr std::uint32_t GuestBgra8 = 0x20U;
    constexpr std::uint32_t GuestRgbx8 = 0x23U;
    constexpr bool IsPackedRgb(std::uint32_t format) {
        return format == GuestRgba8 || format == GuestBgra8 ||
               format == GuestRgbx8;
    }

    struct Surface {
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t blockKind{};
        std::uint32_t format{};
        std::uint32_t blockHeightLog2{};
    };

    constexpr Surface DecodeSurface(std::uint64_t config) {
        return {
            static_cast<std::uint32_t>(((config >> 32) & 0x3FFFU) + 1),
            static_cast<std::uint32_t>(((config >> 46) & 0x3FFFU) + 1),
            static_cast<std::uint32_t>((config >> 11) & 0xFU),
            static_cast<std::uint32_t>(config & 0x7FU),
            static_cast<std::uint32_t>((config >> 15) & 0xFU)
        };
    }

    constexpr bool SupportedSurface(Surface surface) {
        return (surface.format == GuestYuv420 || IsPackedRgb(surface.format)) &&
               surface.blockHeightLog2 <= 4U &&
               surface.width >= 16 && surface.height >= 16 &&
               surface.width <= 4096 && surface.height <= 2160 &&
               !(surface.width & 1U) && !(surface.height & 1U);
    }

    struct Output {
        std::vector<std::uint8_t> luma;
        std::vector<std::uint8_t> chroma;
    };

    // Even-aligned MediaCodec crop origins are respected without reading
    // outside the padded Android YUV plane allocation.
    // Android CPU output buffers may be padded by sourceStride and
    // sliceHeight. Guest VIC linear YUV420 uses 256-byte-aligned pitch.
    inline bool PackLinearNv12(std::span<const std::uint8_t> source,
                               std::uint32_t width, std::uint32_t height,
                               std::uint32_t sourceStride,
                               std::uint32_t sliceHeight,
                               std::int32_t androidFormat,
                               Surface surface, Output &output,
                               std::uint32_t cropLeft = 0,
                               std::uint32_t cropTop = 0) {
        if (!SupportedSurface(surface) || surface.format != GuestYuv420 ||
            width != surface.width || height != surface.height ||
            cropLeft > sourceStride || width > sourceStride - cropLeft ||
            cropTop > sliceHeight || height > sliceHeight - cropTop ||
            (cropLeft & 1U) || (cropTop & 1U) ||
            (sourceStride & 1U) ||
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
            std::copy_n(source.data() + (row + cropTop) * sourceStride + cropLeft, width,
                        staged.luma.data() + row * pitch);
        for (std::size_t row = 0; row < height / 2; ++row) {
            const auto *chromaBase = source.data() + fullYBytes;
            auto *dst = staged.chroma.data() + row * pitch;
            if (androidFormat == AndroidYuvSemiplanar) {
                std::copy_n(chromaBase + (row + cropTop / 2U) * chromaStride + cropLeft,
                            width, dst);
            } else {
                const auto *u = chromaBase + (row + cropTop / 2U) * chromaStride + cropLeft / 2U;
                const auto *v = chromaBase + planeBytes +
                                (row + cropTop / 2U) * chromaStride + cropLeft / 2U;
                for (std::size_t col = 0; col < width / 2; ++col) {
                    dst[col * 2] = u[col];
                    dst[col * 2 + 1] = v[col];
                }
            }
        }
        output = std::move(staged);
        return true;
    }

    // Explicit, limited-range BT.601 YUV420 -> packed 8-bit RGB conversion
    // for VIC outputs that request a 32-bit RGB destination. This is a safe
    // baseline and may need BT.709/color metadata for accurate HD colors.
    constexpr std::uint8_t Channel(int value) {
        return static_cast<std::uint8_t>(
            std::clamp(value, 0, 255));
    }

    struct Rgb {
        std::uint8_t r{}, g{}, b{};
    };

    constexpr Rgb YuvToRgb(std::uint8_t y, std::uint8_t u, std::uint8_t v) {
        const int c = std::max(0, static_cast<int>(y) - 16);
        const int d = static_cast<int>(u) - 128;
        const int e = static_cast<int>(v) - 128;
        return {
            Channel((298 * c + 409 * e + 128) >> 8),
            Channel((298 * c - 100 * d - 208 * e + 128) >> 8),
            Channel((298 * c + 516 * d + 128) >> 8)
        };
    }

    // Only standard Android planar/semi-planar 8-bit layouts are accepted.
    // YUV_420_888 "flexible" byte buffers do not define their plane/pixel
    // strides, so accepting them by guessing corrupts the guest texture.
    // Guest packed RGB is a single output surface, with 256-byte row pitch.
    // Output is staged atomically and uses the existing SMMU validation
    // before the caller commits it to guest memory.
    inline bool PackLinearRgba(std::span<const std::uint8_t> source,
                              std::uint32_t width, std::uint32_t height,
                              std::uint32_t sourceStride,
                              std::uint32_t sliceHeight,
                              std::int32_t androidFormat,
                              Surface surface, Output &output,
                              std::uint32_t cropLeft = 0,
                              std::uint32_t cropTop = 0) {
        if (!SupportedSurface(surface) || !IsPackedRgb(surface.format) ||
            width != surface.width || height != surface.height ||
            cropLeft > sourceStride || width > sourceStride - cropLeft ||
            cropTop > sliceHeight || height > sliceHeight - cropTop ||
            (cropLeft & 1U) || (cropTop & 1U) ||
            (sourceStride & 1U) || (sliceHeight & 1U) ||
            sourceStride > 8192 || sliceHeight > 4096 ||
            (androidFormat != AndroidYuvPlanar &&
             androidFormat != AndroidYuvSemiplanar))
            return false;

        const std::size_t fullY = static_cast<std::size_t>(sourceStride) * sliceHeight;
        const std::size_t uvRows = sliceHeight / 2U;
        const std::size_t chromaStride =
            androidFormat == AndroidYuvPlanar ? sourceStride / 2U : sourceStride;
        const std::size_t planeBytes = chromaStride * uvRows;
        const std::size_t required = fullY +
            (androidFormat == AndroidYuvPlanar ? planeBytes * 2U : planeBytes);
        if (required > source.size())
            return false;

        const std::size_t pitch =
            (static_cast<std::size_t>(width) * 4U + 255U) & ~std::size_t{255U};
        const std::size_t rgbBytes = pitch * height;
        if (rgbBytes > 64ULL * 1024ULL * 1024ULL)
            return false;

        Output staged;
        staged.luma.assign(rgbBytes, 0);
        const auto *yPlane = source.data();
        const auto *uvPlane = source.data() + fullY;
        for (std::size_t row = 0; row < height; ++row) {
            auto *dest = staged.luma.data() + row * pitch;
            for (std::size_t column = 0; column < width; ++column) {
                const auto sampleY = yPlane[(row + cropTop) * sourceStride + cropLeft + column];
                const auto uvRow = ((row + cropTop) / 2U) * chromaStride;
                const auto uvCol = (column + cropLeft) / 2U;
                const auto sampleU = androidFormat == AndroidYuvPlanar
                    ? uvPlane[uvRow + uvCol]
                    : uvPlane[uvRow + uvCol * 2U];
                const auto sampleV = androidFormat == AndroidYuvPlanar
                    ? uvPlane[planeBytes + uvRow + uvCol]
                    : uvPlane[uvRow + uvCol * 2U + 1U];
                const auto rgb = YuvToRgb(sampleY, sampleU, sampleV);
                const auto out = column * 4U;
                if (surface.format == GuestBgra8) {
                    dest[out] = rgb.b;
                    dest[out + 1U] = rgb.g;
                    dest[out + 2U] = rgb.r;
                } else {
                    dest[out] = rgb.r;
                    dest[out + 1U] = rgb.g;
                    dest[out + 2U] = rgb.b;
                }
                dest[out + 3U] = 255U; // Alpha/X bits defined, no junk bytes.
            }
        }
        output = std::move(staged);
        return true;
    }

}

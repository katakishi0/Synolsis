// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

// Standalone protocol and bounds helpers; no GPL reference code is copied.
// Actual NVDEC register IDs documented in T210: 0x80 codec, 0xC0 execute,
// 0x101 picture context, and 0x102 frame bitstream.
namespace skyline::soc::host1x::nvdec_guest {
    constexpr std::uint32_t RegisterCount = 0x178;
    constexpr std::uint32_t CodecMethod = 0x80;
    constexpr std::uint32_t ExecuteMethod = 0xC0;
    constexpr std::uint32_t PictureInfoMethod = 0x101;
    constexpr std::uint32_t BitstreamMethod = 0x102;
    constexpr std::uint32_t MaximumPacketSize = 8 * 1024 * 1024;

    enum class Codec : std::uint8_t {
        Unknown, H264, VP8, H265, VP9
    };

    constexpr Codec TranslateCodec(std::uint32_t id) {
        switch (id) {
            case 3: return Codec::H264;
            case 5: return Codec::VP8;
            case 7: return Codec::H265;
            case 9: return Codec::VP9;
            default: return Codec::Unknown;
        }
    }

    class RegisterState {
        std::array<std::uint32_t, RegisterCount> words{};
      public:
        constexpr bool Write(std::uint32_t method, std::uint32_t argument) {
            if (method >= RegisterCount)
                return false;
            words[method] = argument;
            return true;
        }

        constexpr std::uint32_t Value(std::uint32_t method) const {
            return method < RegisterCount ? words[method] : 0;
        }

        // Hardware addresses are 256-byte aligned and stored shifted right
        // eight bits in the register payload.
        constexpr std::uint64_t Address(std::uint32_t method) const {
            return static_cast<std::uint64_t>(Value(method)) << 8;
        }
    };

    constexpr bool BoundedSmmuRange(std::uint64_t offset, std::uint64_t size) {
        constexpr std::uint64_t GuestAddressLimit = 1ULL << 32;
        return offset >= 0x100 && size > 0 && size <= MaximumPacketSize &&
               offset < GuestAddressLimit && size <= GuestAddressLimit - offset;
    }

    constexpr std::uint32_t LittleEndian32(std::span<const std::uint8_t> bytes,
                                           std::size_t at) {
        if (at > bytes.size() || bytes.size() - at < 4)
            return 0;
        return static_cast<std::uint32_t>(bytes[at]) |
               (static_cast<std::uint32_t>(bytes[at + 1]) << 8) |
               (static_cast<std::uint32_t>(bytes[at + 2]) << 16) |
               (static_cast<std::uint32_t>(bytes[at + 3]) << 24);
    }

    struct H264Context {
        std::uint32_t streamBytes{};
        std::uint32_t width{};
        std::uint32_t height{};
        bool valid{};
    };

    // Tegra H264 picture context: length + macroblock dimensions. This is
    // deliberately not a full hardware SPS/PPS encoder. Native MediaCodec
    // can receive a stream only if the guest supplied valid Annex-B headers.
    constexpr H264Context InspectH264Context(
        std::span<const std::uint8_t> context) {
        constexpr std::uint32_t StreamLengthOffset = 0x48;
        constexpr std::uint32_t MacroblockWidthOffset = 0x64;
        constexpr std::uint32_t MapHeightOffset = 0x68;
        constexpr std::uint32_t FrameOnlyOffset = 0x60;
        if (context.size() < 0x6C)
            return {};
        const auto count = LittleEndian32(context, StreamLengthOffset);
        const auto macroWidth = LittleEndian32(context, MacroblockWidthOffset);
        const auto mapHeight = LittleEndian32(context, MapHeightOffset);
        const auto frameOnly = LittleEndian32(context, FrameOnlyOffset);
        if (!count || count > MaximumPacketSize || !macroWidth ||
            !mapHeight || macroWidth > 256 || mapHeight > 256 ||
            frameOnly != 1)
            return {};
        const auto width = macroWidth * 16;
        const auto height = mapHeight * 16;
        const bool valid = width >= 16 && height >= 16 &&
                           width <= 4096 && height <= 4096 &&
                           static_cast<std::uint64_t>(width) * height <= 4096ULL * 2160ULL;
        return {count, width, height, valid};
    }

    // Verify a complete H264 access unit has decoder configuration NALs,
    // otherwise it must not be passed as if MediaCodec could infer SPS/PPS.
    constexpr bool ContainsH264ParameterSets(
        std::span<const std::uint8_t> data) {
        if (data.empty() || data.size() > MaximumPacketSize)
            return false;
        bool sps = false, pps = false;
        const auto limit = std::min<std::size_t>(data.size(), 128 * 1024);
        for (std::size_t i = 0; i + 4 < limit; ++i) {
            if (data[i] || data[i + 1])
                continue;
            std::size_t pos{};
            if (data[i + 2] == 1)
                pos = i + 3;
            else if (data[i + 2] == 0 && data[i + 3] == 1)
                pos = i + 4;
            if (!pos || pos >= limit)
                continue;
            const auto type = data[pos] & 0x1FU;
            sps |= type == 7U;
            pps |= type == 8U;
            if (sps && pps)
                return true;
        }
        return false;
    }
}

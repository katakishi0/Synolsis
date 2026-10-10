// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace skyline::soc::host1x::nvdec_probe {
    // Falcon NVDEC method indices. This is a new, independent implementation
    // of the public Tegra register interface, NOT borrowed GPL source code.
    constexpr std::uint32_t CodecMethod{0x80};
    constexpr std::uint32_t ExecuteMethod{0xC0};
    constexpr std::uint32_t ControlMethod{0x100};
    constexpr std::uint32_t PictureInfoMethod{0x101};
    constexpr std::uint32_t BitstreamMethod{0x102};
    constexpr std::size_t H264StreamLengthOffset{0x48};
    constexpr std::uint32_t H264CodecId{3};
    constexpr std::uint32_t MaxAccessUnitBytes{8U * 1024U * 1024U};

    // The guest encodes SMMU addresses in 256-byte units.
    constexpr std::optional<std::uint32_t> ExpandAddress(std::uint32_t shifted) {
        if (!shifted || shifted > (std::numeric_limits<std::uint32_t>::max() >> 8))
            return std::nullopt;
        return shifted << 8;
    }

    struct H264Packet {
        std::uint32_t address{};
        std::uint32_t size{};
        bool hasAnnexBPrefix{};
    };

    struct RegisterState {
        std::uint32_t codec{};
        std::uint32_t control{};
        std::uint32_t pictureInfoOffset{};
        std::uint32_t bitstreamOffset{};

        constexpr void Write(std::uint32_t method, std::uint32_t argument) {
            switch (method) {
                case CodecMethod: codec = argument; break;
                case ControlMethod: control = argument; break;
                case PictureInfoMethod: pictureInfoOffset = argument; break;
                case BitstreamMethod: bitstreamOffset = argument; break;
                default: break;
            }
        }

        constexpr bool IsH264() const {
            // Codec may be selected via SET_CODEC_ID or control_params.
            // If both are present and disagree, do not decode guesswork.
            const auto controlled = control & 0x7U;
            return (codec == H264CodecId && (controlled == 0 || controlled == H264CodecId)) ||
                   (codec == 0 && controlled == H264CodecId);
        }
    };

    constexpr bool ValidAddressRange(std::uint32_t address, std::uint32_t count) {
        return address != 0 && count > 0 && count <= MaxAccessUnitBytes &&
               static_cast<std::uint64_t>(address) + count <=
                   static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ULL;
    }

    constexpr std::uint32_t Little32(std::span<const std::uint8_t, 4> bytes) {
        return static_cast<std::uint32_t>(bytes[0]) |
            (static_cast<std::uint32_t>(bytes[1]) << 8) |
            (static_cast<std::uint32_t>(bytes[2]) << 16) |
            (static_cast<std::uint32_t>(bytes[3]) << 24);
    }

    constexpr bool HasAnnexB(std::span<const std::uint8_t, 4> p) {
        return p[0] == 0 && p[1] == 0 &&
               (p[2] == 1 || (p[2] == 0 && p[3] == 1));
    }

    // Read callback signature: bool(uint32_t guestAddress, span<uint8_t> out).
    // Probe only: packet bytes and SPS/PPS must be passed to MediaCodec by a
    // separately validated decoder, then VIC must commit output to guest VRAM.
    // No fake frame or syncpoint completion is emitted by this helper.
    template<typename ReadGuest>
    std::optional<H264Packet> Inspect(const RegisterState &state, ReadGuest &&read) {
        if (!state.IsH264())
            return std::nullopt;
        const auto pic = ExpandAddress(state.pictureInfoOffset);
        const auto bitstream = ExpandAddress(state.bitstreamOffset);
        if (!pic || !bitstream ||
            static_cast<std::uint64_t>(*pic) + H264StreamLengthOffset + 4 >
                static_cast<std::uint64_t>(std::numeric_limits<std::uint32_t>::max()) + 1ULL)
            return std::nullopt;

        std::array<std::uint8_t, 4> lengthBytes{};
        if (!read(*pic + static_cast<std::uint32_t>(H264StreamLengthOffset),
                  std::span<std::uint8_t>{lengthBytes}))
            return std::nullopt;
        const auto length = Little32(lengthBytes);
        if (!ValidAddressRange(*bitstream, length))
            return std::nullopt;
        std::array<std::uint8_t, 4> prefix{};
        if (!read(*bitstream, std::span<std::uint8_t>{prefix}))
            return std::nullopt;
        return H264Packet{*bitstream, length, HasAnnexB(prefix)};
    }
}

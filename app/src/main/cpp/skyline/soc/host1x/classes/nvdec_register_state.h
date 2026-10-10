// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace skyline::soc::host1x {
    // Own, dependency-free register bookkeeping for Tegra's NVDEC class.
    // It intentionally does not read guest memory, submit fake frames, or
    // acknowledge a decode that has not actually occurred.
    class NvdecRegisterState {
      public:
        static constexpr std::size_t RegisterCount = 0x178;
        static constexpr std::uint32_t SetCodec = 0x80;
        static constexpr std::uint32_t Execute = 0xC0;
        static constexpr std::uint32_t Control = 0x100;
        static constexpr std::uint32_t PictureInfo = 0x101;
        static constexpr std::uint32_t Bitstream = 0x102;
        static constexpr std::uint32_t FrameNumber = 0x103;
        static constexpr std::uint32_t H264SliceOffsets = 0x104;
        static constexpr std::uint32_t LumaSurface0 = 0x10C;
        static constexpr std::uint32_t ChromaSurface0 = 0x11D;

        enum class Codec : std::uint32_t {
            None = 0,
            H264 = 3,
            VP8 = 5,
            H265 = 7,
            VP9 = 9,
        };

        struct Snapshot {
            std::uint32_t codecId{};
            std::uint64_t pictureInfoAddress{};
            std::uint64_t bitstreamAddress{};
            std::uint64_t h264SliceOffsetsAddress{};
            std::uint64_t outputLumaAddress{};
            std::uint64_t outputChromaAddress{};
            std::uint64_t frameNumberRegister{};
            bool codecRecognized{};
            bool hasInputAddresses{};
        };

        // Except for the codec selector, Tegra exposes 256-byte address
        // units for the offset registers. Keep the raw codec ID separately.
        constexpr bool Write(std::uint32_t method, std::uint32_t argument) {
            if (method >= RegisterCount)
                return false;
            registers[method] = method == SetCodec
                ? static_cast<std::uint64_t>(argument)
                : (static_cast<std::uint64_t>(argument) << 8);
            return true;
        }

        constexpr std::uint64_t Read(std::uint32_t method) const {
            return method < RegisterCount ? registers[method] : 0;
        }

        static constexpr bool IsRecognizedCodec(std::uint32_t id) {
            return id == static_cast<std::uint32_t>(Codec::H264)
                || id == static_cast<std::uint32_t>(Codec::VP8)
                || id == static_cast<std::uint32_t>(Codec::VP9)
                || id == static_cast<std::uint32_t>(Codec::H265);
        }

        constexpr Snapshot Capture() const {
            const auto codecId = static_cast<std::uint32_t>(Read(SetCodec));
            const auto picture = Read(PictureInfo);
            const auto bitstream = Read(Bitstream);
            return Snapshot{
                codecId,
                picture,
                bitstream,
                Read(H264SliceOffsets),
                Read(LumaSurface0),
                Read(ChromaSurface0),
                Read(FrameNumber),
                IsRecognizedCodec(codecId),
                picture != 0 && bitstream != 0,
            };
        }

        constexpr void Reset() { registers.fill(0); }

      private:
        std::array<std::uint64_t, RegisterCount> registers{};
    };
}

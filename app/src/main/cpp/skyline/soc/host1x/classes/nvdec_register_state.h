// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace skyline::soc::host1x {
    /**
     * Shadow state for the NVDEC host1x method stream.
     *
     * This is NOT a software decoder: it is only the validated register
     * stream required before guest SMMU bitstream access can be implemented.
     * Host1x register indices are documented independently in the Tegra
     * register map (also observed in yuzu/Citron); no GPL code is copied.
     *
     * Values in the method stream are 32-bit words. The NVDEC hardware
     * address registers use 256-byte units, represented here as u64 values
     * shifted left by eight. Raw values remain available for codec selection.
     */
    class NvdecRegisterState final {
      public:
        static constexpr std::size_t Count{0x178};
        static constexpr std::uint32_t SetCodec{0x80};
        static constexpr std::uint32_t Execute{0xC0};
        static constexpr std::uint32_t ControlParams{0x100};
        static constexpr std::uint32_t PictureInfo{0x101};
        static constexpr std::uint32_t BitstreamAddress{0x102};
        static constexpr std::uint32_t FrameNumber{0x103};

        constexpr bool Write(std::uint32_t method, std::uint32_t raw) {
            if (method >= Count)
                return false;
            rawValues[method] = raw;
            addressScaledValues[method] = static_cast<std::uint64_t>(raw) << 8;
            return true;
        }

        constexpr std::uint32_t Raw(std::uint32_t method) const {
            return method < Count ? rawValues[method] : 0;
        }

        constexpr std::uint64_t AddressScaled(std::uint32_t method) const {
            return method < Count ? addressScaledValues[method] : 0;
        }

        constexpr std::uint32_t CodecId() const {
            return Raw(SetCodec);
        }

        constexpr std::uint64_t GuestPictureInfoAddress() const {
            return AddressScaled(PictureInfo);
        }

        constexpr std::uint64_t GuestBitstreamAddress() const {
            return AddressScaled(BitstreamAddress);
        }

        constexpr void Reset() {
            rawValues.fill(0);
            addressScaledValues.fill(0);
        }

      private:
        std::array<std::uint32_t, Count> rawValues{};
        std::array<std::uint64_t, Count> addressScaledValues{};
    };
}

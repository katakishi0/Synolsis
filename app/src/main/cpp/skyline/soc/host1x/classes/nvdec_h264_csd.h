// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once
#include "nvdec_guest_stream.h"
#include <bit>
#include <cstdint>
#include <span>
#include <vector>

// Independently encoded AVC parameter sets from *guest NVDEC parameters*.
// H.264 syntax per ITU-T H.264: never synthesizes a video frame. In addition
// to a conservative parameter-set subset, unsupported interlaced/complex
// picture contexts are deliberately rejected rather than guessed.
namespace skyline::soc::host1x::h264_csd {
    class RbspWriter {
        std::vector<std::uint8_t> data;
        std::uint8_t current{};
        std::uint8_t count{};
      public:
        void Bit(bool v) {
            current = static_cast<std::uint8_t>((current << 1) | (v ? 1 : 0));
            if (++count == 8) {
                data.push_back(current);
                count = 0;
                current = 0;
            }
        }
        void Bits(std::uint32_t value, std::uint32_t width) {
            for (std::uint32_t index = width; index; --index)
                Bit((value >> (index - 1)) & 1U);
        }
        void Ue(std::uint32_t value) {
            const auto code = static_cast<std::uint64_t>(value) + 1ULL;
            const auto leading = static_cast<std::uint32_t>(
                63U - std::countl_zero(code));
            for (std::uint32_t i{}; i<leading; ++i)
                Bit(false);
            Bit(true);
            if (leading)
                Bits(static_cast<std::uint32_t>(code), leading);
        }
        void Se(std::int32_t value) {
            const auto code = value <= 0 ?
                static_cast<std::uint32_t>(-static_cast<std::int64_t>(value) * 2LL) :
                static_cast<std::uint32_t>(static_cast<std::int64_t>(value) * 2LL - 1LL);
            Ue(code);
        }
        void Finish() {
            Bit(true);
            while(count)
                Bit(false);
        }
        const std::vector<std::uint8_t> &Bytes() const { return data; }
    };

    inline void AppendNal(std::vector<std::uint8_t> &destination,
                          std::uint8_t nalHeader,
                          const std::vector<std::uint8_t> &rbsp) {
        destination.insert(destination.end(), {0,0,0,1,nalHeader});
        unsigned zeroCount = 0;
        for (auto b : rbsp) {
            if (zeroCount >= 2 && b <= 3U) {
                destination.push_back(3U);
                zeroCount = 0;
            }
            destination.push_back(b);
            zeroCount = b == 0 ? zeroCount + 1 : 0;
        }
    }

    constexpr std::int32_t SignExtend(std::uint32_t value, unsigned bits) {
        const auto m = 1U << (bits - 1U);
        const auto mask = (1U << bits) - 1U;
        value &= mask;
        return static_cast<std::int32_t>((value ^ m) - m);
    }

    struct HeaderPair {
        std::vector<std::uint8_t> packet;
        bool valid{};
    };

    // We only handle progressive 8-bit 4:2:0 with no explicit scaling
    // matrices. The guest parameter set supplies frame dimensions, POC,
    // reference counts and PPS entropy/quantizer flags.
    inline HeaderPair Build(std::span<const std::uint8_t> context) {
        if (context.size() < 0xB8)
            return {};
        const auto params = nvdec_guest::InspectH264Context(context);
        if (!params.valid)
            return {};
        const auto u32at = [&](std::size_t at) {
            return nvdec_guest::LittleEndian32(context, at);
        };
        const std::uint64_t flags =
            static_cast<std::uint64_t>(u32at(0xB0)) |
            (static_cast<std::uint64_t>(u32at(0xB4)) << 32);
        const auto chroma = static_cast<unsigned>((flags >> 12) & 3U);
        const auto poc = static_cast<unsigned>((flags >> 14) & 3U);
        if (chroma != 1 || poc > 1)
            return {};
        const auto log2frame = static_cast<unsigned>((flags >> 8) & 0xFU);
        const auto pocBits = u32at(0x58);
        if (log2frame > 12 || pocBits > 12)
            return {};
        const auto widthMb = u32at(0x64);
        const auto heightMb = u32at(0x68);
        const auto ref0 = u32at(0x78);
        const auto ref1 = u32at(0x7C);
        if (ref0 > 15 || ref1 > 15)
            return {};

        RbspWriter sps;
        sps.Bits(100,8); // High profile, 8-bit 4:2:0
        sps.Bits(0,8); // constraint flags
        sps.Bits(params.width > 2048 || params.height > 1152 ? 51 : 40,8);
        sps.Ue(0); // seq parameter set id
        sps.Ue(1); // chroma_format_idc 4:2:0
        sps.Ue(0); // bit_depth_luma_minus8
        sps.Ue(0); // bit_depth_chroma_minus8
        sps.Bit(false); // qpprime_y_zero_transform_bypass_flag
        sps.Bit(false); // seq_scaling_matrix_present_flag
        sps.Ue(log2frame);
        sps.Ue(poc);
        if (poc == 0) {
            sps.Ue(pocBits);
        } else {
            sps.Bit(u32at(0x5C) != 0);
            sps.Se(0); sps.Se(0); sps.Ue(0);
        }
        sps.Ue(16); // max_num_ref_frames
        sps.Bit(false); // gaps_in_frame_num_value_allowed_flag
        sps.Ue(widthMb - 1);
        sps.Ue(heightMb - 1);
        sps.Bit(true); // frame_mbs_only_flag verified by policy
        sps.Bit(((flags >> 1) & 1U) != 0); // direct_8x8_inference_flag
        sps.Bit(false); // no frame cropping
        sps.Bit(false); // no vui_parameters
        sps.Finish();

        RbspWriter pps;
        pps.Ue(0); pps.Ue(0); // pic_parameter_set_id, seq_parameter_set_id
        pps.Bit(u32at(0x70) != 0); // entropy_coding_mode_flag
        pps.Bit(u32at(0x74) != 0); // bottom_field_pic_order_in_frame_present_flag
        pps.Ue(0); // num_slice_groups_minus1
        pps.Ue(ref0); pps.Ue(ref1);
        pps.Bit(((flags >> 2) & 1U) != 0); // weighted_pred_flag
        pps.Bits(static_cast<std::uint32_t>((flags >> 32) & 3U), 2);
        pps.Se(SignExtend(static_cast<std::uint32_t>((flags >> 16) & 0x3FU), 6));
        pps.Se(0);
        pps.Se(SignExtend(static_cast<std::uint32_t>((flags >> 22) & 0x1FU), 5));
        pps.Bit(u32at(0x80) != 0);
        pps.Bit(((flags >> 3) & 1U) != 0);
        pps.Bit(u32at(0x84) != 0);
        pps.Bit(u32at(0x88) != 0); // transform_8x8_mode_flag
        pps.Bit(false); // no scaling matrices
        pps.Se(SignExtend(static_cast<std::uint32_t>((flags >> 27) & 0x1FU), 5));
        pps.Finish();

        HeaderPair result;
        AppendNal(result.packet, 0x67, sps.Bytes());
        AppendNal(result.packet, 0x68, pps.Bytes());
        result.valid = result.packet.size() < 2048;
        return result;
    }
}

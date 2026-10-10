// SPDX-License-Identifier: MPL-2.0
// Copyright 2026 Synolsis Emu contributors
#pragma once

#include "nvdec_guest_stream.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace skyline::soc::host1x::h264_access_unit {
    // Decode actual guest payloads only: complete Annex-B or a sequence of
    // 4-byte big-endian NAL lengths. Both formats contain real NAL bytes;
    // this code NEVER generates a video frame or guesses payload offsets.
    enum class Format : std::uint8_t { Invalid, AnnexB, LengthPrefixed4 };
    struct Result {
        std::vector<std::uint8_t> bytes;
        Format format{Format::Invalid};
        std::uint32_t nalCount{};
        bool containsPicture{};
        explicit operator bool() const { return format != Format::Invalid; }
    };
    constexpr bool PlausibleNal(std::uint8_t header) {
        const auto type = header & 0x1FU;
        return (header & 0x80U) == 0 && type >= 1U && type <= 23U;
    }
    constexpr bool IsPictureNal(std::uint8_t header) {
        const auto type = header & 0x1FU;
        return type >= 1U && type <= 5U;
    }
    constexpr std::size_t MaxNals{4096};

    inline std::size_t StartCodeLength(std::span<const std::uint8_t> data,
                                       std::size_t pos) {
        if (pos + 3U > data.size() || data[pos] || data[pos + 1]) return 0;
        if (data[pos + 2] == 1U) return 3;
        return pos + 4U <= data.size() && data[pos + 2] == 0U &&
               data[pos + 3] == 1U ? 4 : 0;
    }

    inline Result ParseAnnexB(std::span<const std::uint8_t> source) {
        Result parsed{};
        if (source.empty() || source.size() > nvdec_guest::MaximumPacketSize) return {};
        std::size_t leadingZeros{};
        while (leadingZeros < source.size() && leadingZeros < 4U &&
               source[leadingZeros] == 0U) ++leadingZeros;
        std::size_t start{};
        bool found{};
        for (std::size_t pos{}; pos <= std::min(leadingZeros, std::size_t{3}); ++pos) {
            if (StartCodeLength(source, pos)) { start = pos; found = true; break; }
        }
        if (!found) return {};
        auto cursor = start;
        while (cursor < source.size()) {
            const auto marker = StartCodeLength(source, cursor);
            if (!marker) return {};
            const auto nalStart = cursor + marker;
            if (nalStart >= source.size() || !PlausibleNal(source[nalStart])) return {};
            parsed.containsPicture |= IsPictureNal(source[nalStart]);
            if (++parsed.nalCount > MaxNals) return {};
            std::size_t next = nalStart + 1;
            while (next + 3U <= source.size() && !StartCodeLength(source, next)) ++next;
            if (next + 3U > source.size()) break;
            cursor = next;
        }
        if (!parsed.containsPicture) return {};
        parsed.bytes.assign(source.begin(), source.end());
        parsed.format = Format::AnnexB;
        return parsed;
    }

    inline Result ParseLengthPrefixed4(std::span<const std::uint8_t> source) {
        Result parsed{};
        if (source.empty() || source.size() > nvdec_guest::MaximumPacketSize) return {};
        parsed.bytes.reserve(source.size());
        std::size_t pos{};
        while (pos < source.size()) {
            if (source.size() - pos < 5U || parsed.nalCount >= MaxNals) return {};
            const std::uint32_t n =
                (std::uint32_t(source[pos]) << 24U) |
                (std::uint32_t(source[pos + 1]) << 16U) |
                (std::uint32_t(source[pos + 2]) << 8U) |
                 std::uint32_t(source[pos + 3]);
            pos += 4U;
            if (!n || n > source.size() - pos || !PlausibleNal(source[pos]))
                return {};
            parsed.containsPicture |= IsPictureNal(source[pos]);
            parsed.bytes.insert(parsed.bytes.end(), {0, 0, 0, 1});
            parsed.bytes.insert(parsed.bytes.end(), source.begin() + pos,
                                source.begin() + pos + n);
            pos += n;
            ++parsed.nalCount;
        }
        if (!parsed.containsPicture ||
            parsed.bytes.size() > nvdec_guest::MaximumPacketSize) return {};
        parsed.format = Format::LengthPrefixed4;
        return parsed;
    }

    inline Result Normalize(std::span<const std::uint8_t> source) {
        if (auto annexB = ParseAnnexB(source); annexB) return annexB;
        return ParseLengthPrefixed4(source);
    }
}

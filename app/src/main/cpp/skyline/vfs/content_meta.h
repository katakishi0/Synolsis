// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>

namespace skyline::vfs::content_meta {
    // nn::ncm::ContentMetaType (CNMT, not NCA contentType).
    enum class Type : std::uint8_t {
        Application = 0x80,
        Patch = 0x81,
        AddOnContent = 0x82,
        Delta = 0x83,
        DataPatch = 0x84,
    };

    struct Summary {
        std::uint64_t titleId{};
        std::uint64_t applicationId{};
        std::uint32_t titleVersion{};
        Type type{};
        std::uint16_t contentCount{};
        std::uint16_t contentMetaCount{};
        std::uint16_t extendedHeaderSize{};
    };

    // Inspect *decrypted* CNMT bytes obtained through the NCA VFS layer.
    // This does NOT authenticate Nintendo signatures/digests or mount patches.
    inline std::optional<Summary> Parse(std::span<const std::uint8_t> bytes) {
        constexpr std::size_t HeaderSize{0x20};
        constexpr std::size_t ContentInfoSize{0x38};
        constexpr std::size_t MetaInfoSize{0x10};
        if (bytes.size() < HeaderSize)
            return std::nullopt;

        const auto read16 = [&bytes](std::size_t offset) -> std::uint16_t {
            return static_cast<std::uint16_t>(
                std::uint16_t(bytes[offset]) |
                (std::uint16_t(bytes[offset + 1]) << 8));
        };
        const auto read32 = [&bytes](std::size_t offset) -> std::uint32_t {
            std::uint32_t value{};
            for (std::size_t i{}; i < 4; ++i)
                value |= static_cast<std::uint32_t>(bytes[offset + i]) << (i * 8);
            return value;
        };
        const auto read64 = [&bytes](std::size_t offset) -> std::uint64_t {
            std::uint64_t value{};
            for (std::size_t i{}; i < 8; ++i)
                value |= static_cast<std::uint64_t>(bytes[offset + i]) << (i * 8);
            return value;
        };

        const auto type = static_cast<Type>(bytes[0xC]);
        if (type != Type::Application && type != Type::Patch &&
            type != Type::AddOnContent && type != Type::Delta &&
            type != Type::DataPatch)
            return std::nullopt;

        const auto extSize = read16(0xE);
        const auto contentCount = read16(0x10);
        const auto metaCount = read16(0x12);

        // Validate each range before reading the optional application ID.
        // Counts are 16-bit; on a 32-bit host, use checked size arithmetic.
        const auto remaining = bytes.size() - HeaderSize;
        if (extSize > remaining)
            return std::nullopt;
        const auto entries = remaining - extSize;
        if (static_cast<std::size_t>(contentCount) > entries / ContentInfoSize)
            return std::nullopt;
        const auto metaBytes = entries - static_cast<std::size_t>(contentCount) * ContentInfoSize;
        if (static_cast<std::size_t>(metaCount) > metaBytes / MetaInfoSize)
            return std::nullopt;

        const auto titleId = read64(0x0);
        if (titleId == 0)
            return std::nullopt;

        std::uint64_t applicationId = titleId;
        if (type != Type::Application) {
            const std::size_t idOffset = type == Type::DataPatch ? 8 : 0;
            if (extSize < idOffset + 8)
                return std::nullopt;
            applicationId = read64(HeaderSize + idOffset);
            if (applicationId == 0)
                return std::nullopt;
        }

        return Summary{
            .titleId = titleId,
            .applicationId = applicationId,
            .titleVersion = read32(0x8),
            .type = type,
            .contentCount = contentCount,
            .contentMetaCount = metaCount,
            .extendedHeaderSize = extSize
        };
    }
}

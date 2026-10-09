// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace skyline::vfs::bktr {
    // The decrypted BKTR indirect relocation table is a 0x4000 header,
    // followed by 0x4000-byte buckets with 0x14-byte entries.
    constexpr std::size_t BucketSize{0x4000};
    constexpr std::size_t BucketEntriesOffset{0x10};
    constexpr std::size_t RelocationSize{0x14};
    constexpr std::size_t MaxBuckets{4096};

    struct Relocation {
        std::uint64_t virtualOffset{};
        std::uint64_t sourceOffset{};
        bool fromPatch{};
    };

    struct Region {
        bool fromPatch{};
        std::uint64_t sourceOffset{};
        std::uint64_t size{};
    };

    class RelocationTable {
        std::vector<Relocation> entries;
        std::uint64_t virtualSize{};

        static std::uint32_t Read32(std::span<const std::uint8_t> bytes, std::size_t at) {
            std::uint32_t v{};
            for (std::size_t i{}; i < 4; ++i)
                v |= std::uint32_t(bytes[at+i]) << (8*i);
            return v;
        }
        static std::uint64_t Read64(std::span<const std::uint8_t> bytes, std::size_t at) {
            std::uint64_t v{};
            for (std::size_t i{}; i < 8; ++i)
                v |= std::uint64_t(bytes[at+i]) << (8*i);
            return v;
        }

      public:
        std::uint64_t Size() const { return virtualSize; }
        std::size_t EntryCount() const { return entries.size(); }

        // Input must be the fully decrypted BKTR *relocation* region.
        // The subsection AES-CTR-Ex table must separately be processed
        // before patched physical bytes can safely be read.
        static std::optional<RelocationTable> Parse(std::span<const std::uint8_t> bytes) {
            if (bytes.size() < BucketSize)
                return std::nullopt;
            const std::size_t count{Read32(bytes, 4)};
            const auto totalSize{Read64(bytes, 8)};
            if (!count || count > MaxBuckets || !totalSize ||
                count > (bytes.size() - BucketSize) / BucketSize)
                return std::nullopt;
            RelocationTable table;
            table.virtualSize = totalSize;
            const auto entryCapacity{(BucketSize - BucketEntriesOffset) / RelocationSize};
            std::uint64_t previous{};
            for (std::size_t bucket{}; bucket < count; ++bucket) {
                const auto expectedBegin{Read64(bytes, BucketEntriesOffset + bucket * 8)};
                const auto offset{BucketSize * (1 + bucket)};
                const auto numEntries{Read32(bytes, offset + 4)};
                const auto bucketEnd{Read64(bytes, offset + 8)};
                if (!numEntries || numEntries > entryCapacity || bucketEnd > totalSize ||
                    bucketEnd <= previous || expectedBegin != previous)
                    return std::nullopt;
                for (std::size_t i{}; i < numEntries; ++i) {
                    const auto at{offset + BucketEntriesOffset + i * RelocationSize};
                    const auto guest{Read64(bytes, at)};
                    const auto source{Read64(bytes, at + 8)};
                    const auto isPatch{Read32(bytes, at + 0x10)};
                    if (isPatch > 1 || guest >= bucketEnd ||
                        (table.entries.empty() ? guest != 0 : guest <= table.entries.back().virtualOffset) ||
                        (i == 0 && guest != previous))
                        return std::nullopt;
                    table.entries.push_back({guest, source, isPatch != 0});
                }
                previous = bucketEnd;
            }
            return previous == totalSize ? std::optional{std::move(table)} : std::nullopt;
        }

        std::optional<Region> Resolve(std::uint64_t offset,
                                      std::uint64_t baseSize,
                                      std::uint64_t patchSize) const {
            if (offset >= virtualSize || entries.empty())
                return std::nullopt;
            const auto it{std::upper_bound(entries.begin(), entries.end(), offset,
                [](std::uint64_t value, const Relocation &record) {
                    return value < record.virtualOffset;
                })};
            if (it == entries.begin())
                return std::nullopt;
            const auto entry{std::prev(it)};
            const auto next{it == entries.end() ? virtualSize : it->virtualOffset};
            const auto delta{offset - entry->virtualOffset};
            const auto limit{entry->fromPatch ? patchSize : baseSize};
            if (entry->sourceOffset > limit || delta > limit - entry->sourceOffset)
                return std::nullopt;
            const auto physical{entry->sourceOffset + delta};
            const auto available{limit - physical};
            const auto length{std::min(next - offset, available)};
            if (!length)
                return std::nullopt;
            return Region{entry->fromPatch, physical, length};
        }
        // Reconstruct a virtual RomFS interval from *already decrypted*
        // base and patch byte images. This is not the NCA on-disk CTR-Ex
        // path; callers must decrypt each patch subsection correctly.
        bool ReadDecrypted(std::span<const std::uint8_t> base,
                           std::span<const std::uint8_t> patch,
                           std::span<std::uint8_t> output,
                           std::uint64_t virtualOffset) const {
            if (virtualOffset > virtualSize ||
                output.size() > virtualSize - virtualOffset)
                return false;
            std::size_t written{};
            while (written < output.size()) {
                auto segment{Resolve(virtualOffset + written, base.size(), patch.size())};
                if (!segment)
                    return false;
                const auto count{static_cast<std::size_t>(
                    std::min<std::uint64_t>(segment->size, output.size() - written))};
                const auto source{segment->fromPatch ? patch : base};
                std::copy_n(source.begin() + static_cast<std::size_t>(segment->sourceOffset),
                            count, output.begin() + written);
                written += count;
            }
            return true;
        }
    };
}

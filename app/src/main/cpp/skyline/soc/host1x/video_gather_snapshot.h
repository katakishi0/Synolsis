// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace skyline::soc::host1x::video_gather {
    // nvhost guest command buffers can be modified/reused while an ioctl
    // processes them. Parse and dispatch one consistent owned snapshot.
    // This helper does not touch guest syncpoints, video decoder or GPU.
    constexpr std::size_t MaximumBuffers = 128;
    constexpr std::size_t MaximumWords = 262144; // 1 MiB, bounded on low-RAM phones.

    class Snapshot final {
        std::vector<std::vector<std::uint32_t>> owned;
        std::vector<std::span<const std::uint32_t>> references;
      public:
        bool Capture(std::span<const std::span<const std::uint32_t>> inputs) {
            if (inputs.size() > MaximumBuffers)
                return false;
            std::size_t total{};
            for (const auto input : inputs) {
                if (input.size() > MaximumWords - total)
                    return false;
                total += input.size();
            }
            // Build an independent candidate so rejection never changes the
            // previous valid snapshot and no partially copied buffers escape.
            Snapshot candidate;
            candidate.owned.reserve(inputs.size());
            for (const auto input : inputs) {
                if (!input.empty() && !input.data())
                    return false;
                if (input.empty())
                    candidate.owned.emplace_back();
                else
                    candidate.owned.emplace_back(input.begin(), input.end());
            }
            candidate.references.reserve(candidate.owned.size());
            for (const auto &buffer : candidate.owned)
                candidate.references.emplace_back(buffer.data(), buffer.size());
            *this = std::move(candidate);
            return true;
        }

        Snapshot() = default;
        Snapshot(const Snapshot &) = delete;
        Snapshot &operator=(const Snapshot &) = delete;
        Snapshot(Snapshot &&) = default;
        Snapshot &operator=(Snapshot &&) = default;

        std::span<const std::span<const std::uint32_t>> Buffers() const {
            return references;
        }
    };
}

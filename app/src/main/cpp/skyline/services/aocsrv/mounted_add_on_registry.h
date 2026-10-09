// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>
#include <vfs/backing.h>
#include "aoc_title_id.h"

namespace skyline::service::aocsrv {
    // AOC is only visible once it has a real, decrypted RomFS backing.
    // An imported/staged NSP by itself must never increase AOC counts.
    class MountedAddOnRegistry {
      public:
        struct Entry {
            std::uint64_t titleId{};
            std::uint32_t index{};
            std::shared_ptr<vfs::Backing> romFs;
        };

      private:
        mutable std::mutex mutex;
        std::uint64_t applicationId{};
        std::vector<Entry> mounted;

      public:
        static std::uint64_t BaseId(std::uint64_t appId) {
            return AddOnBaseId(appId);
        }

        void SetApplication(std::uint64_t appId) {
            std::scoped_lock lock(mutex);
            mounted.clear();
            applicationId = appId;
        }

        bool Add(std::uint64_t aocId, std::shared_ptr<vfs::Backing> backing) {
            if (!backing || !backing->size)
                return false;
            std::scoped_lock lock(mutex);
            const auto index{AddOnIndex(applicationId, aocId)};
            if (!index || mounted.size() >= 2048)
                return false;
            if (std::any_of(mounted.begin(), mounted.end(), [&](const Entry &e) {
                    return e.titleId == aocId;
                }))
                return false;
            mounted.push_back({aocId, *index, std::move(backing)});
            std::sort(mounted.begin(), mounted.end(),
                      [](const Entry &a, const Entry &b) { return a.index < b.index; });
            return true;
        }

        std::uint64_t Application() const {
            std::scoped_lock lock(mutex);
            return applicationId;
        }

        std::size_t Count() const {
            std::scoped_lock lock(mutex);
            return mounted.size();
        }

        std::vector<std::uint32_t> List(std::size_t start, std::size_t count) const {
            std::scoped_lock lock(mutex);
            if (start >= mounted.size())
                return {};
            const auto last{start + std::min(count, mounted.size() - start)};
            std::vector<std::uint32_t> indices;
            indices.reserve(last - start);
            for (auto i{start}; i < last; ++i)
                indices.push_back(mounted[i].index);
            return indices;
        }

        std::shared_ptr<vfs::Backing> GetByTitleId(std::uint64_t titleId) const {
            std::scoped_lock lock(mutex);
            const auto it{std::find_if(mounted.begin(), mounted.end(),
                [&](const Entry &e) { return e.titleId == titleId; })};
            return it == mounted.end() ? nullptr : it->romFs;
        }

        bool HasIndex(std::uint32_t index) const {
            std::scoped_lock lock(mutex);
            return std::any_of(mounted.begin(), mounted.end(),
                [&](const Entry &e) { return e.index == index; });
        }
    };
}

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once
#include <cstdint>
#include <optional>

namespace skyline::service::aocsrv {
    constexpr std::uint64_t AddOnBaseId(std::uint64_t applicationId) {
        return (applicationId & ~std::uint64_t{0x1FFF}) + 0x1000;
    }
    constexpr std::optional<std::uint32_t> AddOnIndex(std::uint64_t applicationId,
                                                       std::uint64_t addonTitleId) {
        if (applicationId == 0)
            return std::nullopt;
        const auto base{AddOnBaseId(applicationId)};
        if (addonTitleId < base || addonTitleId - base > 0x7FF)
            return std::nullopt;
        return static_cast<std::uint32_t>(addonTitleId - base);
    }
}

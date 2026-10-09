// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>
#include <optional>

namespace skyline::vfs::nca_bounds {
    struct PfsRegion {
        std::uint64_t offset;
        std::uint64_t size;
    };

    // The PFS offset is relative to its media-unit section. Its size must
    // NOT be the entire section size (which includes the preceding hash
    // table), otherwise RegionBacking can extend beyond the encrypted NCA.
    constexpr std::optional<PfsRegion> GetPfsRegion(
        std::uint32_t startMediaUnit, std::uint32_t endMediaUnit,
        std::uint64_t pfs0Offset, std::uint64_t pfs0Size,
        std::uint64_t totalNcaSize) {
        constexpr std::uint64_t MediaUnit = 0x200;
        if (endMediaUnit <= startMediaUnit)
            return std::nullopt;

        const std::uint64_t sectionStart = static_cast<std::uint64_t>(startMediaUnit) * MediaUnit;
        const std::uint64_t sectionSize =
            static_cast<std::uint64_t>(endMediaUnit - startMediaUnit) * MediaUnit;
        if (sectionStart > totalNcaSize || sectionSize > totalNcaSize - sectionStart ||
            pfs0Offset >= sectionSize)
            return std::nullopt;

        const std::uint64_t available = sectionSize - pfs0Offset;
        const std::uint64_t size = pfs0Size ? pfs0Size : available;
        if (!size || size > available)
            return std::nullopt;
        return PfsRegion{sectionStart + pfs0Offset, size};
    }
}

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <algorithm>
#include <cstdint>

namespace skyline::gpu {
    // Presentation-only frame skipping. This does not suppress guest GPU
    // rendering or modify game simulation clocks, and therefore is not a
    // true emulation speed-up. Keep a predictable schedule and reset the
    // phase whenever a setting changes to avoid abrupt long blank intervals.
    class PresentationSkipPolicy {
        std::uint32_t lastSkipped{};
        std::uint32_t phase{};

      public:
        constexpr bool ShouldPresent(std::uint32_t requestedSkipped) {
            const auto skipped = std::min(requestedSkipped, 2U);
            if (lastSkipped != skipped) {
                lastSkipped = skipped;
                phase = 0;
            }
            if (skipped == 0) {
                phase = 0;
                return true;
            }

            const bool present = phase == 0;
            phase = phase == skipped ? 0 : phase + 1;
            return present;
        }

        constexpr void Reset() { phase = 0; }
    };
}

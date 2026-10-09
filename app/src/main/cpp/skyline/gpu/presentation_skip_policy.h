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
        std::uint32_t lastMode{};
        std::uint32_t automaticSkipped{};
        std::uint32_t slowPresentStreak{};
        std::uint32_t fastPresentStreak{};
      public:
        // This mode measures host presentation cost, not guest simulation FPS.
        constexpr void ObserveHostPresentCostNs(std::uint64_t ns) {
            if (ns >= 24'000'000ULL) {
                fastPresentStreak = 0;
                if (++slowPresentStreak >= 8) automaticSkipped = 1;
            } else if (ns <= 12'000'000ULL) {
                slowPresentStreak = 0;
                if (++fastPresentStreak >= 16) automaticSkipped = 0;
            } else {
                slowPresentStreak = 0;
                fastPresentStreak = 0;
            }
        }
        constexpr std::uint32_t AutomaticSkipped() const { return automaticSkipped; }
      private:

      public:
        constexpr bool ShouldPresent(std::uint32_t requestedSkipped) {
            const auto mode = std::min(requestedSkipped, 3U);
            const auto skipped = mode == 3U ? automaticSkipped : mode;
            if (lastMode != mode || lastSkipped != skipped) {
                lastMode = mode;
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

        constexpr void Reset() { phase = 0; slowPresentStreak = 0; fastPresentStreak = 0; automaticSkipped = 0; }
    };
}

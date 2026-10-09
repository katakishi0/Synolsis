// SPDX-License-Identifier: MPL-2.0
#include <cassert>
#include <cstdint>
#include "../app/src/main/cpp/skyline/gpu/swapchain_policy.h"

using skyline::gpu::swapchain_policy::ChooseSupportedFlag;

int main() {
    // Identity/INHERIT are retained whenever actually supported.
    static_assert(ChooseSupportedFlag(0b1011, 0b1000, 0b0001) == 0b1000);
    // Android surfaces may omit INHERIT; OPAQUE must be chosen instead.
    static_assert(ChooseSupportedFlag(0b0111, 0b1000, 0b0001) == 0b0001);
    // If identity is unavailable, use current transform when available.
    static_assert(ChooseSupportedFlag(0b0110, 0b0001, 0b0100) == 0b0100);
    // Deterministic fallback to any advertised bit for unusual surfaces.
    static_assert(ChooseSupportedFlag(0b1010, 0b0100, 0b0001) == 0b0010);
    // A zero bit mask must not silently produce an invalid enum.
    static_assert(ChooseSupportedFlag(0, 0b1000, 0b0001) == 0);
    // Reject multi-bit requested enum candidates.
    static_assert(ChooseSupportedFlag(0b0111, 0b0011, 0b0100) == 0b0100);
    assert(ChooseSupportedFlag(0b1100, 0b0010, 0b1000) == 0b1000);
}

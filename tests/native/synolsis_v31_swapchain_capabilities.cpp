// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
// Portable C++20 tests, independent of Android or Nintendo game assets.

#include "gpu/swapchain_compatibility.h"

using namespace skyline::gpu::swapchain_compatibility;

// VK_COMPOSITE_ALPHA_OPAQUE=1, PRE=2, POST=4, INHERIT=8.
static_assert(ChooseCompositeAlphaBits(1U) == 1U);
static_assert(ChooseCompositeAlphaBits(8U) == 8U);
static_assert(ChooseCompositeAlphaBits(1U | 8U) == 1U);
static_assert(ChooseCompositeAlphaBits(2U | 4U) == 2U);
static_assert(ChooseCompositeAlphaBits(4U) == 4U);
static_assert(ChooseCompositeAlphaBits(0U) == 0U);

// Identity=1, ROTATE_90=2, ROTATE_180=4, ROTATE_270=8.
static_assert(ChoosePreTransformBits(1U | 2U, 2U) == 1U);
static_assert(ChoosePreTransformBits(2U | 4U, 4U) == 4U);
static_assert(ChoosePreTransformBits(2U | 4U, 1U) == 2U);
static_assert(ChoosePreTransformBits(0U, 0U) == 0U);
static_assert(ChoosePreTransformBits(8U, 8U) == 8U);

int main() {
    return ChooseCompositeAlphaBits(0) != 0 || ChoosePreTransformBits(0, 0) != 0;
}

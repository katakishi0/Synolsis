// SPDX-License-Identifier: MPL-2.0
#include <cassert>
#include <cstdint>
#include "gpu/surface_policy.h"

using skyline::gpu::surface_policy::ChoosePreTransform;
using skyline::gpu::surface_policy::ChooseCompositeAlpha;

static_assert(ChoosePreTransform(0x1, 0x2) == 0x1);
static_assert(ChoosePreTransform(0x2, 0x2) == 0x2);
static_assert(ChoosePreTransform(0x4, 0x2) == 0x4);
static_assert(ChoosePreTransform(0, 0x1) == 0);
static_assert(ChooseCompositeAlpha(0x8) == 0x8);
static_assert(ChooseCompositeAlpha(0x1) == 0x1);
static_assert(ChooseCompositeAlpha(0x3) == 0x1);
static_assert(ChooseCompositeAlpha(0) == 0);

int main() {
    for (std::uint32_t mask{1}; mask < 0x200; ++mask) {
        const auto transform{ChoosePreTransform(mask, 0x4)};
        const auto alpha{ChooseCompositeAlpha(mask)};
        assert(transform != 0 && (transform & mask) == transform);
        assert(alpha != 0 && (alpha & mask) == alpha);
    }
}

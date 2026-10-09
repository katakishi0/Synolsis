// SPDX-License-Identifier: MPL-2.0
// C++20 native host test: no Android SDK, Vulkan driver, or firmware needed.
#include "../app/src/main/cpp/skyline/gpu/render_scale_geometry.h"
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <limits>

using skyline::gpu::scale::Extent;
using skyline::gpu::scale::Factor;
using skyline::gpu::scale::ScaleExtent;
using skyline::gpu::scale::ScaleDimension;

static_assert(ScaleExtent({1280, 720}, Factor::Half) == Extent{640, 360});
static_assert(ScaleExtent({1280, 720}, Factor::ThreeQuarters) == Extent{960, 540});
static_assert(ScaleExtent({1280, 720}, Factor::Native) == Extent{1280, 720});
static_assert(ScaleExtent({1920, 1080}, Factor::Half) == Extent{960, 540});
static_assert(ScaleExtent({1920, 1080}, Factor::ThreeQuarters) == Extent{1440, 810});
static_assert(ScaleExtent({1, 1}, Factor::Half) == Extent{1, 1});
static_assert(ScaleExtent({3, 5}, Factor::ThreeQuarters) == Extent{3, 4});
static_assert(ScaleExtent({0, 720}, Factor::Half) == Extent{0, 360});
static_assert(ScaleExtent({0, 0}, Factor::Native) == Extent{0, 0});
static_assert(ScaleDimension(std::numeric_limits<std::uint32_t>::max(),
                             Factor::Native) == std::numeric_limits<std::uint32_t>::max());

int main() {
    for (std::uint32_t width = 1; width < 4000; width += 7) {
        for (auto factor : {Factor::Half, Factor::ThreeQuarters, Factor::Native}) {
            const auto scaled = ScaleDimension(width, factor);
            assert(scaled >= 1);
            assert(scaled <= width);
            if (factor == Factor::Native) assert(scaled == width);
        }
    }
}

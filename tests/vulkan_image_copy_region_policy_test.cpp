// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/gpu/texture/copy_region_policy.h"

using namespace skyline::gpu::texture::copy_region_policy;
static_assert(ValidRange(5, 0, 1));
static_assert(ValidRange(5, 3, 2));
static_assert(ValidRange(5, 3, Remaining));
static_assert(!ValidRange(5, 5, Remaining));
static_assert(!ValidRange(5, 3, 3));
static_assert(!ValidRange(5, 3, 0));
static_assert(!ValidRange(0, 0, 1));
static_assert(RangeEnd(5, 3, 2) == 5);
static_assert(RangeEnd(5, 3, Remaining) == 5);
static_assert(RangeCount(5, 3, Remaining) == 2);
static_assert(MipExtent({1280, 720, 1}, 0).width == 1280);
static_assert(MipExtent({1280, 720, 1}, 1).width == 640);
static_assert(MipExtent({1280, 720, 1}, 1).height == 360);
static_assert(MipExtent({1280, 720, 1}, 1).depth == 1);
static_assert(MipExtent({1280, 720, 1}, 3).width == 160);
static_assert(MipExtent({1, 1, 1}, 31).height == 1);
static_assert(MipExtent({64, 32, 16}, 2).depth == 4);
static_assert(MipDimension(512, 33) == 1);
int main() {
    return (ValidRange(6, 4, Remaining) &&
            RangeEnd(6, 4, Remaining) == 6) ? 0 : 1;
}

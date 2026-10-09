// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/gpu/frame_interpolation_planner.h"
#include <cassert>
#include <cstdint>
using skyline::gpu::framegen::Midpoint;
using skyline::gpu::framegen::CanSubmit;
static_assert(Midpoint(0,16'666'666).has_value());
static_assert(Midpoint(0,16'666'666)->generatedNs == 8'333'333);
static_assert(Midpoint(30'000'000, 63'333'334)->generatedNs == 46'666'667);
static_assert(!Midpoint(0,0));
static_assert(!Midpoint(0,7'000'000));
static_assert(!Midpoint(0,61'000'000));
static_assert(!Midpoint(-1,16'666'666));
static_assert(!Midpoint(100,90));
static_assert(CanSubmit(true,true,true,true));
static_assert(!CanSubmit(true,false,true,true));
static_assert(!CanSubmit(true,true,false,true));
static_assert(!CanSubmit(true,true,true,false));
int main() {
    for (std::int64_t ms=8;ms<=60;ms++) {
        auto plan=Midpoint(10'000'000,10'000'000+ms*1'000'000);
        assert(plan);
        assert(plan->generatedNs > plan->previousNs);
        assert(plan->generatedNs < plan->currentNs);
        assert(plan->blendAlpha == 0.5f);
    }
}

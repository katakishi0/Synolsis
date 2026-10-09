// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/gpu/presentation_scale.h"
#include <cassert>
#include <cstdint>
using skyline::gpu::presentation_scale::ChooseExtent;
using skyline::gpu::presentation_scale::ScaleCrop;
using skyline::gpu::presentation_scale::Rect;
using skyline::gpu::scale::Extent;

static_assert(ChooseExtent({1280,720}, 75,{1,1},{4096,4096},true) == Extent{960,540});
static_assert(ChooseExtent({1280,720}, 50,{1,1},{4096,4096},true) == Extent{640,360});
static_assert(ChooseExtent({1280,720},100,{1,1},{4096,4096},true) == Extent{1280,720});
static_assert(ChooseExtent({1280,720}, 75,{1,1},{4096,4096},false) == Extent{1280,720});
static_assert(ChooseExtent({1280,720}, 50,{700,400},{4096,4096},true) == Extent{1280,720});
static_assert(ChooseExtent({1280,720}, 50,{1,1},{600,350},true) == Extent{1280,720});
static_assert(ChooseExtent({3,3}, 50,{1,1},{4096,4096},true) == Extent{2,2});
static_assert(ScaleCrop({0,0,1280,720},{1280,720},{640,360}) == Rect{0,0,640,360});
static_assert(ScaleCrop({100,50,1180,670},{1280,720},{640,360}) == Rect{50,25,590,335});
static_assert(ScaleCrop({-10,-10,1500,900},{1280,720},{640,360}) == Rect{0,0,640,360});
static_assert(ScaleCrop({90,80,30,20},{1280,720},{640,360}) == Rect{45,40,45,40});
int main() {
    for (std::uint32_t width = 1; width < 3000; width+=13) {
        auto out = ChooseExtent({width,720},75,{1,1},{10000,10000},true);
        assert(out.width>=1 && out.width<=width);
        assert(out.height==540);
    }
}

// SPDX-License-Identifier: MPL-2.0
// Host-only deterministic present-skip tests; no guest frame/FPS acceleration.
#include "../app/src/main/cpp/skyline/gpu/presentation_skip_policy.h"
#include <array>
#include <cassert>
using skyline::gpu::PresentationSkipPolicy;

constexpr bool BasicModes() {
    PresentationSkipPolicy p;
    for (int i{};i<16;i++) if (!p.ShouldPresent(0)) return false;
    constexpr std::array<bool,6> one{true,false,true,false,true,false};
    for (bool b:one) if (p.ShouldPresent(1)!=b) return false;
    constexpr std::array<bool,7> two{true,false,false,true,false,false,true};
    for (bool b:two) if (p.ShouldPresent(2)!=b) return false;
    // A mode transition restarts from a displayed frame; no blank burst.
    if (!p.ShouldPresent(1)) return false;
    if (p.ShouldPresent(1)) return false;
    if (!p.ShouldPresent(0)) return false;
    if (!p.ShouldPresent(1)) return false;
    p.Reset();
    if (!p.ShouldPresent(1)) return false;
    return true;
}
static_assert(BasicModes());
int main() {
    PresentationSkipPolicy p;
    for (unsigned mode=0;mode<3;mode++) {
        p.Reset();
        bool first=p.ShouldPresent(mode);
        assert(first);
        const unsigned cap=mode>2?2:mode;
        for(unsigned i=0;i<1000;i++) {
            const bool displayed=p.ShouldPresent(mode);
            const bool expected=((i+1)%(cap+1))==0;
            assert(displayed==expected);
        }
        (void)p.ShouldPresent(0);
    }
    PresentationSkipPolicy automatic;
    for (unsigned i=0;i<8;++i) automatic.ObserveHostPresentCostNs(28'000'000ULL);
    assert(automatic.AutomaticSkipped() == 1);
    assert(automatic.ShouldPresent(3));
    assert(!automatic.ShouldPresent(3));
    for (unsigned i=0;i<16;++i) automatic.ObserveHostPresentCostNs(6'000'000ULL);
    assert(automatic.AutomaticSkipped() == 0);
    assert(automatic.ShouldPresent(3));
    automatic.Reset();
    assert(automatic.AutomaticSkipped() == 0);
}

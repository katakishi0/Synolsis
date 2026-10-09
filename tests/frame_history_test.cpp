// SPDX-License-Identifier: MPL-2.0
// Portable runtime-history tests. No GPU or game assets are required.
#include "../app/src/main/cpp/skyline/gpu/frame_history.h"
#include <cassert>
using skyline::gpu::framegen::FrameHistory;
using skyline::gpu::framegen::SubmittedFrame;
constexpr SubmittedFrame A{1, 10'000'000, 1280, 720, 37, 0x100};
constexpr SubmittedFrame B{2, 26'666'666, 1280, 720, 37, 0x200};
static_assert(FrameHistory::FitsBudget(1280,720));
static_assert(FrameHistory::FitsBudget(1920,1080));
static_assert(!FrameHistory::FitsBudget(3840,2160));
static_assert(!FrameHistory::FitsBudget(0,720));
static_assert(!FrameHistory::FitsBudget(9000,100));
constexpr bool AcceptPair() {
    FrameHistory history;
    if (history.Observe(A)) return false;
    const auto result = history.Observe(B);
    return result && result->generatedNs == 18'333'333 &&
        history.AcceptedPairs() == 1 && history.RejectedPairs() == 0;
}
static_assert(AcceptPair());
int main() {
    FrameHistory history;
    assert(!history.Observe(A));
    assert(!history.Observe({2, 26'666'666, 1280,720,37, 0x100})); // reused input, no copied history
    assert(!history.Observe({3, 43'333'332, 1920,1080,37,0x300})); // resize
    assert(!history.Observe({4, 60'000'000, 1920,1080,38,0x400})); // format change
    assert(history.Observe({5, 76'666'666, 1920,1080,38,0x500}));
    history.Reset();
    assert(!history.Observe({6, 93'333'332, 1920,1080,38,0x600}));
    assert(!history.Observe({7, 100'000'000, 1920,1080,38,0x700})); // too short
    assert(!history.Observe({7, 120'000'000, 1920,1080,38,0x800})); // duplicate frame ID
    assert(!history.Observe({8, 140'000'000, 3840,2160,38,0x900})); // budget
    assert(!history.Observe({9, 150'000'000, 1920,1080,38,0xA00})); // cold after reject
    assert(history.AcceptedPairs() == 1);
    assert(history.RejectedPairs() >= 6);
}

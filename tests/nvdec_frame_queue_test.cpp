// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/classes/decoded_frame_queue.h"
#include <cassert>
#include <cstdint>
#include <vector>

namespace q = skyline::soc::host1x::video_frame_queue;
struct Frame {
    int index{};
    int cropLeft{};
    std::vector<std::uint8_t> data;
};
static Frame Make(int index, int bytes, int crop = 0) {
    return Frame{index, crop, std::vector<std::uint8_t>(bytes, static_cast<std::uint8_t>(index))};
}
int main() {
    q::BoundedFrames<Frame, 2, 12> frames;
    assert(!frames.Take());
    assert(frames.Push(Make(1, 4, 2)));
    assert(frames.Push(Make(2, 4, 4)));
    assert(frames.Size() == 2 && frames.Bytes() == 8);
    assert(!frames.Push(Make(100, 13)));  // Oversized: do not clear valid output.
    assert(frames.Size() == 2 && frames.Bytes() == 8);
    auto first=frames.Take();
    assert(first && first->index == 1 && first->cropLeft == 2);
    assert(first->data.front() == 1);
    assert(frames.Bytes() == 4);
    assert(frames.Push(Make(3, 8)));
    assert(frames.Bytes() == 12);
    // The newest decoded frame must not overwrite pending frame #2.
    assert(frames.Push(Make(4, 8)));
    assert(frames.Size() == 1 && frames.Bytes() == 8);
    auto newest=frames.Take();
    assert(newest && newest->index == 4);
    assert(frames.Size() == 0 && frames.Bytes() == 0);

    q::BoundedFrames<Frame, 2, 30> count;
    assert(count.Push(Make(10, 5)));
    assert(count.Push(Make(11, 5)));
    assert(count.Push(Make(12, 5))); // Drop oldest, keep 11 -> 12.
    assert(count.Size() == 2);
    assert(count.Take()->index == 11);
    assert(count.Take()->index == 12);
    assert(!count.Take());
    assert(count.Push(Make(13, 6)));
    count.Clear();
    assert(count.Size() == 0 && count.Bytes() == 0);
}

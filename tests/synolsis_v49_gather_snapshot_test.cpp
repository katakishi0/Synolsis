// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/soc/host1x/video_gather_snapshot.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

using skyline::soc::host1x::video_gather::Snapshot;
using namespace skyline::soc::host1x::video_gather;

int main() {
    std::array<std::uint32_t,3> first{1,2,3};
    std::array<std::uint32_t,2> second{4,5};
    const std::array<std::span<const std::uint32_t>,2> incoming{first,second};
    Snapshot snapshot;
    assert(snapshot.Capture(incoming));
    assert(snapshot.Buffers().size()==2);
    first[0] = 99;
    second[1] = 99;
    assert(snapshot.Buffers()[0][0]==1);
    assert(snapshot.Buffers()[1][1]==5);
    std::vector<std::uint32_t> oversized(MaximumWords+1);
    const std::array<std::span<const std::uint32_t>,1> bad{oversized};
    assert(!snapshot.Capture(bad));
    assert(snapshot.Buffers()[0][0]==1);
    std::vector<std::span<const std::uint32_t>> tooMany(MaximumBuffers+1);
    assert(!snapshot.Capture(tooMany));
    assert(snapshot.Buffers().size()==2);
    // Exactly 1 MiB of words is accepted, but an extra word is rejected.
    std::vector<std::uint32_t> maximum(MaximumWords,0xFACE);
    const std::array<std::span<const std::uint32_t>,1> accepted{maximum};
    assert(snapshot.Capture(accepted));
    assert(snapshot.Buffers()[0].size()==MaximumWords);
    assert(snapshot.Buffers()[0][0]==0xFACE);
    std::array<std::uint32_t,1> extra{1};
    const std::array<std::span<const std::uint32_t>,2> totalTooLarge{maximum,extra};
    assert(!snapshot.Capture(totalTooLarge));
    assert(snapshot.Buffers().size()==1);
    assert(snapshot.Buffers()[0][0]==0xFACE);
    const std::array<std::span<const std::uint32_t>,1> empty{std::span<const std::uint32_t>{}};
    assert(snapshot.Capture(empty));
    assert(snapshot.Buffers().size()==1 && snapshot.Buffers()[0].empty());
}

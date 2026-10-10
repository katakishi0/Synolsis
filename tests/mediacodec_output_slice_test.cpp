// SPDX-License-Identifier: MPL-2.0
// Android NDK <=35 outputs must be measured using AMediaCodecBufferInfo.size.
#include "../app/src/main/cpp/skyline/soc/host1x/classes/mediacodec_output_slice.h"
#include <cassert>
namespace p = skyline::soc::host1x::codec_output;

static_assert(p::ChooseOutputSlice(34, 0, -1, 4608));
static_assert(p::ChooseOutputSlice(34, 8, 99999, 4608).size == 4608);
static_assert(p::ChooseOutputSlice(35, 1, -1, 7680).size == 7680);
static_assert(p::ChooseOutputSlice(36, 7680, 0, 7680));
static_assert(p::ChooseOutputSlice(36, 7680, 0, 7681).size == 0);
static_assert(!p::ChooseOutputSlice(36, 7680, 2, 7000));
static_assert(!p::ChooseOutputSlice(36, 7680, -1, 7000));
static_assert(!p::ChooseOutputSlice(36, 0, 0, 7000));
static_assert(!p::ChooseOutputSlice(34, 0, -1, 0));
static_assert(!p::ChooseOutputSlice(34, 0, -1, -3));
static_assert(!p::ChooseOutputSlice(34, 0, -1, p::MaxFrameBytes+1ULL));
static_assert(!p::ChooseOutputSlice(20, 0, 0, 2048));

int main() {
    assert(p::ChooseOutputSlice(34, 0, -1000, 384).offset == 0);
    assert(p::ChooseOutputSlice(35, 3, -1000, 384).size == 384);
    assert(p::ChooseOutputSlice(36, 384, 0, 384).size == 384);
    assert(p::ChooseOutputSlice(36, 383, 0, 384).size == 0);
}

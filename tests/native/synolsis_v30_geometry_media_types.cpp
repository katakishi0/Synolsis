// SPDX-License-Identifier: MPL-2.0
// Portable C++20 checks: run on CI without an Android device or Switch files.
#include <cassert>
#include <cstdint>
#include <limits>
#include <string_view>

#include "gpu/scissor_geometry.h"
#include "soc/host1x/classes/media_decoder_types.h"

using namespace skyline::gpu::scissor_geometry;
using namespace skyline::soc::host1x;

static_assert(MakeScissor(ClipToExtent(
    FromOriginSize(-10, -20, 50, 50), 16, 16))
    == VulkanScissor{0, 0, 16, 16},
    "Negative guest origins must preserve intersection, not shift the rectangle");

// Fully outside texture bounds must become an empty rectangle at the edge.
static_assert(MakeScissor(ClipToExtent(
    FromOriginSize(200, 20, 40, 10), 100, 100))
    == VulkanScissor{100, 20, 0, 10},
    "Oversized scissor offsets must not wrap to huge Vulkan regions");

static_assert(MakeScissor(FromBounds(100, 70, 20, 4))
    == VulkanScissor{100, 70, 0, 0},
    "Reversed clear/scissor bounds must not underflow");

static_assert(MakeScissor(Intersect(FromOriginSize(0, 0, 30, 20),
                                    FromOriginSize(20, 10, 60, 60)))
    == VulkanScissor{20, 10, 10, 10},
    "Rect intersection must retain overlapping area");

// Coordinates may arise from arithmetic on signed origins + unsigned
// extents. Avoid overflow and clamp to Vulkan's signed offset range.
static_assert(MakeScissor(FromOriginSize(
    std::numeric_limits<std::int32_t>::max() - 1, 0,
    std::numeric_limits<std::uint32_t>::max(), 2))
    == VulkanScissor{std::numeric_limits<std::int32_t>::max()-1, 0, 1, 2});

static_assert(AndroidMime(VideoCodec::H264) == "video/avc");
static_assert(AndroidMime(VideoCodec::VP9) == "video/x-vnd.on2.vp9");
static_assert(AndroidMime(VideoCodec::H265) == "video/hevc");
static_assert(AndroidMime(VideoCodec::VC1).empty());
static_assert(ValidVideoExtent(1920, 1080));
static_assert(!ValidVideoExtent(0, 1080));
static_assert(!ValidVideoExtent(8192, 8192));

int main() {
    const auto small{MakeScissor(ClipToExtent(FromOriginSize(-5, -5, 10, 10), 4, 4))};
    assert(small.x == 0 && small.y == 0 && small.width == 4 && small.height == 4);
}

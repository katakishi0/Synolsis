// SPDX-License-Identifier: MPL-2.0
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>
#include "../app/src/main/cpp/skyline/soc/host1x/video_command_batch.h"

using skyline::soc::host1x::video_probe::VideoCommandProbe;
using skyline::soc::host1x::video_probe::Summary;
using skyline::soc::host1x::video_probe::DispatchBatch;

int main() {
    constexpr std::uint32_t classNvdec{0xF0U << 6};
    constexpr std::uint32_t setMethod{(1U << 28) | (0x10U << 16) | 1U};
    constexpr std::uint32_t writeMethod{(1U << 28) | (0x11U << 16) | 1U};
    const std::array<std::uint32_t, 3> first{classNvdec, setMethod, 0x80};
    const std::array<std::uint32_t, 2> second{writeMethod, 3};
    const std::array<std::uint32_t, 1> invalid{0xF0000000U};

    VideoCommandProbe probe;
    std::vector<std::array<std::uint32_t, 3>> calls;
    auto sink = [&](std::uint32_t cls, std::uint32_t method, std::uint32_t arg) {
        calls.push_back({cls, method, arg});
    };

    // An invalid second/third gather must NEVER execute the first.
    const std::array<std::span<const std::uint32_t>, 3> malformed{
        first, second, invalid
    };
    Summary rejected{};
    assert(!DispatchBatch(probe, malformed, rejected, sink));
    assert(rejected.truncatedOrUnsupported);
    assert(rejected.videoMethodWrites == 0);
    assert(calls.empty());

    // Invalid submission also must not change class or method selector.
    const std::array<std::span<const std::uint32_t>, 1> onlyData{second};
    Summary noClass{};
    assert(DispatchBatch(probe, onlyData, noClass, sink));
    assert(noClass.nvdecCodecWrites == 0);
    assert(calls.empty());

    // The proper two-gather batch forwards both THI method writes and one
    // NVDEC codec selection, preserving their order.
    const std::array<std::span<const std::uint32_t>, 2> valid{first, second};
    Summary accepted{};
    assert(DispatchBatch(probe, valid, accepted, sink));
    assert(calls.size() == 2);
    assert(calls[0][0] == 0xF0 && calls[0][1] == 0x10 && calls[0][2] == 0x80);
    assert(calls[1][0] == 0xF0 && calls[1][1] == 0x11 && calls[1][2] == 3);
    assert(accepted.nvdecCodecWrites == 1 && accepted.lastNvdecCodec == 3);
    calls.clear();

    // Persist the selector across two separate VALID ioctls.
    Summary continued{};
    assert(DispatchBatch(probe, onlyData, continued, sink));
    assert(continued.nvdecCodecWrites == 1 && calls.size() == 1);

    // Bound total words before any sink side effect.
    const std::vector<std::uint32_t> huge(262145, 0);
    const std::array<std::span<const std::uint32_t>, 1> oversized{huge};
    Summary budget{};
    calls.clear();
    assert(!DispatchBatch(probe, oversized, budget, sink));
    assert(budget.truncatedOrUnsupported && calls.empty());

    // Bound buffer count even if many buffers are zero-sized.
    const std::vector<std::span<const std::uint32_t>> many(129, std::span<const std::uint32_t>{});
    Summary tooMany{};
    assert(!DispatchBatch(probe, many, tooMany, sink));
    assert(tooMany.truncatedOrUnsupported && calls.empty());
}

// SPDX-License-Identifier: MPL-2.0
// Regression: malformed Host1x gathers must have no NVDEC/VIC side effects.
#include <array>
#include <cassert>
#include <cstdint>
#include "skyline/soc/host1x/video_command_probe.h"

using skyline::soc::host1x::video_probe::VideoCommandProbe;
using skyline::soc::host1x::video_probe::Summary;

constexpr std::uint32_t SetClass(unsigned cls, unsigned method, unsigned mask) {
    return (method << 16) | (cls << 6) | mask;
}
constexpr std::uint32_t Header(unsigned opcode, unsigned method, unsigned low) {
    return (opcode << 28) | (method << 16) | low;
}
int main() {
    VideoCommandProbe probe;
    Summary result{};
    unsigned sent = 0;
    auto sink = [&](std::uint32_t, std::uint32_t, std::uint32_t) { ++sent; };

    // A valid NVDEC codec/execute prefix followed by a truncated packet.
    // Prior code executed both methods before discovering invalid input.
    constexpr std::array<std::uint32_t, 7> malformed{{
        SetClass(0xF0, 0x10, 3), 0x80, 3,
        Header(1, 0x10, 2), 0xC0, 1,
        Header(1, 0x10, 2)
    }};
    assert(!probe.Inspect(malformed, result, sink));
    assert(sent == 0);
    assert(result.videoMethodWrites == 0);
    assert(result.nvdecExecutions == 0);
    assert(result.truncatedOrUnsupported);

    // A failed packet may not change the THI selected-method state.
    // A method-1 write alone must not inherit method 0x80 from the
    // rejected prefix.
    Summary afterFailure{};
    constexpr std::array<std::uint32_t, 3> valueOnly{{
        SetClass(0xF0, 0x11, 1), 3,
        Header(4, 0x20, 0)
    }};
    assert(probe.Inspect(valueOnly, afterFailure, sink));
    assert(afterFailure.nvdecCodecWrites == 0);
    assert(afterFailure.nvdecExecutions == 0);
    assert(sent == 1);

    // A fully valid packet dispatches and updates counters once only.
    constexpr std::array<std::uint32_t, 6> valid{{
        SetClass(0xF0, 0x10, 3), 0x80, 3,
        Header(1, 0x10, 2), 0xC0, 1
    }};
    Summary clean{};
    assert(probe.Inspect(valid, clean, sink));
    assert(clean.nvdecCodecWrites == 1);
    assert(clean.nvdecExecutions == 1);
    assert(sent == 5); // 1 previous and 4 THI writes

    // An unsupported opcode after valid input must never invoke VIC.
    constexpr std::array<std::uint32_t, 4> unsupported{{
        SetClass(0x5D, 0x10, 1), 0xC0,
        Header(4, 0x11, 1), Header(6, 0, 0)
    }};
    Summary rejected{};
    assert(!probe.Inspect(unsupported, rejected, sink));
    assert(sent == 5);
    assert(rejected.vicExecutions == 0);
    return 0;
}

// SPDX-License-Identifier: MPL-2.0
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include "skyline/soc/host1x/video_command_probe.h"

using namespace skyline::soc::host1x::video_probe;
static constexpr std::uint32_t Header(unsigned op, unsigned meth,
                                       unsigned low) {
    return (op << 28) | (meth << 16) | low;
}
static constexpr std::uint32_t SetClass(unsigned cls, unsigned meth, unsigned mask) {
    return (meth << 16) | (cls << 6) | mask;
}
int main() {
    VideoCommandProbe probe;
    Summary out{};
    constexpr std::array<std::uint32_t, 7> nvdec{{
        SetClass(0xF0, 0x10, 3), 0x80, 1,
        Header(1, 0x10, 2), 0xC0, 1,
        Header(4, 0x20, 7),
    }};
    assert(probe.Inspect(nvdec, out));
    assert(out.nvdecCodecWrites == 1 && out.lastNvdecCodec == 1);
    assert(out.nvdecExecutions == 1 && out.videoMethodWrites == 5);
    std::array<std::uint32_t, 12> forwarded{};
    std::size_t forwardedCount{};
    VideoCommandProbe attached;
    Summary attachedStats{};
    assert(attached.Inspect(nvdec, attachedStats,
        [&](std::uint32_t cls, std::uint32_t method, std::uint32_t val) {
            if (forwardedCount < 4) {
                forwarded[forwardedCount * 3] = cls;
                forwarded[forwardedCount * 3 + 1] = method;
                forwarded[forwardedCount * 3 + 2] = val;
            }
            ++forwardedCount;
        }));
    assert(forwardedCount == 4);
    assert(forwarded[0] == 0xF0 && forwarded[1] == 0x10 && forwarded[2] == 0x80);
    assert(forwarded[3] == 0xF0 && forwarded[4] == 0x11 && forwarded[5] == 1);
    assert(forwarded[6] == 0xF0 && forwarded[7] == 0x10 && forwarded[8] == 0xC0);
    assert(forwarded[9] == 0xF0 && forwarded[10] == 0x11 && forwarded[11] == 1);

    constexpr std::array<std::uint32_t, 4> vic{{
        SetClass(0x5D, 0x10, 1), 0xC0,
        Header(2, 0x11, 1), 1,
    }};
    assert(probe.Inspect(vic, out));
    assert(out.vicExecutions == 1 && out.lastVicExecute == 1);
    const auto oldExec = out.nvdecExecutions;
    constexpr std::array<std::uint32_t, 2> malformed{{
        SetClass(0xF0, 0x10, 3), 0x80,
    }};
    assert(!probe.Inspect(malformed, out));
    assert(out.truncatedOrUnsupported && out.nvdecExecutions == oldExec);
    constexpr std::array<std::uint32_t, 1> unsupported{{Header(6, 0x10, 0)}};
    Summary blocked{};
    assert(!probe.Inspect(unsupported, blocked));
    assert(blocked.truncatedOrUnsupported);
    std::cout << "PASS Host1x NVDEC/VIC pushbuffer extraction without fabricated decode" << std::endl;
}

// SPDX-License-Identifier: MPL-2.0
// V46: real Host1x THI packets, no mocked MediaCodec/video decode.
#include <array>
#include <cassert>
#include <cstdint>
#include <vector>
#include "skyline/soc/host1x/video_command_probe.h"

using namespace skyline::soc::host1x::video_probe;
constexpr std::uint32_t SetClass(unsigned cls, unsigned m, unsigned mask) {
    return (m << 16) | (cls << 6) | mask;
}
constexpr std::uint32_t Header(unsigned op, unsigned m, unsigned count) {
    return (op << 28) | (m << 16) | count;
}
int main() {
    VideoCommandProbe decoder;
    Summary result{};
    std::vector<std::uint32_t> received;
    const auto sink = [&](std::uint32_t cls, std::uint32_t m, std::uint32_t value) {
        received.push_back(cls);
        received.push_back(m);
        received.push_back(value);
    };
    constexpr std::array<std::uint32_t, 2> select{SetClass(0xF0, 0x10, 1), 0x80};
    assert(decoder.Inspect(select, result, sink));
    assert(received.size() == 3 && received[2] == 0x80);
    received.clear();

    // The next guest nvhost submit can omit SetClass; its THI method1
    // must still target the NvDec device and count a codec write.
    constexpr std::array<std::uint32_t, 2> codec{Header(2, 0x11, 1), 3};
    assert(decoder.Inspect(codec, result, sink));
    assert(received.size() == 3 && received[0] == 0xF0 &&
           received[1] == 0x11 && received[2] == 3);
    assert(result.nvdecCodecWrites == 1 && result.lastNvdecCodec == 3);
    received.clear();

    // A valid-looking command followed by unsupported Gather opcode cannot
    // start a decode and must not alter stored THI state or statistics.
    constexpr std::array<std::uint32_t, 3> invalid{
        Header(2, 0x10, 1), 0xC0, Header(6, 0, 0)
    };
    const auto previousWrites = result.videoMethodWrites;
    assert(!decoder.Inspect(invalid, result, sink));
    assert(result.truncatedOrUnsupported);
    assert(received.empty() && result.videoMethodWrites == previousWrites);
    received.clear();

    // Failed call must not change stored target method from Codec (0x80).
    constexpr std::array<std::uint32_t, 2> nextCodec{
        Header(2, 0x11, 1), 9
    };
    Summary next{};
    assert(decoder.Inspect(nextCodec, next, sink));
    assert(next.nvdecCodecWrites == 1 && next.nvdecExecutions == 0);
    assert(received.size() == 3 && received[2] == 9);
    received.clear();

    // An incomplete masked packet must not perform any partial writes.
    constexpr std::array<std::uint32_t, 2> shortPacket{
        SetClass(0x5D, 0x10, 3), 0xC0
    };
    Summary incomplete{};
    assert(!decoder.Inspect(shortPacket, incomplete, sink));
    assert(received.empty() && incomplete.videoMethodWrites == 0);
    assert(incomplete.truncatedOrUnsupported);

    // VIC class/THI state also persists across separate buffers.
    constexpr std::array<std::uint32_t, 2> vicMethod{
        SetClass(0x5D, 0x10, 1), 0xC0
    };
    Summary vic{};
    assert(decoder.Inspect(vicMethod, vic, sink));
    received.clear();
    constexpr std::array<std::uint32_t, 2> vicExecute{
        Header(2, 0x11, 1), 1
    };
    assert(decoder.Inspect(vicExecute, vic, sink));
    assert(vic.vicExecutions == 1 && received.size() == 3 &&
           received[0] == 0x5D && received[1] == 0x11);

    // Untrusted giant gather rejected without decoder side effects.
    std::vector<std::uint32_t> oversized(262145, Header(4, 0x11, 1));
    const auto oldCount = vic.videoMethodWrites;
    assert(!decoder.Inspect(oversized, vic, sink));
    assert(vic.truncatedOrUnsupported && vic.videoMethodWrites == oldCount);
}

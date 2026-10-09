// SPDX-License-Identifier: MPL-2.0
#include <array>
#include <cassert>
#include <cstdint>
#include "../app/src/main/cpp/skyline/gpu/packed_compute_manifest.h"

int main() {
    using namespace skyline::gpu::packed_compute;
    constexpr std::array<std::uint32_t, 5> good{
        0x07230203U, 0x00010300U, 0, 10, 0
    };
    static_assert(ValidSpirv(good));
    static_assert(Assets.size() == 3);
    static_assert(Assets[0].kind == Kind::Easu);
    static_assert(Assets[1].kind == Kind::Rcas);
    static_assert(Assets[2].kind == Kind::Temporal);
    static_assert(Assets[0].path == "shaders/synolsis/easu.spv");
    static_assert(Assets[1].path == "shaders/synolsis/rcas.spv");
    static_assert(Assets[2].path == "shaders/synolsis/temporal_interpolate.spv");
    static_assert(!Assets[0].usesPushConstants);
    static_assert(Assets[2].usesPushConstants);

    auto bad = good;
    bad[0] = 0;
    assert(!ValidSpirv(bad));
    bad = good;
    bad[3] = 0;
    assert(!ValidSpirv(bad));
    bad = good;
    bad[3] = 1'048'577;
    assert(!ValidSpirv(bad));
    bad = good;
    bad[1] = 0x00010700U;
    assert(!ValidSpirv(bad));
    std::array<std::uint32_t, 262145> huge{};
    assert(!ValidSpirv(huge));
    assert(!ValidSpirv(std::span<const std::uint32_t>{}));
}

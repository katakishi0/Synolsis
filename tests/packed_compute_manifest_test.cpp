#include <array>
#include <cassert>
#include "../app/src/main/cpp/skyline/gpu/packed_compute_manifest.h"
int main() {
    using skyline::gpu::packed_compute::ValidSpirv;
    constexpr std::array<std::uint32_t, 5> good{0x07230203U,0x00010300U,0,10,0};
    static_assert(ValidSpirv(good));
    auto bad = good;
    bad[0] = 0;
    assert(!ValidSpirv(bad));
    bad = good;
    bad[3] = 0;
    assert(!ValidSpirv(bad));
    assert(!ValidSpirv(std::span<const std::uint32_t>{}));
}

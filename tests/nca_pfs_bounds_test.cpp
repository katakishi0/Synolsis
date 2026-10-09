// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/vfs/nca_pfs_bounds.h"

using skyline::vfs::nca_bounds::GetPfsRegion;

constexpr auto valid = GetPfsRegion(3, 11, 0x200, 0xC00, 0x3000);
static_assert(valid.has_value());
static_assert(valid->offset == 0x800);
static_assert(valid->size == 0xC00);
static_assert(GetPfsRegion(3, 11, 0x200, 0, 0x3000)->size == 0xE00);
static_assert(!GetPfsRegion(11, 3, 0, 0x10, 0x3000));
static_assert(!GetPfsRegion(3, 11, 0x1000, 0x10, 0x3000));
static_assert(!GetPfsRegion(3, 11, 0x200, 0xF00, 0x3000));
static_assert(!GetPfsRegion(3, 11, 0x200, 0xC00, 0x1500));
static_assert(!GetPfsRegion(3, 11, 0x200, 0xC00, 0));
int main() { return valid ? 0 : 1; }

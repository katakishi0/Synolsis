// SPDX-License-Identifier: MPL-2.0
#include "../app/src/main/cpp/skyline/services/aocsrv/aoc_title_id.h"
#include <cassert>
using skyline::service::aocsrv::AddOnBaseId;
using skyline::service::aocsrv::AddOnIndex;
static_assert(AddOnBaseId(0x0100567800000000ULL) == 0x0100567800001000ULL);
static_assert(AddOnIndex(0x0100567800000000ULL, 0x0100567800001000ULL).value() == 0);
static_assert(AddOnIndex(0x0100567800000000ULL, 0x010056780000100AULL).value() == 10);
static_assert(AddOnIndex(0x0100567800000000ULL, 0x01005678000017FFULL).value() == 0x7FF);
static_assert(!AddOnIndex(0x0100567800000000ULL, 0x0100567800001800ULL));
static_assert(!AddOnIndex(0x0100567800000000ULL, 0x0100567800011000ULL));
static_assert(!AddOnIndex(0x0100567800000000ULL, 0x0100567800000000ULL));
static_assert(!AddOnIndex(0, 0x1000ULL));
int main() {
    for (std::uint64_t i{}; i <= 0x7FF; ++i)
        assert(AddOnIndex(0x0100567800000000ULL, 0x0100567800001000ULL+i).value() == i);
}

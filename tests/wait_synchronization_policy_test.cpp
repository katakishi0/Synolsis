// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#include <cassert>
#include <cstdint>
#include "../app/src/main/cpp/skyline/kernel/wait_synchronization_policy.h"

using namespace skyline::kernel::svc::wait_synchronization_policy;
static_assert(ValidCount(0));
static_assert(ValidCount(1));
static_assert(ValidCount(0x40));
static_assert(!ValidCount(0x41));
static_assert(!ValidCount(static_cast<std::uint32_t>(-1)));
static_assert(!RequiresHandlePointer(0));
static_assert(RequiresHandlePointer(1));
static_assert(RequiresHandlePointer(64));

int main() {
    assert(ValidCount(0));
    assert(!ValidCount(65));
    assert(!RequiresHandlePointer(0));
}

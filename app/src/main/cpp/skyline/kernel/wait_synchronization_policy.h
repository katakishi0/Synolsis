// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstdint>

namespace skyline::kernel::svc::wait_synchronization_policy {
    // SVC 0x18 supports 0..64 handles. Empty waits are valid and can time
    // out or be cancelled; negative 32-bit counts arrive as large u32 values.
    constexpr bool ValidCount(std::uint32_t count) {
        return count <= 0x40;
    }

    constexpr bool RequiresHandlePointer(std::uint32_t count) {
        return count != 0;
    }
}

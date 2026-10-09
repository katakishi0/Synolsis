// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include "vic.h"

namespace skyline::soc::host1x {
    VicClass::VicClass(std::function<void()> opDoneCallback)
        : opDoneCallback(std::move(opDoneCallback)) {}

    void VicClass::CallMethod(u32 method, u32 argument) {
        // No VIC register parser yet; log only the first few samples and
        // powers of two to prevent thousands of logcat writes per second.
        const auto count{unhandledMethodCount.fetch_add(1, std::memory_order_relaxed) + 1};
        if (count <= 8 || (count & (count - 1)) == 0)
            LOGW("VIC register not handled: sample #{} method 0x{:X}, argument 0x{:X}",
                 count, method, argument);
    }
}

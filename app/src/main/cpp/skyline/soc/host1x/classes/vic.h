// SPDX-License-Identifier: MPL-2.0
// Copyright © 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Copyright © 2026 Synolsis Emu contributors
#pragma once
#include <common.h>
#include <atomic>
#include "vic_yuv420_output.h"
#include "nvdec.h"

namespace skyline::soc::host1x {
    class VicClass {
      private:
        std::function<void()> opDoneCallback;
        const DeviceState &state;
        NvDecClass &decoder;
        std::atomic<std::uint64_t> skippedCount{};
        std::uint64_t configAddress{}, lumaAddress{}, chromaAddress{};
        void Execute();
      public:
        VicClass(std::function<void()> opDoneCallback,
                 const DeviceState &state, NvDecClass &decoder);
        void CallMethod(u32 method, u32 argument);
    };
}

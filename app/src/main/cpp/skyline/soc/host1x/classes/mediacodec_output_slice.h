// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

// Android NDK contract: through API 35, AMediaCodec_getOutputBuffer's
// out_size and AMediaCodecBufferInfo.offset are unreliable. The only
// documented active output byte count is AMediaCodecBufferInfo.size.
// From API 36 onwards the output capacity is correct and offset is zero.
// The returned slice never skips negative/bogus offsets on older devices.
namespace skyline::soc::host1x::codec_output {
    constexpr std::size_t MaxFrameBytes = 32ULL * 1024 * 1024;
    struct Slice {
        std::size_t offset{};
        std::size_t size{};
        constexpr explicit operator bool() const { return size != 0; }
    };

    constexpr Slice ChooseOutputSlice(std::int32_t androidApiLevel,
                                       std::size_t bufferCapacity,
                                       std::int64_t bufferOffset,
                                       std::int64_t activeSize) {
        if (androidApiLevel < 21 || activeSize <= 0 ||
            static_cast<std::uint64_t>(activeSize) > MaxFrameBytes)
            return {};

        const auto length = static_cast<std::size_t>(activeSize);
        if (androidApiLevel <= 35) {
            // The pointer from getOutputBuffer is the beginning of the
            // valid output region for pre-36 byte-buffer mode; neither
            // the reported capacity nor the buffer offset is dependable.
            return {0, length};
        }

        if (bufferOffset != 0 || !bufferCapacity ||
            length > bufferCapacity)
            return {};
        return {0, length};
    }
}

// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#pragma once

#include <cstddef>
#include <deque>
#include <optional>
#include <utility>

namespace skyline::soc::host1x::video_frame_queue {
    // Retains only a small, ordered set of *real* Android MediaCodec frames.
    // This class never generates/interpolates images or signals guest fences.
    // The caller must serialize access with the Host1x video command mutex.
    template <typename Frame, std::size_t MaxFrames = 2,
              std::size_t MaxBytes = 48ULL * 1024 * 1024>
    class BoundedFrames {
        static_assert(MaxFrames > 0 && MaxBytes > 0);
        std::deque<Frame> pending;
        std::size_t retainedBytes{};

      public:
        std::size_t Size() const { return pending.size(); }
        std::size_t Bytes() const { return retainedBytes; }

        void Clear() {
            pending.clear();
            retainedBytes = 0;
        }

        // If the queue is full, discard the oldest still-unconsumed frame,
        // preserving the display order of the remaining frames. The 48-MiB
        // cap prevents an unbounded Android decoding backlog on 6-GB devices.
        bool Push(Frame frame) {
            const auto n = frame.data.size();
            if (!n || n > MaxBytes)
                return false;
            while (!pending.empty() &&
                   (pending.size() >= MaxFrames || n > MaxBytes - retainedBytes)) {
                retainedBytes -= pending.front().data.size();
                pending.pop_front();
            }
            if (n > MaxBytes - retainedBytes)
                return false;
            retainedBytes += n;
            pending.push_back(std::move(frame));
            return true;
        }

        std::optional<Frame> Take() {
            if (pending.empty())
                return std::nullopt;
            const auto n = pending.front().data.size();
            Frame front{std::move(pending.front())};
            pending.pop_front();
            retainedBytes -= n;
            return front;
        }
    };
}

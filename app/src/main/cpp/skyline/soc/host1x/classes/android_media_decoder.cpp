// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#include "android_media_decoder.h"
#include <algorithm>
#include <cstring>
#include <limits>

namespace skyline::soc::host1x {
    void AndroidMediaDecoder::StopLocked() {
        if (!codec)
            return;
        AMediaCodec_stop(codec);
        AMediaCodec_delete(codec);
        codec = nullptr;
        width = height = colorFormat = rowStride = sliceHeight = 0;
    }

    AndroidMediaDecoder::~AndroidMediaDecoder() {
        Stop();
    }

    void AndroidMediaDecoder::Stop() {
        std::scoped_lock guard{mutex};
        StopLocked();
    }

    bool AndroidMediaDecoder::Start(VideoCodec video, std::uint32_t w, std::uint32_t h) {
        std::scoped_lock guard{mutex};
        StopLocked();
        const auto mime{AndroidMime(video)};
        if (mime.empty() || !ValidVideoExtent(w, h))
            return false;
        codec = AMediaCodec_createDecoderByType(mime.data());
        if (!codec)
            return false;
        AMediaFormat *format{AMediaFormat_new()};
        if (!format) {
            StopLocked();
            return false;
        }
        AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime.data());
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, static_cast<std::int32_t>(w));
        AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, static_cast<std::int32_t>(h));
        const auto configured{AMediaCodec_configure(codec, format, nullptr, nullptr, 0)};
        AMediaFormat_delete(format);
        if (configured != AMEDIA_OK || AMediaCodec_start(codec) != AMEDIA_OK) {
            StopLocked();
            return false;
        }
        width = static_cast<std::int32_t>(w);
        height = static_cast<std::int32_t>(h);
        return true;
    }

    bool AndroidMediaDecoder::QueueAccessUnit(std::span<const std::uint8_t> input,
                                               std::int64_t ptsUs, bool codecConfiguration) {
        constexpr std::size_t MaxPacketBytes{8 * 1024 * 1024};
        if (input.empty() || input.size() > MaxPacketBytes || ptsUs < 0)
            return false;
        std::scoped_lock guard{mutex};
        if (!codec)
            return false;
        const ssize_t index{AMediaCodec_dequeueInputBuffer(codec, 0)};
        if (index < 0)
            return false;
        std::size_t capacity{};
        auto *buffer{AMediaCodec_getInputBuffer(codec, static_cast<size_t>(index), &capacity)};
        if (!buffer || input.size() > capacity) {
            // Once dequeued, an input slot must not be abandoned. MediaCodec
            // exposes no input-buffer cancel operation; stop the broken
            // session rather than leaking an outstanding slot forever.
            StopLocked();
            return false;
        }
        std::memcpy(buffer, input.data(), input.size());
        const auto flags{codecConfiguration ? AMEDIACODEC_BUFFER_FLAG_CODEC_CONFIG : 0};
        const auto queued{AMediaCodec_queueInputBuffer(codec, static_cast<size_t>(index),
                                            0, input.size(), static_cast<uint64_t>(ptsUs), flags)};
        if (queued != AMEDIA_OK) {
            StopLocked(); // Do not reuse a codec in an invalid/error state.
            return false;
        }
        return true;
    }

    bool AndroidMediaDecoder::Drain(Frame &frame) {
        constexpr std::size_t MaxFrameBytes{32 * 1024 * 1024};
        std::scoped_lock guard{mutex};
        if (!codec)
            return false;
        AMediaCodecBufferInfo info{};
        const ssize_t index{AMediaCodec_dequeueOutputBuffer(codec, &info, 0)};
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat *format{AMediaCodec_getOutputFormat(codec)};
            if (format) {
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &colorFormat);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &rowStride);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &sliceHeight);
                AMediaFormat_delete(format);
                if (width < 0 || height < 0 ||
                    !ValidVideoExtent(static_cast<std::uint32_t>(width),
                                      static_cast<std::uint32_t>(height))) {
                    // Unsupported/corrupt dynamic format: never expose
                    // unbounded output dimensions to guest texture uploads.
                    StopLocked();
                }
            }
            return false;
        }
        if (index < 0)
            return false;

        std::size_t capacity{};
        auto *buffer{AMediaCodec_getOutputBuffer(codec, static_cast<size_t>(index), &capacity)};
        bool valid{buffer && info.size > 0 && info.offset >= 0 &&
                   static_cast<std::size_t>(info.offset) <= capacity &&
                   static_cast<std::size_t>(info.size) <= capacity - static_cast<std::size_t>(info.offset) &&
                   static_cast<std::size_t>(info.size) <= MaxFrameBytes};
        if (valid) {
            frame.data.assign(buffer + info.offset, buffer + info.offset + info.size);
            frame.ptsUs = info.presentationTimeUs;
            frame.width = width;
            frame.height = height;
            frame.colorFormat = colorFormat;
            frame.rowStride = rowStride;
            frame.sliceHeight = sliceHeight;
        }
        AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(index), false);
        return valid;
    }
}

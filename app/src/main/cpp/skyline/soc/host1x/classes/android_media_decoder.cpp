// SPDX-License-Identifier: MPL-2.0
// Copyright © 2026 Synolsis Emu contributors
#include "android_media_decoder.h"
#include "mediacodec_output_slice.h"
#include <android/api-level.h>
#include <algorithm>
#include <atomic>
#include <common.h>
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
        cropLeft = cropTop = cropRight = cropBottom = 0;
    }

    AndroidMediaDecoder::~AndroidMediaDecoder() {
        Stop();
    }

    void AndroidMediaDecoder::Stop() {
        std::scoped_lock guard{mutex};
        StopLocked();
    }

    bool AndroidMediaDecoder::Start(VideoCodec video, std::uint32_t w, std::uint32_t h,
                                    DecoderMode mode) {
        std::scoped_lock guard{mutex};
        StopLocked();
        const auto mime{AndroidMime(video)};
        if (mime.empty() || !ValidVideoExtent(w, h))
            return false;

        // Android mediacodec names are optional device-specific services.
        // Auto: Android chooses the decoder.
        // HardwarePreferred: first try known vendor decoders, then system.
        // SoftwareAndroid: explicit Android software AVC only; no silent
        // fallback that misleadingly uses a hardware decoder.
        // Hybrid: system chosen decoder followed by software if configure
        // or start fails. This is not mid-frame switching.
        struct Candidate { const char *name; bool system; };
        std::vector<Candidate> candidates;
        const bool h264{video == VideoCodec::H264};
        if (mode == DecoderMode::HardwarePreferred && h264) {
            for (const char *name : {
                "c2.qti.avc.decoder", "c2.qcom.avc.decoder",
                "OMX.qcom.video.decoder.avc", "c2.mtk.avc.decoder",
                "OMX.MTK.VIDEO.DECODER.AVC", "c2.exynos.avc.decoder",
                "OMX.Exynos.AVC.Decoder"
            }) candidates.push_back({name, false});
        }
        if (mode != DecoderMode::SoftwareAndroid)
            candidates.push_back({"system-auto", true});
        if (h264 && (mode == DecoderMode::SoftwareAndroid || mode == DecoderMode::Hybrid)) {
            candidates.push_back({"c2.android.avc.decoder", false});
            candidates.push_back({"OMX.google.h264.decoder", false});
        }
        if (candidates.empty()) {
            LOGW("NVDEC selected software codec mode is unavailable for this video format");
            return false;
        }

        for (const auto &candidate : candidates) {
            for (int attempt = 0; attempt < 2; ++attempt) {
                codec = candidate.system
                    ? AMediaCodec_createDecoderByType(mime.data())
                    : AMediaCodec_createCodecByName(candidate.name);
                if (!codec) break;
                AMediaFormat *format{AMediaFormat_new()};
                if (!format) {
                    StopLocked();
                    return false;
                }
                AMediaFormat_setString(format, AMEDIAFORMAT_KEY_MIME, mime.data());
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_WIDTH, static_cast<std::int32_t>(w));
                AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_HEIGHT, static_cast<std::int32_t>(h));
                if (attempt == 0)
                    AMediaFormat_setInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, 0x15);
                const auto configured{AMediaCodec_configure(codec, format, nullptr, nullptr, 0)};
                AMediaFormat_delete(format);
                if (configured == AMEDIA_OK && AMediaCodec_start(codec) == AMEDIA_OK) {
                    width = static_cast<std::int32_t>(w);
                    height = static_cast<std::int32_t>(h);
                    cropLeft = cropTop = 0;
                    cropRight = width - 1;
                    cropBottom = height - 1;
                    LOGI("Synolsis NVDEC selected Android codec={} mode={} output={}x{} requestNV12={}",
                         candidate.name, static_cast<std::uint32_t>(mode),
                         width, height, attempt == 0);
                    return true;
                }
                StopLocked();
            }
        }
        LOGW("Synolsis NVDEC no Android MediaCodec decoder accepted mode={} {}x{}",
             static_cast<std::uint32_t>(mode), w, h);
        return false;
    }

    bool AndroidMediaDecoder::QueueAccessUnit(std::span<const std::uint8_t> input,
                                               std::int64_t ptsUs, bool codecConfiguration) {
        constexpr std::size_t MaxPacketBytes{8 * 1024 * 1024};
        if (input.empty() || input.size() > MaxPacketBytes || ptsUs < 0)
            return false;
        std::scoped_lock guard{mutex};
        if (!codec)
            return false;
        // The guest issues NVDEC Execute immediately after selecting the codec.
        // MediaCodec may not yet have a free input slot. A zero-time dequeue
        // regularly drops valid guest access units at cutscene start.
        // This bounded wait is not a guest syncpoint acknowledgement.
        const ssize_t index{AMediaCodec_dequeueInputBuffer(codec, 4'000)};
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

    bool AndroidMediaDecoder::Drain(Frame &frame, std::int64_t waitUs) {
        std::scoped_lock guard{mutex};
        if (!codec)
            return false;
        AMediaCodecBufferInfo info{};
        // MediaCodec decode is asynchronous. The old immediate-only poll
        // consistently missed VIC execute on some Android codecs. Permit a
        // bounded wait (<=12ms) from VIC, but keep NVDEC submit nonblocking.
        const auto boundedWait{std::clamp<std::int64_t>(waitUs, 0, 12'000)};
        ssize_t index{AMediaCodec_dequeueOutputBuffer(codec, &info, boundedWait)};
        if (index == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
            AMediaFormat *format{AMediaCodec_getOutputFormat(codec)};
            if (format) {
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &width);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &height);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &colorFormat);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_STRIDE, &rowStride);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SLICE_HEIGHT, &sliceHeight);
                // Some MediaCodec implementations omit optional stride/
                // slice-height keys. Tight packing is the only safe fallback
                // until a device exposes a different documented layout.
                if (rowStride <= 0) rowStride = width;
                if (sliceHeight <= 0) sliceHeight = height;
                // Android reports a *visible* crop inclusive of right/bottom
                // while the byte buffer may retain padded stride/slice rows.
                // Only accept a fully specified crop within coded dimensions.
                std::int32_t left{}, top{}, right{}, bottom{};
                const bool hasCrop =
                    AMediaFormat_getInt32(format, "crop-left", &left) &&
                    AMediaFormat_getInt32(format, "crop-top", &top) &&
                    AMediaFormat_getInt32(format, "crop-right", &right) &&
                    AMediaFormat_getInt32(format, "crop-bottom", &bottom);
                if (hasCrop && left >= 0 && top >= 0 && right >= left &&
                    bottom >= top && right < width && bottom < height) {
                    cropLeft = left; cropTop = top;
                    cropRight = right; cropBottom = bottom;
                } else {
                    cropLeft = cropTop = 0;
                    cropRight = width - 1;
                    cropBottom = height - 1;
                }
                LOGI("NVDEC Android decoded output format: {}x{} color=0x{:X} stride={} sliceHeight={} crop=({},{})-({},{})",
                     width, height, colorFormat, rowStride, sliceHeight,
                     cropLeft, cropTop, cropRight, cropBottom);
                AMediaFormat_delete(format);
                if (width < 0 || height < 0 ||
                    !ValidVideoExtent(static_cast<std::uint32_t>(width),
                                      static_cast<std::uint32_t>(height))) {
                    // Unsupported/corrupt dynamic format: never expose
                    // unbounded output dimensions to guest texture uploads.
                    StopLocked();
                }
            }
            if (!codec) return false;
            // The next output event can be ready immediately after the format
            // transition; do not silently drop it and strand VIC's one shot.
            index = AMediaCodec_dequeueOutputBuffer(codec, &info, 0);
        }
        if (index < 0)
            return false;

        std::size_t capacity{};
        auto *buffer{AMediaCodec_getOutputBuffer(codec, static_cast<size_t>(index), &capacity)};
        // On Android 14 / API 34, NDK's getOutputBuffer out_size and
        // AMediaCodecBufferInfo.offset are documented as invalid. The
        // previous capacity/offset check rejected valid decoded YUV frames
        // before VIC ever received them. Use info.size only on <= API 35.
        const auto slice{codec_output::ChooseOutputSlice(
            android_get_device_api_level(), capacity,
            static_cast<std::int64_t>(info.offset),
            static_cast<std::int64_t>(info.size))};
        bool valid{buffer && static_cast<bool>(slice)};
        if (valid) {
            frame.data.assign(buffer + slice.offset,
                              buffer + slice.offset + slice.size);
            frame.ptsUs = info.presentationTimeUs;
            frame.width = cropRight - cropLeft + 1;
            frame.height = cropBottom - cropTop + 1;
            frame.cropLeft = cropLeft;
            frame.cropTop = cropTop;
            frame.colorFormat = colorFormat;
            frame.rowStride = rowStride;
            frame.sliceHeight = sliceHeight;
            // Unknown/odd 4:2:0 crop origins cannot be safely sampled as
            // chroma pairs by VIC, so do not hand those frames to the guest.
            valid = frame.width > 0 && frame.height > 0 &&
                    (cropLeft & 1) == 0 && (cropTop & 1) == 0;
        }
        if (!valid) {
            // Do not silently drop a decoded frame: log enough non-sensitive
            // layout data to diagnose Android 14/15 vendor codec differences.
            static std::atomic<std::uint64_t> rejectedOutputs{};
            const auto n{rejectedOutputs.fetch_add(1, std::memory_order_relaxed) + 1};
            if (n <= 5 || (n & (n - 1)) == 0)
                LOGW("Synolsis NVDEC MediaCodec output rejected #{}: api={} buffer={} size={} offset={} capacity={} width={} height={} crop=({},{})",
                     n, android_get_device_api_level(), buffer != nullptr,
                     info.size, info.offset, capacity, width, height, cropLeft, cropTop);
        }
        AMediaCodec_releaseOutputBuffer(codec, static_cast<size_t>(index), false);
        return valid;
    }
}

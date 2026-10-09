// SPDX-License-Identifier: MPL-2.0
// Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)

#include <android/native_window_jni.h>
#include <android/choreographer.h>
#include <common/settings.h>
#include <common/signal.h>
#include <jvm.h>
#include <gpu.h>
#include <soc.h>
#include <loader/loader.h>
#include <kernel/types/KProcess.h>
#include "presentation_engine.h"
#include "presentation_scale.h"
#include "swapchain_policy.h"
#include <os.h>
#include "native_window.h"
#include "texture/format.h"

extern jint Fps;
extern jfloat AverageFrametimeMs;
extern jfloat AverageFrametimeDeviationMs;

namespace skyline::gpu {
    using namespace service::hosbinder;

    PresentationEngine::PresentationEngine(const DeviceState &state, GPU &gpu)
        : state{state},
          gpu{gpu},
          presentSemaphores{util::MakeFilledArray<vk::raii::Semaphore, MaxSwapchainImageCount>(gpu.vkDevice, vk::SemaphoreCreateInfo{})},
          acquireSemaphores{util::MakeFilledArray<vk::raii::Semaphore, MaxSwapchainImageCount>(gpu.vkDevice, vk::SemaphoreCreateInfo{})},
          presentationTrack{static_cast<u64>(trace::TrackIds::Presentation), perfetto::ProcessTrack::Current()},
          vsyncEvent{std::make_shared<kernel::type::KEvent>(state, true)},
          choreographerThread{&PresentationEngine::ChoreographerThread, this},
          presentationThread{&PresentationEngine::PresentationThread, this} {
        auto desc{presentationTrack.Serialize()};
        desc.set_name("Presentation");
        perfetto::TrackEvent::SetTrackDescriptor(presentationTrack, desc);
    }

    PresentationEngine::~PresentationEngine() {
        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(jSurface, nullptr))
            env->DeleteGlobalRef(jSurface);

        if (choreographerThread.joinable()) {
            if (choreographerLooper) {
                choreographerStop = true;
                ALooper_wake(choreographerLooper);
            }
            choreographerThread.join();
        }
    }

    void PresentationEngine::ChoreographerCallback(int64_t frameTimeNanos, PresentationEngine *engine) {
        // If the duration of this cycle deviates by ±0.5ms from the current refresh cycle duration then we reevaluate it
        i64 cycleLength{frameTimeNanos - engine->lastChoreographerTime};
        if (std::abs(cycleLength - engine->refreshCycleDuration) > (constant::NsInMillisecond / 2)) {
            if (engine->window)
                engine->window->perform(engine->window, NATIVE_WINDOW_GET_REFRESH_CYCLE_DURATION, &engine->refreshCycleDuration);
            else
                engine->refreshCycleDuration = cycleLength;
        }

        // Record the current cycle's timestamp and signal the V-Sync event to notify the game that a frame has been displayed
        engine->lastChoreographerTime = frameTimeNanos;
        if (!engine->skipSignal.exchange(false))
            engine->vsyncEvent->Signal();

        // Post the frame callback to be triggered on the next display refresh
        AChoreographer_postFrameCallback64(AChoreographer_getInstance(), reinterpret_cast<AChoreographer_frameCallback64>(&ChoreographerCallback), engine);
    }

    void PresentationEngine::ChoreographerThread() {
        if (int result{pthread_setname_np(pthread_self(), "Sky-Choreo")})
            LOGW("Failed to set the thread name: {}", strerror(result));
        AsyncLogger::UpdateTag();

        try {
            choreographerLooper = ALooper_prepare(0);
            AChoreographer_postFrameCallback64(AChoreographer_getInstance(), reinterpret_cast<AChoreographer_frameCallback64>(&ChoreographerCallback), this);
            while (ALooper_pollAll(-1, nullptr, nullptr, nullptr) == ALOOPER_POLL_WAKE && !choreographerStop); // Will block and process callbacks till ALooper_wake() is called with choreographerStop set
        } catch (const signal::SignalException &e) {
            LOGE("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        } catch (const std::exception &e) {
            LOGE("{}", e.what());
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        }
    }

    void PresentationEngine::PresentFrame(const PresentableFrame &frame) {
        std::unique_lock lock(mutex);
        surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });

        frame.fence.Wait(state.soc->host1x);

        std::scoped_lock textureLock(*frame.textureView);

        auto texture{frame.textureView->texture};
        // Observe completed guest frame submissions only. This tracker
        // retains NO Vulkan image snapshots, so synthetic presentation is
        // deliberately disabled until a synchronized compute path exists.
        const auto candidate{frameHistory.Observe({
            static_cast<std::uint64_t>(frame.id),
            static_cast<std::int64_t>(util::GetTimeNs()),
            texture->dimensions.width, texture->dimensions.height,
            static_cast<std::uint32_t>(*frame.textureView->format),
            reinterpret_cast<std::uintptr_t>(texture.get())
        })};
        if (candidate && (frameHistory.AcceptedPairs() % 120) == 0)
            LOGD("Observed {} eligible frame-history pairs; frame interpolation output not enabled",
                 frameHistory.AcceptedPairs());
        // This is *presentation* scaling only. The emulated GPU still renders
        // all game attachments at their original size, and this setting must
        // never be described as an internal resolution or FSR implementation.
        // V25 flicker diagnostic: keep native 100% final swapchain size.
        // The old 50/75% option only affected host presentation, not
        // guest render targets, and was unreliable on some Turnip paths.
        constexpr u32 percent{100U};
        constexpr bool canBlit{false}; // A/B test: no scaled-image blit on Turnip.
        const auto desired{presentation_scale::ChooseExtent(
            {texture->dimensions.width, texture->dimensions.height},
            percent,
            {vkSurfaceCapabilities.minImageExtent.width, vkSurfaceCapabilities.minImageExtent.height},
            {vkSurfaceCapabilities.maxImageExtent.width, vkSurfaceCapabilities.maxImageExtent.height},
            canBlit)};
        const texture::Dimensions outputExtent{desired.width, desired.height, texture->dimensions.depth};
        if (frame.textureView->format != swapchainFormat || outputExtent != swapchainExtent)
            UpdateSwapchain(frame.textureView->format, outputExtent);

        auto finalCrop{frame.crop};
        if (finalCrop && outputExtent != texture->dimensions) {
            // Guest crop fields are unsigned; saturate before converting to
            // signed crop geometry to avoid -Wc++11-narrowing or wraparound.
            const auto safeCrop{[](u32 value) -> i32 {
                return static_cast<i32>(std::min<u32>(
                    value, static_cast<u32>(std::numeric_limits<i32>::max())));
            }};
            const auto scaled{presentation_scale::ScaleCrop(
                {safeCrop(finalCrop.left), safeCrop(finalCrop.top),
                 safeCrop(finalCrop.right), safeCrop(finalCrop.bottom)},
                {texture->dimensions.width, texture->dimensions.height},
                {outputExtent.width, outputExtent.height})};
            finalCrop.left = scaled.left;
            finalCrop.top = scaled.top;
            finalCrop.right = scaled.right;
            finalCrop.bottom = scaled.bottom;
        }

        int result;
        if (finalCrop && finalCrop != windowCrop) {
            if ((result = window->perform(window, NATIVE_WINDOW_SET_CROP, &finalCrop)))
                throw exception("Setting the layer crop to ({}-{})x({}-{}) failed with {}", finalCrop.left, finalCrop.right, finalCrop.top, finalCrop.bottom, result);
            windowCrop = finalCrop;
        }

        if (frame.scalingMode != NativeWindowScalingMode::Freeze && windowScalingMode != frame.scalingMode) {
            if ((result = window->perform(window, NATIVE_WINDOW_SET_SCALING_MODE, static_cast<i32>(frame.scalingMode))))
                throw exception("Setting the layer scaling mode to '{}' failed with {}", ToString(frame.scalingMode), result);
            windowScalingMode = frame.scalingMode;
        }

        // Avoid a redundant native-window perform/Binder transition on every
        // frame if the Switch output transform did not change.
        if (!windowTransformApplied || frame.transform != windowTransform) {
            if ((result = window->perform(window, NATIVE_WINDOW_SET_BUFFERS_TRANSFORM, static_cast<i32>(frame.transform))))
                throw exception("Setting the buffer transform to '{}' failed with {}", ToString(frame.transform), result);
            windowTransform = frame.transform;
            windowTransformApplied = true;
        }

        auto &acquireSemaphore{acquireSemaphores[frameIndex]};
        auto &frameFence{frameFences[frameIndex]};
        if (frameFence)
            frameFence->Wait();

        frameIndex = (frameIndex + 1) % swapchainImageCount;

        // VK_SUBOPTIMAL_KHR still returns a valid acquired image and signals
        // acquireSemaphore. Retrying would reuse an already-signaled binary
        // semaphore, violating Vulkan sync rules on Android/Turnip.
        const auto nextImage{vkSwapchain->acquireNextImage(
            std::numeric_limits<u64>::max(), *acquireSemaphore, {})};
        if (nextImage.first != vk::Result::eSuccess &&
            nextImage.first != vk::Result::eSuboptimalKHR)
            throw exception("vkAcquireNextImageKHR returned an unhandled result '{}'",
                            vk::to_string(nextImage.first));

        auto &nextImageTexture{images.at(nextImage.second)};
        auto &presentSemaphore{presentSemaphores[nextImage.second]};

        texture->SynchronizeHost();
        nextImageTexture->CopyFrom(texture, *acquireSemaphore, *presentSemaphore, swapchainFormat, vk::ImageSubresourceRange{
            .aspectMask = vk::ImageAspectFlagBits::eColor,
            .levelCount = 1,
            .layerCount = 1,
        }, outputExtent != texture->dimensions);

        frameFence = nextImageTexture->cycle;

        auto getMonotonicNsNow{[]() -> i64 {
            timespec time;
            if (clock_gettime(CLOCK_MONOTONIC, &time))
                throw exception("Failed to clock_gettime with '{}'", strerror(errno));
            return (time.tv_sec * constant::NsInSecond) + time.tv_nsec;
        }};

        i64 timestamp{frame.timestamp};
        if (timestamp) {
            // If the timestamp is specified, we need to convert it from the util::GetTimeNs base to the CLOCK_MONOTONIC one
            // We do so by getting an offset from the current time in nanoseconds and then adding it to the current time in CLOCK_MONOTONIC
            // Note: It's important we do this right before present as going past the timestamp could lead to fewer Binder IPC calls
            i64 current{util::GetTimeNs()};
            if (current < timestamp) {
                timestamp = getMonotonicNsNow() + (timestamp - current);
            } else {
                timestamp = 0;
            }
        }

        if (frame.swapInterval) {
            // If we have a swap interval, we have to adjust the timestamp to emulate the swap interval
            i64 lastFramePresentTime{util::AlignUpNpot(windowLastTimestamp, refreshCycleDuration)};
            if (lastFramePresentTime > lastChoreographerTime)
                // If the last frame was presented after the last choreographer callback, calculate the new frame's timestamp relative to it
                timestamp = std::max(timestamp, lastFramePresentTime + (refreshCycleDuration * frame.swapInterval));
            else
                // If there has been a choreographer callback since the last frame, calculate the new frame's timestamp relative to it
                timestamp = std::max(timestamp, lastChoreographerTime + (2 * refreshCycleDuration * frame.swapInterval));
        }

        i64 lastTimestamp{std::exchange(windowLastTimestamp, timestamp)};
        if (!timestamp && lastTimestamp)
            // We need to nullify the timestamp if it transitioned from being specified (non-zero) to unspecified (zero)
            timestamp = NativeWindowTimestampAuto;

        if (timestamp && (result = window->perform(window, NATIVE_WINDOW_SET_BUFFERS_TIMESTAMP, timestamp)))
            throw exception("Setting the buffer timestamp to {} failed with {}", timestamp, result);

        u64 frameId{};
        if ((result = window->perform(window, NATIVE_WINDOW_GET_NEXT_FRAME_ID, &frameId)))
            throw exception("Retrieving the next frame's ID failed with {}", result);

        {
            std::scoped_lock queueLock{gpu.queueMutex};
            std::ignore = gpu.vkQueue.presentKHR(vk::PresentInfoKHR{
                .swapchainCount = 1,
                .pSwapchains = &**vkSwapchain,
                .pImageIndices = &nextImage.second,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &*presentSemaphore,
            }); // We don't care about suboptimal images as they are caused by not respecting the transform hint, we handle transformations externally
        }

        timestamp = (timestamp && !*state.settings->disableFrameThrottling) ? timestamp : getMonotonicNsNow(); // We tie FPS to the submission time rather than presentation timestamp, if we don't have the presentation timestamp available or if frame throttling is disabled as we want the maximum measured FPS to not be restricted to the refresh rate
        if (frameTimestamp) {
            i64 sampleWeight{Fps ? Fps : 1}; //!< The weight of each sample in calculating the average, we want to roughly average the past second

            auto weightedAverage{[](auto weight, auto previousAverage, auto current) {
                return (((weight - 1) * previousAverage) + current) / weight;
            }}; //!< Modified moving average (https://en.wikipedia.org/wiki/Moving_average#Modified_moving_average)

            i64 currentFrametime{timestamp - frameTimestamp};
            averageFrametimeNs = weightedAverage(sampleWeight, averageFrametimeNs, currentFrametime);
            AverageFrametimeMs = static_cast<jfloat>(averageFrametimeNs) / constant::NsInMillisecond;

            i64 currentFrametimeDeviation{std::abs(averageFrametimeNs - currentFrametime)};
            averageFrametimeDeviationNs = weightedAverage(sampleWeight, averageFrametimeDeviationNs, currentFrametimeDeviation);
            AverageFrametimeDeviationMs = static_cast<jfloat>(averageFrametimeDeviationNs) / constant::NsInMillisecond;

            Fps = static_cast<jint>(std::round(static_cast<float>(constant::NsInSecond) / static_cast<float>(averageFrametimeNs)));

            TRACE_EVENT_INSTANT("gpu", "Present", presentationTrack, "FrameTimeNs", timestamp - frameTimestamp, "Fps", Fps);

            frameTimestamp = timestamp;
        } else {
            frameTimestamp = timestamp;
        }
    }

    void PresentationEngine::PresentationThread() {
        if (int result{pthread_setname_np(pthread_self(), "Sky-Present")})
            LOGW("Failed to set the thread name: {}", strerror(result));
        AsyncLogger::UpdateTag();

        try {
            presentQueue.Process([this](const PresentableFrame &frame) {
                // Experimental HOST presentation frame skipping. It cannot
                // skip game simulation or guest GPU rendering. Keeping fence
                // waits and callbacks is necessary so Horizon buffer slots
                // get released and the guest does not deadlock.
                const bool shouldPresent{skipPolicy.ShouldPresent(
                    *state.settings->presentationFrameSkip)};
                if (!shouldPresent) {
                    // The guest fence and release callback are still required:
                    // skipping these can recycle incomplete guest buffers.
                    frame.fence.Wait(state.soc->host1x);
                    // Temporal history must not span a dropped display frame.
                    // Do not advertise a generated frame without snapshots.
                    {
                        std::scoped_lock presentLock{mutex};
                        frameHistory.Reset();
                    }
                } else {
                    PresentFrame(frame);
                }
                frame.presentCallback(); // Must happen even if host output is skipped
                skipSignal = true;
                vsyncEvent->Signal(); // Preserve the guest's VI synchronization
            }, [] {});
        } catch (const signal::SignalException &e) {
            LOGE("{}\nStack Trace:{}", e.what(), state.loader->GetStackTrace(e.frames));
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        } catch (const std::exception &e) {
            LOGE("{}", e.what());
            if (state.process)
                state.process->Kill(false);
            else
                std::rethrow_exception(std::current_exception());
        }
    }

    NativeWindowTransform GetAndroidTransform(vk::SurfaceTransformFlagBitsKHR transform) {
        using NativeWindowTransform = NativeWindowTransform;
        switch (transform) {
            case vk::SurfaceTransformFlagBitsKHR::eIdentity:
            case vk::SurfaceTransformFlagBitsKHR::eInherit:
                return NativeWindowTransform::Identity;
            case vk::SurfaceTransformFlagBitsKHR::eRotate90:
                return NativeWindowTransform::Rotate90;
            case vk::SurfaceTransformFlagBitsKHR::eRotate180:
                return NativeWindowTransform::Rotate180;
            case vk::SurfaceTransformFlagBitsKHR::eRotate270:
                return NativeWindowTransform::Rotate270;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirror:
                return NativeWindowTransform::MirrorHorizontal;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate90:
                return NativeWindowTransform::MirrorHorizontalRotate90;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate180:
                return NativeWindowTransform::MirrorVertical;
            case vk::SurfaceTransformFlagBitsKHR::eHorizontalMirrorRotate270:
                return NativeWindowTransform::MirrorVerticalRotate90;
        }
    }

    void PresentationEngine::UpdateSwapchain(texture::Format format, texture::Dimensions extent) {
        // Some Adreno/Turnip surfaces expose only two swapchain buffers.
        // Prefer triple buffering when available but avoid failing startup
        // when the Vulkan implementation advertises a smaller maximum.
        const auto &capabilities{vkSurfaceCapabilities};
        auto minImageCount{std::max(capabilities.minImageCount, *state.settings->forceTripleBuffering ? 3U : 2U)};
        if (capabilities.maxImageCount && minImageCount > capabilities.maxImageCount) {
            LOGW("Clamping requested swapchain images {} to device maximum {}", minImageCount, capabilities.maxImageCount);
            minImageCount = capabilities.maxImageCount;
        }
        if (minImageCount > MaxSwapchainImageCount)
            throw exception("Requesting swapchain with higher image count ({}) than maximum slot count ({})", minImageCount, MaxSwapchainImageCount);
        if (minImageCount < capabilities.minImageCount)
            throw exception("Swapchain cannot satisfy the minimum image count {}", capabilities.minImageCount);
        else if (capabilities.minImageExtent.height > extent.height || capabilities.minImageExtent.width > extent.width || capabilities.maxImageExtent.height < extent.height || capabilities.maxImageExtent.width < extent.width)
            throw exception("Cannot update swapchain to accomodate image extent: {}x{} ({}x{}-{}x{})", extent.width, extent.height, capabilities.minImageExtent.width, capabilities.minImageExtent.height, capabilities.maxImageExtent.width, capabilities.maxImageExtent.height);

        // Surface formats may differ between Android windows and drivers.
        // Never assume RGBA is supported simply because the guest format is
        // unsupported: requesting an undeclared format is invalid Vulkan.
        const auto formats{gpu.vkPhysicalDevice.getSurfaceFormatsKHR(**vkSurface)};
        if (formats.empty())
            throw exception("Vulkan surface does not expose any image formats");
        const bool unrestricted{formats.size() == 1 &&
            formats.front().format == vk::Format::eUndefined};
        const auto supportsFormat{[&](vk::Format candidate) {
            return unrestricted || std::any_of(formats.begin(), formats.end(),
                [candidate](const vk::SurfaceFormatKHR &entry) {
                    return entry.format == candidate &&
                        entry.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
                });
        }};
        texture::Format underlyingFormat{format};
        if (!supportsFormat(*format)) {
            if (supportsFormat(format::R8G8B8A8Unorm.vkFormat))
                underlyingFormat = format::R8G8B8A8Unorm;
            else if (supportsFormat(format::B8G8R8A8Unorm.vkFormat))
                underlyingFormat = format::B8G8R8A8Unorm;
            else
                throw exception("Vulkan surface cannot present guest format '{}' or RGBA/BGRA fallbacks",
                                vk::to_string(static_cast<vk::Format>(*format)));
            LOGW("Vulkan surface lacks requested image format {}, selecting {}",
                 vk::to_string(static_cast<vk::Format>(*format)),
                 vk::to_string(static_cast<vk::Format>(*underlyingFormat)));
        }

        constexpr vk::ImageUsageFlags presentUsage{vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst};
        if ((capabilities.supportedUsageFlags & presentUsage) != presentUsage)
            throw exception("Swapchain doesn't support image usage '{}': {}", vk::to_string(presentUsage), vk::to_string(capabilities.supportedUsageFlags));

        vk::PresentModeKHR requestedMode{
            *state.settings->disableFrameThrottling ? vk::PresentModeKHR::eMailbox : vk::PresentModeKHR::eFifo
        };
        // Keep Auto equivalent to the original Strato behavior. The other
        // selections are real Vulkan present modes, opt-in and capability-
        // checked below; unsupported choices fall back to mandatory FIFO.
        switch (*state.settings->vulkanPresentMode) {
            case 1: requestedMode = vk::PresentModeKHR::eFifo; break;
            case 2: requestedMode = vk::PresentModeKHR::eMailbox; break;
            case 3: requestedMode = vk::PresentModeKHR::eImmediate; break;
            default: break;
        }
        auto modes{gpu.vkPhysicalDevice.getSurfacePresentModesKHR(**vkSurface)};
        if (std::find(modes.begin(), modes.end(), requestedMode) == modes.end()) {
            // FIFO is Vulkan's portable presentation mode and must be used
            // when MAILBOX is unavailable instead of terminating game launch.
            LOGW("Present mode {} unavailable; falling back to FIFO", vk::to_string(requestedMode));
            requestedMode = vk::PresentModeKHR::eFifo;
            if (std::find(modes.begin(), modes.end(), requestedMode) == modes.end())
                throw exception("Swapchain does not support required FIFO present mode");
        }

        // Android Vulkan surface capabilities are authoritative. The old
        // Strato code always requested INHERIT alpha and implicitly IDENTITY
        // transform even when the WSI advertised neither. These are explicit
        // Vulkan validity requirements (VUID 01279/01280).
        const auto transformFlag{swapchain_policy::ChooseSupportedFlag(
            static_cast<VkSurfaceTransformFlagsKHR>(capabilities.supportedTransforms),
            VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            static_cast<VkSurfaceTransformFlagBitsKHR>(capabilities.currentTransform))};
        if (!transformFlag)
            throw exception("Vulkan surface has no usable pre-transform");
        const auto preTransform{static_cast<vk::SurfaceTransformFlagBitsKHR>(transformFlag)};

        const auto alphaFlag{swapchain_policy::ChooseSupportedFlag(
            static_cast<VkCompositeAlphaFlagsKHR>(capabilities.supportedCompositeAlpha),
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)};
        if (!alphaFlag)
            throw exception("Vulkan surface has no supported composite alpha mode");
        const auto compositeAlpha{static_cast<vk::CompositeAlphaFlagBitsKHR>(alphaFlag)};
        LOGI("Swapchain WSI: format={} transform={} alpha={} mode={}",
             vk::to_string(static_cast<vk::Format>(*underlyingFormat)),
             vk::to_string(preTransform), vk::to_string(compositeAlpha),
             vk::to_string(requestedMode));

        if (vkSwapchain) {
            // Only on a resize/recreation, never every frame: retire old
            // swapchain images after all device work using them is complete.
            gpu.vkDevice.waitIdle();
            for (auto &fence : frameFences)
                fence.reset();
            frameIndex = 0;
        }

        vkSwapchain.emplace(gpu.vkDevice, vk::SwapchainCreateInfoKHR{
            .surface = **vkSurface,
            .minImageCount = minImageCount,
            .imageFormat = *underlyingFormat,
            .imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = presentUsage,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform = preTransform,
            .compositeAlpha = compositeAlpha,
            .presentMode = requestedMode,
            .clipped = true,
        });

        auto vkImages{vkSwapchain->getImages()};
        if (vkImages.size() > MaxSwapchainImageCount)
            throw exception("Swapchain has higher image count ({}) than maximum slot count ({})", minImageCount, MaxSwapchainImageCount);

        for (size_t index{}; index < vkImages.size(); index++) {
            auto &slot{images[index]};
            slot = std::make_shared<Texture>(*state.gpu, vkImages[index], extent, underlyingFormat, vk::ImageLayout::eUndefined, vk::ImageTiling::eOptimal, vk::ImageCreateFlags{}, presentUsage);
            slot->TransitionLayout(vk::ImageLayout::ePresentSrcKHR);
        }
        for (size_t index{vkImages.size()}; index < MaxSwapchainImageCount; index++)
            // We need to clear all the slots which aren't filled, keeping around stale slots could lead to issues
            images[index] = {};

        swapchainFormat = format;
        swapchainExtent = extent;
        swapchainImageCount = vkImages.size();
    }

    void PresentationEngine::UpdateSurface(jobject newSurface) {
        std::scoped_lock guard{mutex};

        auto env{state.jvm->GetEnv()};
        if (!env->IsSameObject(jSurface, nullptr)) {
            env->DeleteGlobalRef(jSurface);
            jSurface = nullptr;
        }
        if (!env->IsSameObject(newSurface, nullptr))
            jSurface = env->NewGlobalRef(newSurface);

        vkSwapchain.reset();
        frameHistory.Reset(); // Surface recreation invalidates old frame identity and size.
        windowTransformApplied = false; // New native window needs fresh transform

        if (jSurface) {
            window = ANativeWindow_fromSurface(env, jSurface);
            vkSurface.emplace(gpu.vkInstance, vk::AndroidSurfaceCreateInfoKHR{
                .window = window,
            });
            if (!gpu.vkPhysicalDevice.getSurfaceSupportKHR(gpu.vkQueueFamilyIndex, **vkSurface))
                throw exception("Vulkan Queue doesn't support presentation with surface");
            vkSurfaceCapabilities = gpu.vkPhysicalDevice.getSurfaceCapabilitiesKHR(**vkSurface);
            // Construct real FSR1/temporal compute pipelines exactly once
            // per Vulkan device. They cannot dispatch until intermediate
            // VkImages, descriptors and proper barriers are wired up.
            // On unsupported drivers this must never break game display.
            if (!computePipelinesAttempted && state.os && state.os->assetFileSystem) {
                computePipelinesAttempted = true;
                try {
                    computePipelines = std::make_unique<packed_compute::PipelineSet>(
                        gpu.vkDevice, *state.os->assetFileSystem);
                    LOGI("FSR1 EASU/RCAS + temporal Vulkan pipelines initialized; dispatch remains disabled");
                } catch (const std::exception &e) {
                    computePipelines.reset();
                    LOGW("Optional Vulkan compute module unavailable: {}. Continuing with native presentation.", e.what());
                }
            }


            if (swapchainExtent && swapchainFormat)
                UpdateSwapchain(swapchainFormat, swapchainExtent);

            if (window->common.magic != AndroidNativeWindowMagic)
                throw exception("ANativeWindow* has unexpected magic: {} instead of {}", span(&window->common.magic, 1).as_string(true), span<const u8>(reinterpret_cast<const u8 *>(&AndroidNativeWindowMagic), sizeof(u32)).as_string(true));
            if (window->common.version != sizeof(ANativeWindow))
                throw exception("ANativeWindow* has unexpected version: {} instead of {}", window->common.version, sizeof(ANativeWindow));

            int result;
            if (windowCrop && (result = window->perform(window, NATIVE_WINDOW_SET_CROP, &windowCrop)))
                throw exception("Setting the layer crop to ({}-{})x({}-{}) failed with {}", windowCrop.left, windowCrop.right, windowCrop.top, windowCrop.bottom, result);

            if (windowScalingMode != NativeWindowScalingMode::ScaleToWindow && (result = window->perform(window, NATIVE_WINDOW_SET_SCALING_MODE, static_cast<i32>(windowScalingMode))))
                throw exception("Setting the layer scaling mode to '{}' failed with {}", ToString(windowScalingMode), result);

            if (windowTransform != NativeWindowTransform::Identity && (result = window->perform(window, NATIVE_WINDOW_SET_BUFFERS_TRANSFORM, static_cast<i32>(windowTransform))))
                throw exception("Setting the buffer transform to '{}' failed with {}", ToString(windowTransform), result);

            if ((result = window->perform(window, NATIVE_WINDOW_ENABLE_FRAME_TIMESTAMPS, true)))
                throw exception("Enabling frame timestamps failed with {}", result);

            surfaceCondition.notify_all();
        } else {
            vkSurface.reset();
            window = nullptr;
        }
    }

    u64 PresentationEngine::Present(const std::shared_ptr<TextureView> &texture, i64 timestamp, i64 swapInterval, AndroidRect crop, NativeWindowScalingMode scalingMode, NativeWindowTransform transform, skyline::service::hosbinder::AndroidFence fence, const std::function<void()> &presentCallback) {
        if (!vkSurface.has_value()) {
            // We want this function to generally (not necessarily always) block when a surface is not present to implicitly pause the game
            std::unique_lock lock{mutex};
            surfaceCondition.wait(lock, [this] { return vkSurface.has_value(); });
        }

        presentQueue.Push(PresentableFrame{
            texture,
            fence,
            timestamp,
            swapInterval,
            presentCallback,
            nextFrameId,
            crop,
            scalingMode,
            transform
        });

        return nextFrameId++;
    }

    NativeWindowTransform PresentationEngine::GetTransformHint() {
        if (!vkSurface.has_value()) {
            std::unique_lock lock{mutex};
            surfaceCondition.wait(lock, [this]() { return vkSurface.has_value(); });
        }

        return GetAndroidTransform(vkSurfaceCapabilities.currentTransform);
    }
}

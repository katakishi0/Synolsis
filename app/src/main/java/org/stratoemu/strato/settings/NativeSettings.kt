/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.settings

import android.content.Context
import org.stratoemu.strato.BuildConfig
import org.stratoemu.strato.utils.GpuDriverHelper
import kotlinx.serialization.Serializable

/**
 * The settings that will be passed to libskyline when running and executable
 */
@Serializable
@Suppress("unused")
data class NativeSettings(
    // System
    var isDocked : Boolean,
    var usernameValue : String,
    var profilePictureValue : String,
    var systemLanguage : Int,
    var systemRegion : Int,
    var isInternetEnabled : Boolean,

    // Audio
    var isAudioOutputDisabled : Boolean,

    // GPU
    var gpuDriver : String,
    var gpuDriverLibraryName : String,
    var forceTripleBuffering : Boolean,
    var disableFrameThrottling : Boolean,
    var presentationFrameSkip : Int,
    var vulkanPresentationScalePercent : Int,
    var vulkanPresentMode : Int,
    var shaderCompileThreads : Int,
    var executorSlotCountScale : Int,
    var executorFlushThreshold : Int,
    var useDirectMemoryImport : Boolean,
    var forceMaxGpuClocks : Boolean,
    var freeGuestTextureMemory : Boolean,
    var disableShaderCache : Boolean,

    // Hacks
    var enableFastGpuReadbackHack : Boolean,
    var enableFastReadbackWrites : Boolean,
    var disableSubgroupShuffle : Boolean,

    // Debug
    var logLevel : Int,
    var validationLayer : Boolean
) {
    constructor(context : Context, pref : EmulationSettings) : this(
        pref.isDocked,
        pref.usernameValue,
        pref.profilePictureValue,
        pref.systemLanguage,
        pref.systemRegion,
        pref.isInternetEnabled,
        pref.isAudioOutputDisabled,
        if (pref.gpuDriver == EmulationSettings.SYSTEM_GPU_DRIVER) "" else pref.gpuDriver,
        if (pref.gpuDriver == EmulationSettings.SYSTEM_GPU_DRIVER) "" else GpuDriverHelper.getLibraryName(context, pref.gpuDriver),
        pref.forceTripleBuffering,
        pref.disableFrameThrottling,
        pref.presentationFrameSkip.coerceIn(0, 2),
        pref.vulkanPresentationScalePercent.takeIf { it == 50 || it == 75 } ?: 100,
        pref.vulkanPresentMode.coerceIn(0, 3),
        pref.shaderCompileThreads.coerceIn(0, 4),
        pref.executorSlotCountScale,
        pref.executorFlushThreshold,
        // The Direct Memory Import path is only supported with the system
        // proprietary Adreno driver, never a custom Turnip Vulkan driver.
        pref.useDirectMemoryImport && pref.gpuDriver == EmulationSettings.SYSTEM_GPU_DRIVER,
        pref.forceMaxGpuClocks,
        pref.freeGuestTextureMemory,
        pref.disableShaderCache,
        pref.enableFastGpuReadbackHack,
        pref.enableFastReadbackWrites,
        pref.disableSubgroupShuffle,
        pref.logLevel,
        BuildConfig.BUILD_TYPE != "release" && pref.validationLayer
    )

    /**
     * Updates settings in libskyline during emulation
     */
    external fun updateNative()
}

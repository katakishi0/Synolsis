/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Skyline-Strato-Android contributors
 */

package org.stratoemu.strato.settings

import android.os.Bundle
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.Preference
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.TwoStatePreference
import org.stratoemu.strato.R
import org.stratoemu.strato.data.AppItem
import org.stratoemu.strato.data.AppItemTag
import org.stratoemu.strato.utils.GpuDriverHelper
import org.stratoemu.strato.utils.WindowInsetsHelper
import org.stratoemu.strato.utils.serializable

/**
 * Only options already implemented in libskyline are shown here.
 * No mock frame generation, asynchronous pipeline or resolution-scaling toggles.
 */
class ExperimentalSettingsFragment : PreferenceFragmentCompat() {
    override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
        val item = arguments?.serializable<AppItem>(AppItemTag)
        if (item != null) {
            preferenceManager.sharedPreferencesName =
                EmulationSettings.prefNameForTitle(item.titleId ?: item.key())
        }
        addPreferencesFromResource(R.xml.experimental_preferences)

        if (item != null) {
            val titleId = item.titleId ?: item.key()
            if (!EmulationSettings.forTitleId(titleId).useCustomSettings) {
                findPreference<Preference>("experimental_warning")?.summary =
                    getString(R.string.experimental_warning_custom)
            }
        }

        // This applies only to the Qualcomm/Adreno devices with a supported
        // clock-control path. Never attempt to force unavailable GPU clocks.
        if (!GpuDriverHelper.supportsForceMaxGpuClocks()) {
            findPreference<TwoStatePreference>("force_max_gpu_clocks")?.apply {
                isEnabled = false
                summary = getString(R.string.force_max_gpu_clocks_desc_unsupported)
            }
        }

        // Direct import relies on proprietary Adreno behavior. The Turnip
        // custom driver is not that driver: disable the unsafe preference UI.
        val active = if (item != null)
            EmulationSettings.forEmulation(item.titleId ?: item.key())
        else
            EmulationSettings.global
        if (active.gpuDriver != EmulationSettings.SYSTEM_GPU_DRIVER) {
            findPreference<TwoStatePreference>("use_direct_memory_import")?.apply {
                isEnabled = false
                summary = getString(R.string.experimental_direct_import_turnip)
            }
        }
    }

    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val recyclerView = view.findViewById<View>(androidx.preference.R.id.recycler_view)
        WindowInsetsHelper.setPadding(recyclerView, bottom = true)
        arguments?.serializable<AppItem>(AppItemTag)?.let {
            (activity as? AppCompatActivity)?.supportActionBar?.subtitle = it.title
        }
    }
}

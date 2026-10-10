/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2023 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato.settings

import android.content.Intent
import android.os.Bundle
import android.view.View
import androidx.preference.Preference
import androidx.preference.PreferenceCategory
import androidx.preference.PreferenceFragmentCompat
import androidx.preference.SwitchPreferenceCompat
import androidx.preference.TwoStatePreference
import androidx.window.layout.FoldingFeature
import androidx.window.layout.WindowInfoTracker
import org.stratoemu.strato.BuildConfig
import org.stratoemu.strato.MainActivity
import org.stratoemu.strato.R
import org.stratoemu.strato.utils.WindowInsetsHelper
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

/**
 * This fragment is used to display the global preferences
 */
class GlobalSettingsFragment : PreferenceFragmentCompat() {
    override fun onViewCreated(view : View, savedInstanceState : Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        val recyclerView = view.findViewById<View>(androidx.preference.R.id.recycler_view)
        WindowInsetsHelper.setPadding(recyclerView, bottom = true)
        // The settings hub deep-links directly to the existing preference;
        // it does not duplicate any settings dialogs or data stores.
        activity?.intent?.getStringExtra("synolsis_target_preference")?.let { target ->
            view.post { scrollToPreference(target) }
        }
    }

    /**
     * This constructs the preferences from XML preference resources
     */
    override fun onCreatePreferences(savedInstanceState : Bundle?, rootKey : String?) {
        // Each hub entry has its own real preference screen. Do not
        // duplicate GPU driver management in Advanced or hide a dummy page.
        // The app's existing preference store and emulation config are shared.
        val section = requireActivity().intent.getStringExtra(
            SettingsActivity.EXTRA_SYNOLSIS_SECTION
        ) ?: SettingsActivity.SECTION_ADVANCED
        when (section) {
            SettingsActivity.SECTION_APPEARANCE, SettingsActivity.SECTION_CONTENT -> {
                addPreferencesFromResource(R.xml.app_preferences)
                val categoryToKeep = if (section == SettingsActivity.SECTION_APPEARANCE)
                    "category_appearance" else "category_content"
                // Remove unused categories rather than merely hiding them;
                // search will then never reveal wrong entries in this section.
                for (index in preferenceScreen.preferenceCount - 1 downTo 0) {
                    val item = preferenceScreen.getPreference(index)
                    if (item.key != categoryToKeep)
                        preferenceScreen.removePreference(item)
                }
                // The Saves entry must open only its own import/export preference.
                // Firmware remains the single entry for firmware, prod.keys and title.keys.
                val focused = requireActivity().intent
                    .getStringExtra("synolsis_target_preference")
                    ?.takeIf { section == SettingsActivity.SECTION_CONTENT && it == "save_data_manager" }
                if (focused != null) {
                    val category = findPreference<PreferenceCategory>("category_content")
                    if (category != null) {
                        for (index in category.preferenceCount - 1 downTo 0) {
                            if (category.getPreference(index).key != focused)
                                category.removePreference(category.getPreference(index))
                        }
                    }
                }
            }
            SettingsActivity.SECTION_CONTROLS ->
                addPreferencesFromResource(R.xml.input_preferences)
            SettingsActivity.SECTION_ABOUT ->
                addPreferencesFromResource(R.xml.credits_preferences)
            else ->
                addPreferencesFromResource(R.xml.emulation_preferences)
        }


        // Recreate the interface after the new palette preference is persisted.
        findPreference<Preference>("synolsis_palette")?.setOnPreferenceChangeListener { _, _ ->
            val host = requireActivity()
            host.window.decorView.post {
                // Relaunch only after IntegerListPreference has persisted its value.
                val restart = Intent(host, MainActivity::class.java).apply {
                    addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK)
                }
                host.startActivity(restart)
            }
            true
        }

        // Re-launch the app if Material You is toggled
        findPreference<Preference>("use_material_you")?.setOnPreferenceChangeListener { _, _ ->
            requireActivity().finishAffinity()
            startActivity(Intent(requireContext(), MainActivity::class.java))
            true
        }

        CoroutineScope(Dispatchers.IO).launch {
            WindowInfoTracker.getOrCreate(requireContext()).windowLayoutInfo(requireActivity()).collect { newLayoutInfo ->
                withContext(Dispatchers.Main) {
                    findPreference<SwitchPreferenceCompat>("enable_foldable_layout")?.isVisible = newLayoutInfo.displayFeatures.find { it is FoldingFeature } != null
                }
            }
        }

        // Uncheck `disable_frame_throttling` if `force_triple_buffering` gets disabled
        val disableFrameThrottlingPref = findPreference<TwoStatePreference>("disable_frame_throttling")
        findPreference<TwoStatePreference>("force_triple_buffering")?.setOnPreferenceChangeListener { _, newValue ->
            if (newValue == false)
                disableFrameThrottlingPref?.isChecked = false
            true
        }

        // Only show validation layer setting in debug builds
        @Suppress("SENSELESS_COMPARISON")
        if (BuildConfig.BUILD_TYPE != "release")
            findPreference<Preference>("validation_layer")?.isVisible = true

        // The Experimental category now appears directly in this same screen.
        configureInlineExperimentalOptions(EmulationSettings.global.gpuDriver)
        wireSynolsisGpuProfiles(EmulationSettings.global)
        wireSynolsisReadbackPrecision(EmulationSettings.global)
    }
}

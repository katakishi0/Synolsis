/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2020 Skyline Team and Contributors (https://github.com/skyline-emu/)
 */

package org.stratoemu.strato

import android.app.Application
import android.app.Activity
import android.os.Bundle
import org.stratoemu.strato.settings.AppSettings
import org.stratoemu.strato.utils.applySynolsisPalette
import android.content.Context
import com.google.android.material.color.DynamicColors
import com.google.android.material.color.DynamicColorsOptions
import dagger.hilt.android.HiltAndroidApp
import org.stratoemu.strato.di.getSettings
import java.io.File

/**
 * @return The optimal directory for putting public files inside, this may return a private directory if a public directory cannot be retrieved
 */
fun Context.getPublicFilesDir() : File = getExternalFilesDir(null) ?: filesDir

@HiltAndroidApp
class StratoApplication : Application() {
    init {
        instance = this
    }

    companion object {
        lateinit var instance : StratoApplication
            private set

        val context : Context get() = instance.applicationContext
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        System.loadLibrary("skyline")

        // Theme resources must be applied before Activity.onCreate, including
        // the driver manager, per-game settings and emulation overlay.
        registerActivityLifecycleCallbacks(object : Application.ActivityLifecycleCallbacks {
            override fun onActivityPreCreated(activity : Activity, savedInstanceState : Bundle?) {
                activity.applySynolsisPalette()
            }
            override fun onActivityCreated(activity : Activity, savedInstanceState : Bundle?) {}
            override fun onActivityStarted(activity : Activity) {}
            override fun onActivityResumed(activity : Activity) {}
            override fun onActivityPaused(activity : Activity) {}
            override fun onActivityStopped(activity : Activity) {}
            override fun onActivitySaveInstanceState(activity : Activity, outState : Bundle) {}
            override fun onActivityDestroyed(activity : Activity) {}
        })
        // Material You would overwrite deliberately selected custom palettes.
        val dynamicColorsOptions = DynamicColorsOptions.Builder()
            .setPrecondition { _, _ -> getSettings().useMaterialYou && AppSettings(this).synolsisPalette == 0 }
            .build()
        DynamicColors.applyToActivitiesIfAvailable(this, dynamicColorsOptions)
    }
}

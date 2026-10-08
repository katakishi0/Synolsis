/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 *
 * An original Material frontend inspired by Yuzu/Nyushu's card navigation.
 * It reuses existing Strato activities, not foreign licensed frontend code.
 */
package org.stratoemu.strato

import android.content.Intent
import android.os.Bundle
import android.view.Gravity
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.google.android.material.card.MaterialCardView
import com.google.android.material.color.MaterialColors
import org.stratoemu.strato.input.ControllerActivity
import org.stratoemu.strato.preference.GpuDriverActivity
import org.stratoemu.strato.settings.SettingsActivity
import kotlin.math.roundToInt

/**
 * Modern settings landing screen. The cards are navigation only;
 * all actual settings and their persistence stay in their original
 * Strato classes and SharedPreferences.
 */
class SynolsisSettingsHubActivity : AppCompatActivity() {
    private fun dp(value : Int) = (resources.displayMetrics.density * value).roundToInt()

    private fun openSettings(key : String? = null) {
        startActivity(Intent(this, SettingsActivity::class.java).apply {
            key?.let { putExtra("synolsis_target_preference", it) }
        })
    }

    override fun onCreate(state : Bundle?) {
        super.onCreate(state)

        val scroll = ScrollView(this).apply {
            clipToPadding = false
            isFillViewport = true
        }
        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(8), dp(20), dp(8), dp(32))
        }
        scroll.addView(column)
        setContentView(scroll)

        val header = TextView(this).apply {
            text = getString(R.string.synolsis_hub_title)
            textSize = 30f
            setPadding(dp(24), dp(20), dp(20), dp(22))
            setOnClickListener { finish() }
            isClickable = true
            contentDescription = getString(R.string.synolsis_hub_back)
        }
        column.addView(header)

        fun addCard(titleRes : Int, descRes : Int, iconRes : Int, click : () -> Unit) {
            val card = MaterialCardView(this).apply {
                radius = dp(18).toFloat()
                cardElevation = dp(1).toFloat()
                isClickable = true
                isFocusable = true
                setOnClickListener { click() }
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    ViewGroup.LayoutParams.WRAP_CONTENT
                ).apply { setMargins(dp(12), dp(7), dp(12), dp(7)) }
            }
            val inner = LinearLayout(this).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(dp(16), dp(18), dp(16), dp(18))
            }
            val icon = ImageView(this).apply {
                setImageResource(iconRes)
                imageTintList = ContextCompat.getColorStateList(this@SynolsisSettingsHubActivity, android.R.color.white)
                layoutParams = LinearLayout.LayoutParams(dp(36), dp(36)).apply {
                    marginEnd = dp(18)
                }
                contentDescription = null // The whole card has title+description.
            }
            // Use theme's accent so the palette picker affects these cards.
            icon.setColorFilter(MaterialColors.getColor(card, com.google.android.material.R.attr.colorPrimary))
            inner.addView(icon)
            val labels = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
            }
            labels.addView(TextView(this).apply {
                text = getString(titleRes)
                textSize = 18f
                setTypeface(typeface, android.graphics.Typeface.BOLD)
            })
            labels.addView(TextView(this).apply {
                text = getString(descRes)
                textSize = 14f
                setPadding(0, dp(5), 0, 0)
            })
            inner.addView(labels, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
            card.addView(inner)
            column.addView(card)
        }

        addCard(R.string.synolsis_hub_advanced, R.string.synolsis_hub_advanced_desc, R.drawable.ic_settings) {
            openSettings()
        }
        addCard(R.string.synolsis_hub_gpu, R.string.synolsis_hub_gpu_desc, R.drawable.ic_settings) {
            startActivity(Intent(this, GpuDriverActivity::class.java))
        }
        addCard(R.string.synolsis_hub_controls, R.string.synolsis_hub_controls_desc, R.drawable.ic_controller) {
            startActivity(Intent(this, ControllerActivity::class.java).putExtra("index", 0))
        }
        addCard(R.string.synolsis_hub_theme, R.string.synolsis_hub_theme_desc, R.drawable.ic_settings) {
            openSettings("synolsis_palette")
        }
        addCard(R.string.synolsis_hub_firmware, R.string.synolsis_hub_firmware_desc, R.drawable.ic_play) {
            openSettings("firmware")
        }
        addCard(R.string.synolsis_hub_data, R.string.synolsis_hub_data_desc, R.drawable.ic_search) {
            openSettings("document_provider")
        }
        // QLauncher will be shown here only after the native system-applet
        // bootstrap and missing-service handling are implemented and tested.
    }
}

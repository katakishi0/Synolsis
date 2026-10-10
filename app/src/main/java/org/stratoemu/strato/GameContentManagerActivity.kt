/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato

import android.content.res.ColorStateList
import android.graphics.Color
import android.net.Uri
import android.os.Bundle
import android.text.TextUtils
import android.view.Gravity
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import com.google.android.material.button.MaterialButton
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.stratoemu.strato.data.GameContentStore
import kotlin.math.roundToInt

/**
 * Minimal game add-on screen matching the black, single-action UX reference.
 * Verified packages are staged only; no claims of native BKTR/AOC mounting.
 */
class GameContentManagerActivity : AppCompatActivity() {
    companion object {
        const val EXTRA_TITLE_ID = "synolsis_game_content_title"
        const val EXTRA_GAME_TITLE = "synolsis_game_content_name"
        const val EXTRA_CATEGORY = "synolsis_game_content_category"
    }

    private lateinit var titleId: String
    private lateinit var listContainer: LinearLayout
    private lateinit var installButton: MaterialButton
    private var chosenKind = GameContentStore.Kind.UPDATE
    private var busy = false

    private val picker = registerForActivityResult(ActivityResultContracts.OpenDocument()) { uri: Uri? ->
        if (uri != null) importPackage(uri, chosenKind)
    }

    private fun dp(value: Int) = (value * resources.displayMetrics.density).roundToInt()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        titleId = intent.getStringExtra(EXTRA_TITLE_ID) ?: ""
        if (!GameContentStore.isValidTitleId(titleId)) {
            Toast.makeText(this, R.string.synolsis_content_invalid_title, Toast.LENGTH_LONG).show()
            finish()
            return
        }
        chosenKind = if ((savedInstanceState?.getString("chosen_type")
                ?: intent.getStringExtra(EXTRA_CATEGORY)) == "dlc")
            GameContentStore.Kind.DLC else GameContentStore.Kind.UPDATE

        supportActionBar?.hide()
        window.statusBarColor = Color.BLACK
        window.navigationBarColor = Color.BLACK

        val root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }
        val scroll = ScrollView(this).apply {
            isFillViewport = true
            clipToPadding = false
            setFillViewport(true)
        }
        val column = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(18), dp(28), dp(18), dp(112))
        }
        scroll.addView(column)
        root.addView(scroll, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))

        val bar = LinearLayout(this).apply {
            gravity = Gravity.CENTER_VERTICAL
            orientation = LinearLayout.HORIZONTAL
        }
        bar.addView(TextView(this).apply {
            text = "‹"
            textSize = 39f
            setTextColor(Color.WHITE)
            gravity = Gravity.CENTER
            contentDescription = getString(android.R.string.cancel)
            setOnClickListener { finish() }
        }, LinearLayout.LayoutParams(dp(50), dp(56)))
        bar.addView(TextView(this).apply {
            val game = intent.getStringExtra(EXTRA_GAME_TITLE) ?: titleId
            text = "${getString(R.string.synolsis_v54_complements)}: $game"
            textSize = 23f
            setTextColor(Color.WHITE)
            maxLines = 1
            ellipsize = TextUtils.TruncateAt.END
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        column.addView(bar)

        // A clean empty screen is intentional: headings appear only for
        // categories containing staged packages.
        listContainer = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(8), dp(18), dp(8), 0)
        }
        column.addView(listContainer)

        installButton = MaterialButton(this).apply {
            text = getString(R.string.synolsis_v54_install)
            textSize = 16f
            minHeight = dp(58)
            cornerRadius = dp(23)
            setIconResource(R.drawable.ic_add)
            iconPadding = dp(8)
            backgroundTintList = ColorStateList.valueOf(
                MaterialColors.getColor(this, com.google.android.material.R.attr.colorPrimary))
            setOnClickListener { if (!busy) showKindSelector() }
        }
        root.addView(installButton, FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.WRAP_CONTENT, dp(60), Gravity.BOTTOM or Gravity.END).apply {
            setMargins(dp(24), 0, dp(24), dp(24))
        })
        setContentView(root)
        refreshList()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putString("chosen_type",
            if (chosenKind == GameContentStore.Kind.DLC) "dlc" else "updates")
        super.onSaveInstanceState(outState)
    }

    private fun showKindSelector() {
        var selection = 0
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.synolsis_v54_choose_type)
            .setSingleChoiceItems(arrayOf(
                getString(R.string.synolsis_v54_install_update),
                getString(R.string.synolsis_v54_install_dlc)
            ), 0) { _, which -> selection = which }
            .setNegativeButton(android.R.string.cancel, null)
            .setPositiveButton(android.R.string.ok) { _, _ ->
                chosenKind = if (selection == 0) GameContentStore.Kind.UPDATE
                             else GameContentStore.Kind.DLC
                picker.launch(arrayOf("*/*"))
            }.show()
    }

    private fun refreshList() {
        listContainer.removeAllViews()
        var any = false
        for (kind in listOf(GameContentStore.Kind.UPDATE, GameContentStore.Kind.DLC)) {
            val entries = GameContentStore.list(this, titleId, kind)
            if (entries.isEmpty()) continue
            any = true
            listContainer.addView(TextView(this).apply {
                text = getString(if (kind == GameContentStore.Kind.UPDATE)
                    R.string.synolsis_v54_install_update else R.string.synolsis_v54_install_dlc)
                textSize = 19f
                setTextColor(Color.WHITE)
                setPadding(dp(2), dp(26), 0, dp(12))
            })
            entries.forEach { entry ->
                listContainer.addView(TextView(this).apply {
                    val short = entry.displayName
                    text = "$short   ·   ${"%.1f".format(entry.size / 1048576.0)} MB"
                    textSize = 15f
                    setTextColor(0xFFC9CDD5.toInt())
                    setPadding(dp(12), dp(16), dp(12), dp(16))
                    setOnClickListener {
                        if (busy) return@setOnClickListener
                        MaterialAlertDialogBuilder(this@GameContentManagerActivity)
                            .setTitle(entry.displayName)
                            .setItems(arrayOf(
                                getString(R.string.synolsis_content_verify),
                                getString(R.string.synolsis_content_delete)
                            )) { _, action ->
                                if (action == 0) verifyPackage(entry)
                                else deletePackage(kind, entry.file.name)
                            }.show()
                    }
                })
            }
        }
        if (any) {
            listContainer.addView(TextView(this).apply {
                text = getString(R.string.synolsis_v55_staged_notice)
                textSize = 13f
                setTextColor(0xFFAAB0BC.toInt())
                setPadding(0, dp(20), 0, 0)
            })
        }
    }

    private fun verifyPackage(entry: GameContentStore.Entry) {
        if (busy) return
        busy = true
        installButton.isEnabled = false
        lifecycleScope.launch {
            val verified = withContext(Dispatchers.IO) {
                runCatching { GameContentStore.verifyIntegrity(entry) }
            }
            busy = false
            installButton.isEnabled = true
            Toast.makeText(this@GameContentManagerActivity,
                if (verified.getOrNull() == true) R.string.synolsis_content_checksum_ok
                else R.string.synolsis_content_checksum_failed, Toast.LENGTH_LONG).show()
        }
    }

    private fun deletePackage(kind: GameContentStore.Kind, name: String) {
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.synolsis_content_delete_confirm)
            .setMessage(name)
            .setNegativeButton(R.string.no, null)
            .setPositiveButton(R.string.yes) { _, _ ->
                if (!GameContentStore.delete(this, titleId, kind, name)) {
                    Toast.makeText(this, R.string.synolsis_content_error, Toast.LENGTH_LONG).show()
                }
                refreshList()
            }.show()
    }

    private fun importPackage(uri: Uri, importKind: GameContentStore.Kind) {
        if (busy) return
        busy = true
        installButton.isEnabled = false
        lifecycleScope.launch {
            val result = withContext(Dispatchers.IO) {
                runCatching { GameContentStore.importNsp(
                    this@GameContentManagerActivity, titleId, importKind, uri) }
            }
            busy = false
            installButton.isEnabled = true
            result.fold(
                onSuccess = {
                    Toast.makeText(this@GameContentManagerActivity, R.string.synolsis_content_staged_success, Toast.LENGTH_LONG).show()
                    refreshList()
                },
                onFailure = { error ->
                    val message = when (error) {
                        is GameContentStore.WrongTitleException -> getString(R.string.synolsis_content_wrong_title)
                        is GameContentStore.WrongContentKindException -> getString(R.string.synolsis_content_wrong_kind)
                        is GameContentStore.DuplicatePackageException -> getString(R.string.synolsis_content_duplicate)
                        else -> error.localizedMessage ?: getString(R.string.synolsis_content_error)
                    }
                    MaterialAlertDialogBuilder(this@GameContentManagerActivity)
                        .setTitle(R.string.synolsis_content_error)
                        .setMessage(message)
                        .setPositiveButton(android.R.string.ok, null).show()
                }
            )
        }
    }
}

/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.utils

import android.content.Context
import org.stratoemu.strato.getPublicFilesDir
import java.io.File
import java.nio.file.Files

/**
 * Deletes only cached pipeline data for a requested Switch title ID,
 * including the current driver-fingerprinted caches and legacy flat paths.
 * Caller must stop emulation before invoking this operation.
 */
object GameShaderCacheUtils {
    private val titleIdPattern = Regex("[0-9a-fA-F]{16}")
    data class Result(val filesDeleted: Int, val bytesFreed: Long)

    fun clear(context: Context, titleId: String): Result {
        require(titleIdPattern.matches(titleId)) { "Invalid game title ID" }
        val root = context.getPublicFilesDir()
        val names = listOf("graphics_pipeline_cache", "vk_graphics_pipeline_cache")
        var removed = 0
        var freed = 0L

        fun removeTarget(file: File) {
            if (!file.exists()) return
            // Do not follow symlinks from app-visible external storage.
            if (!Files.isSymbolicLink(file.toPath()) && file.isDirectory) {
                file.listFiles()?.forEach(::removeTarget)
            } else if (file.isFile) {
                freed += file.length()
            }
            if (file.delete()) removed++
        }

        for (name in names) {
            val category = File(root, name)
            if (!category.isDirectory || Files.isSymbolicLink(category.toPath())) continue
            val driverDirs = listOf(category) + (category.listFiles()?.filter {
                it.isDirectory && !Files.isSymbolicLink(it.toPath())
            } ?: emptyList())
            for (driverDir in driverDirs) {
                for (suffix in listOf("", ".staging", ".merging")) {
                    removeTarget(File(driverDir, titleId.uppercase() + suffix))
                    removeTarget(File(driverDir, titleId.lowercase() + suffix))
                }
            }
        }
        return Result(removed, freed)
    }
}

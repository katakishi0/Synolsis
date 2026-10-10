/*
 * SPDX-License-Identifier: MPL-2.0
 * Copyright © 2026 Synolsis Emu contributors
 */
package org.stratoemu.strato.provider

import android.content.ContentProvider
import android.content.ContentValues
import android.database.Cursor
import android.net.Uri
import android.os.ParcelFileDescriptor
import org.stratoemu.strato.getPublicFilesDir
import java.io.File
import java.io.FileNotFoundException

/**
 * Read-only provider scoped to the installed HOME Program NCA. Unlike a
 * FileProvider root declaration it works with the actual app public-files
 * location, including nonstandard Android storage configurations. No raw
 * paths, directory traversal, or arbitrary shared files are accepted.
 */
class SynolsisHomeNcaProvider : ContentProvider() {
    companion object {
        fun uriFor(packageName: String, basename: String): Uri =
            Uri.Builder().scheme("content")
                .authority("$packageName.synolsis.home")
                .appendPath("nca").appendPath(basename).build()
    }

    override fun onCreate() = true

    override fun getType(uri: Uri) = "application/octet-stream"
    override fun query(uri: Uri, projection: Array<out String>?, selection: String?,
                       selectionArgs: Array<out String>?, sortOrder: String?): Cursor? = null
    override fun insert(uri: Uri, values: ContentValues?): Uri? = null
    override fun delete(uri: Uri, selection: String?, selectionArgs: Array<out String>?) = 0
    override fun update(uri: Uri, values: ContentValues?, selection: String?,
                        selectionArgs: Array<out String>?) = 0

    override fun openFile(uri: Uri, mode: String): ParcelFileDescriptor {
        if (mode != "r") throw FileNotFoundException("QLauncher provider is read only")
        val ctx = context ?: throw FileNotFoundException("Provider has no context")
        val parts = uri.pathSegments
        if (parts.size != 2 || parts[0] != "nca" ||
            !parts[1].endsWith(".nca", ignoreCase = true) ||
            parts[1].length !in 5..128 || parts[1].contains("..") ||
            parts[1].contains('/') || parts[1].contains('\\'))
            throw FileNotFoundException("Invalid Program NCA reference")

        val root = File(ctx.getPublicFilesDir(), "switch/nand/system/Contents/registered").canonicalFile
        val file = File(root, parts[1]).canonicalFile
        if (file.parentFile != root || !file.isFile || !file.canRead())
            throw FileNotFoundException("Installed Program NCA is not readable")
        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY)
    }
}

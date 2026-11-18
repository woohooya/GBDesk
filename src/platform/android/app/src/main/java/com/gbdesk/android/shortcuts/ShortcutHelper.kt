package com.gbdesk.android.shortcuts

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.graphics.ImageDecoder
import android.net.Uri
import android.os.Build
import android.provider.OpenableColumns
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.graphics.drawable.IconCompat
import com.gbdesk.android.EmulatorActivity
import com.gbdesk.android.R
import java.security.MessageDigest

object ShortcutHelper {

    fun isPinSupported(context: Context): Boolean {
        return ShortcutManagerCompat.isRequestPinShortcutSupported(context)
    }

    fun resolveDisplayName(context: Context, uri: Uri): String {
        context.contentResolver.query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { cursor ->
            if (cursor.moveToFirst()) {
                val idx = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME)
                if (idx >= 0) {
                    val name = cursor.getString(idx)
                    if (!name.isNullOrBlank()) {
                        return name.substringBeforeLast('.')
                    }
                }
            }
        }
        return uri.lastPathSegment?.substringAfterLast('/')?.substringBeforeLast('.') ?: "Game"
    }

    fun decodeScaledIcon(context: Context, imageUri: Uri?, maxSizePx: Int = 192): Bitmap? {
        if (imageUri == null) return null
        return try {
            val raw = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                val source = ImageDecoder.createSource(context.contentResolver, imageUri)
                ImageDecoder.decodeBitmap(source) { decoder, _, _ ->
                    decoder.isMutableRequired = false
                }
            } else {
                context.contentResolver.openInputStream(imageUri)?.use { stream ->
                    BitmapFactory.decodeStream(stream)
                }
            } ?: return null

            Bitmap.createScaledBitmap(raw, maxSizePx, maxSizePx, true)
        } catch (_: Exception) {
            null
        }
    }

    fun createGameShortcut(
        context: Context,
        romUri: Uri,
        shortcutName: String,
        iconUri: Uri?
    ): Boolean {
        if (!isPinSupported(context)) {
            return false
        }

        // Persist read permission so the shortcut can launch the ROM anytime
        try {
            context.contentResolver.takePersistableUriPermission(
                romUri,
                Intent.FLAG_GRANT_READ_URI_PERMISSION
            )
        } catch (_: SecurityException) {
            // Some providers do not support persistable permissions; proceed anyway
        }

        val launchIntent = Intent(context, EmulatorActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            data = romUri
            flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
        }

        val bitmap = decodeScaledIcon(context, iconUri)
        val icon = if (bitmap != null) {
            IconCompat.createWithBitmap(bitmap)
        } else {
            IconCompat.createWithResource(context, R.mipmap.ic_launcher)
        }

        val idHash = MessageDigest.getInstance("SHA-1")
            .digest(romUri.toString().toByteArray())
            .joinToString("") { "%02x".format(it) }
            .take(12)

        val shortcutInfo = ShortcutInfoCompat.Builder(context, "game_$idHash")
            .setShortLabel(shortcutName.ifBlank { "Game" })
            .setLongLabel(shortcutName.ifBlank { "Game" })
            .setIcon(icon)
            .setIntent(launchIntent)
            .build()

        return ShortcutManagerCompat.requestPinShortcut(context, shortcutInfo, null)
    }
}

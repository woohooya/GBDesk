package com.gbdesk.android

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import com.gbdesk.android.shortcuts.ShortcutHelper
import com.gbdesk.android.ui.LibraryScreen
import com.gbdesk.android.ui.RecentGame
import com.gbdesk.android.ui.ShortcutDialog

class MainActivity : ComponentActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContent {
            MaterialTheme {
                var recentGames by remember { mutableStateOf(loadRecentGames()) }
                var shortcutTarget by remember { mutableStateOf<Pair<Uri?, String>?>(null) }

                val romLauncher = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.OpenDocument()
                ) { uri ->
                    if (uri != null) {
                        try {
                            contentResolver.takePersistableUriPermission(
                                uri,
                                Intent.FLAG_GRANT_READ_URI_PERMISSION
                            )
                        } catch (_: SecurityException) {
                        }
                        val title = ShortcutHelper.resolveDisplayName(this, uri)
                        recentGames = saveRecentGame(uri, title)
                        launchEmulator(uri)
                    }
                }

                LibraryScreen(
                    recentGames = recentGames,
                    onPickRom = { romLauncher.launch(arrayOf("*/*")) },
                    onLaunchGame = { uri -> launchEmulator(uri) },
                    onCreateShortcut = { uri, name -> shortcutTarget = uri to name }
                )

                shortcutTarget?.let { (initialUri, initialName) ->
                    ShortcutDialog(
                        initialRomUri = initialUri,
                        initialName = initialName,
                        onDismiss = { shortcutTarget = null },
                        onCreateShortcut = { romUri, name, iconUri ->
                            val ok = ShortcutHelper.createGameShortcut(
                                context = this,
                                romUri = romUri,
                                shortcutName = name,
                                iconUri = iconUri
                            )
                            Toast.makeText(
                                this,
                                if (ok) "Home screen link requested for $name"
                                else "Home screen shortcuts are not supported by this launcher",
                                Toast.LENGTH_SHORT
                            ).show()
                            shortcutTarget = null
                        }
                    )
                }
            }
        }
    }

    private fun launchEmulator(uri: Uri) {
        val intent = Intent(this, EmulatorActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            data = uri
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
        }
        startActivity(intent)
    }

    private fun loadRecentGames(): List<RecentGame> {
        val prefs = getSharedPreferences("gbdesk_recent", Context.MODE_PRIVATE)
        val raw = prefs.getStringSet("uris", emptySet()) ?: emptySet()
        return raw.mapNotNull { entry ->
            val parts = entry.split("|", limit = 2)
            if (parts.size == 2) {
                RecentGame(uri = Uri.parse(parts[0]), title = parts[1])
            } else {
                null
            }
        }.sortedBy { it.title }
    }

    private fun saveRecentGame(uri: Uri, title: String): List<RecentGame> {
        val prefs = getSharedPreferences("gbdesk_recent", Context.MODE_PRIVATE)
        val existing = (prefs.getStringSet("uris", emptySet()) ?: emptySet()).toMutableSet()
        existing.removeAll { it.startsWith("$uri|") }
        existing.add("$uri|$title")
        prefs.edit().putStringSet("uris", existing).apply()
        return loadRecentGames()
    }
}

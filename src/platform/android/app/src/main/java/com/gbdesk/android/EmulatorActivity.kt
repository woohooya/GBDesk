package com.gbdesk.android

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.KeyEvent
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
import com.gbdesk.android.native.CoreBridge
import com.gbdesk.android.native.SaveSlotInfo
import com.gbdesk.android.shortcuts.ShortcutHelper
import com.gbdesk.android.ui.EmulatorScreen
import com.gbdesk.android.ui.SessionPromptDialog
import com.gbdesk.android.ui.ShortcutDialog
import java.io.File
import java.security.MessageDigest

class EmulatorActivity : ComponentActivity() {

    private var currentRomUri: Uri? = null
    private var gameTitle: String = "Game"
    private var hardwareKeyMask: Int = 0
    private var touchKeyMask: Int = 0

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val romUri = intent?.data
        if (romUri == null) {
            Toast.makeText(this, "No ROM provided", Toast.LENGTH_SHORT).show()
            finish()
            return
        }

        currentRomUri = romUri
        gameTitle = ShortcutHelper.resolveDisplayName(this, romUri)

        if (!loadRom(romUri)) {
            Toast.makeText(this, "Failed to load ROM", Toast.LENGTH_LONG).show()
            finish()
            return
        }

        val initialSlots: List<SaveSlotInfo> = CoreBridge.getSaveSlots()

        setContent {
            MaterialTheme {
                var showSessionPrompt by remember { mutableStateOf(true) }
                var showShortcutDialog by remember { mutableStateOf(false) }

                val stateFilePicker = rememberLauncherForActivityResult(
                    contract = ActivityResultContracts.OpenDocument()
                ) { stateUri ->
                    if (stateUri != null) {
                        contentResolver.openFileDescriptor(stateUri, "r")?.use { pfd ->
                            CoreBridge.nativeLoadStateFd(pfd.detachFd())
                        }
                    }
                    showSessionPrompt = false
                    CoreBridge.nativeSetPaused(false)
                }

                EmulatorScreen(
                    gameTitle = gameTitle,
                    onSurfaceChanged = { surface -> CoreBridge.nativeSetSurface(surface) },
                    onKeysChanged = { mask ->
                        touchKeyMask = mask
                        CoreBridge.nativeSetKeys(touchKeyMask or hardwareKeyMask)
                    },
                    onSaveSlot = { slot ->
                        val ok = CoreBridge.nativeSaveStateSlot(slot)
                        Toast.makeText(
                            this,
                            if (ok) "Saved to slot $slot" else "Failed to save slot $slot",
                            Toast.LENGTH_SHORT
                        ).show()
                    },
                    onLoadSlot = { slot ->
                        val ok = CoreBridge.nativeLoadStateSlot(slot)
                        if (!ok) {
                            Toast.makeText(this, "Slot $slot is empty", Toast.LENGTH_SHORT).show()
                        }
                    },
                    onCreateShortcut = {
                        CoreBridge.nativeSetPaused(true)
                        showShortcutDialog = true
                    },
                    onReset = { CoreBridge.nativeReset() },
                    onExit = { finish() }
                )

                if (showSessionPrompt) {
                    SessionPromptDialog(
                        gameTitle = gameTitle,
                        slots = initialSlots,
                        onLoadSlot = { slot ->
                            CoreBridge.nativeLoadStateSlot(slot)
                            showSessionPrompt = false
                            CoreBridge.nativeSetPaused(false)
                        },
                        onLoadStateFile = {
                            stateFilePicker.launch(arrayOf("*/*"))
                        },
                        onStartNewSession = {
                            showSessionPrompt = false
                            CoreBridge.nativeSetPaused(false)
                        }
                    )
                }

                if (showShortcutDialog) {
                    ShortcutDialog(
                        initialRomUri = currentRomUri,
                        initialName = gameTitle,
                        onDismiss = {
                            showShortcutDialog = false
                            CoreBridge.nativeSetPaused(false)
                        },
                        onCreateShortcut = { uri, name, iconUri ->
                            val ok = ShortcutHelper.createGameShortcut(this, uri, name, iconUri)
                            Toast.makeText(
                                this,
                                if (ok) "Home screen link requested for $name"
                                else "Home screen shortcuts are not supported by this launcher",
                                Toast.LENGTH_SHORT
                            ).show()
                            showShortcutDialog = false
                            CoreBridge.nativeSetPaused(false)
                        }
                    )
                }
            }
        }
    }

    private fun loadRom(uri: Uri): Boolean {
        val saveDir = File(filesDir, "saves").apply { mkdirs() }
        val baseHash = MessageDigest.getInstance("SHA-1")
            .digest(uri.toString().toByteArray())
            .joinToString("") { "%02x".format(it) }
            .take(16)

        return try {
            val pfd = contentResolver.openFileDescriptor(uri, "r") ?: return false
            CoreBridge.nativeLoadRomFd(pfd.detachFd(), baseHash, saveDir.absolutePath)
        } catch (_: Exception) {
            false
        }
    }

    override fun onPause() {
        super.onPause()
        CoreBridge.nativeSetPaused(true)
    }

    override fun onDestroy() {
        CoreBridge.nativeStop()
        super.onDestroy()
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        val bit = mapKeyCode(keyCode)
        if (bit != 0) {
            hardwareKeyMask = hardwareKeyMask or bit
            CoreBridge.nativeSetKeys(touchKeyMask or hardwareKeyMask)
            return true
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        val bit = mapKeyCode(keyCode)
        if (bit != 0) {
            hardwareKeyMask = hardwareKeyMask and bit.inv()
            CoreBridge.nativeSetKeys(touchKeyMask or hardwareKeyMask)
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    private fun mapKeyCode(keyCode: Int): Int {
        return when (keyCode) {
            KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_Z -> CoreBridge.KEY_A
            KeyEvent.KEYCODE_BUTTON_B, KeyEvent.KEYCODE_X -> CoreBridge.KEY_B
            KeyEvent.KEYCODE_BUTTON_SELECT, KeyEvent.KEYCODE_BACK -> CoreBridge.KEY_SELECT
            KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ENTER -> CoreBridge.KEY_START
            KeyEvent.KEYCODE_DPAD_RIGHT -> CoreBridge.KEY_RIGHT
            KeyEvent.KEYCODE_DPAD_LEFT -> CoreBridge.KEY_LEFT
            KeyEvent.KEYCODE_DPAD_UP -> CoreBridge.KEY_UP
            KeyEvent.KEYCODE_DPAD_DOWN -> CoreBridge.KEY_DOWN
            KeyEvent.KEYCODE_BUTTON_R1, KeyEvent.KEYCODE_S -> CoreBridge.KEY_R
            KeyEvent.KEYCODE_BUTTON_L1, KeyEvent.KEYCODE_A -> CoreBridge.KEY_L
            else -> 0
        }
    }
}

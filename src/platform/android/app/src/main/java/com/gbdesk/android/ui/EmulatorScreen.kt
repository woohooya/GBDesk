package com.gbdesk.android.ui

import android.view.MotionEvent
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.Menu
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.DropdownMenu
import androidx.compose.material3.DropdownMenuItem
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.ExperimentalComposeUiApi
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInteropFilter
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import com.gbdesk.android.native.CoreBridge

@Composable
fun EmulatorScreen(
    gameTitle: String,
    onSurfaceChanged: (android.view.Surface?) -> Unit,
    onKeysChanged: (Int) -> Unit,
    onSaveSlot: (Int) -> Unit,
    onLoadSlot: (Int) -> Unit,
    onCreateShortcut: () -> Unit,
    onReset: () -> Unit,
    onExit: () -> Unit
) {
    var keyMask by remember { mutableIntStateOf(0) }
    var menuExpanded by remember { mutableStateOf(false) }
    var slotDialogMode by remember { mutableStateOf<String?>(null) } // "save" or "load"

    fun setKey(bit: Int, pressed: Boolean) {
        keyMask = if (pressed) keyMask or bit else keyMask and bit.inv()
        onKeysChanged(keyMask)
    }

    Box(
        modifier = Modifier
            .fillMaxSize()
            .background(Color.Black)
    ) {
        AndroidView(
            factory = { ctx ->
                SurfaceView(ctx).apply {
                    holder.addCallback(object : SurfaceHolder.Callback {
                        override fun surfaceCreated(holder: SurfaceHolder) {
                            onSurfaceChanged(holder.surface)
                        }

                        override fun surfaceChanged(
                            holder: SurfaceHolder,
                            format: Int,
                            width: Int,
                            height: Int
                        ) {
                            onSurfaceChanged(holder.surface)
                        }

                        override fun surfaceDestroyed(holder: SurfaceHolder) {
                            onSurfaceChanged(null)
                        }
                    })
                }
            },
            modifier = Modifier.fillMaxSize()
        )

        // Top-right menu button
        Box(
            modifier = Modifier
                .align(Alignment.TopEnd)
                .padding(12.dp)
        ) {
            IconButton(
                onClick = { menuExpanded = true },
                modifier = Modifier.background(Color.Black.copy(alpha = 0.5f), CircleShape)
            ) {
                Icon(Icons.Default.Menu, contentDescription = "Menu", tint = Color.White)
            }
            DropdownMenu(
                expanded = menuExpanded,
                onDismissRequest = { menuExpanded = false }
            ) {
                DropdownMenuItem(
                    text = { Text("Save State") },
                    onClick = {
                        menuExpanded = false
                        slotDialogMode = "save"
                    }
                )
                DropdownMenuItem(
                    text = { Text("Load State") },
                    onClick = {
                        menuExpanded = false
                        slotDialogMode = "load"
                    }
                )
                DropdownMenuItem(
                    text = { Text("Create Home Screen Link...") },
                    onClick = {
                        menuExpanded = false
                        onCreateShortcut()
                    }
                )
                DropdownMenuItem(
                    text = { Text("Reset") },
                    onClick = {
                        menuExpanded = false
                        onReset()
                    }
                )
                DropdownMenuItem(
                    text = { Text("Exit Game") },
                    onClick = {
                        menuExpanded = false
                        onExit()
                    }
                )
            }
        }

        // Top L / R shoulder buttons
        Row(
            horizontalArrangement = Arrangement.SpaceBetween,
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.TopCenter)
                .padding(horizontal = 16.dp, vertical = 12.dp)
        ) {
            TouchKeyButton("L", Modifier.size(64.dp, 36.dp)) { setKey(CoreBridge.KEY_L, it) }
            Spacer(modifier = Modifier.weight(1f))
            TouchKeyButton("R", Modifier.size(64.dp, 36.dp)) { setKey(CoreBridge.KEY_R, it) }
            Spacer(modifier = Modifier.size(56.dp))
        }

        // Bottom D-Pad, Start/Select, and A/B controls
        Row(
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.Bottom,
            modifier = Modifier
                .fillMaxWidth()
                .align(Alignment.BottomCenter)
                .padding(16.dp)
        ) {
            // D-Pad
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                TouchKeyButton("▲", Modifier.size(48.dp)) { setKey(CoreBridge.KEY_UP, it) }
                Row(horizontalArrangement = Arrangement.spacedBy(48.dp)) {
                    TouchKeyButton("◀", Modifier.size(48.dp)) { setKey(CoreBridge.KEY_LEFT, it) }
                    TouchKeyButton("▶", Modifier.size(48.dp)) { setKey(CoreBridge.KEY_RIGHT, it) }
                }
                TouchKeyButton("▼", Modifier.size(48.dp)) { setKey(CoreBridge.KEY_DOWN, it) }
            }

            // Select / Start
            Row(
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.padding(bottom = 8.dp)
            ) {
                TouchKeyButton("SEL", Modifier.size(56.dp, 32.dp)) { setKey(CoreBridge.KEY_SELECT, it) }
                TouchKeyButton("STA", Modifier.size(56.dp, 32.dp)) { setKey(CoreBridge.KEY_START, it) }
            }

            // B / A
            Row(
                horizontalArrangement = Arrangement.spacedBy(16.dp),
                verticalAlignment = Alignment.CenterVertically,
                modifier = Modifier.padding(bottom = 16.dp)
            ) {
                TouchKeyButton("B", Modifier.size(58.dp), isCircle = true) { setKey(CoreBridge.KEY_B, it) }
                TouchKeyButton("A", Modifier.size(58.dp), isCircle = true) { setKey(CoreBridge.KEY_A, it) }
            }
        }
    }

    slotDialogMode?.let { mode ->
        AlertDialog(
            onDismissRequest = { slotDialogMode = null },
            title = { Text(if (mode == "save") "Save State ($gameTitle)" else "Load State ($gameTitle)") },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    for (row in 0 until 3) {
                        Row(
                            horizontalArrangement = Arrangement.spacedBy(8.dp),
                            modifier = Modifier.fillMaxWidth()
                        ) {
                            for (col in 1..3) {
                                val slot = row * 3 + col
                                TextButton(
                                    onClick = {
                                        if (mode == "save") onSaveSlot(slot) else onLoadSlot(slot)
                                        slotDialogMode = null
                                    },
                                    modifier = Modifier.weight(1f)
                                ) {
                                    Text("Slot $slot")
                                }
                            }
                        }
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = { slotDialogMode = null }) {
                    Text("Close")
                }
            }
        )
    }
}

@OptIn(ExperimentalComposeUiApi::class)
@Composable
private fun TouchKeyButton(
    label: String,
    modifier: Modifier = Modifier,
    isCircle: Boolean = false,
    onPressChanged: (Boolean) -> Unit
) {
    var pressed by remember { mutableStateOf(false) }
    Surface(
        shape = if (isCircle) CircleShape else RoundedCornerShape(8.dp),
        color = if (pressed) Color.White.copy(alpha = 0.45f) else Color.White.copy(alpha = 0.2f),
        modifier = modifier.pointerInteropFilter { event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN, MotionEvent.ACTION_POINTER_DOWN -> {
                    if (!pressed) {
                        pressed = true
                        onPressChanged(true)
                    }
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_POINTER_UP, MotionEvent.ACTION_CANCEL -> {
                    if (pressed) {
                        pressed = false
                        onPressChanged(false)
                    }
                    true
                }
                else -> true
            }
        }
    ) {
        Box(contentAlignment = Alignment.Center, modifier = Modifier.fillMaxSize()) {
            Text(
                text = label,
                color = Color.White,
                style = MaterialTheme.typography.labelLarge
            )
        }
    }
}

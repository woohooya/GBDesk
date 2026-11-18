package com.gbdesk.android.ui

import android.net.Uri
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.PickVisualMediaRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.Image
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import com.gbdesk.android.shortcuts.ShortcutHelper

@Composable
fun ShortcutDialog(
    initialRomUri: Uri? = null,
    initialName: String = "",
    onDismiss: () -> Unit,
    onCreateShortcut: (romUri: Uri, name: String, iconUri: Uri?) -> Unit
) {
    val context = LocalContext.current
    var romUri by remember { mutableStateOf(initialRomUri) }
    var shortcutName by remember { mutableStateOf(initialName) }
    var iconUri by remember { mutableStateOf<Uri?>(null) }

    val romPicker = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.OpenDocument()
    ) { uri ->
        if (uri != null) {
            romUri = uri
            if (shortcutName.isBlank()) {
                shortcutName = ShortcutHelper.resolveDisplayName(context, uri)
            }
        }
    }

    val iconPicker = rememberLauncherForActivityResult(
        contract = ActivityResultContracts.PickVisualMedia()
    ) { uri ->
        if (uri != null) {
            iconUri = uri
        }
    }

    val previewBitmap = remember(iconUri) {
        ShortcutHelper.decodeScaledIcon(context, iconUri, 192)
    }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text("Create Home Screen Link") },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(12.dp)) {
                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(8.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Text(
                        text = romUri?.let { ShortcutHelper.resolveDisplayName(context, it) } ?: "No ROM selected",
                        style = MaterialTheme.typography.bodyMedium,
                        modifier = Modifier.weight(1f)
                    )
                    OutlinedButton(onClick = { romPicker.launch(arrayOf("*/*")) }) {
                        Text("Select ROM")
                    }
                }

                OutlinedTextField(
                    value = shortcutName,
                    onValueChange = { shortcutName = it },
                    label = { Text("Shortcut Name") },
                    singleLine = true,
                    modifier = Modifier.fillMaxWidth()
                )

                Row(
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.spacedBy(12.dp),
                    modifier = Modifier.fillMaxWidth()
                ) {
                    Box(
                        contentAlignment = Alignment.Center,
                        modifier = Modifier
                            .size(56.dp)
                            .clip(RoundedCornerShape(12.dp))
                            .border(1.dp, MaterialTheme.colorScheme.outline, RoundedCornerShape(12.dp))
                    ) {
                        if (previewBitmap != null) {
                            Image(
                                bitmap = previewBitmap.asImageBitmap(),
                                contentDescription = "Selected icon",
                                contentScale = ContentScale.Crop,
                                modifier = Modifier.size(56.dp)
                            )
                        } else {
                            Text("Default", style = MaterialTheme.typography.labelSmall)
                        }
                    }

                    OutlinedButton(
                        onClick = {
                            iconPicker.launch(
                                PickVisualMediaRequest(ActivityResultContracts.PickVisualMedia.ImageOnly)
                            )
                        },
                        modifier = Modifier.weight(1f)
                    ) {
                        Text("Select Icon Image")
                    }
                }
            }
        },
        confirmButton = {
            Button(
                onClick = {
                    val targetRom = romUri ?: return@Button
                    onCreateShortcut(targetRom, shortcutName.trim(), iconUri)
                },
                enabled = romUri != null && shortcutName.isNotBlank()
            ) {
                Text("Create Link")
            }
        },
        dismissButton = {
            TextButton(onClick = onDismiss) {
                Text("Cancel")
            }
        }
    )
}

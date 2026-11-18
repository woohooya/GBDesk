package com.gbdesk.android.ui

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.foundation.lazy.grid.items
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import com.gbdesk.android.native.SaveSlotInfo
import java.text.DateFormat
import java.util.Date

@Composable
fun SessionPromptDialog(
    gameTitle: String,
    slots: List<SaveSlotInfo>,
    onLoadSlot: (Int) -> Unit,
    onLoadStateFile: () -> Unit,
    onStartNewSession: () -> Unit
) {
    var showSlotPicker by remember { mutableStateOf(false) }

    if (!showSlotPicker) {
        AlertDialog(
            onDismissRequest = onStartNewSession,
            title = { Text(text = gameTitle.ifBlank { "Start Session" }) },
            text = {
                Text("Would you like to load from a save state or start a new session?")
            },
            confirmButton = {
                Button(onClick = onStartNewSession) {
                    Text("Start New Session")
                }
            },
            dismissButton = {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = onLoadStateFile) {
                        Text("Load File...")
                    }
                    FilledTonalButton(onClick = { showSlotPicker = true }) {
                        Text("Load Save State")
                    }
                }
            }
        )
    } else {
        AlertDialog(
            onDismissRequest = { showSlotPicker = false },
            title = { Text("Select Save State Slot") },
            text = {
                Column {
                    LazyVerticalGrid(
                        columns = GridCells.Fixed(3),
                        horizontalArrangement = Arrangement.spacedBy(8.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                        modifier = Modifier
                            .fillMaxWidth()
                            .height(220.dp)
                    ) {
                        items(slots) { info ->
                            FilledTonalButton(
                                onClick = { onLoadSlot(info.slot) },
                                enabled = info.exists
                            ) {
                                Column {
                                    Text(
                                        text = "Slot ${info.slot}",
                                        style = MaterialTheme.typography.labelLarge
                                    )
                                    val sub = if (!info.exists) {
                                        "Empty"
                                    } else if (info.timestampMs > 0L) {
                                        DateFormat.getDateTimeInstance(DateFormat.SHORT, DateFormat.SHORT)
                                            .format(Date(info.timestampMs))
                                    } else {
                                        "Saved"
                                    }
                                    Text(
                                        text = sub,
                                        style = MaterialTheme.typography.labelSmall,
                                        maxLines = 1
                                    )
                                }
                            }
                        }
                    }
                    Spacer(modifier = Modifier.height(8.dp))
                    OutlinedButton(
                        onClick = onLoadStateFile,
                        modifier = Modifier.fillMaxWidth()
                    ) {
                        Text("Browse Save State File...")
                    }
                }
            },
            confirmButton = {
                TextButton(onClick = onStartNewSession) {
                    Text("Start New Session")
                }
            },
            dismissButton = {
                TextButton(onClick = { showSlotPicker = false }) {
                    Text("Back")
                }
            }
        )
    }
}

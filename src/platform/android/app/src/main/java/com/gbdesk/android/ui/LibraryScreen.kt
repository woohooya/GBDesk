package com.gbdesk.android.ui

import android.net.Uri
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.AddLink
import androidx.compose.material.icons.filled.FolderOpen
import androidx.compose.material.icons.filled.PlayArrow
import androidx.compose.material3.Button
import androidx.compose.material3.Card
import androidx.compose.material3.ExperimentalMaterial3Api
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.TopAppBar
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp

data class RecentGame(
    val uri: Uri,
    val title: String
)

@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun LibraryScreen(
    recentGames: List<RecentGame>,
    onPickRom: () -> Unit,
    onLaunchGame: (Uri) -> Unit,
    onCreateShortcut: (Uri?, String) -> Unit
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("GBDesk") },
                actions = {
                    IconButton(onClick = { onCreateShortcut(null, "") }) {
                        Icon(Icons.Default.AddLink, contentDescription = "Create Home Screen Link")
                    }
                }
            )
        }
    ) { padding ->
        Column(
            modifier = Modifier
                .fillMaxSize()
                .padding(padding)
                .padding(16.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp)
        ) {
            Row(
                horizontalArrangement = Arrangement.spacedBy(12.dp),
                modifier = Modifier.fillMaxWidth()
            ) {
                Button(
                    onClick = onPickRom,
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(Icons.Default.FolderOpen, contentDescription = null)
                    Text("  Load ROM", modifier = Modifier.padding(start = 4.dp))
                }
                OutlinedButton(
                    onClick = { onCreateShortcut(null, "") },
                    modifier = Modifier.weight(1f)
                ) {
                    Icon(Icons.Default.AddLink, contentDescription = null)
                    Text("  Add Home Link", modifier = Modifier.padding(start = 4.dp))
                }
            }

            Spacer(modifier = Modifier.height(4.dp))
            Text(
                text = "Recent Games",
                style = MaterialTheme.typography.titleMedium
            )

            if (recentGames.isEmpty()) {
                Text(
                    text = "No recent games yet. Tap Load ROM to start playing or Add Home Link to pin a game to your home screen.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            } else {
                LazyColumn(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    items(recentGames, key = { it.uri.toString() }) { game ->
                        Card(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clickable { onLaunchGame(game.uri) }
                        ) {
                            Row(
                                verticalAlignment = Alignment.CenterVertically,
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 16.dp, vertical = 12.dp)
                            ) {
                                Text(
                                    text = game.title,
                                    style = MaterialTheme.typography.bodyLarge,
                                    modifier = Modifier.weight(1f)
                                )
                                IconButton(onClick = { onCreateShortcut(game.uri, game.title) }) {
                                    Icon(Icons.Default.AddLink, contentDescription = "Create Home Screen Link")
                                }
                                IconButton(onClick = { onLaunchGame(game.uri) }) {
                                    Icon(Icons.Default.PlayArrow, contentDescription = "Play")
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

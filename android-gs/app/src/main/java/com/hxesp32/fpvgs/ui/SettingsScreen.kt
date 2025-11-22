package com.hxesp32.fpvgs.ui

import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.hxesp32.fpvgs.osd.OsdConfig

/**
 * Settings screen for configuring OSD and application settings
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun SettingsScreen(
    osdConfig: OsdConfig,
    onOsdConfigChange: (OsdConfig) -> Unit,
    onBackClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Settings") },
                navigationIcon = {
                    IconButton(onClick = onBackClick) {
                        Icon(Icons.Default.ArrowBack, contentDescription = "Back")
                    }
                }
            )
        }
    ) { paddingValues ->
        LazyColumn(
            modifier = modifier
                .fillMaxSize()
                .padding(paddingValues),
            contentPadding = PaddingValues(16.dp),
            verticalArrangement = Arrangement.spacedBy(8.dp)
        ) {
            // OSD Settings Section
            item {
                SettingsSectionHeader("OSD Display")
            }

            item {
                SwitchSettingItem(
                    title = "Enable OSD",
                    description = "Show on-screen display overlay",
                    checked = osdConfig.enabled,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(enabled = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "RSSI Indicator",
                    description = "Show signal strength",
                    checked = osdConfig.showRssi,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showRssi = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Link Quality",
                    description = "Show link quality and packet loss",
                    checked = osdConfig.showLinkQuality,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showLinkQuality = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Video Stats",
                    description = "Show FPS and bitrate",
                    checked = osdConfig.showVideoStats,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showVideoStats = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Latency",
                    description = "Show video latency",
                    checked = osdConfig.showLatency,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showLatency = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Battery",
                    description = "Show battery voltage and status",
                    checked = osdConfig.showBattery,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showBattery = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "GPS",
                    description = "Show GPS coordinates",
                    checked = osdConfig.showGps,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showGps = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Recording Indicator",
                    description = "Show recording status",
                    checked = osdConfig.showRecording,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showRecording = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Artificial Horizon",
                    description = "Show flight attitude indicator",
                    checked = osdConfig.showArtificialHorizon,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showArtificialHorizon = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Crosshair",
                    description = "Show center crosshair",
                    checked = osdConfig.showCrosshair,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showCrosshair = it)) }
                )
            }

            item {
                SwitchSettingItem(
                    title = "Detailed Stats",
                    description = "Show additional statistics",
                    checked = osdConfig.showDetailedStats,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showDetailedStats = it)) }
                )
            }

            // Appearance Section
            item {
                Spacer(modifier = Modifier.height(16.dp))
                SettingsSectionHeader("Appearance")
            }

            item {
                SliderSettingItem(
                    title = "Font Size",
                    description = "Adjust OSD text size",
                    value = osdConfig.fontSize,
                    valueRange = 16f..48f,
                    onValueChange = { onOsdConfigChange(osdConfig.copy(fontSize = it)) },
                    valueFormatter = { "%.0f px".format(it) }
                )
            }

            item {
                SliderSettingItem(
                    title = "Transparency",
                    description = "Adjust OSD transparency",
                    value = osdConfig.transparency,
                    valueRange = 0.3f..1.0f,
                    onValueChange = { onOsdConfigChange(osdConfig.copy(transparency = it)) },
                    valueFormatter = { "%.0f%%".format(it * 100) }
                )
            }

            // Custom Text Section
            item {
                Spacer(modifier = Modifier.height(16.dp))
                SettingsSectionHeader("Custom Text")
            }

            item {
                SwitchSettingItem(
                    title = "Show Custom Text",
                    description = "Display custom text overlay",
                    checked = osdConfig.showCustomText,
                    onCheckedChange = { onOsdConfigChange(osdConfig.copy(showCustomText = it)) }
                )
            }

            item {
                var text by remember { mutableStateOf(osdConfig.customText) }

                OutlinedTextField(
                    value = text,
                    onValueChange = {
                        text = it
                        onOsdConfigChange(osdConfig.copy(customText = it))
                    },
                    label = { Text("Custom Text") },
                    placeholder = { Text("Enter custom text...") },
                    modifier = Modifier.fillMaxWidth(),
                    maxLines = 3
                )
            }

            // Video Settings Section
            item {
                Spacer(modifier = Modifier.height(16.dp))
                SettingsSectionHeader("Video")
            }

            item {
                SettingItem(
                    title = "Scale Mode",
                    description = "How video is displayed on screen"
                ) {
                    // Scale mode selector would go here
                    Text("Fit", style = MaterialTheme.typography.bodyMedium)
                }
            }

            item {
                SettingItem(
                    title = "Rotation",
                    description = "Rotate video display"
                ) {
                    Text("0°", style = MaterialTheme.typography.bodyMedium)
                }
            }

            // Connection Settings Section
            item {
                Spacer(modifier = Modifier.height(16.dp))
                SettingsSectionHeader("Connection")
            }

            item {
                SettingItem(
                    title = "WiFi Channel",
                    description = "WiFi channel for FPV link"
                ) {
                    Text("Ch 7", style = MaterialTheme.typography.bodyMedium)
                }
            }

            item {
                SettingItem(
                    title = "Data Rate",
                    description = "WiFi transmission rate"
                ) {
                    Text("36M ODFM", style = MaterialTheme.typography.bodyMedium)
                }
            }
        }
    }
}

@Composable
fun SettingsSectionHeader(
    title: String,
    modifier: Modifier = Modifier
) {
    Text(
        text = title,
        style = MaterialTheme.typography.titleMedium,
        fontWeight = FontWeight.Bold,
        color = MaterialTheme.colorScheme.primary,
        modifier = modifier.padding(vertical = 8.dp)
    )
}

@Composable
fun SwitchSettingItem(
    title: String,
    description: String,
    checked: Boolean,
    onCheckedChange: (Boolean) -> Unit,
    modifier: Modifier = Modifier
) {
    SettingItem(
        title = title,
        description = description,
        modifier = modifier
    ) {
        Switch(
            checked = checked,
            onCheckedChange = onCheckedChange
        )
    }
}

@Composable
fun SettingItem(
    title: String,
    description: String,
    modifier: Modifier = Modifier,
    action: @Composable () -> Unit = {}
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp),
        horizontalArrangement = Arrangement.SpaceBetween,
        verticalAlignment = Alignment.CenterVertically
    ) {
        Column(
            modifier = Modifier.weight(1f)
        ) {
            Text(
                text = title,
                style = MaterialTheme.typography.bodyLarge,
                fontWeight = FontWeight.Medium
            )
            Text(
                text = description,
                style = MaterialTheme.typography.bodySmall,
                color = MaterialTheme.colorScheme.onSurfaceVariant
            )
        }
        action()
    }
}

@Composable
fun SliderSettingItem(
    title: String,
    description: String,
    value: Float,
    valueRange: ClosedFloatingPointRange<Float>,
    onValueChange: (Float) -> Unit,
    valueFormatter: (Float) -> String,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 8.dp)
    ) {
        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column(modifier = Modifier.weight(1f)) {
                Text(
                    text = title,
                    style = MaterialTheme.typography.bodyLarge,
                    fontWeight = FontWeight.Medium
                )
                Text(
                    text = description,
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant
                )
            }
            Text(
                text = valueFormatter(value),
                style = MaterialTheme.typography.bodyMedium,
                fontWeight = FontWeight.Bold
            )
        }
        Slider(
            value = value,
            onValueChange = onValueChange,
            valueRange = valueRange,
            modifier = Modifier.fillMaxWidth()
        )
    }
}

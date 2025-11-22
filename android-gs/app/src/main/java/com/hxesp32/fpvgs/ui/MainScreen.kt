package com.hxesp32.fpvgs.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.compose.ui.viewinterop.AndroidView
import com.hxesp32.fpvgs.data.TelemetryState
import com.hxesp32.fpvgs.osd.OsdConfig
import com.hxesp32.fpvgs.osd.OsdOverlay
import com.hxesp32.fpvgs.video.VideoRenderer

/**
 * Main FPV screen with video and OSD overlay
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun MainScreen(
    telemetryState: TelemetryState,
    osdConfig: OsdConfig,
    isRecording: Boolean,
    onRecordingToggle: () -> Unit,
    onSettingsClick: () -> Unit,
    onStatsClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    var showControls by remember { mutableStateOf(false) }

    Scaffold(
        topBar = {
            if (showControls) {
                TopAppBar(
                    title = { Text("HX ESP32 FPV") },
                    colors = TopAppBarDefaults.topAppBarColors(
                        containerColor = Color.Black.copy(alpha = 0.7f),
                        titleContentColor = Color.White
                    ),
                    actions = {
                        // Connection status
                        ConnectionStatusIndicator(isConnected = telemetryState.isConnected)

                        Spacer(modifier = Modifier.width(8.dp))

                        // Settings button
                        IconButton(onClick = onSettingsClick) {
                            Icon(
                                Icons.Default.Settings,
                                contentDescription = "Settings",
                                tint = Color.White
                            )
                        }

                        // Stats button
                        IconButton(onClick = onStatsClick) {
                            Icon(
                                Icons.Default.Info,
                                contentDescription = "Statistics",
                                tint = Color.White
                            )
                        }
                    }
                )
            }
        },
        floatingActionButton = {
            if (showControls) {
                Column(
                    horizontalAlignment = Alignment.End,
                    verticalArrangement = Arrangement.spacedBy(8.dp)
                ) {
                    // Recording button
                    FloatingActionButton(
                        onClick = onRecordingToggle,
                        containerColor = if (isRecording) Color.Red else MaterialTheme.colorScheme.primary,
                        modifier = Modifier.size(56.dp)
                    ) {
                        Icon(
                            if (isRecording) Icons.Default.Stop else Icons.Default.FiberManualRecord,
                            contentDescription = if (isRecording) "Stop Recording" else "Start Recording",
                            tint = Color.White
                        )
                    }
                }
            }
        }
    ) { paddingValues ->
        Box(
            modifier = modifier
                .fillMaxSize()
                .padding(paddingValues)
        ) {
            // Video layer
            VideoRendererView(
                modifier = Modifier.fillMaxSize()
            )

            // OSD overlay layer
            OsdOverlayView(
                telemetryState = telemetryState,
                osdConfig = osdConfig,
                modifier = Modifier.fillMaxSize()
            )

            // Control toggle button (tap anywhere to show/hide)
            Box(
                modifier = Modifier
                    .fillMaxSize()
                    .systemBarsPadding(),
                contentAlignment = Alignment.TopEnd
            ) {
                IconButton(
                    onClick = { showControls = !showControls },
                    modifier = Modifier.padding(8.dp)
                ) {
                    Icon(
                        if (showControls) Icons.Default.VisibilityOff else Icons.Default.Visibility,
                        contentDescription = "Toggle Controls",
                        tint = Color.White.copy(alpha = 0.5f)
                    )
                }
            }
        }
    }
}

/**
 * Connection status indicator
 */
@Composable
fun ConnectionStatusIndicator(
    isConnected: Boolean,
    modifier: Modifier = Modifier
) {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(4.dp),
        modifier = modifier.padding(horizontal = 8.dp)
    ) {
        Box(
            modifier = Modifier
                .size(12.dp)
                .clip(CircleShape)
                .background(if (isConnected) Color.Green else Color.Red)
        )
        Text(
            text = if (isConnected) "Connected" else "Disconnected",
            color = Color.White,
            fontSize = 12.sp
        )
    }
}

/**
 * Video renderer view wrapper for Compose
 */
@Composable
fun VideoRendererView(
    modifier: Modifier = Modifier
) {
    AndroidView(
        factory = { context ->
            VideoRenderer(context).apply {
                setScaleMode(VideoRenderer.ScaleMode.FIT)
            }
        },
        modifier = modifier
    )
}

/**
 * OSD overlay view wrapper for Compose
 */
@Composable
fun OsdOverlayView(
    telemetryState: TelemetryState,
    osdConfig: OsdConfig,
    modifier: Modifier = Modifier
) {
    AndroidView(
        factory = { context ->
            OsdOverlay(context).apply {
                updateConfig(osdConfig)
            }
        },
        update = { view ->
            view.updateTelemetry(telemetryState)
            view.updateConfig(osdConfig)
        },
        modifier = modifier
    )
}

/**
 * Quick stats overlay (compact version)
 */
@Composable
fun QuickStatsOverlay(
    telemetryState: TelemetryState,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .padding(16.dp)
            .clip(RoundedCornerShape(8.dp))
            .background(Color.Black.copy(alpha = 0.7f))
            .padding(8.dp),
        verticalArrangement = Arrangement.spacedBy(4.dp)
    ) {
        // RSSI
        StatRow(
            label = "RSSI",
            value = "-${telemetryState.airStats.rssiDbm} dBm",
            color = getRssiColor(telemetryState.airStats.rssiDbm)
        )

        // FPS
        StatRow(
            label = "FPS",
            value = "${telemetryState.videoStats.fps}",
            color = Color.White
        )

        // Latency
        StatRow(
            label = "Latency",
            value = "${telemetryState.latencyMs} ms",
            color = getLatencyColor(telemetryState.latencyMs)
        )

        // Link Quality
        val linkQuality = telemetryState.calculateLinkQuality()
        StatRow(
            label = "Link",
            value = "%.0f%%".format(linkQuality),
            color = getLinkQualityColor(linkQuality)
        )
    }
}

@Composable
private fun StatRow(
    label: String,
    value: String,
    color: Color,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = label,
            color = Color.Gray,
            fontSize = 12.sp,
            fontWeight = FontWeight.Medium
        )
        Text(
            text = value,
            color = color,
            fontSize = 12.sp,
            fontWeight = FontWeight.Bold
        )
    }
}

// Helper functions for color coding
private fun getRssiColor(rssi: Int): Color = when {
    rssi < 30 -> Color.Green
    rssi < 60 -> Color.Yellow
    rssi < 80 -> Color(0xFFFFA500) // Orange
    else -> Color.Red
}

private fun getLatencyColor(latency: Int): Color = when {
    latency < 100 -> Color.Green
    latency < 150 -> Color.Yellow
    else -> Color.Red
}

private fun getLinkQualityColor(quality: Float): Color = when {
    quality > 80 -> Color.Green
    quality > 60 -> Color.Yellow
    quality > 40 -> Color(0xFFFFA500)
    else -> Color.Red
}

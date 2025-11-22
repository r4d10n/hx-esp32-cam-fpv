package com.hxesp32.fpvgs.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.ArrowBack
import androidx.compose.material.icons.filled.SignalCellularAlt
import androidx.compose.material.icons.filled.Videocam
import androidx.compose.material.icons.filled.Wifi
import androidx.compose.material3.*
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.hxesp32.fpvgs.data.TelemetryState

/**
 * Statistics screen showing detailed telemetry information
 */
@OptIn(ExperimentalMaterial3Api::class)
@Composable
fun StatisticsScreen(
    telemetryState: TelemetryState,
    onBackClick: () -> Unit,
    modifier: Modifier = Modifier
) {
    Scaffold(
        topBar = {
            TopAppBar(
                title = { Text("Statistics") },
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
            verticalArrangement = Arrangement.spacedBy(16.dp)
        ) {
            // Connection Status Card
            item {
                StatisticsCard(
                    title = "Connection",
                    icon = Icons.Default.Wifi
                ) {
                    StatRow("Status", if (telemetryState.isConnected) "Connected" else "Disconnected")
                    StatRow("Link Quality", "%.0f%%".format(telemetryState.calculateLinkQuality()))
                    StatRow("WiFi Channel", "Ch ${telemetryState.airStats.wifiChannel}")
                    StatRow("WiFi Rate", telemetryState.airStats.currentWifiRate.displayName)
                    StatRow("FEC Codec", "${telemetryState.airStats.fecCodecK}/12")
                }
            }

            // Signal Quality Card
            item {
                StatisticsCard(
                    title = "Signal Quality",
                    icon = Icons.Default.SignalCellularAlt
                ) {
                    StatRow("Air RSSI", "-${telemetryState.airStats.rssiDbm} dBm")
                    StatRow("Air Noise Floor", "-${telemetryState.airStats.noiseFloorDbm} dBm")
                    StatRow("Air SNR", "${telemetryState.airStats.snr} dB")
                    Divider(modifier = Modifier.padding(vertical = 8.dp))
                    StatRow("GS RSSI 1", "${telemetryState.groundStats.rssiDbm[0]} dBm")
                    StatRow("GS RSSI 2", "${telemetryState.groundStats.rssiDbm[1]} dBm")
                    StatRow("GS Noise Floor", "${telemetryState.groundStats.noiseFloorDbm} dBm")
                    StatRow("GS SNR", "${telemetryState.groundStats.snr} dB")
                }
            }

            // Video Statistics Card
            item {
                StatisticsCard(
                    title = "Video",
                    icon = Icons.Default.Videocam
                ) {
                    StatRow("Resolution", telemetryState.airStats.resolution.displayName)
                    StatRow("Capture FPS", "${telemetryState.airStats.captureFPS}")
                    StatRow("Display FPS", "${telemetryState.videoStats.fps}")
                    StatRow("Bitrate", "%.2f Mbps".format(telemetryState.videoStats.bitrateMbps))
                    StatRow("Frame Size", "${telemetryState.airStats.cameraFrameSizeMin}-${telemetryState.airStats.cameraFrameSizeMax} bytes")
                    StatRow("Frames Decoded", "${telemetryState.videoStats.framesDecoded}")
                    StatRow("Frames Dropped", "${telemetryState.videoStats.framesDropped}")
                    StatRow("Drop Rate", "%.2f%%".format(telemetryState.videoStats.dropRate))
                }
            }

            // Latency Card
            item {
                StatisticsCard(title = "Latency") {
                    StatRow("Current", "${telemetryState.latencyMs} ms")
                    StatRow("Min Ping", "${telemetryState.groundStats.pingMinMS} ms")
                    StatRow("Max Ping", "${telemetryState.groundStats.pingMaxMS} ms")
                    StatRow("Avg Frame Time", "%.1f ms".format(telemetryState.videoStats.averageFrameTime))
                }
            }

            // Packet Statistics Card
            item {
                StatisticsCard(title = "Packet Statistics") {
                    StatRow("Air Out Rate", "${telemetryState.airStats.outPacketRate} pkt/s")
                    StatRow("Air In Rate", "${telemetryState.airStats.inPacketRate} pkt/s")
                    StatRow("Air Rejected", "${telemetryState.airStats.inRejectedPacketRate} pkt/s")
                    Divider(modifier = Modifier.padding(vertical = 8.dp))
                    StatRow("GS Out Rate", "${telemetryState.groundStats.outPacketCounter} pkt/s")
                    StatRow("GS In Rate", "${telemetryState.groundStats.totalInPackets} pkt/s")
                    StatRow("GS Unique", "${telemetryState.groundStats.inUniquePacketCounter} pkt/s")
                    StatRow("GS Duplicated", "${telemetryState.groundStats.inDuplicatedPacketCounter} pkt/s")
                    StatRow("Packet Loss", "%.1f%%".format(telemetryState.groundStats.packetLoss))
                    StatRow("FEC Success", "%.1f%%".format(telemetryState.groundStats.fecSuccessRate))
                }
            }

            // Air Unit Status Card
            item {
                StatisticsCard(title = "Air Unit") {
                    StatRow("Camera", if (telemetryState.airStats.isOV5640) "OV5640" else "OV2640")
                    StatRow("Recording", if (telemetryState.airStats.airRecordState) "Yes" else "No")
                    StatRow("Temperature", if (telemetryState.airStats.temperature > 0) "${telemetryState.airStats.temperature}°C" else "N/A")
                    StatRow("Throttling", if (telemetryState.airStats.overheatThrottling) "Yes" else "No")
                    StatRow("Camera Overflow", "${telemetryState.airStats.cameraOverflowCount}")
                    StatRow("WiFi Queue", "${telemetryState.airStats.wifiQueueMin}-${telemetryState.airStats.wifiQueueMax}")
                    StatRow("WiFi Overflow", if (telemetryState.airStats.wifiOverflow) "Yes" else "No")
                }
            }

            // SD Card Status (if available)
            if (telemetryState.airStats.sdDetected) {
                item {
                    StatisticsCard(title = "SD Card") {
                        StatRow("Status", "Detected")
                        StatRow("Free Space", "%.2f GB".format(telemetryState.airStats.sdFreeSpaceGB))
                        StatRow("Total Space", "%.2f GB".format(telemetryState.airStats.sdTotalSpaceGB))
                        StatRow("Free %", "%.1f%%".format(telemetryState.airStats.sdFreeSpacePercent))
                        StatRow("Slow", if (telemetryState.airStats.sdSlow) "Yes" else "No")
                        StatRow("Error", if (telemetryState.airStats.sdError) "Yes" else "No")
                    }
                }
            }

            // Battery Status (if available)
            if (telemetryState.batteryData.available) {
                item {
                    StatisticsCard(title = "Battery") {
                        StatRow("Voltage", "%.2f V".format(telemetryState.batteryData.voltage))
                        StatRow("Current", "%.2f A".format(telemetryState.batteryData.current))
                        StatRow("Cells", "${telemetryState.batteryData.cells}S")
                        StatRow("Cell Voltage", "%.3f V".format(telemetryState.batteryData.cellVoltage))
                        StatRow("Percentage", "${telemetryState.batteryData.percentage}%")
                        StatRow("Status", if (telemetryState.batteryData.isLow) "LOW" else "OK")
                    }
                }
            }

            // GPS Status (if available)
            if (telemetryState.gpsData.fix) {
                item {
                    StatisticsCard(title = "GPS") {
                        StatRow("Fix", if (telemetryState.gpsData.fix) "Yes" else "No")
                        StatRow("Satellites", "${telemetryState.gpsData.satellites}")
                        StatRow("Latitude", "%.6f".format(telemetryState.gpsData.latitude))
                        StatRow("Longitude", "%.6f".format(telemetryState.gpsData.longitude))
                        StatRow("Altitude", "%.1f m".format(telemetryState.gpsData.altitude))
                        StatRow("Speed", "%.1f m/s".format(telemetryState.gpsData.speed))
                        StatRow("Heading", "%.0f°".format(telemetryState.gpsData.heading))
                    }
                }
            }

            // Telemetry (MAVLink)
            item {
                StatisticsCard(title = "Telemetry") {
                    StatRow("In Rate", "${telemetryState.airStats.inMavlinkRate} b/s")
                    StatRow("Out Rate", "${telemetryState.airStats.outMavlinkRate} b/s")
                    StatRow("RC Period", if (telemetryState.airStats.rcPeriodMax > 0) "${telemetryState.airStats.rcPeriodMax} ms" else "N/A")
                }
            }
        }
    }
}

@Composable
fun StatisticsCard(
    title: String,
    modifier: Modifier = Modifier,
    icon: ImageVector? = null,
    content: @Composable ColumnScope.() -> Unit
) {
    Card(
        modifier = modifier.fillMaxWidth(),
        elevation = CardDefaults.cardElevation(defaultElevation = 2.dp)
    ) {
        Column(
            modifier = Modifier.padding(16.dp)
        ) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(8.dp),
                modifier = Modifier.padding(bottom = 12.dp)
            ) {
                icon?.let {
                    Icon(
                        imageVector = it,
                        contentDescription = null,
                        tint = MaterialTheme.colorScheme.primary
                    )
                }
                Text(
                    text = title,
                    style = MaterialTheme.typography.titleMedium,
                    fontWeight = FontWeight.Bold
                )
            }
            content()
        }
    }
}

@Composable
fun StatRow(
    label: String,
    value: String,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier
            .fillMaxWidth()
            .padding(vertical = 4.dp),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = label,
            style = MaterialTheme.typography.bodyMedium,
            color = MaterialTheme.colorScheme.onSurfaceVariant
        )
        Text(
            text = value,
            style = MaterialTheme.typography.bodyMedium,
            fontWeight = FontWeight.Medium
        )
    }
}

/**
 * Compact statistics panel (for overlay on video)
 */
@Composable
fun CompactStatsPanel(
    telemetryState: TelemetryState,
    modifier: Modifier = Modifier
) {
    Column(
        modifier = modifier
            .clip(RoundedCornerShape(8.dp))
            .background(Color.Black.copy(alpha = 0.7f))
            .padding(12.dp),
        verticalArrangement = Arrangement.spacedBy(6.dp)
    ) {
        Text(
            text = "Statistics",
            fontWeight = FontWeight.Bold,
            fontSize = 14.sp,
            color = Color.White
        )

        Divider(color = Color.White.copy(alpha = 0.3f))

        CompactStatRow("RSSI", "-${telemetryState.airStats.rssiDbm} dBm")
        CompactStatRow("FPS", "${telemetryState.videoStats.fps}")
        CompactStatRow("Latency", "${telemetryState.latencyMs} ms")
        CompactStatRow("Quality", "%.0f%%".format(telemetryState.calculateLinkQuality()))
        CompactStatRow("Loss", "%.1f%%".format(telemetryState.groundStats.packetLoss))
    }
}

@Composable
fun CompactStatRow(
    label: String,
    value: String,
    modifier: Modifier = Modifier
) {
    Row(
        modifier = modifier.fillMaxWidth(),
        horizontalArrangement = Arrangement.SpaceBetween
    ) {
        Text(
            text = label,
            fontSize = 12.sp,
            color = Color.Gray
        )
        Text(
            text = value,
            fontSize = 12.sp,
            color = Color.White,
            fontWeight = FontWeight.Medium
        )
    }
}

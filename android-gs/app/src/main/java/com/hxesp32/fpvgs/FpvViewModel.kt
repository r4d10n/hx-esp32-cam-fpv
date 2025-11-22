package com.hxesp32.fpvgs

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hxesp32.fpvgs.data.*
import com.hxesp32.fpvgs.osd.OsdConfig
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch

/**
 * Main ViewModel for FPV ground station
 */
class FpvViewModel : ViewModel() {

    // Telemetry state
    private val _telemetryState = MutableStateFlow(TelemetryState())
    val telemetryState: StateFlow<TelemetryState> = _telemetryState.asStateFlow()

    // OSD configuration
    private val _osdConfig = MutableStateFlow(OsdConfig())
    val osdConfig: StateFlow<OsdConfig> = _osdConfig.asStateFlow()

    // Recording state
    private val _isRecording = MutableStateFlow(false)
    val isRecording: StateFlow<Boolean> = _isRecording.asStateFlow()

    init {
        // Initialize with demo data for testing
        startDemoMode()
    }

    /**
     * Update OSD configuration
     */
    fun updateOsdConfig(config: OsdConfig) {
        _osdConfig.value = config
    }

    /**
     * Toggle recording state
     */
    fun toggleRecording() {
        _isRecording.value = !_isRecording.value
    }

    /**
     * Update telemetry from received data
     */
    fun updateTelemetry(state: TelemetryState) {
        _telemetryState.value = state
    }

    /**
     * Demo mode with simulated data for testing UI
     */
    private fun startDemoMode() {
        viewModelScope.launch {
            kotlinx.coroutines.delay(1000)

            // Simulate telemetry data
            val demoState = TelemetryState(
                airStats = AirStats(
                    rssiDbm = 45,
                    noiseFloorDbm = 90,
                    currentWifiRate = WifiRate.RATE_G_36M_ODFM,
                    wifiChannel = 7,
                    resolution = Resolution.HD,
                    captureFPS = 30,
                    outPacketRate = 850,
                    inPacketRate = 50,
                    temperature = 45,
                    sdDetected = true,
                    sdTotalSpaceGB = 32f,
                    sdFreeSpaceGB = 24.5f,
                    isOV5640 = true,
                    airRecordState = false,
                    currentQuality = 50,
                    cameraFrameSizeMin = 8000,
                    cameraFrameSizeMax = 15000
                ),
                groundStats = GroundStats(
                    rssiDbm = intArrayOf(-45, -50),
                    inPacketCounter = intArrayOf(450, 400),
                    outPacketCounter = 50,
                    pingMinMS = 85,
                    pingMaxMS = 120,
                    inUniquePacketCounter = 840,
                    inDuplicatedPacketCounter = 10,
                    fecSuccessRate = 98.5f
                ),
                videoStats = VideoStats(
                    fps = 30,
                    bitrate = 8_000_000,
                    framesDecoded = 1500,
                    framesDropped = 5,
                    averageFrameTime = 33.3f
                ),
                batteryData = BatteryData(
                    voltage = 12.6f,
                    current = 2.5f,
                    percentage = 75,
                    available = true
                ),
                gpsData = GpsData(
                    latitude = 37.7749,
                    longitude = -122.4194,
                    altitude = 150.5f,
                    speed = 5.2f,
                    heading = 180f,
                    satellites = 12,
                    fix = true
                ),
                imuData = ImuData(
                    pitch = 5f,
                    roll = -2f,
                    yaw = 180f,
                    available = true
                ),
                isConnected = true,
                latencyMs = 95
            )

            _telemetryState.value = demoState
        }
    }
}

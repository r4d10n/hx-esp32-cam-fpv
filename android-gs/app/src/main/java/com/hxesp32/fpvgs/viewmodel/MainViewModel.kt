package com.hxesp32.fpvgs.viewmodel

import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import com.hxesp32.fpvgs.data.model.*
import kotlinx.coroutines.flow.*
import kotlinx.coroutines.launch
import timber.log.Timber

/**
 * Main ViewModel following MVVM architecture
 * Coordinates between USB communication, video decoding, OSD, and recording
 */
class MainViewModel : ViewModel() {

    // State flows for UI
    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val _videoState = MutableStateFlow<VideoState>(VideoState.Idle)
    val videoState: StateFlow<VideoState> = _videoState.asStateFlow()

    private val _recordingState = MutableStateFlow<RecordingState>(RecordingState.Idle)
    val recordingState: StateFlow<RecordingState> = _recordingState.asStateFlow()

    private val _osdData = MutableStateFlow(OsdData())
    val osdData: StateFlow<OsdData> = _osdData.asStateFlow()

    private val _osdSettings = MutableStateFlow(OsdSettings())
    val osdSettings: StateFlow<OsdSettings> = _osdSettings.asStateFlow()

    private val _statistics = MutableStateFlow(VideoStatistics())
    val statistics: StateFlow<VideoStatistics> = _statistics.asStateFlow()

    /**
     * Update connection state from service
     */
    fun updateConnectionState(state: ConnectionState) {
        _connectionState.value = state
    }

    /**
     * Update video state from service
     */
    fun updateVideoState(state: VideoState) {
        _videoState.value = state
    }

    /**
     * Update recording state from service
     */
    fun updateRecordingState(state: RecordingState) {
        _recordingState.value = state
    }

    /**
     * Update video statistics
     */
    fun updateStatistics(stats: VideoStatistics) {
        _statistics.value = stats
    }

    /**
     * Update OSD data received from air unit
     */
    fun updateOsdData(data: OsdData) {
        _osdData.value = data
    }

    /**
     * Update OSD settings
     */
    fun updateOsdSettings(settings: OsdSettings) {
        _osdSettings.value = settings
        // TODO: Save to DataStore
    }

    /**
     * Toggle OSD element visibility
     */
    fun toggleOsdElement(element: OsdElement) {
        val current = _osdSettings.value
        _osdSettings.value = when (element) {
            OsdElement.FPS -> current.copy(showFps = !current.showFps)
            OsdElement.BITRATE -> current.copy(showBitrate = !current.showBitrate)
            OsdElement.LATENCY -> current.copy(showLatency = !current.showLatency)
            OsdElement.RSSI -> current.copy(showRssi = !current.showRssi)
            OsdElement.BATTERY -> current.copy(showBattery = !current.showBattery)
            OsdElement.GPS -> current.copy(showGps = !current.showGps)
            OsdElement.ALTITUDE -> current.copy(showAltitude = !current.showAltitude)
            OsdElement.SPEED -> current.copy(showSpeed = !current.showSpeed)
            OsdElement.ATTITUDE -> current.copy(showAttitude = !current.showAttitude)
            OsdElement.RECORDING -> current.copy(showRecordingIndicator = !current.showRecordingIndicator)
        }
    }

    override fun onCleared() {
        super.onCleared()
        Timber.d("MainViewModel cleared")
    }
}

/**
 * OSD elements that can be toggled
 */
enum class OsdElement {
    FPS, BITRATE, LATENCY, RSSI, BATTERY, GPS, ALTITUDE, SPEED, ATTITUDE, RECORDING
}

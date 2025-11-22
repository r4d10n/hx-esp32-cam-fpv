package com.hxesp32.fpvgs.data.model

/**
 * On-Screen Display data received from the air unit
 */
data class OsdData(
    val rssi: Int = 0, // Signal strength (0-100)
    val batteryVoltage: Float = 0f, // Volts
    val batteryPercent: Int = 0, // 0-100
    val temperature: Float = 0f, // Celsius
    val flightMode: String = "",
    val armed: Boolean = false,
    val gpsLat: Double = 0.0,
    val gpsLon: Double = 0.0,
    val gpsAlt: Float = 0f, // meters
    val gpsSats: Int = 0,
    val gpsHdop: Float = 0f,
    val altitude: Float = 0f, // meters (barometric)
    val speed: Float = 0f, // m/s
    val heading: Float = 0f, // degrees
    val pitch: Float = 0f, // degrees
    val roll: Float = 0f, // degrees
    val timestamp: Long = System.currentTimeMillis()
)

/**
 * OSD display settings
 */
data class OsdSettings(
    val showFps: Boolean = true,
    val showBitrate: Boolean = true,
    val showLatency: Boolean = true,
    val showRssi: Boolean = true,
    val showBattery: Boolean = true,
    val showGps: Boolean = true,
    val showAltitude: Boolean = true,
    val showSpeed: Boolean = true,
    val showAttitude: Boolean = false,
    val showRecordingIndicator: Boolean = true,
    val textSize: Float = 14f,
    val opacity: Float = 0.8f
)

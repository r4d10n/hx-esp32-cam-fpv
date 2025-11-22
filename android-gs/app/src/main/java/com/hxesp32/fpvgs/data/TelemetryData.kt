package com.hxesp32.fpvgs.data

import kotlin.math.max
import kotlin.math.min

/**
 * WiFi transmission rates matching the desktop GS implementation
 */
enum class WifiRate(val value: Int, val displayName: String) {
    RATE_B_2M_CCK(0, "2M CCK"),
    RATE_B_2M_CCK_S(1, "2M CCK S"),
    RATE_B_5_5M_CCK(2, "5.5M CCK"),
    RATE_B_5_5M_CCK_S(3, "5.5M CCK S"),
    RATE_B_11M_CCK(4, "11M CCK"),
    RATE_B_11M_CCK_S(5, "11M CCK S"),
    RATE_G_6M_ODFM(6, "6M ODFM"),
    RATE_G_9M_ODFM(7, "9M ODFM"),
    RATE_G_12M_ODFM(8, "12M ODFM"),
    RATE_G_18M_ODFM(9, "18M ODFM"),
    RATE_G_24M_ODFM(10, "24M ODFM"),
    RATE_G_36M_ODFM(11, "36M ODFM"),
    RATE_G_48M_ODFM(12, "48M ODFM"),
    RATE_G_54M_ODFM(13, "54M ODFM"),
    RATE_N_26M_MCS3(20, "26M MCS3");

    companion object {
        fun fromValue(value: Int): WifiRate {
            return entries.find { it.value == value } ?: RATE_N_26M_MCS3
        }
    }
}

/**
 * Video resolution settings
 */
enum class Resolution(val width: Int, val height: Int, val displayName: String) {
    QVGA(320, 240, "320x240"),
    CIF(400, 296, "400x296"),
    HVGA(480, 320, "480x320"),
    VGA(640, 480, "640x480"),
    VGA16(640, 360, "640x360"),
    SVGA(800, 600, "800x600"),
    SVGA16(800, 456, "800x456"),
    XGA(1024, 768, "1024x768"),
    XGA16(1024, 576, "1024x576"),
    SXGA(1280, 960, "1280x960"),
    HD(1280, 720, "1280x720"),
    UXGA(1600, 1200, "1600x1200");

    val aspectRatio: Float
        get() = width.toFloat() / height.toFloat()
}

/**
 * Air unit statistics - matches AirStats struct from packets.h
 */
data class AirStats(
    val sdDetected: Boolean = false,
    val sdSlow: Boolean = false,
    val sdError: Boolean = false,
    val currentWifiRate: WifiRate = WifiRate.RATE_N_26M_MCS3,
    val wifiQueueMin: Int = 0,
    val airRecordState: Boolean = false,
    val wifiQueueMax: Int = 0,
    val sdFreeSpaceGB: Float = 0f,
    val sdTotalSpaceGB: Float = 0f,
    val currentQuality: Int = 0,
    val wifiOverflow: Boolean = false,
    val isOV5640: Boolean = false,
    val outPacketRate: Int = 0,
    val inPacketRate: Int = 0,
    val inRejectedPacketRate: Int = 0,
    val rssiDbm: Int = 0,
    val noiseFloorDbm: Int = 0,
    val captureFPS: Int = 0,
    val cameraOverflowCount: Int = 0,
    val cameraFrameSizeMin: Int = 0,
    val cameraFrameSizeMax: Int = 0,
    val inMavlinkRate: Int = 0,
    val outMavlinkRate: Int = 0,
    val rcPeriodMax: Int = 0,
    val wifiChannel: Int = 7,
    val resolution: Resolution = Resolution.SVGA16,
    val temperature: Int = 0,
    val overheatThrottling: Boolean = false,
    val suspended: Boolean = false,
    val fecCodecK: Int = 6,
    val inSession: Boolean = false,
    val brightness: Int = 0,
    val contrast: Int = 0,
    val saturation: Int = 1,
    val sharpness: Int = 0,
    val aeLevel: Int = 1
) {
    val snr: Int
        get() = max(0, noiseFloorDbm - rssiDbm)

    val sdFreeSpacePercent: Float
        get() = if (sdTotalSpaceGB > 0) (sdFreeSpaceGB / sdTotalSpaceGB) * 100f else 0f
}

/**
 * Ground station statistics
 */
data class GroundStats(
    val outPacketCounter: Int = 0,
    val inPacketCounter: IntArray = intArrayOf(0, 0), // For dual WiFi cards
    val rssiDbm: IntArray = intArrayOf(0, 0),
    val noiseFloorDbm: Int = 0,
    val pingMinMS: Int = 0,
    val pingMaxMS: Int = 0,
    val inUniquePacketCounter: Int = 0,
    val inDuplicatedPacketCounter: Int = 0,
    val fecSuccessRate: Float = 0f,
    val lastPacketIndex: Long = 0,
    val statsPacketIndex: Long = 0
) {
    val totalInPackets: Int
        get() = inPacketCounter.sum()

    val bestRssi: Int
        get() = inPacketCounter.indices.maxOfOrNull { i ->
            if (inPacketCounter[i] > 0) rssiDbm[i] else Int.MIN_VALUE
        } ?: 0

    val snr: Int
        get() = max(0, -(noiseFloorDbm - bestRssi))

    val packetLoss: Float
        get() {
            val expected = (lastPacketIndex - statsPacketIndex).toInt()
            return if (expected > 0) {
                (1f - (inUniquePacketCounter.toFloat() / expected)) * 100f
            } else 0f
        }
}

/**
 * Video stream statistics
 */
data class VideoStats(
    val fps: Int = 0,
    val bitrate: Int = 0, // bits per second
    val framesDecoded: Long = 0,
    val framesDropped: Long = 0,
    val averageFrameTime: Float = 0f,
    val lastFrameTimestamp: Long = System.currentTimeMillis()
) {
    val bitrateKbps: Float
        get() = bitrate / 1024f

    val bitrateMbps: Float
        get() = bitrate / (1024f * 1024f)

    val dropRate: Float
        get() {
            val total = framesDecoded + framesDropped
            return if (total > 0) (framesDropped.toFloat() / total) * 100f else 0f
        }
}

/**
 * GPS coordinates (if available from telemetry)
 */
data class GpsData(
    val latitude: Double = 0.0,
    val longitude: Double = 0.0,
    val altitude: Float = 0f,
    val speed: Float = 0f,
    val heading: Float = 0f,
    val satellites: Int = 0,
    val fix: Boolean = false
) {
    fun toFormattedString(): String {
        return if (fix) {
            "%.6f, %.6f Alt:%.1fm".format(latitude, longitude, altitude)
        } else {
            "No GPS Fix"
        }
    }
}

/**
 * IMU data for artificial horizon (if available)
 */
data class ImuData(
    val pitch: Float = 0f,
    val roll: Float = 0f,
    val yaw: Float = 0f,
    val available: Boolean = false
)

/**
 * Battery information
 */
data class BatteryData(
    val voltage: Float = 0f,
    val current: Float = 0f,
    val percentage: Int = 0,
    val available: Boolean = false
) {
    val cells: Int
        get() = when {
            voltage < 3.7f -> 1
            voltage < 7.4f -> 2
            voltage < 11.1f -> 3
            voltage < 14.8f -> 4
            voltage < 18.5f -> 5
            voltage < 22.2f -> 6
            else -> 0
        }

    val cellVoltage: Float
        get() = if (cells > 0) voltage / cells else 0f

    val isLow: Boolean
        get() = cellVoltage < 3.5f && cells > 0
}

/**
 * Complete telemetry state
 */
data class TelemetryState(
    val airStats: AirStats = AirStats(),
    val groundStats: GroundStats = GroundStats(),
    val videoStats: VideoStats = VideoStats(),
    val gpsData: GpsData = GpsData(),
    val imuData: ImuData = ImuData(),
    val batteryData: BatteryData = BatteryData(),
    val isConnected: Boolean = false,
    val linkQuality: Float = 0f,
    val latencyMs: Int = 0
) {
    /**
     * Calculate overall link quality based on RSSI, packet loss, and latency
     */
    fun calculateLinkQuality(): Float {
        if (!isConnected) return 0f

        val rssiQuality = when {
            airStats.rssiDbm < 30 -> 100f
            airStats.rssiDbm < 50 -> 80f
            airStats.rssiDbm < 70 -> 60f
            airStats.rssiDbm < 85 -> 40f
            else -> 20f
        }

        val packetLossQuality = max(0f, 100f - groundStats.packetLoss * 2f)
        val latencyQuality = when {
            latencyMs < 100 -> 100f
            latencyMs < 150 -> 80f
            latencyMs < 200 -> 60f
            else -> 40f
        }

        return (rssiQuality + packetLossQuality + latencyQuality) / 3f
    }
}

/**
 * Rolling statistics calculator for smoothing values
 */
class RollingStats(private val windowSize: Int = 60) {
    private val buffer = IntArray(windowSize)
    private var head = 0
    private var sum = 0

    fun add(value: Int) {
        sum -= buffer[head]
        sum += value
        buffer[head] = value
        head = (head + 1) % windowSize
    }

    fun average(): Float = sum.toFloat() / windowSize

    fun max(): Int = buffer.maxOrNull() ?: 0

    fun min(): Int = buffer.minOrNull() ?: 0
}

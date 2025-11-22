package com.hxesp32.fpvgs.protocol

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * Camera configuration
 */
data class CameraConfig(
    val resolution: Resolution = Resolution.SVGA,
    val fpsLimit: UByte = 60u,
    val quality: UByte = 0u,  // 0 = auto, 1-63 manual
    val brightness: Byte = 0,  // -2 to 2
    val contrast: Byte = 0,    // -2 to 2
    val saturation: Byte = 1,  // -2 to 2
    val sharpness: Byte = 0,   // -2 to 3
    val denoise: UByte = 0u,   // 0-8, OV5640 only
    val specialEffect: UByte = 0u,  // 0-6
    val awb: Boolean = true,
    val awbGain: Boolean = true,
    val wbMode: UByte = 0u,    // 0-4
    val aec: Boolean = true,
    val aec2: Boolean = true,
    val aeLevel: Byte = 1,     // -2 to 2
    val aecValue: UShort = 204u,  // 0-1200 ISO
    val agc: Boolean = true,
    val agcGain: UByte = 0u,   // 30-6
    val gainceiling: UByte = 0u,  // 0-6
    val bpc: Boolean = true,
    val wpc: Boolean = true,
    val rawGma: Boolean = true,
    val lenc: Boolean = true,
    val hmirror: Boolean = false,
    val vflip: Boolean = false,
    val dcw: Boolean = true,
    val ov2640HighFPS: Boolean = false,
    val ov5640HighFPS: Boolean = false,
    val ov5640NightMode: Boolean = false
)

/**
 * Data channel configuration
 */
data class DataChannelConfig(
    val wifiPower: Byte = 20,  // dBm
    val wifiRate: WifiRate = WifiRate.RATE_N_26M_MCS3,
    val wifiChannel: UByte = ProtocolConstants.DEFAULT_WIFI_CHANNEL.toUByte(),
    val fecCodecK: UByte = ProtocolConstants.FEC_K.toUByte(),
    val fecCodecN: UByte = ProtocolConstants.FEC_N.toUByte(),
    val fecCodecMtu: UShort = 0u,
    val airRecordBtn: UByte = 0u,
    val profile1Btn: UByte = 0u,
    val profile2Btn: UByte = 0u,
    val cameraStopChannel: UByte = 0u,  // 0 = none, 1-31 = channel
    val autostartRecord: Boolean = true,
    val mavlink2mspRC: Boolean = false
)

/**
 * Air unit statistics
 */
data class AirStats(
    val sdDetected: Boolean,
    val sdSlow: Boolean,
    val sdError: Boolean,
    val currWifiRate: WifiRate,
    val wifiQueueMin: UByte,
    val airRecordState: Boolean,
    val wifiQueueMax: UByte,
    val sdFreeSpaceGB16: UShort,  // In 1/16 GB
    val sdTotalSpaceGB16: UShort, // In 1/16 GB
    val currQuality: UByte,
    val wifiOvf: Boolean,
    val isOV5640: Boolean,
    val outPacketRate: UShort,
    val inPacketRate: UShort,
    val inRejectedPacketRate: UShort,
    val rssiDbm: UByte,
    val noiseFloorDbm: UByte,
    val captureFPS: UByte,
    val camOvfCount: UByte,
    val camFrameSizeMin: UShort,
    val camFrameSizeMax: UShort,
    val inMavlinkRate: UShort,
    val outMavlinkRate: UShort,
    val rcPeriodMax: UByte,
    val wifiChannel: UByte,
    val resolution: Resolution,
    val temperature: UByte,
    val overheatThrottling: Boolean,
    val suspended: Boolean,
    val fecCodecK: UByte,
    val inSession: Boolean,
    val screenAspectRatio: UByte,
    val brightness: Byte,
    val contrast: Byte,
    val saturation: Byte,
    val sharpness: Byte,
    val aeLevel: Byte
) {
    fun getRssiDbmSigned(): Int = -rssiDbm.toInt()
    fun getNoiseFloorDbmSigned(): Int = -noiseFloorDbm.toInt()
    fun getSdFreeSpaceGB(): Float = sdFreeSpaceGB16.toFloat() / 16f
    fun getSdTotalSpaceGB(): Float = sdTotalSpaceGB16.toFloat() / 16f
}

/**
 * USB Frame wrapper for all packet types
 */
sealed class UsbFrame {
    abstract val timestamp: Long

    data class VideoFrame(
        val resolution: Resolution,
        val partIndex: UByte,
        val isLastPart: Boolean,
        val frameIndex: UInt,
        val data: ByteArray,
        override val timestamp: Long = System.currentTimeMillis()
    ) : UsbFrame() {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false
            other as VideoFrame
            if (resolution != other.resolution) return false
            if (partIndex != other.partIndex) return false
            if (isLastPart != other.isLastPart) return false
            if (frameIndex != other.frameIndex) return false
            if (!data.contentEquals(other.data)) return false
            return true
        }

        override fun hashCode(): Int {
            var result = resolution.hashCode()
            result = 31 * result + partIndex.hashCode()
            result = 31 * result + isLastPart.hashCode()
            result = 31 * result + frameIndex.hashCode()
            result = 31 * result + data.contentHashCode()
            return result
        }
    }

    data class TelemetryFrame(
        val data: ByteArray,
        override val timestamp: Long = System.currentTimeMillis()
    ) : UsbFrame() {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false
            other as TelemetryFrame
            if (!data.contentEquals(other.data)) return false
            return true
        }

        override fun hashCode(): Int = data.contentHashCode()
    }

    data class OsdFrame(
        val stats: AirStats,
        val osdBuffer: ByteArray,
        override val timestamp: Long = System.currentTimeMillis()
    ) : UsbFrame() {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false
            other as OsdFrame
            if (stats != other.stats) return false
            if (!osdBuffer.contentEquals(other.osdBuffer)) return false
            return true
        }

        override fun hashCode(): Int {
            var result = stats.hashCode()
            result = 31 * result + osdBuffer.contentHashCode()
            return result
        }
    }

    data class ConfigFrame(
        val airDeviceId: UShort,
        val gsDeviceId: UShort,
        val camera: CameraConfig,
        val dataChannel: DataChannelConfig,
        override val timestamp: Long = System.currentTimeMillis()
    ) : UsbFrame()
}

/**
 * Assembled video frame (all parts combined)
 */
data class AssembledVideoFrame(
    val resolution: Resolution,
    val frameIndex: UInt,
    val data: ByteArray,
    val timestamp: Long = System.currentTimeMillis()
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as AssembledVideoFrame
        if (resolution != other.resolution) return false
        if (frameIndex != other.frameIndex) return false
        if (!data.contentEquals(other.data)) return false
        return true
    }

    override fun hashCode(): Int {
        var result = resolution.hashCode()
        result = 31 * result + frameIndex.hashCode()
        result = 31 * result + data.contentHashCode()
        return result
    }
}

/**
 * Statistics for USB communication
 */
data class UsbStats(
    var bytesReceived: Long = 0,
    var bytesSent: Long = 0,
    var framesReceived: Long = 0,
    var framesSent: Long = 0,
    var videoFramesReceived: Long = 0,
    var telemetryFramesReceived: Long = 0,
    var osdFramesReceived: Long = 0,
    var configFramesReceived: Long = 0,
    var crcErrors: Long = 0,
    var framingErrors: Long = 0,
    var assemblyErrors: Long = 0,
    var usbErrors: Long = 0,
    var lastErrorMessage: String? = null,
    var lastErrorTimestamp: Long = 0
) {
    fun getThroughputBps(durationMs: Long): Double {
        return if (durationMs > 0) {
            (bytesReceived * 8000.0) / durationMs
        } else 0.0
    }

    fun getFrameRate(durationMs: Long): Double {
        return if (durationMs > 0) {
            (framesReceived * 1000.0) / durationMs
        } else 0.0
    }

    fun recordError(message: String) {
        lastErrorMessage = message
        lastErrorTimestamp = System.currentTimeMillis()
        usbErrors++
    }
}

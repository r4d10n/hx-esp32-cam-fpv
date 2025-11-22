package com.hxesp32.fpvgs.protocol

import android.util.Log
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

/**
 * Parser for ESP32-S3 USB protocol frames
 * Frame format: [SYNC1][SYNC2][TYPE][SIZE(4)][PAYLOAD][CRC]
 */
class UsbProtocolParser {
    companion object {
        private const val TAG = "UsbProtocolParser"
        private const val MIN_FRAME_SIZE = ProtocolConstants.FRAME_HEADER_SIZE + 1
    }

    private val buffer = ByteArray(ProtocolConstants.MAX_FRAME_SIZE)
    private var bufferPosition = 0
    private val crc = CRC32()

    /**
     * Parse incoming data and extract frames
     * @param data Raw USB data
     * @return List of parsed frames
     */
    fun parseData(data: ByteArray): List<UsbFrame> {
        val frames = mutableListOf<UsbFrame>()

        // Append to buffer
        if (bufferPosition + data.size > buffer.size) {
            Log.w(TAG, "Buffer overflow, resetting. Lost ${bufferPosition} bytes")
            bufferPosition = 0
        }

        System.arraycopy(data, 0, buffer, bufferPosition, data.size)
        bufferPosition += data.size

        // Try to extract frames
        while (bufferPosition >= MIN_FRAME_SIZE) {
            val frame = extractFrame()
            if (frame != null) {
                frames.add(frame)
            } else {
                break
            }
        }

        return frames
    }

    /**
     * Extract a single frame from the buffer
     */
    private fun extractFrame(): UsbFrame? {
        // Find sync bytes
        val syncIndex = findSyncBytes()
        if (syncIndex < 0) {
            // No sync found, keep last byte in case it's the start of sync
            if (bufferPosition > 1) {
                buffer[0] = buffer[bufferPosition - 1]
                bufferPosition = 1
            }
            return null
        }

        // Remove data before sync
        if (syncIndex > 0) {
            System.arraycopy(buffer, syncIndex, buffer, 0, bufferPosition - syncIndex)
            bufferPosition -= syncIndex
        }

        // Check if we have enough data for header
        if (bufferPosition < ProtocolConstants.FRAME_HEADER_SIZE) {
            return null
        }

        // Parse header
        val type = buffer[2]
        val size = ByteBuffer.wrap(buffer, 3, 4)
            .order(ByteOrder.LITTLE_ENDIAN)
            .int

        // Validate size
        if (size < 0 || size > ProtocolConstants.MAX_FRAME_SIZE) {
            Log.e(TAG, "Invalid frame size: $size, skipping sync bytes")
            bufferPosition -= 2
            System.arraycopy(buffer, 2, buffer, 0, bufferPosition)
            return null
        }

        // Check if we have complete frame
        val totalFrameSize = ProtocolConstants.FRAME_HEADER_SIZE + size
        if (bufferPosition < totalFrameSize) {
            return null  // Wait for more data
        }

        // Extract CRC from frame end
        val receivedCrc = buffer[totalFrameSize - 1].toInt() and 0xFF

        // Calculate CRC (excluding sync bytes and CRC byte itself)
        crc.reset()
        crc.update(buffer, 2, totalFrameSize - 3)
        val calculatedCrc = (crc.value and 0xFF).toInt()

        if (receivedCrc != calculatedCrc) {
            Log.e(TAG, "CRC mismatch: received=$receivedCrc, calculated=$calculatedCrc")
            // Skip this frame
            bufferPosition -= 2
            System.arraycopy(buffer, 2, buffer, 0, bufferPosition)
            return null
        }

        // Extract payload
        val payload = ByteArray(size - 1)  // -1 for CRC
        System.arraycopy(buffer, ProtocolConstants.FRAME_HEADER_SIZE, payload, 0, payload.size)

        // Remove frame from buffer
        bufferPosition -= totalFrameSize
        if (bufferPosition > 0) {
            System.arraycopy(buffer, totalFrameSize, buffer, 0, bufferPosition)
        }

        // Parse frame based on type
        return parseFrame(type, payload)
    }

    /**
     * Find sync bytes in buffer
     */
    private fun findSyncBytes(): Int {
        for (i in 0 until bufferPosition - 1) {
            if (buffer[i] == ProtocolConstants.FRAME_SYNC_BYTE1 &&
                buffer[i + 1] == ProtocolConstants.FRAME_SYNC_BYTE2) {
                return i
            }
        }
        return -1
    }

    /**
     * Parse frame payload based on type
     */
    private fun parseFrame(type: Byte, payload: ByteArray): UsbFrame? {
        return try {
            when (Air2GroundPacketType.fromValue(type)) {
                Air2GroundPacketType.VIDEO -> parseVideoFrame(payload)
                Air2GroundPacketType.TELEMETRY -> parseTelemetryFrame(payload)
                Air2GroundPacketType.OSD -> parseOsdFrame(payload)
                Air2GroundPacketType.CONFIG -> parseConfigFrame(payload)
                null -> {
                    Log.w(TAG, "Unknown frame type: $type")
                    null
                }
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error parsing frame type $type", e)
            null
        }
    }

    /**
     * Parse video frame
     */
    private fun parseVideoFrame(payload: ByteArray): UsbFrame.VideoFrame {
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)

        // Skip Air2Ground_Header (11 bytes)
        buffer.position(11)

        val resolutionByte = buffer.get()
        val resolution = Resolution.fromValue(resolutionByte)
            ?: throw IllegalArgumentException("Invalid resolution: $resolutionByte")

        val partIndexAndLastPart = buffer.get()
        val partIndex = (partIndexAndLastPart.toInt() and 0x7F).toUByte()
        val isLastPart = (partIndexAndLastPart.toInt() and 0x80) != 0

        val frameIndex = buffer.int.toUInt()

        // Rest is video data
        val videoData = ByteArray(payload.size - buffer.position())
        buffer.get(videoData)

        return UsbFrame.VideoFrame(
            resolution = resolution,
            partIndex = partIndex,
            isLastPart = isLastPart,
            frameIndex = frameIndex,
            data = videoData
        )
    }

    /**
     * Parse telemetry frame
     */
    private fun parseTelemetryFrame(payload: ByteArray): UsbFrame.TelemetryFrame {
        // Skip Air2Ground_Header (11 bytes)
        val telemetryData = ByteArray(payload.size - 11)
        System.arraycopy(payload, 11, telemetryData, 0, telemetryData.size)

        return UsbFrame.TelemetryFrame(data = telemetryData)
    }

    /**
     * Parse OSD frame
     */
    private fun parseOsdFrame(payload: ByteArray): UsbFrame.OsdFrame {
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)

        // Skip Air2Ground_Header (11 bytes)
        buffer.position(11)

        val stats = parseAirStats(buffer)

        // Rest is OSD buffer
        val osdBuffer = ByteArray(ProtocolConstants.OSD_BUFFER_SIZE)
        val availableBytes = payload.size - buffer.position()
        val bytesToCopy = minOf(availableBytes, osdBuffer.size)
        buffer.get(osdBuffer, 0, bytesToCopy)

        return UsbFrame.OsdFrame(stats = stats, osdBuffer = osdBuffer)
    }

    /**
     * Parse air statistics from buffer
     */
    private fun parseAirStats(buffer: ByteBuffer): AirStats {
        val byte0 = buffer.get().toInt() and 0xFF
        val byte1 = buffer.get().toInt() and 0xFF
        val byte2 = buffer.get().toInt() and 0xFF

        val bytes3to6 = buffer.int

        return AirStats(
            sdDetected = (byte0 and 0x01) != 0,
            sdSlow = (byte0 and 0x02) != 0,
            sdError = (byte0 and 0x04) != 0,
            currWifiRate = WifiRate.fromValue(((byte0 shr 3) and 0x1F).toByte())
                ?: WifiRate.RATE_N_26M_MCS3,
            wifiQueueMin = (byte1 and 0x7F).toUByte(),
            airRecordState = (byte1 and 0x80) != 0,
            wifiQueueMax = byte2.toUByte(),
            sdFreeSpaceGB16 = (bytes3to6 and 0xFFF).toUShort(),
            sdTotalSpaceGB16 = ((bytes3to6 shr 12) and 0xFFF).toUShort(),
            currQuality = ((bytes3to6 shr 24) and 0x3F).toUByte(),
            wifiOvf = ((bytes3to6 shr 30) and 0x01) != 0,
            isOV5640 = ((bytes3to6 shr 31) and 0x01) != 0,
            outPacketRate = buffer.short.toUShort(),
            inPacketRate = buffer.short.toUShort(),
            inRejectedPacketRate = buffer.short.toUShort(),
            rssiDbm = buffer.get().toUByte(),
            noiseFloorDbm = buffer.get().toUByte(),
            captureFPS = buffer.get().toUByte(),
            camOvfCount = buffer.get().toUByte(),
            camFrameSizeMin = buffer.short.toUShort(),
            camFrameSizeMax = buffer.short.toUShort(),
            inMavlinkRate = buffer.short.toUShort(),
            outMavlinkRate = buffer.short.toUShort(),
            rcPeriodMax = buffer.get().toUByte(),
            wifiChannel = (buffer.get().toInt() and 0x0F).toUByte(),
            resolution = Resolution.fromValue(((buffer.position(-1).get().toInt() shr 4) and 0x0F).toByte())
                ?: Resolution.SVGA,
            temperature = run {
                buffer.position(buffer.position() + 1)
                val tempByte = buffer.get(-1).toInt() and 0xFF
                (tempByte and 0x7F).toUByte()
            },
            overheatThrottling = run {
                val tempByte = buffer.get(-1).toInt() and 0xFF
                (tempByte and 0x80) != 0
            },
            suspended = run {
                val byte28 = buffer.get().toInt() and 0xFF
                (byte28 and 0x80) != 0
            },
            fecCodecK = run {
                val byte29 = buffer.get().toInt() and 0xFF
                (byte29 and 0x0F).toUByte()
            },
            inSession = run {
                val byte29 = buffer.get(-1).toInt() and 0xFF
                ((byte29 shr 4) and 0x01) != 0
            },
            screenAspectRatio = run {
                val byte29 = buffer.get(-1).toInt() and 0xFF
                ((byte29 shr 5) and 0x07).toUByte()
            },
            brightness = run {
                val word30 = buffer.short.toInt()
                ((word30 and 0x07) - if ((word30 and 0x04) != 0) 8 else 0).toByte()
            },
            contrast = run {
                val word30 = buffer.get(-2).toInt() and 0xFF or ((buffer.get(-1).toInt() and 0xFF) shl 8)
                (((word30 shr 3) and 0x07) - if (((word30 shr 3) and 0x04) != 0) 8 else 0).toByte()
            },
            saturation = run {
                val word30 = buffer.get(-2).toInt() and 0xFF or ((buffer.get(-1).toInt() and 0xFF) shl 8)
                (((word30 shr 6) and 0x07) - if (((word30 shr 6) and 0x04) != 0) 8 else 0).toByte()
            },
            sharpness = run {
                val word30 = buffer.get(-2).toInt() and 0xFF or ((buffer.get(-1).toInt() and 0xFF) shl 8)
                (((word30 shr 9) and 0x07) - if (((word30 shr 9) and 0x04) != 0) 8 else 0).toByte()
            },
            aeLevel = run {
                val word30 = buffer.get(-2).toInt() and 0xFF or ((buffer.get(-1).toInt() and 0xFF) shl 8)
                (((word30 shr 12) and 0x07) - if (((word30 shr 12) and 0x04) != 0) 8 else 0).toByte()
            }
        )
    }

    /**
     * Parse config frame
     */
    private fun parseConfigFrame(payload: ByteArray): UsbFrame.ConfigFrame {
        val buffer = ByteBuffer.wrap(payload).order(ByteOrder.LITTLE_ENDIAN)

        // Parse Air2Ground_Header
        buffer.position(5)  // Skip type and size
        val airDeviceId = buffer.short.toUShort()
        val gsDeviceId = buffer.short.toUShort()

        // Parse camera and data channel config
        // For now, return defaults - full parsing would be implemented based on needs
        return UsbFrame.ConfigFrame(
            airDeviceId = airDeviceId,
            gsDeviceId = gsDeviceId,
            camera = CameraConfig(),
            dataChannel = DataChannelConfig()
        )
    }

    /**
     * Reset parser state
     */
    fun reset() {
        bufferPosition = 0
    }
}

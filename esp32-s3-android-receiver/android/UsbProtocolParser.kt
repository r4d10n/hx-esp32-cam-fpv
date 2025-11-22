package com.example.esp32usb

import android.util.Log
import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * USB OTG Protocol Parser for ESP32-S3 Video Streaming (Kotlin version)
 *
 * This class implements the USB streaming protocol parser for Android,
 * compatible with the ESP32-S3 USB streamer component.
 *
 * Protocol Format:
 * [SYNC(2)][TYPE(1)][FLAGS(1)][SIZE(2)][SEQ(2)][TIMESTAMP(4)][PAYLOAD(n)][CRC16(2)]
 *
 * @author ESP32-S3 USB Streamer Team
 * @version 1.0
 */
class UsbProtocolParser(private val callback: PacketCallback?) {

    companion object {
        private const val TAG = "UsbProtocolParser"

        // Protocol constants
        private const val SYNC_MARKER: Short = 0xA55A.toShort()
        private const val HEADER_SIZE = 12
        private const val CRC_SIZE = 2
        private const val MAX_PAYLOAD_SIZE = 16384
        private const val MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE + CRC_SIZE

        // Packet types
        const val PACKET_TYPE_VIDEO: Byte = 0x01
        const val PACKET_TYPE_METADATA: Byte = 0x02
        const val PACKET_TYPE_CONTROL: Byte = 0x03
        const val PACKET_TYPE_ACK: Byte = 0x04
        const val PACKET_TYPE_HEARTBEAT: Byte = 0x05
        const val PACKET_TYPE_DEBUG: Byte = 0x06

        // Packet flags
        const val FLAG_FRAGMENT: Byte = 0x01
        const val FLAG_LAST_FRAGMENT: Byte = 0x02
        const val FLAG_PRIORITY_HIGH: Byte = 0x04
        const val FLAG_REQUIRE_ACK: Byte = 0x08

        // NAL unit types
        const val NAL_UNIT_TYPE_NON_IDR: Byte = 0x01
        const val NAL_UNIT_TYPE_IDR: Byte = 0x05
        const val NAL_UNIT_TYPE_SPS: Byte = 0x07
        const val NAL_UNIT_TYPE_PPS: Byte = 0x08
    }

    /**
     * Packet data class
     */
    data class Packet(
        val type: Byte,
        val flags: Byte,
        val payloadSize: Int,
        val sequence: Int,
        val timestamp: Long,
        val payload: ByteArray
    ) {
        fun isFragmented() = (flags.toInt() and FLAG_FRAGMENT.toInt()) != 0
        fun isLastFragment() = (flags.toInt() and FLAG_LAST_FRAGMENT.toInt()) != 0
        fun isHighPriority() = (flags.toInt() and FLAG_PRIORITY_HIGH.toInt()) != 0
        fun requiresAck() = (flags.toInt() and FLAG_REQUIRE_ACK.toInt()) != 0

        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is Packet) return false
            if (type != other.type) return false
            if (flags != other.flags) return false
            if (payloadSize != other.payloadSize) return false
            if (sequence != other.sequence) return false
            if (timestamp != other.timestamp) return false
            if (!payload.contentEquals(other.payload)) return false
            return true
        }

        override fun hashCode(): Int {
            var result = type.toInt()
            result = 31 * result + flags.toInt()
            result = 31 * result + payloadSize
            result = 31 * result + sequence
            result = 31 * result + timestamp.hashCode()
            result = 31 * result + payload.contentHashCode()
            return result
        }
    }

    /**
     * Video packet (NAL unit)
     */
    data class VideoPacket(
        val nalType: Byte,
        val nalSize: Int,
        val nalData: ByteArray
    ) {
        fun isKeyFrame() = nalType == NAL_UNIT_TYPE_IDR ||
                           nalType == NAL_UNIT_TYPE_SPS ||
                           nalType == NAL_UNIT_TYPE_PPS

        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (other !is VideoPacket) return false
            if (nalType != other.nalType) return false
            if (nalSize != other.nalSize) return false
            if (!nalData.contentEquals(other.nalData)) return false
            return true
        }

        override fun hashCode(): Int {
            var result = nalType.toInt()
            result = 31 * result + nalSize
            result = 31 * result + nalData.contentHashCode()
            return result
        }
    }

    /**
     * Metadata packet
     */
    data class MetadataPacket(
        val rssi: Byte,              // WiFi RSSI in dBm
        val fps: Int,                // FPS × 10 (e.g., 300 = 30.0 fps)
        val bitrate: Long,           // Bitrate in kbps
        val frameCount: Long,        // Total frames transmitted
        val bytesTransmitted: Long,  // Total bytes transmitted
        val bufferUsage: Byte,       // Buffer usage percentage (0-100)
        val temperature: Byte        // Temperature in Celsius
    )

    /**
     * Parser statistics
     */
    data class ParserStatistics(
        var packetsReceived: Long = 0,
        var bytesReceived: Long = 0,
        var crcErrors: Long = 0,
        var syncErrors: Long = 0,
        var fragmentsReceived: Long = 0,
        var packetsDropped: Long = 0,
        var lastSequence: Long = 0,
        var sequenceGaps: Long = 0
    ) {
        fun reset() {
            packetsReceived = 0
            bytesReceived = 0
            crcErrors = 0
            syncErrors = 0
            fragmentsReceived = 0
            packetsDropped = 0
            lastSequence = 0
            sequenceGaps = 0
        }
    }

    /**
     * Callback interface for parsed packets
     */
    interface PacketCallback {
        fun onVideoPacket(video: VideoPacket)
        fun onMetadataPacket(metadata: MetadataPacket)
        fun onHeartbeat(uptime: Long)
        fun onDebugMessage(message: String)
        fun onParseError(error: String)
    }

    // Parser state
    private val receiveBuffer = ByteBuffer.allocate(MAX_PACKET_SIZE * 2).apply {
        order(ByteOrder.LITTLE_ENDIAN)
    }

    private val fragmentBuffer = ByteBuffer.allocate(MAX_PAYLOAD_SIZE * 4)
    private var fragmentSequence = -1

    val statistics = ParserStatistics()

    /**
     * Parse incoming USB data
     */
    fun parse(data: ByteArray, length: Int = data.size) {
        if (length <= 0) return

        // Append to receive buffer
        if (receiveBuffer.remaining() < length) {
            receiveBuffer.compact()
        }

        receiveBuffer.put(data, 0, length)
        receiveBuffer.flip()

        // Try to parse packets
        while (receiveBuffer.remaining() >= HEADER_SIZE + CRC_SIZE) {
            receiveBuffer.mark()

            // Find sync marker
            val sync = receiveBuffer.short
            if (sync != SYNC_MARKER) {
                receiveBuffer.reset()
                if (!seekToSync()) break
                statistics.syncErrors++
                continue
            }

            // Read header
            val type = receiveBuffer.get()
            val flags = receiveBuffer.get()
            val payloadSize = receiveBuffer.short.toInt() and 0xFFFF
            val sequence = receiveBuffer.short.toInt() and 0xFFFF
            val timestamp = receiveBuffer.int.toLong() and 0xFFFFFFFFL

            // Validate payload size
            if (payloadSize > MAX_PAYLOAD_SIZE) {
                Log.w(TAG, "Invalid payload size: $payloadSize")
                receiveBuffer.reset()
                seekToSync()
                statistics.packetsDropped++
                continue
            }

            // Check if we have complete packet
            if (receiveBuffer.remaining() < payloadSize + CRC_SIZE) {
                receiveBuffer.reset()
                break
            }

            // Read payload
            val payload = ByteArray(payloadSize)
            receiveBuffer.get(payload)

            // Read CRC
            val receivedCrc = receiveBuffer.short.toInt() and 0xFFFF

            // Verify CRC
            val packetData = ByteArray(HEADER_SIZE + payloadSize)
            receiveBuffer.reset()
            receiveBuffer.get(packetData)
            val calculatedCrc = calculateCrc16(packetData)

            if (receivedCrc != calculatedCrc) {
                Log.w(TAG, "CRC mismatch: received=0x%04X, calculated=0x%04X"
                    .format(receivedCrc, calculatedCrc))
                statistics.crcErrors++
                statistics.packetsDropped++
                continue
            }

            // Valid packet received
            statistics.packetsReceived++
            statistics.bytesReceived += HEADER_SIZE + payloadSize + CRC_SIZE

            // Check sequence
            if (statistics.lastSequence != 0L &&
                sequence != ((statistics.lastSequence + 1) % 65536).toInt()) {
                statistics.sequenceGaps++
                Log.d(TAG, "Sequence gap: expected ${(statistics.lastSequence + 1) % 65536}, got $sequence")
            }
            statistics.lastSequence = sequence.toLong()

            // Create packet object
            val packet = Packet(type, flags, payloadSize, sequence, timestamp, payload)

            // Process packet
            processPacket(packet)
        }

        // Compact buffer for next read
        receiveBuffer.compact()
    }

    /**
     * Process parsed packet
     */
    private fun processPacket(packet: Packet) {
        try {
            when (packet.type) {
                PACKET_TYPE_VIDEO -> processVideoPacket(packet)
                PACKET_TYPE_METADATA -> processMetadataPacket(packet)
                PACKET_TYPE_HEARTBEAT -> processHeartbeatPacket(packet)
                PACKET_TYPE_DEBUG -> processDebugPacket(packet)
                PACKET_TYPE_ACK -> Log.d(TAG, "ACK received for sequence: ${packet.sequence}")
                else -> Log.w(TAG, "Unknown packet type: 0x${packet.type.toString(16)}")
            }
        } catch (e: Exception) {
            Log.e(TAG, "Error processing packet: ${e.message}")
            callback?.onParseError("Packet processing error: ${e.message}")
        }
    }

    /**
     * Process video packet (NAL unit)
     */
    private fun processVideoPacket(packet: Packet) {
        if (packet.payload.size < 4) {
            Log.w(TAG, "Invalid video packet: too small")
            return
        }

        // Handle fragmentation
        if (packet.isFragmented()) {
            statistics.fragmentsReceived++

            if (fragmentSequence == -1 || packet.sequence == fragmentSequence + 1) {
                fragmentBuffer.put(packet.payload)
                fragmentSequence = packet.sequence

                if (packet.isLastFragment()) {
                    fragmentBuffer.flip()
                    val completePayload = ByteArray(fragmentBuffer.remaining())
                    fragmentBuffer.get(completePayload)
                    fragmentBuffer.clear()
                    fragmentSequence = -1

                    parseNalUnit(completePayload)
                }
            } else {
                Log.w(TAG, "Fragment sequence mismatch")
                fragmentBuffer.clear()
                fragmentSequence = -1
            }
        } else {
            parseNalUnit(packet.payload)
        }
    }

    /**
     * Parse NAL unit from payload
     */
    private fun parseNalUnit(payload: ByteArray) {
        if (payload.size < 4) return

        val buffer = ByteBuffer.wrap(payload).apply {
            order(ByteOrder.LITTLE_ENDIAN)
        }

        val nalType = buffer.get()
        var nalSize = buffer.get().toInt() and 0xFF
        nalSize = nalSize or ((buffer.get().toInt() and 0xFF) shl 8)
        nalSize = nalSize or ((buffer.get().toInt() and 0xFF) shl 16)

        if (nalSize > payload.size - 4) {
            Log.w(TAG, "Invalid NAL size: $nalSize")
            return
        }

        val nalData = ByteArray(nalSize)
        buffer.get(nalData)

        val video = VideoPacket(nalType, nalSize, nalData)
        callback?.onVideoPacket(video)
    }

    /**
     * Process metadata packet
     */
    private fun processMetadataPacket(packet: Packet) {
        if (packet.payload.size < 24) {
            Log.w(TAG, "Invalid metadata packet: too small")
            return
        }

        val buffer = ByteBuffer.wrap(packet.payload).apply {
            order(ByteOrder.LITTLE_ENDIAN)
        }

        val metadata = MetadataPacket(
            rssi = buffer.get(),
            fps = buffer.short.toInt() and 0xFFFF,
            bitrate = buffer.int.toLong() and 0xFFFFFFFFL,
            frameCount = buffer.int.toLong() and 0xFFFFFFFFL,
            bytesTransmitted = buffer.long,
            bufferUsage = buffer.get(),
            temperature = buffer.get()
        )

        callback?.onMetadataPacket(metadata)
    }

    /**
     * Process heartbeat packet
     */
    private fun processHeartbeatPacket(packet: Packet) {
        if (packet.payload.size < 4) return

        val buffer = ByteBuffer.wrap(packet.payload).apply {
            order(ByteOrder.LITTLE_ENDIAN)
        }
        val uptime = buffer.int.toLong() and 0xFFFFFFFFL

        callback?.onHeartbeat(uptime)
    }

    /**
     * Process debug packet
     */
    private fun processDebugPacket(packet: Packet) {
        if (packet.payload.size < 2) return

        val level = packet.payload[0]
        val message = String(packet.payload, 1, packet.payload.size - 1)

        callback?.onDebugMessage(message)
        Log.d(TAG, "Debug [$level]: $message")
    }

    /**
     * Seek to next sync marker
     */
    private fun seekToSync(): Boolean {
        while (receiveBuffer.remaining() >= 2) {
            receiveBuffer.mark()
            val sync = receiveBuffer.short
            if (sync == SYNC_MARKER) {
                receiveBuffer.reset()
                return true
            }
            receiveBuffer.reset()
            receiveBuffer.get() // Advance by 1 byte
        }
        return false
    }

    /**
     * Calculate CRC16-CCITT
     */
    private fun calculateCrc16(data: ByteArray, length: Int = data.size): Int {
        var crc = 0xFFFF
        for (i in 0 until length) {
            crc = crc xor ((data[i].toInt() and 0xFF) shl 8)
            for (j in 0 until 8) {
                crc = if ((crc and 0x8000) != 0) {
                    (crc shl 1) xor 0x1021
                } else {
                    crc shl 1
                }
            }
        }
        return crc and 0xFFFF
    }

    /**
     * Reset parser state
     */
    fun reset() {
        receiveBuffer.clear()
        fragmentBuffer.clear()
        fragmentSequence = -1
        statistics.reset()
    }
}

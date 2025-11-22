package com.hxesp32.fpvgs.video

import timber.log.Timber
import java.nio.ByteBuffer

/**
 * Parses H.264 NAL units from the incoming byte stream
 */
class VideoStreamParser {

    private val buffer = ByteBuffer.allocate(BUFFER_SIZE)
    private var nalStartFound = false

    /**
     * Parse incoming data and extract complete NAL units
     */
    fun parse(data: ByteArray, onNalUnit: (ByteArray) -> Unit) {
        for (byte in data) {
            if (buffer.remaining() == 0) {
                Timber.w("Parser buffer overflow, resetting")
                buffer.clear()
                nalStartFound = false
                continue
            }

            buffer.put(byte)

            // Look for NAL unit start code (0x00 0x00 0x00 0x01 or 0x00 0x00 0x01)
            if (isNalStartCode(buffer)) {
                if (nalStartFound) {
                    // Extract the NAL unit (excluding the new start code)
                    val nalSize = buffer.position() - 4 // or 3 for short start code
                    if (nalSize > 0) {
                        val nalUnit = ByteArray(nalSize)
                        buffer.position(0)
                        buffer.get(nalUnit, 0, nalSize)
                        onNalUnit(nalUnit)
                    }

                    // Reset buffer with the new start code
                    buffer.clear()
                    buffer.put(0x00)
                    buffer.put(0x00)
                    buffer.put(0x00)
                    buffer.put(0x01)
                } else {
                    nalStartFound = true
                }
            }
        }
    }

    /**
     * Check if current buffer position contains a NAL start code
     */
    private fun isNalStartCode(buffer: ByteBuffer): Boolean {
        val pos = buffer.position()

        // Check for 4-byte start code: 0x00 0x00 0x00 0x01
        if (pos >= 4) {
            if (buffer.get(pos - 4) == 0x00.toByte() &&
                buffer.get(pos - 3) == 0x00.toByte() &&
                buffer.get(pos - 2) == 0x00.toByte() &&
                buffer.get(pos - 1) == 0x01.toByte()) {
                return true
            }
        }

        // Check for 3-byte start code: 0x00 0x00 0x01
        if (pos >= 3) {
            if (buffer.get(pos - 3) == 0x00.toByte() &&
                buffer.get(pos - 2) == 0x00.toByte() &&
                buffer.get(pos - 1) == 0x01.toByte()) {
                return true
            }
        }

        return false
    }

    /**
     * Flush any remaining data
     */
    fun flush(onNalUnit: (ByteArray) -> Unit) {
        if (nalStartFound && buffer.position() > 0) {
            val nalUnit = ByteArray(buffer.position())
            buffer.position(0)
            buffer.get(nalUnit)
            onNalUnit(nalUnit)
        }
        reset()
    }

    /**
     * Reset the parser
     */
    fun reset() {
        buffer.clear()
        nalStartFound = false
    }

    companion object {
        private const val BUFFER_SIZE = 1024 * 1024 // 1MB
    }
}

/**
 * H.264 NAL unit types
 */
object NalUnitType {
    const val UNSPECIFIED = 0
    const val CODED_SLICE_NON_IDR = 1
    const val CODED_SLICE_PARTITION_A = 2
    const val CODED_SLICE_PARTITION_B = 3
    const val CODED_SLICE_PARTITION_C = 4
    const val CODED_SLICE_IDR = 5
    const val SEI = 6
    const val SPS = 7
    const val PPS = 8
    const val ACCESS_UNIT_DELIMITER = 9

    fun getNalType(nalUnit: ByteArray): Int {
        if (nalUnit.isEmpty()) return UNSPECIFIED
        return (nalUnit[0].toInt() and 0x1F)
    }

    fun isKeyFrame(nalUnit: ByteArray): Boolean {
        return getNalType(nalUnit) == CODED_SLICE_IDR
    }

    fun isConfigData(nalUnit: ByteArray): Boolean {
        val type = getNalType(nalUnit)
        return type == SPS || type == PPS
    }
}

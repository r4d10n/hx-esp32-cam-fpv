package com.hxesp32.fpvgs.usb

import com.hxesp32.fpvgs.protocol.*
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.zip.CRC32

/**
 * Unit tests for UsbProtocolParser
 */
class UsbProtocolParserTest {

    private lateinit var parser: UsbProtocolParser

    @Before
    fun setup() {
        parser = UsbProtocolParser()
    }

    @Test
    fun testParseVideoFrame() {
        // Create a test video frame
        val videoData = "Test JPEG data".toByteArray()
        val frame = buildTestVideoFrame(
            resolution = Resolution.SVGA,
            partIndex = 0u,
            isLastPart = true,
            frameIndex = 100u,
            data = videoData
        )

        val frames = parser.parseData(frame)

        assertEquals(1, frames.size)
        assertTrue(frames[0] is UsbFrame.VideoFrame)

        val videoFrame = frames[0] as UsbFrame.VideoFrame
        assertEquals(Resolution.SVGA, videoFrame.resolution)
        assertEquals(0u, videoFrame.partIndex.toUInt())
        assertTrue(videoFrame.isLastPart)
        assertEquals(100u, videoFrame.frameIndex)
        assertArrayEquals(videoData, videoFrame.data)
    }

    @Test
    fun testParseMultipleFrames() {
        val frame1 = buildTestVideoFrame(Resolution.VGA, 0u, true, 1u, "Frame1".toByteArray())
        val frame2 = buildTestVideoFrame(Resolution.VGA, 0u, true, 2u, "Frame2".toByteArray())

        val combined = frame1 + frame2
        val frames = parser.parseData(combined)

        assertEquals(2, frames.size)
        assertTrue(frames[0] is UsbFrame.VideoFrame)
        assertTrue(frames[1] is UsbFrame.VideoFrame)

        val video1 = frames[0] as UsbFrame.VideoFrame
        val video2 = frames[1] as UsbFrame.VideoFrame

        assertEquals(1u, video1.frameIndex)
        assertEquals(2u, video2.frameIndex)
    }

    @Test
    fun testParseIncompleteFrame() {
        val completeFrame = buildTestVideoFrame(
            Resolution.VGA, 0u, true, 1u, "Data".toByteArray()
        )

        // Send only first half
        val halfFrame = completeFrame.copyOf(completeFrame.size / 2)
        val frames1 = parser.parseData(halfFrame)

        // Should not parse incomplete frame
        assertEquals(0, frames1.size)

        // Send rest
        val restFrame = completeFrame.copyOfRange(completeFrame.size / 2, completeFrame.size)
        val frames2 = parser.parseData(restFrame)

        // Should now have complete frame
        assertEquals(1, frames2.size)
    }

    @Test
    fun testCrcValidation() {
        val validFrame = buildTestVideoFrame(
            Resolution.VGA, 0u, true, 1u, "Data".toByteArray()
        )

        // Corrupt CRC
        val corruptedFrame = validFrame.copyOf()
        corruptedFrame[corruptedFrame.size - 1] = 0xFF.toByte()

        val frames = parser.parseData(corruptedFrame)

        // Should reject frame with bad CRC
        assertEquals(0, frames.size)
    }

    @Test
    fun testSyncByteRecovery() {
        val validFrame = buildTestVideoFrame(
            Resolution.VGA, 0u, true, 1u, "Data".toByteArray()
        )

        // Add garbage before sync bytes
        val garbage = "GarbageData".toByteArray()
        val frameWithGarbage = garbage + validFrame

        val frames = parser.parseData(frameWithGarbage)

        // Should find sync and parse frame
        assertEquals(1, frames.size)
    }

    @Test
    fun testInvalidFrameSize() {
        val buffer = ByteBuffer.allocate(100).order(ByteOrder.LITTLE_ENDIAN)

        // Build frame with invalid size
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE1)
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE2)
        buffer.put(Air2GroundPacketType.VIDEO.value)
        buffer.putInt(1000000)  // Invalid huge size

        val frames = parser.parseData(buffer.array())

        // Should reject invalid size
        assertEquals(0, frames.size)
    }

    @Test
    fun testReset() {
        val frame = buildTestVideoFrame(
            Resolution.VGA, 0u, true, 1u, "Data".toByteArray()
        )

        // Parse partial frame
        val halfFrame = frame.copyOf(frame.size / 2)
        parser.parseData(halfFrame)

        // Reset
        parser.reset()

        // Try to parse rest - should not work because buffer was cleared
        val restFrame = frame.copyOfRange(frame.size / 2, frame.size)
        val frames = parser.parseData(restFrame)

        assertEquals(0, frames.size)
    }

    @Test
    fun testTelemetryFrame() {
        val telemetryData = "MAVLink data".toByteArray()
        val frame = buildTestTelemetryFrame(telemetryData)

        val frames = parser.parseData(frame)

        assertEquals(1, frames.size)
        assertTrue(frames[0] is UsbFrame.TelemetryFrame)

        val telemetryFrame = frames[0] as UsbFrame.TelemetryFrame
        assertArrayEquals(telemetryData, telemetryFrame.data)
    }

    @Test
    fun testOsdFrame() {
        val frame = buildTestOsdFrame()

        val frames = parser.parseData(frame)

        assertEquals(1, frames.size)
        assertTrue(frames[0] is UsbFrame.OsdFrame)

        val osdFrame = frames[0] as UsbFrame.OsdFrame
        assertNotNull(osdFrame.stats)
        assertEquals(ProtocolConstants.OSD_BUFFER_SIZE, osdFrame.osdBuffer.size)
    }

    @Test
    fun testLargeVideoFrame() {
        // Test with large video data (simulating high-res frame)
        val largeData = ByteArray(50000) { it.toByte() }
        val frame = buildTestVideoFrame(
            Resolution.HD, 0u, true, 1u, largeData
        )

        val frames = parser.parseData(frame)

        assertEquals(1, frames.size)
        val videoFrame = frames[0] as UsbFrame.VideoFrame
        assertArrayEquals(largeData, videoFrame.data)
    }

    @Test
    fun testMultiPartVideoFrameMarkers() {
        // Test part 0 of 3
        val part0 = buildTestVideoFrame(Resolution.VGA, 0u, false, 100u, "Part0".toByteArray())
        val frames0 = parser.parseData(part0)

        val video0 = frames0[0] as UsbFrame.VideoFrame
        assertFalse(video0.isLastPart)
        assertEquals(0u, video0.partIndex.toUInt())

        // Test part 2 of 3 (last)
        val part2 = buildTestVideoFrame(Resolution.VGA, 2u, true, 100u, "Part2".toByteArray())
        val frames2 = parser.parseData(part2)

        val video2 = frames2[0] as UsbFrame.VideoFrame
        assertTrue(video2.isLastPart)
        assertEquals(2u, video2.partIndex.toUInt())
    }

    // Helper functions

    private fun buildTestVideoFrame(
        resolution: Resolution,
        partIndex: UInt,
        isLastPart: Boolean,
        frameIndex: UInt,
        data: ByteArray
    ): ByteArray {
        val headerSize = 11  // Air2Ground_Header
        val videoHeaderSize = 6  // Video packet specific fields
        val payloadSize = headerSize + videoHeaderSize + data.size

        val buffer = ByteBuffer.allocate(
            ProtocolConstants.FRAME_HEADER_SIZE + payloadSize
        ).order(ByteOrder.LITTLE_ENDIAN)

        // Frame header
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE1)
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE2)
        buffer.put(Air2GroundPacketType.VIDEO.value)
        buffer.putInt(payloadSize + 1)  // +1 for CRC

        // Air2Ground_Header (11 bytes)
        buffer.put(Air2GroundPacketType.VIDEO.value)  // type
        buffer.putInt(payloadSize - 11)  // size (excluding header)
        buffer.put(0)  // pong
        buffer.put(ProtocolConstants.PACKET_VERSION)  // version
        buffer.put(0)  // crc (placeholder)
        buffer.putShort(1234)  // airDeviceId
        buffer.putShort(5678)  // gsDeviceId

        // Video packet fields
        buffer.put(resolution.value)
        val partIndexByte = if (isLastPart) {
            (partIndex.toByte().toInt() or 0x80).toByte()
        } else {
            partIndex.toByte()
        }
        buffer.put(partIndexByte)
        buffer.putInt(frameIndex.toInt())

        // Video data
        buffer.put(data)

        // Calculate and add CRC
        val crc = CRC32()
        crc.update(buffer.array(), 2, buffer.position() - 2)
        buffer.put((crc.value and 0xFF).toByte())

        return buffer.array()
    }

    private fun buildTestTelemetryFrame(data: ByteArray): ByteArray {
        val headerSize = 11
        val payloadSize = headerSize + data.size

        val buffer = ByteBuffer.allocate(
            ProtocolConstants.FRAME_HEADER_SIZE + payloadSize
        ).order(ByteOrder.LITTLE_ENDIAN)

        // Frame header
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE1)
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE2)
        buffer.put(Air2GroundPacketType.TELEMETRY.value)
        buffer.putInt(payloadSize + 1)

        // Air2Ground_Header
        buffer.put(Air2GroundPacketType.TELEMETRY.value)
        buffer.putInt(data.size)
        buffer.put(0)  // pong
        buffer.put(ProtocolConstants.PACKET_VERSION)
        buffer.put(0)  // crc
        buffer.putShort(1234)  // airDeviceId
        buffer.putShort(5678)  // gsDeviceId

        // Telemetry data
        buffer.put(data)

        // CRC
        val crc = CRC32()
        crc.update(buffer.array(), 2, buffer.position() - 2)
        buffer.put((crc.value and 0xFF).toByte())

        return buffer.array()
    }

    private fun buildTestOsdFrame(): ByteArray {
        val headerSize = 11
        val statsSize = 40  // AirStats approximate size
        val osdSize = ProtocolConstants.OSD_BUFFER_SIZE
        val payloadSize = headerSize + statsSize + osdSize

        val buffer = ByteBuffer.allocate(
            ProtocolConstants.FRAME_HEADER_SIZE + payloadSize
        ).order(ByteOrder.LITTLE_ENDIAN)

        // Frame header
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE1)
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE2)
        buffer.put(Air2GroundPacketType.OSD.value)
        buffer.putInt(payloadSize + 1)

        // Air2Ground_Header
        buffer.put(Air2GroundPacketType.OSD.value)
        buffer.putInt(statsSize + osdSize)
        buffer.put(0)  // pong
        buffer.put(ProtocolConstants.PACKET_VERSION)
        buffer.put(0)  // crc
        buffer.putShort(1234)  // airDeviceId
        buffer.putShort(5678)  // gsDeviceId

        // AirStats (simplified)
        buffer.put(0x01)  // byte0: sdDetected
        buffer.put(0x40)  // byte1: wifiQueueMin
        buffer.put(0x80.toByte())  // byte2: wifiQueueMax
        buffer.putInt(0x12345678)  // bytes 3-6

        // Fill rest with zeros
        for (i in 0 until (statsSize - 7)) {
            buffer.put(0)
        }

        // OSD buffer
        for (i in 0 until osdSize) {
            buffer.put((i % 256).toByte())
        }

        // CRC
        val crc = CRC32()
        crc.update(buffer.array(), 2, buffer.position() - 2)
        buffer.put((crc.value and 0xFF).toByte())

        return buffer.array()
    }
}

package com.hxesp32.fpvgs.usb

import com.hxesp32.fpvgs.protocol.*
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test

/**
 * Unit tests for UsbFrameAssembler
 */
class UsbFrameAssemblerTest {

    private lateinit var assembler: UsbFrameAssembler

    @Before
    fun setup() {
        assembler = UsbFrameAssembler()
    }

    @Test
    fun testSinglePartFrame() {
        val data = "Complete frame data".toByteArray()
        val frame = UsbFrame.VideoFrame(
            resolution = Resolution.VGA,
            partIndex = 0u,
            isLastPart = true,
            frameIndex = 1u,
            data = data
        )

        val assembled = assembler.addFramePart(frame)

        assertNotNull(assembled)
        assertEquals(1u, assembled!!.frameIndex)
        assertEquals(Resolution.VGA, assembled.resolution)
        assertArrayEquals(data, assembled.data)
    }

    @Test
    fun testMultiPartFrameInOrder() {
        val frameIndex = 100u
        val part0 = "Part0".toByteArray()
        val part1 = "Part1".toByteArray()
        val part2 = "Part2".toByteArray()

        // Add parts in order
        val result0 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, frameIndex, part0)
        )
        assertNull(result0)  // Not complete yet

        val result1 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 1u, false, frameIndex, part1)
        )
        assertNull(result1)  // Not complete yet

        val result2 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 2u, true, frameIndex, part2)
        )
        assertNotNull(result2)  // Should be complete

        // Verify assembled data
        val expected = part0 + part1 + part2
        assertArrayEquals(expected, result2!!.data)
        assertEquals(frameIndex, result2.frameIndex)
    }

    @Test
    fun testMultiPartFrameOutOfOrder() {
        val frameIndex = 200u
        val part0 = "Part0".toByteArray()
        val part1 = "Part1".toByteArray()
        val part2 = "Part2".toByteArray()

        // Add parts out of order: 2, 0, 1
        val result2 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 2u, true, frameIndex, part2)
        )
        assertNull(result2)  // Not complete yet

        val result0 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, frameIndex, part0)
        )
        assertNull(result0)  // Not complete yet

        val result1 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 1u, false, frameIndex, part1)
        )
        assertNotNull(result1)  // Should be complete

        // Verify assembled data is in correct order
        val expected = part0 + part1 + part2
        assertArrayEquals(expected, result1!!.data)
    }

    @Test
    fun testMultipleFramesInParallel() {
        // Add parts from two different frames interleaved
        val frame1Part0 = UsbFrame.VideoFrame(Resolution.VGA, 0u, false, 1u, "F1P0".toByteArray())
        val frame2Part0 = UsbFrame.VideoFrame(Resolution.VGA, 0u, false, 2u, "F2P0".toByteArray())
        val frame1Part1 = UsbFrame.VideoFrame(Resolution.VGA, 1u, true, 1u, "F1P1".toByteArray())
        val frame2Part1 = UsbFrame.VideoFrame(Resolution.VGA, 1u, true, 2u, "F2P1".toByteArray())

        // Add interleaved
        assertNull(assembler.addFramePart(frame1Part0))
        assertNull(assembler.addFramePart(frame2Part0))

        val assembled1 = assembler.addFramePart(frame1Part1)
        assertNotNull(assembled1)
        assertEquals(1u, assembled1!!.frameIndex)

        val assembled2 = assembler.addFramePart(frame2Part1)
        assertNotNull(assembled2)
        assertEquals(2u, assembled2!!.frameIndex)
    }

    @Test
    fun testDuplicatePart() {
        val frameIndex = 300u
        val part0 = "Part0".toByteArray()
        val part1 = "Part1".toByteArray()

        // Add part 0
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, frameIndex, part0)
        )

        // Add part 0 again (duplicate)
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, frameIndex, "DuplicatePart0".toByteArray())
        )

        // Add part 1 (last)
        val result = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 1u, true, frameIndex, part1)
        )

        assertNotNull(result)
        // The duplicate should have overwritten the original
        assertTrue(result!!.data.isNotEmpty())
    }

    @Test
    fun testMissingPart() {
        val frameIndex = 400u

        // Add part 0
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, frameIndex, "Part0".toByteArray())
        )

        // Add part 2 (last), skipping part 1
        val result = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 2u, true, frameIndex, "Part2".toByteArray())
        )

        // Should not assemble because part 1 is missing
        assertNull(result)

        val stats = assembler.getStats()
        assertEquals(1, stats.pendingFrames)
    }

    @Test
    fun testFrameTimeout() {
        val assemblerWithTimeout = UsbFrameAssembler(
            maxPendingFrames = 10,
            frameTimeoutMs = 100  // Short timeout for testing
        )

        val frameIndex = 500u

        // Add part 0
        assemblerWithTimeout.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, frameIndex, "Part0".toByteArray())
        )

        // Wait for timeout
        Thread.sleep(200)

        // Add another frame to trigger cleanup
        assemblerWithTimeout.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, true, 501u, "NewFrame".toByteArray())
        )

        val stats = assemblerWithTimeout.getStats()
        // Old frame should have been cleaned up
        assertEquals(1L, stats.droppedFrames)
    }

    @Test
    fun testMaxPendingFrames() {
        val assemblerWithLimit = UsbFrameAssembler(
            maxPendingFrames = 3,
            frameTimeoutMs = 10000
        )

        // Add 5 incomplete frames
        for (i in 1..5) {
            assemblerWithLimit.addFramePart(
                UsbFrame.VideoFrame(Resolution.VGA, 0u, false, i.toUInt(), "Part0".toByteArray())
            )
        }

        val stats = assemblerWithLimit.getStats()
        // Should only keep 3 frames
        assertTrue(stats.pendingFrames <= 3)
        assertTrue(stats.droppedFrames > 0)
    }

    @Test
    fun testReset() {
        // Add incomplete frame
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, 1u, "Part0".toByteArray())
        )

        val statsBefore = assembler.getStats()
        assertEquals(1, statsBefore.pendingFrames)

        // Reset
        assembler.reset()

        val statsAfter = assembler.getStats()
        assertEquals(0, statsAfter.pendingFrames)
        assertEquals(0, statsAfter.assembledFrames)
        assertEquals(0, statsAfter.droppedFrames)
    }

    @Test
    fun testFlushPendingFrames() {
        val frameIndex = 600u

        // Add incomplete frame (part 0 and 1 of 3)
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, frameIndex, "Part0".toByteArray())
        )
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 1u, false, frameIndex, "Part1".toByteArray())
        )

        // Flush
        val flushed = assembler.flushPendingFrames()

        assertEquals(1, flushed.size)
        assertEquals(frameIndex, flushed[0].frameIndex)
        // Should contain parts 0 and 1
        assertTrue(flushed[0].data.isNotEmpty())

        // Pending frames should be cleared
        val stats = assembler.getStats()
        assertEquals(0, stats.pendingFrames)
    }

    @Test
    fun testLargeFrame() {
        val frameIndex = 700u
        val numParts = 50
        val parts = mutableListOf<ByteArray>()

        // Create 50 parts
        for (i in 0 until numParts) {
            val partData = ByteArray(1000) { (i * 1000 + it).toByte() }
            parts.add(partData)
        }

        // Add all parts
        for (i in 0 until numParts - 1) {
            val result = assembler.addFramePart(
                UsbFrame.VideoFrame(
                    Resolution.HD,
                    i.toUByte(),
                    false,
                    frameIndex,
                    parts[i]
                )
            )
            assertNull(result)  // Not complete until last part
        }

        // Add last part
        val result = assembler.addFramePart(
            UsbFrame.VideoFrame(
                Resolution.HD,
                (numParts - 1).toUByte(),
                true,
                frameIndex,
                parts[numParts - 1]
            )
        )

        assertNotNull(result)
        assertEquals(frameIndex, result!!.frameIndex)
        assertEquals(numParts * 1000, result.data.size)

        // Verify data integrity
        for (i in 0 until numParts) {
            val expectedPart = parts[i]
            val actualPart = result.data.copyOfRange(i * 1000, (i + 1) * 1000)
            assertArrayEquals(expectedPart, actualPart)
        }
    }

    @Test
    fun testStatistics() {
        // Assemble one complete frame
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, true, 1u, "Frame1".toByteArray())
        )

        // Add one incomplete frame
        assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, false, 2u, "Frame2Part0".toByteArray())
        )

        val stats = assembler.getStats()
        assertEquals(1, stats.pendingFrames)
        assertEquals(1, stats.assembledFrames)
        assertEquals(1u, stats.lastCompletedFrameIndex)
    }

    @Test
    fun testOutOfOrderFrameDrop() {
        // Assemble frames 1, 2, 3
        for (i in 1..3) {
            assembler.addFramePart(
                UsbFrame.VideoFrame(Resolution.VGA, 0u, true, i.toUInt(), "Frame$i".toByteArray())
            )
        }

        // Try to add a very old frame (should be dropped)
        val result = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, true, 1u, "OldFrame".toByteArray())
        )

        // Should be null because frame is too old
        assertNull(result)

        val stats = assembler.getStats()
        assertEquals(3u, stats.lastCompletedFrameIndex)
    }

    @Test
    fun testDifferentResolutions() {
        // Frame 1 at VGA
        val frame1 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.VGA, 0u, true, 1u, "VGA".toByteArray())
        )
        assertEquals(Resolution.VGA, frame1!!.resolution)

        // Frame 2 at HD
        val frame2 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.HD, 0u, true, 2u, "HD".toByteArray())
        )
        assertEquals(Resolution.HD, frame2!!.resolution)

        // Frame 3 at QVGA
        val frame3 = assembler.addFramePart(
            UsbFrame.VideoFrame(Resolution.QVGA, 0u, true, 3u, "QVGA".toByteArray())
        )
        assertEquals(Resolution.QVGA, frame3!!.resolution)
    }
}

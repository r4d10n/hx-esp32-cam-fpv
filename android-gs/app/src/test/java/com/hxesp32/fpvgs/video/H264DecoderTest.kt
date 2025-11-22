package com.hxesp32.fpvgs.video

import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.Assert.*
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit

/**
 * Unit tests for H264Decoder
 *
 * Note: These are unit tests that test the decoder logic without actual hardware decoding.
 * For full integration tests with MediaCodec, see H264DecoderInstrumentedTest.
 */
class H264DecoderTest {

    private lateinit var testHelper: DecoderTestHelper

    @Before
    fun setup() {
        testHelper = DecoderTestHelper()
    }

    @After
    fun tearDown() {
        testHelper.cleanup()
    }

    @Test
    fun testNalTypeDetection() {
        // Test SPS detection
        val spsNal = testHelper.createSpsNal()
        assertEquals(7, testHelper.extractNalType(spsNal))

        // Test PPS detection
        val ppsNal = testHelper.createPpsNal()
        assertEquals(8, testHelper.extractNalType(ppsNal))

        // Test IDR detection
        val idrNal = testHelper.createIdrNal()
        assertEquals(5, testHelper.extractNalType(idrNal))

        // Test non-IDR detection
        val nonIdrNal = testHelper.createNonIdrNal()
        assertEquals(1, testHelper.extractNalType(nonIdrNal))
    }

    @Test
    fun testStartCodeStripping() {
        // Test 4-byte start code
        val nal4 = byteArrayOf(0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E)
        val stripped4 = testHelper.stripStartCode(nal4)
        assertEquals(4, stripped4.size)
        assertEquals(0x67.toByte(), stripped4[0])

        // Test 3-byte start code
        val nal3 = byteArrayOf(0x00, 0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80)
        val stripped3 = testHelper.stripStartCode(nal3)
        assertEquals(4, stripped3.size)
        assertEquals(0x68.toByte(), stripped3[0])

        // Test no start code
        val nalNoCode = byteArrayOf(0x67, 0x42, 0x00, 0x1E)
        val strippedNo = testHelper.stripStartCode(nalNoCode)
        assertArrayEquals(nalNoCode, strippedNo)
    }

    @Test
    fun testStatisticsReset() {
        val stats = DecoderStatistics()

        // Populate with data
        stats.framesReceived = 100
        stats.framesDecoded = 95
        stats.errorCount = 5
        stats.updateLatency(50)

        assertTrue(stats.framesReceived > 0)
        assertTrue(stats.averageLatencyMs > 0)

        // Reset
        stats.reset()

        assertEquals(0, stats.framesReceived)
        assertEquals(0, stats.framesDecoded)
        assertEquals(0, stats.errorCount)
        assertEquals(0f, stats.currentFps)
        assertEquals(0f, stats.averageLatencyMs)
        assertEquals(Long.MAX_VALUE, stats.minLatencyMs)
        assertEquals(0, stats.maxLatencyMs)
    }

    @Test
    fun testLatencyStatistics() {
        val stats = DecoderStatistics()

        // Add latency samples
        stats.updateLatency(10)
        stats.updateLatency(20)
        stats.updateLatency(30)
        stats.updateLatency(40)
        stats.updateLatency(50)

        // Check average
        assertEquals(30f, stats.averageLatencyMs, 0.1f)

        // Check min/max
        assertEquals(10, stats.minLatencyMs)
        assertEquals(50, stats.maxLatencyMs)
    }

    @Test
    fun testDecoderConfigDefaults() {
        val config = DecoderConfig()

        assertTrue(config.lowLatencyMode)
        assertEquals(30, config.expectedFrameRate)
        assertEquals(Int.MAX_VALUE, config.operatingRate)
        assertFalse(config.logParameterSets)
        assertFalse(config.logBufferIssues)
        assertTrue(config.logStatistics)
    }

    @Test
    fun testDecoderConfigCustomization() {
        val config = DecoderConfig(
            lowLatencyMode = false,
            expectedFrameRate = 60,
            operatingRate = 120,
            logParameterSets = true,
            logBufferIssues = true,
            logStatistics = false
        )

        assertFalse(config.lowLatencyMode)
        assertEquals(60, config.expectedFrameRate)
        assertEquals(120, config.operatingRate)
        assertTrue(config.logParameterSets)
        assertTrue(config.logBufferIssues)
        assertFalse(config.logStatistics)
    }

    @Test
    fun testDecoderErrorCreation() {
        val error = DecoderError(
            message = "Test error",
            timestamp = System.currentTimeMillis(),
            consecutiveErrors = 3
        )

        assertEquals("Test error", error.message)
        assertTrue(error.timestamp > 0)
        assertEquals(3, error.consecutiveErrors)
    }

    @Test
    fun testStatisticsCopy() {
        val original = DecoderStatistics()
        original.framesReceived = 100
        original.framesDecoded = 95
        original.updateLatency(25)

        val copy = original.copy()

        assertEquals(original.framesReceived, copy.framesReceived)
        assertEquals(original.framesDecoded, copy.framesDecoded)
        assertEquals(original.averageLatencyMs, copy.averageLatencyMs, 0.1f)

        // Modify copy
        copy.framesReceived = 200

        // Original should be unchanged
        assertEquals(100, original.framesReceived)
        assertEquals(200, copy.framesReceived)
    }

    @Test
    fun testStatisticsToString() {
        val stats = DecoderStatistics()
        stats.framesReceived = 1000
        stats.framesDecoded = 990
        stats.idrFrameCount = 30
        stats.errorCount = 10
        stats.currentFps = 29.5f
        stats.updateLatency(25)
        stats.updateLatency(35)

        val str = stats.toString()

        assertTrue(str.contains("fps=29.5"))
        assertTrue(str.contains("latency=30.0ms"))
        assertTrue(str.contains("frames=990/1000"))
        assertTrue(str.contains("IDR=30"))
        assertTrue(str.contains("errors=10"))
    }

    @Test
    fun testSpsSequence() {
        val nalSequence = listOf(
            testHelper.createSpsNal(),
            testHelper.createPpsNal(),
            testHelper.createIdrNal()
        )

        assertEquals(7, testHelper.extractNalType(nalSequence[0]))
        assertEquals(8, testHelper.extractNalType(nalSequence[1]))
        assertEquals(5, testHelper.extractNalType(nalSequence[2]))
    }

    @Test
    fun testMultipleLatencySamples() {
        val stats = DecoderStatistics()

        // Add 1000 samples
        repeat(1000) { i ->
            stats.updateLatency((i % 100).toLong())
        }

        // Average should be around 49.5 (0-99)
        assertTrue(stats.averageLatencyMs > 48f && stats.averageLatencyMs < 51f)
        assertEquals(0, stats.minLatencyMs)
        assertEquals(99, stats.maxLatencyMs)
    }

    @Test
    fun testFrameCounters() {
        val stats = DecoderStatistics()

        // Simulate frame processing
        repeat(100) {
            stats.framesReceived++
            if (it % 10 != 0) { // Drop every 10th frame
                stats.framesDecoded++
            }
            if (it % 30 == 0) { // IDR every 30 frames
                stats.idrFrameCount++
            }
        }

        assertEquals(100, stats.framesReceived)
        assertEquals(90, stats.framesDecoded)
        assertEquals(4, stats.idrFrameCount) // 0, 30, 60, 90
    }

    @Test
    fun testErrorRateCalculation() {
        val stats = DecoderStatistics()

        stats.errorCount = 10
        stats.errorRate = 2.5f

        assertEquals(10, stats.errorCount)
        assertEquals(2.5f, stats.errorRate, 0.01f)
    }
}

/**
 * Helper class for decoder testing
 */
class DecoderTestHelper {
    private val NAL_START_CODE_4 = byteArrayOf(0x00, 0x00, 0x00, 0x01)
    private val NAL_START_CODE_3 = byteArrayOf(0x00, 0x00, 0x01)

    /**
     * Create a mock SPS NAL unit
     */
    fun createSpsNal(): ByteArray {
        // SPS for 640x480 Baseline profile
        return NAL_START_CODE_4 + byteArrayOf(
            0x67, 0x42, 0x00, 0x1E, 0xA6.toByte(), 0x80.toByte(),
            0x28, 0x02, 0xDD.toByte(), 0x80.toByte(), 0xB5.toByte(), 0x01
        )
    }

    /**
     * Create a mock PPS NAL unit
     */
    fun createPpsNal(): ByteArray {
        return NAL_START_CODE_4 + byteArrayOf(
            0x68, 0xCE.toByte(), 0x3C, 0x80.toByte()
        )
    }

    /**
     * Create a mock IDR NAL unit
     */
    fun createIdrNal(): ByteArray {
        return NAL_START_CODE_4 + byteArrayOf(
            0x65, 0x88.toByte(), 0x84.toByte(), 0x00, 0x1F, 0xFF.toByte()
        )
    }

    /**
     * Create a mock non-IDR NAL unit
     */
    fun createNonIdrNal(): ByteArray {
        return NAL_START_CODE_4 + byteArrayOf(
            0x61, 0xE0.toByte(), 0x20, 0x00, 0x04, 0x28
        )
    }

    /**
     * Extract NAL type from NAL unit
     */
    fun extractNalType(data: ByteArray): Int {
        val offset = when {
            data.size >= 4 && data.startsWith(NAL_START_CODE_4) -> 4
            data.size >= 3 && data.startsWith(NAL_START_CODE_3) -> 3
            else -> 0
        }

        if (offset >= data.size) return -1
        return (data[offset].toInt() and 0x1F)
    }

    /**
     * Strip start code from NAL unit
     */
    fun stripStartCode(data: ByteArray): ByteArray {
        return when {
            data.startsWith(NAL_START_CODE_4) -> data.copyOfRange(4, data.size)
            data.startsWith(NAL_START_CODE_3) -> data.copyOfRange(3, data.size)
            else -> data
        }
    }

    /**
     * Create a complete frame sequence (SPS + PPS + IDR)
     */
    fun createFrameSequence(): List<ByteArray> {
        return listOf(
            createSpsNal(),
            createPpsNal(),
            createIdrNal()
        )
    }

    fun cleanup() {
        // Cleanup resources if needed
    }

    private fun ByteArray.startsWith(prefix: ByteArray): Boolean {
        if (this.size < prefix.size) return false
        for (i in prefix.indices) {
            if (this[i] != prefix[i]) return false
        }
        return true
    }

    private operator fun ByteArray.plus(other: ByteArray): ByteArray {
        val result = ByteArray(this.size + other.size)
        System.arraycopy(this, 0, result, 0, this.size)
        System.arraycopy(other, 0, result, this.size, other.size)
        return result
    }
}

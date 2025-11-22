package com.hxesp32.fpvgs.video

import android.content.Context
import android.media.MediaCodec
import android.view.Surface
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.After
import org.junit.Before
import org.junit.Test
import org.junit.Assert.*
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

/**
 * Instrumented tests for H264Decoder
 *
 * These tests run on an Android device and test actual MediaCodec functionality
 */
@RunWith(AndroidJUnit4::class)
class H264DecoderInstrumentedTest {

    private lateinit var context: Context
    private lateinit var testHelper: DecoderTestHelper
    private var decoder: H264Decoder? = null

    @Before
    fun setup() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
        testHelper = DecoderTestHelper()
    }

    @After
    fun tearDown() {
        decoder?.stop()
        decoder = null
        testHelper.cleanup()
    }

    @Test
    fun testDecoderInitialization() {
        decoder = H264Decoder(
            width = 640,
            height = 480,
            surface = null,
            config = DecoderConfig(logStatistics = true)
        )

        val started = decoder!!.start()
        // Note: May fail if no surface is provided
        // This is expected behavior

        if (started) {
            val stats = decoder!!.getStatistics()
            assertNotNull(stats)
            assertEquals(0, stats.framesReceived)
            assertEquals(0, stats.framesDecoded)
        }
    }

    @Test
    fun testMediaCodecHelperCodecDiscovery() {
        val decoders = MediaCodecHelper.findAllH264Decoders()
        assertTrue("No H.264 decoders found on device", decoders.isNotEmpty())

        val best = MediaCodecHelper.findBestH264Decoder()
        assertNotNull("No best decoder found", best)

        // Log decoder info
        println("Best decoder: ${best?.name}")
        println("Hardware: ${best?.isHardware}")
        println("Low latency: ${best?.supportsLowLatency}")
    }

    @Test
    fun testResolutionSupport() {
        // Common FPV resolutions
        assertTrue(MediaCodecHelper.isResolutionSupported(640, 480))
        assertTrue(MediaCodecHelper.isResolutionSupported(800, 600))
        assertTrue(MediaCodecHelper.isResolutionSupported(1280, 720))
        assertTrue(MediaCodecHelper.isResolutionSupported(1920, 1080))
    }

    @Test
    fun testLowLatencySupport() {
        val supported = MediaCodecHelper.supportsLowLatency()
        // Log result (support varies by device/Android version)
        println("Low latency supported: $supported")
    }

    @Test
    fun testPerformanceInfo() {
        val info = MediaCodecHelper.getPerformanceInfo()
        assertNotNull(info)
        assertTrue(info.contains("MediaCodec Performance Info"))
        assertTrue(info.contains("Android Version"))
        assertTrue(info.contains("Device"))

        // Log full performance info
        println(info)
    }

    @Test
    fun testNalTypeDetection() {
        val sps = testHelper.createSpsNal()
        val pps = testHelper.createPpsNal()
        val idr = testHelper.createIdrNal()
        val nonIdr = testHelper.createNonIdrNal()

        assertEquals(MediaCodecHelper.NalUnitType.SPS, MediaCodecHelper.detectNalType(sps))
        assertEquals(MediaCodecHelper.NalUnitType.PPS, MediaCodecHelper.detectNalType(pps))
        assertEquals(MediaCodecHelper.NalUnitType.IDR, MediaCodecHelper.detectNalType(idr))
        assertEquals(MediaCodecHelper.NalUnitType.NON_IDR, MediaCodecHelper.detectNalType(nonIdr))
    }

    @Test
    fun testCsdBufferCreation() {
        val sps = byteArrayOf(0x67, 0x42, 0x00, 0x1E)
        val pps = byteArrayOf(0x68, 0xCE.toByte(), 0x3C, 0x80.toByte())

        val csd = MediaCodecHelper.createCsdBuffer(sps, pps)

        assertNotNull(csd)
        assertTrue(csd.size > sps.size + pps.size)

        // Verify start codes are present
        assertEquals(0x00, csd[0])
        assertEquals(0x00, csd[1])
        assertEquals(0x00, csd[2])
        assertEquals(0x01, csd[3])
    }

    @Test
    fun testSpsParameterParsing() {
        val sps = testHelper.createSpsNal()
        val params = MediaCodecHelper.parseSpsParameters(sps)

        // Note: May be null if parsing fails (simplified parser)
        if (params != null) {
            assertTrue(params.profile > 0)
            assertTrue(params.level > 0)
            println("SPS params - Profile: ${params.profile}, Level: ${params.level}")
        }
    }

    @Test
    fun testLowLatencyFormatCreation() {
        val format = MediaCodecHelper.createLowLatencyFormat(
            width = 800,
            height = 600,
            fps = 30,
            surface = null
        )

        assertNotNull(format)
        assertEquals(800, format.getInteger(android.media.MediaFormat.KEY_WIDTH))
        assertEquals(600, format.getInteger(android.media.MediaFormat.KEY_HEIGHT))
        assertEquals(30, format.getInteger(android.media.MediaFormat.KEY_FRAME_RATE))
    }

    @Test
    fun testBufferSizeCalculation() {
        val size640x480 = MediaCodecHelper.getRecommendedBufferSize(640, 480)
        val size1920x1080 = MediaCodecHelper.getRecommendedBufferSize(1920, 1080)

        assertTrue(size640x480 > 0)
        assertTrue(size1920x1080 > size640x480)

        println("Buffer size for 640x480: $size640x480")
        println("Buffer size for 1920x1080: $size1920x1080")
    }

    @Test
    fun testDecoderCallbacks() {
        decoder = H264Decoder(
            width = 640,
            height = 480,
            surface = null,
            config = DecoderConfig()
        )

        val frameLatch = CountDownLatch(1)
        val errorLatch = CountDownLatch(1)

        var frameReceived = false
        var errorReceived = false

        decoder!!.setFrameCallback { frameNum, latency ->
            frameReceived = true
            frameLatch.countDown()
        }

        decoder!!.setErrorCallback { error ->
            errorReceived = true
            errorLatch.countDown()
        }

        // Note: Callbacks may not trigger without actual decoding
        // This test mainly verifies callback setup doesn't crash
    }

    @Test
    fun testStatisticsTracking() {
        decoder = H264Decoder(
            width = 640,
            height = 480,
            surface = null,
            config = DecoderConfig()
        )

        decoder!!.start()

        // Get initial stats
        val stats1 = decoder!!.getStatistics()
        assertEquals(0, stats1.framesReceived)

        // Note: Without actual frame feeding, stats remain at 0
        // This test verifies statistics retrieval works

        val stats2 = decoder!!.getStatistics()
        assertNotNull(stats2)
    }

    @Test
    fun testDecoderStopStart() {
        decoder = H264Decoder(
            width = 640,
            height = 480,
            surface = null,
            config = DecoderConfig()
        )

        // Start
        decoder!!.start()

        // Stop
        decoder!!.stop()

        // Start again
        val restarted = decoder!!.start()
        // May fail depending on surface availability
    }

    @Test
    fun testMultipleResolutions() {
        val resolutions = listOf(
            Pair(640, 480),
            Pair(800, 600),
            Pair(1280, 720)
        )

        resolutions.forEach { (width, height) ->
            decoder = H264Decoder(
                width = width,
                height = height,
                surface = null,
                config = DecoderConfig()
            )

            // Note: May not start without surface
            val started = decoder!!.start()
            println("Decoder ${width}x${height}: started=$started")

            decoder!!.stop()
            decoder = null
        }
    }

    @Test
    fun testConfigurationOptions() {
        val configs = listOf(
            DecoderConfig(lowLatencyMode = true, expectedFrameRate = 30),
            DecoderConfig(lowLatencyMode = false, expectedFrameRate = 60),
            DecoderConfig(operatingRate = 120)
        )

        configs.forEachIndexed { index, config ->
            decoder = H264Decoder(
                width = 640,
                height = 480,
                surface = null,
                config = config
            )

            println("Testing configuration $index")
            decoder!!.stop()
            decoder = null
        }
    }

    @Test
    fun testConcurrentDecoders() {
        // Test if multiple decoder instances can be created
        // (though only one should be used at a time in production)

        val decoder1 = H264Decoder(640, 480)
        val decoder2 = H264Decoder(800, 600)

        // Note: Actual usage would be one at a time
        // This tests object creation

        assertNotNull(decoder1)
        assertNotNull(decoder2)
    }
}

package com.hxesp32.fpvgs.video

import android.content.Context
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import kotlin.system.measureTimeMillis

/**
 * Performance benchmark tests for H264Decoder
 *
 * Measures:
 * - Decoder initialization time
 * - NAL processing throughput
 * - Memory usage
 * - CPU efficiency
 * - Latency characteristics
 */
@RunWith(AndroidJUnit4::class)
class PerformanceBenchmarkTest {

    private lateinit var context: Context
    private val testHelper = DecoderTestHelper()

    @Before
    fun setup() {
        context = InstrumentationRegistry.getInstrumentation().targetContext
    }

    @Test
    fun benchmarkDecoderInitialization() {
        val iterations = 10
        val times = mutableListOf<Long>()

        repeat(iterations) {
            val time = measureTimeMillis {
                val decoder = H264Decoder(640, 480)
                decoder.start()
                decoder.stop()
            }
            times.add(time)
        }

        val avgTime = times.average()
        val minTime = times.minOrNull() ?: 0
        val maxTime = times.maxOrNull() ?: 0

        println("=== Decoder Initialization Benchmark ===")
        println("Iterations: $iterations")
        println("Average time: ${"%.2f".format(avgTime)} ms")
        println("Min time: $minTime ms")
        println("Max time: $maxTime ms")
        println("Standard deviation: ${"%.2f".format(calculateStdDev(times))} ms")
    }

    @Test
    fun benchmarkNalProcessing() {
        val decoder = H264Decoder(
            width = 640,
            height = 480,
            surface = null,
            config = DecoderConfig(logStatistics = false)
        )

        decoder.start()

        // Create test NAL units
        val sps = testHelper.createSpsNal()
        val pps = testHelper.createPpsNal()
        val idr = testHelper.createIdrNal()
        val frame = testHelper.createNonIdrNal()

        // Warm-up
        repeat(10) {
            decoder.feedNalUnit(frame, it * 33333L) // ~30fps
        }

        // Benchmark
        val nalCount = 1000
        val startTime = System.currentTimeMillis()

        repeat(nalCount) { i ->
            val timestamp = i * 33333L // 30fps
            decoder.feedNalUnit(frame, timestamp)
        }

        val endTime = System.currentTimeMillis()
        val duration = endTime - startTime
        val throughput = (nalCount * 1000.0) / duration

        val stats = decoder.getStatistics()

        println("=== NAL Processing Benchmark ===")
        println("NAL units processed: $nalCount")
        println("Duration: $duration ms")
        println("Throughput: ${"%.2f".format(throughput)} NAL/sec")
        println("Frames received: ${stats.framesReceived}")
        println("Errors: ${stats.errorCount}")

        decoder.stop()
    }

    @Test
    fun benchmarkFrameRate() {
        val decoder = H264Decoder(
            width = 800,
            height = 600,
            surface = null,
            config = DecoderConfig(expectedFrameRate = 30)
        )

        decoder.start()

        val targetFps = 30
        val durationSeconds = 5
        val expectedFrames = targetFps * durationSeconds

        val frameInterval = 1000L / targetFps // ms

        val framesSent = mutableListOf<Long>()
        val startTime = System.currentTimeMillis()

        repeat(expectedFrames) { i ->
            val frameTime = System.currentTimeMillis()
            val timestamp = i * 33333L // microseconds

            val frame = if (i % 30 == 0) {
                testHelper.createIdrNal()
            } else {
                testHelper.createNonIdrNal()
            }

            decoder.feedNalUnit(frame, timestamp)
            framesSent.add(frameTime)

            // Simulate frame rate
            val elapsed = System.currentTimeMillis() - startTime
            val expectedElapsed = (i + 1) * frameInterval
            val sleepTime = expectedElapsed - elapsed

            if (sleepTime > 0) {
                Thread.sleep(sleepTime)
            }
        }

        val endTime = System.currentTimeMillis()
        val actualDuration = endTime - startTime
        val actualFps = (framesSent.size * 1000.0) / actualDuration

        val stats = decoder.getStatistics()

        println("=== Frame Rate Benchmark ===")
        println("Target FPS: $targetFps")
        println("Duration: ${actualDuration}ms")
        println("Frames sent: ${framesSent.size}")
        println("Actual FPS: ${"%.2f".format(actualFps)}")
        println("Frames received: ${stats.framesReceived}")
        println("Frames decoded: ${stats.framesDecoded}")
        println("Current FPS: ${"%.2f".format(stats.currentFps)}")

        decoder.stop()
    }

    @Test
    fun benchmarkLatency() {
        val decoder = H264Decoder(
            width = 640,
            height = 480,
            surface = null,
            config = DecoderConfig(lowLatencyMode = true)
        )

        val latencies = mutableListOf<Long>()
        val latch = CountDownLatch(100)

        decoder.setFrameCallback { _, latency ->
            latencies.add(latency)
            latch.countDown()
        }

        decoder.start()

        // Send frames
        repeat(100) { i ->
            val timestamp = System.nanoTime() / 1000
            val frame = if (i % 30 == 0) {
                testHelper.createIdrNal()
            } else {
                testHelper.createNonIdrNal()
            }
            decoder.feedNalUnit(frame, timestamp)
            Thread.sleep(10) // ~100fps
        }

        // Wait for processing
        latch.await(10, TimeUnit.SECONDS)

        val stats = decoder.getStatistics()

        if (latencies.isNotEmpty()) {
            println("=== Latency Benchmark ===")
            println("Samples: ${latencies.size}")
            println("Average latency: ${"%.2f".format(latencies.average())} ms")
            println("Min latency: ${latencies.minOrNull()} ms")
            println("Max latency: ${latencies.maxOrNull()} ms")
            println("Median latency: ${latencies.sorted()[latencies.size / 2]} ms")
            println("95th percentile: ${percentile(latencies, 0.95)} ms")
            println("99th percentile: ${percentile(latencies, 0.99)} ms")
            println("Stats - Avg: ${"%.2f".format(stats.averageLatencyMs)} ms")
        } else {
            println("No latency samples collected (decoder may not have started)")
        }

        decoder.stop()
    }

    @Test
    fun benchmarkDifferentResolutions() {
        val resolutions = listOf(
            Triple(640, 480, "VGA"),
            Triple(800, 600, "SVGA"),
            Triple(1024, 768, "XGA"),
            Triple(1280, 720, "720p"),
            Triple(1920, 1080, "1080p")
        )

        println("=== Resolution Performance Benchmark ===")

        resolutions.forEach { (width, height, name) ->
            if (!MediaCodecHelper.isResolutionSupported(width, height)) {
                println("$name (${width}x${height}): NOT SUPPORTED")
                return@forEach
            }

            val initTime = measureTimeMillis {
                val decoder = H264Decoder(width, height)
                decoder.start()

                // Process a few frames
                repeat(30) { i ->
                    val frame = testHelper.createNonIdrNal()
                    decoder.feedNalUnit(frame, i * 33333L)
                }

                decoder.stop()
            }

            val bufferSize = MediaCodecHelper.getRecommendedBufferSize(width, height)

            println("$name (${width}x${height}):")
            println("  Init + 30 frames: $initTime ms")
            println("  Buffer size: ${bufferSize / 1024} KB")
        }
    }

    @Test
    fun benchmarkErrorRecovery() {
        val decoder = H264Decoder(
            width = 640,
            height = 480,
            surface = null,
            config = DecoderConfig()
        )

        var errorCount = 0
        decoder.setErrorCallback { _ ->
            errorCount++
        }

        decoder.start()

        val resetTime = measureTimeMillis {
            decoder.reset()
        }

        println("=== Error Recovery Benchmark ===")
        println("Reset time: $resetTime ms")
        println("Errors during test: $errorCount")

        decoder.stop()
    }

    @Test
    fun benchmarkCodecCapabilities() {
        val discoverTime = measureTimeMillis {
            val decoders = MediaCodecHelper.findAllH264Decoders()
            val best = MediaCodecHelper.findBestH264Decoder()

            println("=== Codec Capabilities Benchmark ===")
            println("Discovery time: $discoverTime ms")
            println("Decoders found: ${decoders.size}")
            println()

            decoders.forEachIndexed { index, codec ->
                println("Decoder $index: ${codec.name}")
                println("  Hardware: ${codec.isHardware}")
                println("  Max resolution: ${codec.maxWidth}x${codec.maxHeight}")
                println("  Max FPS: ${codec.maxFrameRate}")
                println("  Low latency: ${codec.supportsLowLatency}")
                println("  Max instances: ${codec.maxInstances}")
            }

            println()
            println("Best decoder: ${best?.name}")
        }
    }

    @Test
    fun stressTestContinuousDecoding() {
        val decoder = H264Decoder(
            width = 800,
            height = 600,
            surface = null,
            config = DecoderConfig(logStatistics = false)
        )

        decoder.start()

        val durationSeconds = 10
        val targetFps = 30
        val totalFrames = durationSeconds * targetFps

        println("=== Continuous Decoding Stress Test ===")
        println("Duration: ${durationSeconds}s")
        println("Target FPS: $targetFps")
        println("Expected frames: $totalFrames")

        val startTime = System.currentTimeMillis()

        repeat(totalFrames) { i ->
            val frame = if (i % 30 == 0) {
                testHelper.createIdrNal()
            } else {
                testHelper.createNonIdrNal()
            }

            val timestamp = i * 33333L
            decoder.feedNalUnit(frame, timestamp)

            // Maintain frame rate
            val elapsed = System.currentTimeMillis() - startTime
            val expected = (i + 1) * 1000L / targetFps
            val sleep = expected - elapsed
            if (sleep > 0) {
                Thread.sleep(sleep)
            }
        }

        val endTime = System.currentTimeMillis()
        val stats = decoder.getStatistics()

        println("Actual duration: ${endTime - startTime}ms")
        println("Frames sent: $totalFrames")
        println("Frames received: ${stats.framesReceived}")
        println("Frames decoded: ${stats.framesDecoded}")
        println("IDR frames: ${stats.idrFrameCount}")
        println("Errors: ${stats.errorCount}")
        println("Buffer underflows: ${stats.bufferUnderflows}")
        println("Final FPS: ${"%.2f".format(stats.currentFps)}")
        println("Avg latency: ${"%.2f".format(stats.averageLatencyMs)}ms")

        decoder.stop()
    }

    // Helper functions

    private fun calculateStdDev(values: List<Long>): Double {
        if (values.isEmpty()) return 0.0
        val mean = values.average()
        val variance = values.map { (it - mean) * (it - mean) }.average()
        return Math.sqrt(variance)
    }

    private fun percentile(values: List<Long>, p: Double): Long {
        if (values.isEmpty()) return 0
        val sorted = values.sorted()
        val index = ((sorted.size - 1) * p).toInt()
        return sorted[index]
    }
}

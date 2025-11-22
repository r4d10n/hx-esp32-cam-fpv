package com.hxesp32.fpvgs.video

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.os.Handler
import android.os.HandlerThread
import android.util.Log
import android.view.Surface
import java.nio.ByteBuffer
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicLong
import kotlin.math.abs

/**
 * H.264 Video Decoder optimized for low-latency FPV applications
 *
 * Features:
 * - MediaCodec-based hardware acceleration
 * - Low-latency configuration for minimal buffering
 * - SPS/PPS parameter set handling
 * - IDR frame detection for decoder recovery
 * - Timestamp management for latency measurement
 * - Error recovery with automatic decoder reset
 * - Real-time statistics (FPS, latency, errors)
 * - Surface rendering integration
 */
class H264Decoder(
    private val width: Int,
    private val height: Int,
    private val surface: Surface? = null,
    private val config: DecoderConfig = DecoderConfig()
) {
    companion object {
        private const val TAG = "H264Decoder"
        private const val MIME_TYPE = MediaFormat.MIMETYPE_VIDEO_AVC
        private const val TIMEOUT_US = 10000L // 10ms timeout for low latency

        // NAL unit types
        private const val NAL_TYPE_SPS = 7
        private const val NAL_TYPE_PPS = 8
        private const val NAL_TYPE_IDR = 5
        private const val NAL_TYPE_NON_IDR = 1

        // NAL unit start codes
        private val NAL_START_CODE_3 = byteArrayOf(0x00, 0x00, 0x01)
        private val NAL_START_CODE_4 = byteArrayOf(0x00, 0x00, 0x00, 0x01)
    }

    // Decoder state
    private var mediaCodec: MediaCodec? = null
    private var codecThread: HandlerThread? = null
    private var codecHandler: Handler? = null
    private val isRunning = AtomicBoolean(false)
    private val isConfigured = AtomicBoolean(false)

    // SPS/PPS tracking
    private var spsData: ByteArray? = null
    private var ppsData: ByteArray? = null
    private var lastSpsTimestamp = 0L
    private var lastPpsTimestamp = 0L

    // Statistics
    private val stats = DecoderStatistics()
    private var lastStatsTime = System.currentTimeMillis()
    private var framesSinceLastStats = 0
    private var errorsSinceLastStats = 0

    // Frame tracking
    private var lastFrameTime = 0L
    private var consecutiveErrors = 0
    private val maxConsecutiveErrors = 10

    // Callbacks
    private var frameCallback: ((frameNumber: Long, latencyMs: Long) -> Unit)? = null
    private var errorCallback: ((error: DecoderError) -> Unit)? = null

    /**
     * Initialize and start the decoder
     */
    @Synchronized
    fun start(): Boolean {
        if (isRunning.get()) {
            Log.w(TAG, "Decoder already running")
            return true
        }

        try {
            // Create codec thread for asynchronous processing
            codecThread = HandlerThread("H264DecoderThread").apply {
                start()
                codecHandler = Handler(looper)
            }

            // Initialize MediaCodec
            if (!initializeCodec()) {
                return false
            }

            isRunning.set(true)
            stats.reset()
            lastStatsTime = System.currentTimeMillis()

            Log.i(TAG, "Decoder started successfully: ${width}x${height}")
            return true

        } catch (e: Exception) {
            Log.e(TAG, "Failed to start decoder", e)
            cleanup()
            return false
        }
    }

    /**
     * Stop the decoder and release resources
     */
    @Synchronized
    fun stop() {
        if (!isRunning.get()) {
            return
        }

        isRunning.set(false)
        cleanup()

        Log.i(TAG, "Decoder stopped. Final stats: $stats")
    }

    /**
     * Feed a NAL unit to the decoder
     *
     * @param data NAL unit data (with or without start code)
     * @param timestamp Presentation timestamp in microseconds
     * @param flags MediaCodec buffer flags (default: 0)
     */
    fun feedNalUnit(data: ByteArray, timestamp: Long = 0, flags: Int = 0): Boolean {
        if (!isRunning.get()) {
            Log.w(TAG, "Cannot feed NAL unit - decoder not running")
            return false
        }

        try {
            val nalType = getNalType(data)
            val nalData = stripStartCode(data)

            // Handle SPS/PPS
            when (nalType) {
                NAL_TYPE_SPS -> {
                    handleSps(nalData, timestamp)
                    return true
                }
                NAL_TYPE_PPS -> {
                    handlePps(nalData, timestamp)
                    return true
                }
                NAL_TYPE_IDR -> {
                    // IDR frame - configure decoder if needed
                    if (!isConfigured.get() && spsData != null && ppsData != null) {
                        configureCodecWithParameters()
                    }
                    stats.idrFrameCount++
                }
            }

            // Feed to decoder
            val codec = mediaCodec ?: return false

            val inputBufferIndex = codec.dequeueInputBuffer(TIMEOUT_US)
            if (inputBufferIndex >= 0) {
                val inputBuffer = codec.getInputBuffer(inputBufferIndex)
                inputBuffer?.let {
                    it.clear()
                    it.put(nalData)

                    val presentationTimeUs = if (timestamp > 0) {
                        timestamp
                    } else {
                        System.nanoTime() / 1000
                    }

                    codec.queueInputBuffer(
                        inputBufferIndex,
                        0,
                        nalData.size,
                        presentationTimeUs,
                        flags
                    )

                    stats.framesReceived++
                    lastFrameTime = System.currentTimeMillis()

                    // Process output buffers
                    processOutputBuffers()
                }
            } else {
                stats.bufferUnderflows++
                if (config.logBufferIssues) {
                    Log.w(TAG, "No input buffer available")
                }
            }

            consecutiveErrors = 0
            return true

        } catch (e: Exception) {
            handleDecoderError(e)
            return false
        }
    }

    /**
     * Set callback for decoded frames
     */
    fun setFrameCallback(callback: (frameNumber: Long, latencyMs: Long) -> Unit) {
        this.frameCallback = callback
    }

    /**
     * Set callback for decoder errors
     */
    fun setErrorCallback(callback: (error: DecoderError) -> Unit) {
        this.errorCallback = callback
    }

    /**
     * Get current decoder statistics
     */
    fun getStatistics(): DecoderStatistics {
        updateStatistics()
        return stats.copy()
    }

    /**
     * Reset decoder (useful for error recovery)
     */
    @Synchronized
    fun reset() {
        Log.i(TAG, "Resetting decoder")

        val wasRunning = isRunning.get()
        stop()

        if (wasRunning) {
            start()
        }
    }

    // ========== Private Methods ==========

    private fun initializeCodec(): Boolean {
        try {
            val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height).apply {
                // Low-latency configuration
                setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 0)
                setInteger(MediaFormat.KEY_PRIORITY, 0) // Realtime priority

                // Disable buffering for minimal latency
                if (config.lowLatencyMode) {
                    setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
                    setInteger(MediaFormat.KEY_LATENCY, 0)
                }

                // Color format
                setInteger(
                    MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface
                )

                // Frame rate hint (helps with buffer management)
                config.expectedFrameRate?.let {
                    setInteger(MediaFormat.KEY_FRAME_RATE, it)
                }

                // Operating rate for low latency
                if (config.operatingRate > 0) {
                    setInteger(MediaFormat.KEY_OPERATING_RATE, config.operatingRate)
                }
            }

            mediaCodec = MediaCodec.createDecoderByType(MIME_TYPE).apply {
                configure(format, surface, null, 0)
                start()
            }

            Log.i(TAG, "MediaCodec initialized: $format")
            return true

        } catch (e: Exception) {
            Log.e(TAG, "Failed to initialize codec", e)
            return false
        }
    }

    private fun configureCodecWithParameters() {
        try {
            val sps = spsData ?: return
            val pps = ppsData ?: return

            // Create CSD-0 buffer (SPS + PPS)
            val csd0 = ByteBuffer.allocate(sps.size + pps.size + 8)
            csd0.put(NAL_START_CODE_4)
            csd0.put(sps)
            csd0.put(NAL_START_CODE_4)
            csd0.put(pps)
            csd0.flip()

            val format = MediaFormat.createVideoFormat(MIME_TYPE, width, height).apply {
                setByteBuffer("csd-0", csd0)
            }

            // Reconfigure codec with SPS/PPS
            mediaCodec?.stop()
            mediaCodec?.configure(format, surface, null, 0)
            mediaCodec?.start()

            isConfigured.set(true)
            Log.i(TAG, "Codec configured with SPS/PPS")

        } catch (e: Exception) {
            Log.e(TAG, "Failed to configure codec with parameters", e)
        }
    }

    private fun handleSps(data: ByteArray, timestamp: Long) {
        spsData = data.copyOf()
        lastSpsTimestamp = timestamp
        stats.spsCount++

        if (config.logParameterSets) {
            Log.d(TAG, "SPS received: ${data.size} bytes")
        }
    }

    private fun handlePps(data: ByteArray, timestamp: Long) {
        ppsData = data.copyOf()
        lastPpsTimestamp = timestamp
        stats.ppsCount++

        if (config.logParameterSets) {
            Log.d(TAG, "PPS received: ${data.size} bytes")
        }
    }

    private fun processOutputBuffers() {
        val codec = mediaCodec ?: return

        try {
            val bufferInfo = MediaCodec.BufferInfo()
            var outputBufferIndex = codec.dequeueOutputBuffer(bufferInfo, 0)

            while (outputBufferIndex >= 0) {
                val currentTime = System.nanoTime() / 1000
                val latencyUs = currentTime - bufferInfo.presentationTimeUs
                val latencyMs = latencyUs / 1000

                // Update statistics
                stats.framesDecoded++
                framesSinceLastStats++

                if (latencyMs > 0 && latencyMs < 10000) { // Sanity check
                    stats.updateLatency(latencyMs)
                }

                // Release buffer to render
                codec.releaseOutputBuffer(outputBufferIndex, true)

                // Callback
                frameCallback?.invoke(stats.framesDecoded, latencyMs)

                // Check for more buffers
                outputBufferIndex = codec.dequeueOutputBuffer(bufferInfo, 0)
            }

            when (outputBufferIndex) {
                MediaCodec.INFO_OUTPUT_FORMAT_CHANGED -> {
                    val newFormat = codec.outputFormat
                    Log.d(TAG, "Output format changed: $newFormat")
                }
                MediaCodec.INFO_TRY_AGAIN_LATER -> {
                    // No output available right now
                }
            }

        } catch (e: Exception) {
            Log.e(TAG, "Error processing output buffers", e)
        }
    }

    private fun handleDecoderError(e: Exception) {
        consecutiveErrors++
        errorsSinceLastStats++
        stats.errorCount++

        Log.e(TAG, "Decoder error ($consecutiveErrors consecutive)", e)

        val error = DecoderError(
            message = e.message ?: "Unknown error",
            timestamp = System.currentTimeMillis(),
            consecutiveErrors = consecutiveErrors
        )
        errorCallback?.invoke(error)

        // Reset on too many consecutive errors
        if (consecutiveErrors >= maxConsecutiveErrors) {
            Log.e(TAG, "Too many consecutive errors, resetting decoder")
            codecHandler?.post {
                reset()
            }
        }
    }

    private fun updateStatistics() {
        val now = System.currentTimeMillis()
        val deltaMs = now - lastStatsTime

        if (deltaMs >= 1000) { // Update every second
            val fps = (framesSinceLastStats * 1000.0 / deltaMs).toFloat()
            stats.currentFps = fps
            stats.errorRate = (errorsSinceLastStats * 1000.0 / deltaMs).toFloat()

            framesSinceLastStats = 0
            errorsSinceLastStats = 0
            lastStatsTime = now

            if (config.logStatistics && fps > 0) {
                Log.d(TAG, "Stats: FPS=${"%.1f".format(fps)}, " +
                           "Latency=${"%.1f".format(stats.averageLatencyMs)}ms, " +
                           "Errors=${stats.errorCount}")
            }
        }
    }

    private fun cleanup() {
        try {
            mediaCodec?.stop()
            mediaCodec?.release()
            mediaCodec = null
        } catch (e: Exception) {
            Log.e(TAG, "Error stopping codec", e)
        }

        codecThread?.quitSafely()
        codecThread = null
        codecHandler = null

        isConfigured.set(false)
    }

    private fun getNalType(data: ByteArray): Int {
        val offset = when {
            data.size < 4 -> return -1
            data.startsWith(NAL_START_CODE_4) -> 4
            data.startsWith(NAL_START_CODE_3) -> 3
            else -> 0
        }

        if (offset >= data.size) return -1
        return (data[offset].toInt() and 0x1F)
    }

    private fun stripStartCode(data: ByteArray): ByteArray {
        return when {
            data.startsWith(NAL_START_CODE_4) -> data.copyOfRange(4, data.size)
            data.startsWith(NAL_START_CODE_3) -> data.copyOfRange(3, data.size)
            else -> data
        }
    }

    private fun ByteArray.startsWith(prefix: ByteArray): Boolean {
        if (this.size < prefix.size) return false
        for (i in prefix.indices) {
            if (this[i] != prefix[i]) return false
        }
        return true
    }
}

/**
 * Decoder configuration options
 */
data class DecoderConfig(
    /** Enable low-latency mode (minimal buffering) */
    val lowLatencyMode: Boolean = true,

    /** Expected frame rate (helps with buffer management) */
    val expectedFrameRate: Int? = 30,

    /** Operating rate for decoder scheduling */
    val operatingRate: Int = Int.MAX_VALUE,

    /** Log parameter sets (SPS/PPS) reception */
    val logParameterSets: Boolean = false,

    /** Log buffer availability issues */
    val logBufferIssues: Boolean = false,

    /** Log statistics periodically */
    val logStatistics: Boolean = true
)

/**
 * Decoder statistics
 */
data class DecoderStatistics(
    var framesReceived: Long = 0,
    var framesDecoded: Long = 0,
    var idrFrameCount: Long = 0,
    var spsCount: Long = 0,
    var ppsCount: Long = 0,
    var errorCount: Long = 0,
    var bufferUnderflows: Long = 0,

    var currentFps: Float = 0f,
    var averageLatencyMs: Float = 0f,
    var minLatencyMs: Long = Long.MAX_VALUE,
    var maxLatencyMs: Long = 0,
    var errorRate: Float = 0f,

    private var totalLatencyMs: Long = 0,
    private var latencySampleCount: Long = 0
) {
    fun reset() {
        framesReceived = 0
        framesDecoded = 0
        idrFrameCount = 0
        spsCount = 0
        ppsCount = 0
        errorCount = 0
        bufferUnderflows = 0
        currentFps = 0f
        averageLatencyMs = 0f
        minLatencyMs = Long.MAX_VALUE
        maxLatencyMs = 0
        errorRate = 0f
        totalLatencyMs = 0
        latencySampleCount = 0
    }

    fun updateLatency(latencyMs: Long) {
        totalLatencyMs += latencyMs
        latencySampleCount++
        averageLatencyMs = totalLatencyMs.toFloat() / latencySampleCount

        if (latencyMs < minLatencyMs) minLatencyMs = latencyMs
        if (latencyMs > maxLatencyMs) maxLatencyMs = latencyMs
    }

    fun copy(): DecoderStatistics {
        return DecoderStatistics(
            framesReceived, framesDecoded, idrFrameCount, spsCount, ppsCount,
            errorCount, bufferUnderflows, currentFps, averageLatencyMs,
            minLatencyMs, maxLatencyMs, errorRate, totalLatencyMs, latencySampleCount
        )
    }

    override fun toString(): String {
        return "DecoderStats(fps=${"%.1f".format(currentFps)}, " +
               "latency=${"%.1f".format(averageLatencyMs)}ms " +
               "(${minLatencyMs}-${maxLatencyMs}ms), " +
               "frames=$framesDecoded/$framesReceived, " +
               "IDR=$idrFrameCount, errors=$errorCount)"
    }
}

/**
 * Decoder error information
 */
data class DecoderError(
    val message: String,
    val timestamp: Long,
    val consecutiveErrors: Int
)

# H264Decoder Usage Examples

This document provides practical usage examples for the Android H.264 decoder in various scenarios.

## Table of Contents

1. [Basic Decoder Setup](#basic-decoder-setup)
2. [Integration with SurfaceView](#integration-with-surfaceview)
3. [Network Stream Decoding](#network-stream-decoding)
4. [Error Handling](#error-handling)
5. [Statistics Monitoring](#statistics-monitoring)
6. [Advanced Configuration](#advanced-configuration)

---

## Basic Decoder Setup

### Minimal Example

```kotlin
import android.view.Surface
import com.hxesp32.fpvgs.video.H264Decoder

class VideoPlayer {
    private var decoder: H264Decoder? = null

    fun start(surface: Surface, width: Int, height: Int) {
        // Create decoder
        decoder = H264Decoder(
            width = width,
            height = height,
            surface = surface
        )

        // Start decoder
        if (decoder!!.start()) {
            println("Decoder started successfully")
        } else {
            println("Failed to start decoder")
        }
    }

    fun feedFrame(nalUnit: ByteArray) {
        val timestamp = System.nanoTime() / 1000 // microseconds
        decoder?.feedNalUnit(nalUnit, timestamp)
    }

    fun stop() {
        decoder?.stop()
        decoder = null
    }
}
```

---

## Integration with SurfaceView

### Complete Activity Example

```kotlin
import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import androidx.appcompat.app.AppCompatActivity
import com.hxesp32.fpvgs.video.H264Decoder
import com.hxesp32.fpvgs.video.DecoderConfig

class VideoActivity : AppCompatActivity() {

    private lateinit var surfaceView: SurfaceView
    private var decoder: H264Decoder? = null
    private var isDecoderReady = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_video)

        surfaceView = findViewById(R.id.surfaceView)
        setupSurface()
    }

    private fun setupSurface() {
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                // Surface is ready, create decoder
                initializeDecoder(holder.surface)
            }

            override fun surfaceChanged(
                holder: SurfaceHolder,
                format: Int,
                width: Int,
                height: Int
            ) {
                // Handle surface size changes if needed
            }

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                // Clean up decoder
                stopDecoder()
            }
        })
    }

    private fun initializeDecoder(surface: Surface) {
        val config = DecoderConfig(
            lowLatencyMode = true,
            expectedFrameRate = 30,
            logStatistics = true
        )

        decoder = H264Decoder(
            width = 800,
            height = 600,
            surface = surface,
            config = config
        )

        // Set callbacks
        decoder?.setFrameCallback { frameNum, latency ->
            runOnUiThread {
                // Update UI with frame info
                updateFrameInfo(frameNum, latency)
            }
        }

        decoder?.setErrorCallback { error ->
            runOnUiThread {
                showError(error.message)
            }
        }

        // Start decoder
        isDecoderReady = decoder?.start() ?: false
    }

    private fun stopDecoder() {
        decoder?.stop()
        decoder = null
        isDecoderReady = false
    }

    private fun updateFrameInfo(frameNum: Long, latency: Long) {
        // Update UI
    }

    private fun showError(message: String) {
        // Show error to user
    }

    override fun onDestroy() {
        super.onDestroy()
        stopDecoder()
    }
}
```

### Layout XML

```xml
<?xml version="1.0" encoding="utf-8"?>
<FrameLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent">

    <SurfaceView
        android:id="@+id/surfaceView"
        android:layout_width="match_parent"
        android:layout_height="match_parent" />

</FrameLayout>
```

---

## Network Stream Decoding

### UDP Stream Receiver

```kotlin
import com.hxesp32.fpvgs.video.H264Decoder
import kotlinx.coroutines.*
import java.net.DatagramPacket
import java.net.DatagramSocket
import java.nio.ByteBuffer

class UdpVideoReceiver(
    private val decoder: H264Decoder,
    private val port: Int = 5000
) {
    private var socket: DatagramSocket? = null
    private var receiveJob: Job? = null
    private val scope = CoroutineScope(Dispatchers.IO)

    // NAL unit accumulator
    private val nalBuffer = ByteBuffer.allocate(1024 * 1024) // 1MB buffer
    private var frameTimestamp = 0L

    fun start() {
        socket = DatagramSocket(port)

        receiveJob = scope.launch {
            val buffer = ByteArray(65536) // Max UDP packet size
            val packet = DatagramPacket(buffer, buffer.size)

            while (isActive) {
                try {
                    // Receive packet
                    socket?.receive(packet)

                    // Process packet
                    processPacket(buffer, packet.length)

                } catch (e: Exception) {
                    if (isActive) {
                        println("Error receiving packet: ${e.message}")
                    }
                }
            }
        }
    }

    private fun processPacket(data: ByteArray, length: Int) {
        // Check for NAL start code
        if (isNalStartCode(data)) {
            // Previous NAL complete, feed to decoder
            if (nalBuffer.position() > 0) {
                val nalUnit = ByteArray(nalBuffer.position())
                nalBuffer.flip()
                nalBuffer.get(nalUnit)

                decoder.feedNalUnit(nalUnit, frameTimestamp)

                nalBuffer.clear()
                frameTimestamp = System.nanoTime() / 1000
            }
        }

        // Accumulate data
        nalBuffer.put(data, 0, length)
    }

    private fun isNalStartCode(data: ByteArray): Boolean {
        return data.size >= 4 &&
               data[0] == 0x00.toByte() &&
               data[1] == 0x00.toByte() &&
               data[2] == 0x00.toByte() &&
               data[3] == 0x01.toByte()
    }

    fun stop() {
        receiveJob?.cancel()
        socket?.close()
        socket = null
    }
}

// Usage
val receiver = UdpVideoReceiver(decoder, port = 5000)
receiver.start()
// ... later ...
receiver.stop()
```

### FEC-Decoded Stream

```kotlin
import com.hxesp32.fpvgs.video.H264Decoder

class FecVideoDecoder(
    private val decoder: H264Decoder,
    private val fecDecoder: FecDecoder  // Your FEC implementation
) {
    private val nalQueue = mutableListOf<ByteArray>()

    fun processFecBlock(fecBlock: FecBlock) {
        // Decode FEC block
        val recoveredPackets = fecDecoder.decode(fecBlock)

        recoveredPackets.forEach { packet ->
            // Extract NAL units from packet
            val nalUnits = extractNalUnits(packet)

            nalUnits.forEach { nalUnit ->
                val timestamp = System.nanoTime() / 1000
                decoder.feedNalUnit(nalUnit, timestamp)
            }
        }
    }

    private fun extractNalUnits(packet: ByteArray): List<ByteArray> {
        val nalUnits = mutableListOf<ByteArray>()
        var offset = 0

        while (offset < packet.size) {
            // Find next NAL start code
            val startCode = findStartCode(packet, offset)
            if (startCode == -1) break

            // Find end of NAL unit
            val nextStartCode = findStartCode(packet, startCode + 4)
            val nalEnd = if (nextStartCode != -1) nextStartCode else packet.size

            // Extract NAL unit
            val nalUnit = packet.copyOfRange(startCode, nalEnd)
            nalUnits.add(nalUnit)

            offset = nalEnd
        }

        return nalUnits
    }

    private fun findStartCode(data: ByteArray, startOffset: Int): Int {
        for (i in startOffset until data.size - 3) {
            if (data[i] == 0x00.toByte() &&
                data[i + 1] == 0x00.toByte() &&
                data[i + 2] == 0x00.toByte() &&
                data[i + 3] == 0x01.toByte()) {
                return i
            }
        }
        return -1
    }
}
```

---

## Error Handling

### Robust Error Recovery

```kotlin
import com.hxesp32.fpvgs.video.H264Decoder
import com.hxesp32.fpvgs.video.DecoderError

class RobustVideoDecoder(
    private val decoder: H264Decoder
) {
    private var consecutiveErrors = 0
    private val maxErrorsBeforeReset = 5
    private var lastResetTime = 0L
    private val minResetInterval = 5000L // 5 seconds

    init {
        setupErrorHandling()
    }

    private fun setupErrorHandling() {
        decoder.setErrorCallback { error ->
            handleError(error)
        }
    }

    private fun handleError(error: DecoderError) {
        consecutiveErrors++

        println("Decoder error: ${error.message}")
        println("Consecutive errors: $consecutiveErrors")

        when {
            // Too many consecutive errors - reset decoder
            consecutiveErrors >= maxErrorsBeforeReset -> {
                val now = System.currentTimeMillis()
                if (now - lastResetTime > minResetInterval) {
                    println("Resetting decoder due to multiple errors")
                    resetDecoder()
                    lastResetTime = now
                }
            }

            // Moderate errors - request keyframe
            consecutiveErrors >= 3 -> {
                println("Requesting keyframe from encoder")
                requestKeyframe()
            }

            // Single error - log and continue
            else -> {
                println("Transient error, continuing")
            }
        }
    }

    private fun resetDecoder() {
        decoder.reset()
        consecutiveErrors = 0
    }

    private fun requestKeyframe() {
        // Send request to encoder (implementation specific)
        // e.g., send RTCP message or custom protocol message
    }

    fun onFrameDecoded() {
        // Reset error counter on successful decode
        consecutiveErrors = 0
    }
}
```

---

## Statistics Monitoring

### Real-Time Statistics Display

```kotlin
import com.hxesp32.fpvgs.video.H264Decoder
import kotlinx.coroutines.*

class VideoStatisticsMonitor(
    private val decoder: H264Decoder
) {
    private var monitorJob: Job? = null
    private val scope = CoroutineScope(Dispatchers.Default)

    fun startMonitoring(updateIntervalMs: Long = 1000) {
        monitorJob = scope.launch {
            while (isActive) {
                val stats = decoder.getStatistics()

                // Calculate additional metrics
                val decodeSuccessRate = if (stats.framesReceived > 0) {
                    (stats.framesDecoded.toFloat() / stats.framesReceived * 100)
                } else {
                    0f
                }

                // Log statistics
                println("""
                    === Video Decoder Statistics ===
                    FPS: %.1f
                    Latency: %.1f ms (min: %d, max: %d)
                    Frames: %d received, %d decoded (%.1f%%)
                    IDR frames: %d
                    Errors: %d (rate: %.2f/sec)
                    Buffer underflows: %d
                """.trimIndent().format(
                    stats.currentFps,
                    stats.averageLatencyMs,
                    stats.minLatencyMs,
                    stats.maxLatencyMs,
                    stats.framesReceived,
                    stats.framesDecoded,
                    decodeSuccessRate,
                    stats.idrFrameCount,
                    stats.errorCount,
                    stats.errorRate,
                    stats.bufferUnderflows
                ))

                delay(updateIntervalMs)
            }
        }
    }

    fun stopMonitoring() {
        monitorJob?.cancel()
    }
}

// Usage
val monitor = VideoStatisticsMonitor(decoder)
monitor.startMonitoring(updateIntervalMs = 1000)
// ... later ...
monitor.stopMonitoring()
```

### Performance Alert System

```kotlin
import com.hxesp32.fpvgs.video.DecoderStatistics

class PerformanceMonitor(
    private val onAlert: (String) -> Unit
) {
    // Thresholds
    private val maxAcceptableLatency = 100L // ms
    private val minAcceptableFps = 25f
    private val maxErrorRate = 1f // errors per second

    fun checkPerformance(stats: DecoderStatistics) {
        // Check latency
        if (stats.averageLatencyMs > maxAcceptableLatency) {
            onAlert("High latency: ${stats.averageLatencyMs.toInt()}ms")
        }

        // Check frame rate
        if (stats.currentFps < minAcceptableFps && stats.framesDecoded > 30) {
            onAlert("Low FPS: ${stats.currentFps.toInt()}")
        }

        // Check error rate
        if (stats.errorRate > maxErrorRate) {
            onAlert("High error rate: ${stats.errorRate} errors/sec")
        }

        // Check buffer issues
        if (stats.bufferUnderflows > 10) {
            onAlert("Frequent buffer underflows: ${stats.bufferUnderflows}")
        }
    }
}

// Usage
val perfMonitor = PerformanceMonitor { alert ->
    println("⚠️ PERFORMANCE ALERT: $alert")
}

// Check periodically
scope.launch {
    while (isActive) {
        val stats = decoder.getStatistics()
        perfMonitor.checkPerformance(stats)
        delay(1000)
    }
}
```

---

## Advanced Configuration

### Multi-Resolution Support

```kotlin
import com.hxesp32.fpvgs.video.H264Decoder
import com.hxesp32.fpvgs.video.MediaCodecHelper

class AdaptiveVideoDecoder(private val surface: Surface) {
    private var decoder: H264Decoder? = null
    private var currentWidth = 0
    private var currentHeight = 0

    fun switchResolution(width: Int, height: Int) {
        // Check if resolution is supported
        if (!MediaCodecHelper.isResolutionSupported(width, height)) {
            println("Resolution ${width}x${height} not supported")
            return
        }

        // Same resolution, no need to restart
        if (width == currentWidth && height == currentHeight) {
            return
        }

        println("Switching resolution to ${width}x${height}")

        // Stop current decoder
        decoder?.stop()

        // Create new decoder with new resolution
        decoder = H264Decoder(width, height, surface)

        if (decoder!!.start()) {
            currentWidth = width
            currentHeight = height
            println("Resolution switched successfully")
        } else {
            println("Failed to switch resolution")
        }
    }

    fun cleanup() {
        decoder?.stop()
    }
}
```

### Custom Frame Processing

```kotlin
import com.hxesp32.fpvgs.video.H264Decoder

class VideoProcessor(
    width: Int,
    height: Int,
    surface: Surface
) {
    private val decoder = H264Decoder(width, height, surface)
    private val frameBuffer = mutableListOf<FrameInfo>()

    data class FrameInfo(
        val frameNumber: Long,
        val timestamp: Long,
        val latency: Long
    )

    init {
        decoder.setFrameCallback { frameNum, latency ->
            onFrameDecoded(frameNum, latency)
        }
    }

    private fun onFrameDecoded(frameNum: Long, latency: Long) {
        val info = FrameInfo(
            frameNumber = frameNum,
            timestamp = System.currentTimeMillis(),
            latency = latency
        )

        frameBuffer.add(info)

        // Keep only last 100 frames
        if (frameBuffer.size > 100) {
            frameBuffer.removeAt(0)
        }

        // Analyze frame pattern
        analyzeFramePattern()
    }

    private fun analyzeFramePattern() {
        if (frameBuffer.size < 30) return

        // Calculate frame interval variance
        val intervals = frameBuffer.zipWithNext { a, b ->
            b.timestamp - a.timestamp
        }

        val avgInterval = intervals.average()
        val variance = intervals.map { (it - avgInterval) * (it - avgInterval) }.average()
        val jitter = Math.sqrt(variance)

        if (jitter > 5.0) {
            println("High jitter detected: $jitter ms")
        }
    }

    fun start() = decoder.start()
    fun stop() = decoder.stop()
    fun feedNalUnit(data: ByteArray, timestamp: Long) =
        decoder.feedNalUnit(data, timestamp)
}
```

---

## Complete Application Example

### FPV Video Receiver App

```kotlin
import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.hxesp32.fpvgs.video.H264Decoder
import com.hxesp32.fpvgs.video.DecoderConfig
import kotlinx.coroutines.*

class FpvActivity : AppCompatActivity() {

    private lateinit var surfaceView: SurfaceView
    private lateinit var statsTextView: TextView

    private var decoder: H264Decoder? = null
    private var videoReceiver: UdpVideoReceiver? = null
    private var statsMonitor: VideoStatisticsMonitor? = null

    private val scope = CoroutineScope(Dispatchers.Main + SupervisorJob())

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_fpv)

        surfaceView = findViewById(R.id.surfaceView)
        statsTextView = findViewById(R.id.statsTextView)

        setupSurface()
        startStatsUpdates()
    }

    private fun setupSurface() {
        surfaceView.holder.addCallback(object : SurfaceHolder.Callback {
            override fun surfaceCreated(holder: SurfaceHolder) {
                startVideo(holder.surface)
            }

            override fun surfaceChanged(holder: SurfaceHolder, format: Int, w: Int, h: Int) {}

            override fun surfaceDestroyed(holder: SurfaceHolder) {
                stopVideo()
            }
        })
    }

    private fun startVideo(surface: Surface) {
        // Create decoder
        decoder = H264Decoder(
            width = 800,
            height = 600,
            surface = surface,
            config = DecoderConfig(
                lowLatencyMode = true,
                expectedFrameRate = 30,
                logStatistics = false
            )
        )

        if (decoder!!.start()) {
            // Start network receiver
            videoReceiver = UdpVideoReceiver(decoder!!, port = 5000)
            videoReceiver!!.start()

            // Start statistics monitoring
            statsMonitor = VideoStatisticsMonitor(decoder!!)
            statsMonitor!!.startMonitoring()
        }
    }

    private fun stopVideo() {
        videoReceiver?.stop()
        statsMonitor?.stopMonitoring()
        decoder?.stop()
    }

    private fun startStatsUpdates() {
        scope.launch {
            while (isActive) {
                decoder?.let { dec ->
                    val stats = dec.getStatistics()
                    statsTextView.text = """
                        FPS: ${stats.currentFps.toInt()}
                        Latency: ${stats.averageLatencyMs.toInt()}ms
                        Frames: ${stats.framesDecoded}
                    """.trimIndent()
                }
                delay(500)
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        scope.cancel()
        stopVideo()
    }
}
```

---

## Best Practices

1. **Always provide a Surface** for hardware-accelerated rendering
2. **Handle Surface lifecycle** properly (create/destroy callbacks)
3. **Monitor statistics** to detect performance issues
4. **Implement error recovery** with automatic decoder reset
5. **Feed frames on background thread** to avoid UI blocking
6. **Request keyframes** when errors occur
7. **Use proper timestamps** for accurate latency measurement
8. **Clean up resources** in onDestroy()

---

For more examples and detailed API documentation, see the README.md and source code.

# Android USB Communication with ESP32-S3 - Usage Guide

## Overview

This guide explains how to use the Android USB communication layer to receive video, telemetry, and OSD data from the ESP32-S3 FPV air unit.

## Architecture

The USB communication system consists of several components:

1. **UsbCommunicationManager** - Main class handling USB device discovery, connection, and data transfer
2. **UsbProtocolParser** - Parses incoming USB data and extracts protocol frames
3. **UsbFrameAssembler** - Assembles multi-part video frames into complete JPEG images
4. **Protocol Data Classes** - Type-safe representations of packets and configurations

## Quick Start

### 1. Add Permissions to AndroidManifest.xml

```xml
<manifest>
    <uses-feature android:name="android.hardware.usb.host" />
    <uses-permission android:name="android.permission.USB_PERMISSION" />

    <application>
        <activity>
            <intent-filter>
                <action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
            </intent-filter>
            <meta-data
                android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED"
                android:resource="@xml/device_filter" />
        </activity>
    </application>
</manifest>
```

### 2. Create USB Device Filter

Create `res/xml/device_filter.xml`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <!-- ESP32-S3 -->
    <usb-device vendor-id="12346" product-id="4097" />
    <!-- Add additional vendor/product IDs as needed -->
</resources>
```

### 3. Initialize USB Communication Manager

```kotlin
class MainActivity : AppCompatActivity() {
    private lateinit var usbManager: UsbCommunicationManager

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Create listener
        val listener = object : UsbCommunicationManager.UsbCommunicationListener {
            override fun onDeviceConnected(device: UsbDevice) {
                Log.i(TAG, "Device connected: ${device.deviceName}")
            }

            override fun onDeviceDisconnected() {
                Log.i(TAG, "Device disconnected")
            }

            override fun onPermissionGranted(device: UsbDevice) {
                Log.i(TAG, "Permission granted for ${device.deviceName}")
            }

            override fun onPermissionDenied(device: UsbDevice) {
                Log.w(TAG, "Permission denied for ${device.deviceName}")
            }

            override fun onVideoFrameReceived(frame: AssembledVideoFrame) {
                // Decode and display JPEG frame
                decodeAndDisplayFrame(frame)
            }

            override fun onTelemetryReceived(data: ByteArray) {
                // Process MAVLink telemetry
                processTelemetry(data)
            }

            override fun onOsdDataReceived(stats: AirStats, osdBuffer: ByteArray) {
                // Update OSD display
                updateOsd(stats, osdBuffer)
            }

            override fun onConfigReceived(config: UsbFrame.ConfigFrame) {
                // Handle configuration updates
                handleConfig(config)
            }

            override fun onError(error: UsbError) {
                // Handle errors
                handleError(error)
            }

            override fun onStatsUpdated(stats: UsbStats) {
                // Update statistics display
                updateStats(stats)
            }
        }

        // Initialize manager
        usbManager = UsbCommunicationManager(this, listener)
    }

    override fun onDestroy() {
        super.onDestroy()
        usbManager.cleanup()
    }
}
```

### 4. Discover and Connect to Devices

```kotlin
fun discoverAndConnect() {
    // Discover ESP32-S3 devices
    val devices = usbManager.discoverDevices()

    if (devices.isEmpty()) {
        Log.w(TAG, "No ESP32-S3 devices found")
        return
    }

    // Request permission for first device
    val device = devices.first()
    usbManager.requestPermission(device)

    // Connection will happen automatically after permission is granted
}
```

## Processing Received Data

### Video Frames

Video frames are assembled from multiple parts and delivered as complete JPEG images:

```kotlin
override fun onVideoFrameReceived(frame: AssembledVideoFrame) {
    // Decode JPEG
    val bitmap = BitmapFactory.decodeByteArray(frame.data, 0, frame.data.size)

    if (bitmap != null) {
        // Display on ImageView
        runOnUiThread {
            imageView.setImageBitmap(bitmap)

            // Update info
            resolutionText.text = "${frame.resolution.width}x${frame.resolution.height}"
            frameIndexText.text = "Frame: ${frame.frameIndex}"
        }
    } else {
        Log.e(TAG, "Failed to decode JPEG frame ${frame.frameIndex}")
    }
}
```

### Telemetry Data

Telemetry data is typically MAVLink packets:

```kotlin
override fun onTelemetryReceived(data: ByteArray) {
    // Parse MAVLink (you'll need a MAVLink parser library)
    try {
        val mavlinkParser = MAVLinkParser()
        val message = mavlinkParser.parse(data)

        when (message) {
            is Heartbeat -> updateHeartbeat(message)
            is Attitude -> updateAttitude(message)
            is GPSRaw -> updateGPS(message)
            // Handle other message types
        }
    } catch (e: Exception) {
        Log.e(TAG, "Error parsing telemetry", e)
    }
}
```

### OSD Data and Statistics

OSD data includes air unit statistics and the OSD character buffer:

```kotlin
override fun onOsdDataReceived(stats: AirStats, osdBuffer: ByteArray) {
    runOnUiThread {
        // Display statistics
        rssiText.text = "RSSI: ${stats.getRssiDbmSigned()} dBm"
        fpsText.text = "FPS: ${stats.captureFPS}"
        bitrateText.text = String.format("%.1f Mbps",
            stats.outPacketRate * 8.0 / 1_000_000)

        // Display recording status
        recordingIndicator.isVisible = stats.airRecordState

        // Display temperature
        if (stats.temperature.toInt() > 0) {
            temperatureText.text = "${stats.temperature}°C"
            if (stats.overheatThrottling) {
                temperatureText.setTextColor(Color.RED)
            }
        }

        // Display SD card info
        if (stats.sdDetected) {
            sdCardText.text = String.format("SD: %.1f / %.1f GB",
                stats.getSdFreeSpaceGB(),
                stats.getSdTotalSpaceGB())

            if (stats.sdSlow) {
                sdCardText.append(" (SLOW)")
            }
        }

        // Render OSD buffer if needed
        renderOsd(osdBuffer)
    }
}
```

## Sending Commands to ESP32-S3

### Send Camera Configuration

```kotlin
fun updateCameraConfig() {
    val cameraConfig = CameraConfig(
        resolution = Resolution.SVGA16,  // 800x456
        fpsLimit = 30u,
        quality = 0u,  // Auto quality
        brightness = 0,
        contrast = 0,
        saturation = 1,
        awb = true,
        aec = true
    )

    val dataChannelConfig = DataChannelConfig(
        wifiPower = 20,
        wifiRate = WifiRate.RATE_N_26M_MCS3,
        wifiChannel = 7u,
        fecCodecK = 6u,
        fecCodecN = 12u
    )

    usbManager.sendConfig(
        airDeviceId = 1234u,
        gsDeviceId = 5678u,
        camera = cameraConfig,
        dataChannel = dataChannelConfig
    )
}
```

### Send Telemetry (MAVLink RC commands)

```kotlin
fun sendRCCommand(channels: IntArray) {
    // Build MAVLink RC_CHANNELS_OVERRIDE packet
    val mavlinkPacket = buildMavlinkRCPacket(channels)

    usbManager.sendTelemetry(mavlinkPacket)
}
```

## Statistics and Monitoring

### Get Real-time Statistics

```kotlin
fun updateStatistics() {
    val stats = usbManager.getStats()
    val assemblyStats = usbManager.getAssemblyStats()
    val uptimeMs = usbManager.getUptimeMs()

    Log.i(TAG, """
        USB Statistics:
        - Bytes received: ${stats.bytesReceived}
        - Frames received: ${stats.framesReceived}
        - Video frames: ${stats.videoFramesReceived}
        - Telemetry frames: ${stats.telemetryFramesReceived}
        - OSD frames: ${stats.osdFramesReceived}
        - CRC errors: ${stats.crcErrors}
        - Framing errors: ${stats.framingErrors}
        - Throughput: ${stats.getThroughputBps(uptimeMs)} bps
        - Frame rate: ${stats.getFrameRate(uptimeMs)} fps

        Assembly Statistics:
        - Pending frames: ${assemblyStats.pendingFrames}
        - Assembled frames: ${assemblyStats.assembledFrames}
        - Dropped frames: ${assemblyStats.droppedFrames}
        - Last frame index: ${assemblyStats.lastCompletedFrameIndex}
    """.trimIndent())
}
```

## Error Handling

### Handle USB Errors

```kotlin
override fun onError(error: UsbError) {
    val message = when (error) {
        is UsbError.NoSuitableInterface ->
            "Device doesn't have required USB interface"
        is UsbError.ConnectionFailed ->
            "Failed to open USB connection"
        is UsbError.InterfaceClaimFailed ->
            "Failed to claim USB interface"
        is UsbError.NoEndpoints ->
            "Device doesn't have required endpoints"
        is UsbError.ConnectionException ->
            "Connection error: ${error.exception.message}"
        is UsbError.ReadError ->
            "USB read error: ${error.message}"
        is UsbError.WriteError ->
            "USB write error: ${error.message}"
    }

    Log.e(TAG, "USB Error: $message")

    // Show error to user
    runOnUiThread {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show()
    }
}
```

## Best Practices

### 1. Handle Lifecycle Properly

```kotlin
override fun onResume() {
    super.onResume()

    // Check if device is still connected
    if (!usbManager.isConnected()) {
        discoverAndConnect()
    }
}

override fun onPause() {
    super.onPause()

    // Optional: disconnect when app goes to background
    // usbManager.disconnect()
}

override fun onDestroy() {
    super.onDestroy()

    // Always cleanup
    usbManager.cleanup()
}
```

### 2. Use Background Threads for Heavy Processing

```kotlin
private val videoDecoder = Executors.newSingleThreadExecutor()

override fun onVideoFrameReceived(frame: AssembledVideoFrame) {
    videoDecoder.execute {
        // Decode on background thread
        val bitmap = BitmapFactory.decodeByteArray(frame.data, 0, frame.data.size)

        // Update UI on main thread
        runOnUiThread {
            imageView.setImageBitmap(bitmap)
        }
    }
}
```

### 3. Monitor Performance

```kotlin
private var frameCount = 0
private var lastStatsTime = System.currentTimeMillis()

override fun onVideoFrameReceived(frame: AssembledVideoFrame) {
    frameCount++

    val now = System.currentTimeMillis()
    if (now - lastStatsTime >= 1000) {  // Every second
        val fps = frameCount.toFloat()
        Log.i(TAG, "Receiving at $fps FPS")

        frameCount = 0
        lastStatsTime = now
    }

    // Process frame...
}
```

### 4. Handle Disconnections Gracefully

```kotlin
override fun onDeviceDisconnected() {
    Log.w(TAG, "Device disconnected")

    runOnUiThread {
        // Clear display
        imageView.setImageBitmap(null)

        // Show reconnect option
        reconnectButton.isVisible = true

        // Stop any ongoing operations
        videoDecoder.shutdownNow()
    }
}
```

## USB Protocol Specification

### Frame Format

```
[SYNC1][SYNC2][TYPE][SIZE(4 bytes)][PAYLOAD][CRC]

- SYNC1: 0xA5
- SYNC2: 0x5A
- TYPE: Frame type (Video=0, Telemetry=1, OSD=2, Config=3)
- SIZE: Payload size + 1 (for CRC) in little-endian
- PAYLOAD: Frame-specific data
- CRC: CRC32 & 0xFF of bytes from TYPE to end of PAYLOAD
```

### Video Frame Payload

```
[Air2Ground_Header(11)][Resolution(1)][PartIndex|LastPart(1)][FrameIndex(4)][JPEG_DATA]

- Air2Ground_Header: Common header for all Air2Ground packets
- Resolution: Video resolution enum
- PartIndex: Lower 7 bits = part index, bit 7 = last part flag
- FrameIndex: Sequential frame number
- JPEG_DATA: Partial or complete JPEG image data
```

## Testing

Run the included unit tests:

```bash
./gradlew test
```

Tests include:
- **UsbProtocolParserTest** - Protocol parsing and validation
- **UsbFrameAssemblerTest** - Frame assembly logic
- **MockUsbCommunicationTest** - End-to-end communication scenarios

## Troubleshooting

### No Devices Found

1. Check USB cable connection
2. Verify ESP32-S3 is in correct mode (not bootloader mode)
3. Check vendor/product IDs match your device
4. Verify USB host mode is supported on Android device

### Permission Denied

1. Check AndroidManifest.xml has USB permissions
2. Verify device_filter.xml includes your device IDs
3. User may need to manually grant permission in dialog

### Poor Performance

1. Reduce video resolution on ESP32-S3
2. Use hardware-accelerated JPEG decoding
3. Process frames on background threads
4. Check USB cable quality (use USB 2.0 or better)

### Frame Assembly Errors

1. Check for USB data corruption (bad cables)
2. Verify CRC validation is working
3. Monitor assembly statistics for dropped frames
4. Increase frame timeout if needed

### High CPU Usage

1. Use BitmapFactory.Options to decode to specific size
2. Implement frame skipping if falling behind
3. Use Android's Hardware Accelerated decoder
4. Profile with Android Profiler

## Advanced Usage

### Custom Packet Types

To add support for custom packet types:

1. Add enum value to `Air2GroundPacketType` or `Ground2AirPacketType`
2. Create data class for the packet in `ProtocolPackets.kt`
3. Add parsing logic in `UsbProtocolParser.kt`
4. Add listener callback in `UsbCommunicationListener`

### Optimizing Buffer Sizes

Adjust buffer sizes based on your needs:

```kotlin
// In ProtocolConstants.kt
const val USB_BULK_TRANSFER_SIZE = 32768  // Increase for higher throughput
const val MAX_FRAME_SIZE = 131072  // Increase for higher resolutions
```

### Video Recording

To record received video:

```kotlin
private val mediaRecorder = MediaCodec.createEncoderByType("video/avc")

override fun onVideoFrameReceived(frame: AssembledVideoFrame) {
    // Decode JPEG
    val bitmap = BitmapFactory.decodeByteArray(frame.data, 0, frame.data.size)

    // Encode to H.264 and save to file
    encodeAndSaveFrame(bitmap)
}
```

## Performance Metrics

Expected performance with ESP32-S3:

- **Video Resolution**: Up to 1280x720 (HD)
- **Frame Rate**: 10-30 FPS depending on resolution
- **Latency**: 90-110 ms
- **Bitrate**: 8-14 Mbps
- **USB Bandwidth**: ~2-3 MB/s average

## Support

For issues and questions:
- Check the main project README: `/home/user/hx-esp32-cam-fpv/README.md`
- Review test cases for usage examples
- Check protocol definitions in `components/common/packets.h`

## License

Same as parent project (see LICENSE file in project root).

# Android USB OTG Integration Guide

This directory contains Android-side code for integrating with the ESP32-S3 USB OTG video streaming system.

## Overview

The Android integration enables real-time H.264 video streaming from ESP32-S3 to Android devices via USB OTG connection. The implementation provides:

- **Protocol Parser:** Parse USB streaming protocol packets
- **NAL Unit Handling:** Extract H.264 NAL units for MediaCodec
- **Statistics Tracking:** Monitor connection quality, throughput, and errors
- **Example Application:** Complete working example

## Files

| File | Description |
|------|-------------|
| `UsbProtocolParser.java` | Java implementation of protocol parser |
| `UsbProtocolParser.kt` | Kotlin implementation of protocol parser |
| `ExampleUsbActivity.kt` | Example Android Activity with USB integration |
| `README.md` | This file |

## Requirements

### Android

- **Minimum SDK:** Android 6.0 (API 23) - USB CDC support
- **Recommended SDK:** Android 8.0 (API 26+) - Better USB performance
- **Permissions:**
  ```xml
  <uses-feature android:name="android.hardware.usb.host" />
  <uses-permission android:name="android.permission.USB_PERMISSION" />
  ```

### Dependencies

Add to your `build.gradle`:

```gradle
dependencies {
    // USB Serial library
    implementation 'com.github.felHR85:UsbSerial:6.1.0'

    // Kotlin Coroutines (if using Kotlin)
    implementation 'org.jetbrains.kotlinx:kotlinx-coroutines-android:1.6.4'

    // AndroidX Core
    implementation 'androidx.core:core-ktx:1.9.0'
    implementation 'androidx.appcompat:appcompat:1.6.1'
}
```

Add to your project's `build.gradle`:

```gradle
allprojects {
    repositories {
        maven { url 'https://jitpack.io' }
    }
}
```

## Quick Start

### 1. Add USB Device Filter

Create `res/xml/device_filter.xml`:

```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <!-- ESP32-S3 USB Device -->
    <usb-device vendor-id="12346" product-id="16384" />
    <!-- Espressif VID -->
    <usb-device vendor-id="12346" />
</resources>
```

### 2. Update AndroidManifest.xml

```xml
<manifest>
    <uses-feature android:name="android.hardware.usb.host" />
    <uses-permission android:name="android.permission.USB_PERMISSION" />

    <application>
        <activity android:name=".UsbActivity">
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

### 3. Basic Usage (Kotlin)

```kotlin
import com.example.esp32usb.UsbProtocolParser

class VideoActivity : AppCompatActivity() {
    private var parser: UsbProtocolParser? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Create parser with callback
        parser = UsbProtocolParser(object : UsbProtocolParser.PacketCallback {
            override fun onVideoPacket(video: UsbProtocolParser.VideoPacket) {
                // Handle video NAL unit
                decodeNalUnit(video.nalData)
            }

            override fun onMetadataPacket(metadata: UsbProtocolParser.MetadataPacket) {
                // Update UI with statistics
                updateStats(metadata)
            }

            override fun onHeartbeat(uptime: Long) {
                Log.d(TAG, "ESP32 uptime: $uptime seconds")
            }

            override fun onDebugMessage(message: String) {
                Log.d(TAG, "ESP32: $message")
            }

            override fun onParseError(error: String) {
                Log.e(TAG, "Parse error: $error")
            }
        })

        // Connect to USB device
        connectToUsb()
    }

    private fun onUsbDataReceived(data: ByteArray, length: Int) {
        // Parse incoming USB data
        parser?.parse(data, length)
    }

    private fun decodeNalUnit(nalData: ByteArray) {
        // Send to MediaCodec for H.264 decoding
        // See section "H.264 Decoding" below
    }
}
```

### 4. Basic Usage (Java)

```java
import com.example.esp32usb.UsbProtocolParser;

public class VideoActivity extends AppCompatActivity {
    private UsbProtocolParser parser;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        parser = new UsbProtocolParser(new UsbProtocolParser.PacketCallback() {
            @Override
            public void onVideoPacket(UsbProtocolParser.VideoPacket video) {
                decodeNalUnit(video.nalData);
            }

            @Override
            public void onMetadataPacket(UsbProtocolParser.MetadataPacket metadata) {
                updateStats(metadata);
            }

            @Override
            public void onHeartbeat(long uptime) {
                Log.d(TAG, "ESP32 uptime: " + uptime + " seconds");
            }

            @Override
            public void onDebugMessage(String message) {
                Log.d(TAG, "ESP32: " + message);
            }

            @Override
            public void onParseError(String error) {
                Log.e(TAG, "Parse error: " + error);
            }
        });
    }
}
```

## H.264 Decoding with MediaCodec

### Setup MediaCodec

```kotlin
import android.media.MediaCodec
import android.media.MediaFormat
import android.view.Surface

class H264Decoder(private val surface: Surface) {
    private var decoder: MediaCodec? = null
    private var sps: ByteArray? = null
    private var pps: ByteArray? = null

    fun start(width: Int, height: Int) {
        val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, width, height)

        decoder = MediaCodec.createDecoderByType(MediaFormat.MIMETYPE_VIDEO_AVC)
        decoder?.configure(format, surface, null, 0)
        decoder?.start()
    }

    fun decodeNalUnit(nalData: ByteArray, nalType: Byte) {
        when (nalType) {
            UsbProtocolParser.NAL_UNIT_TYPE_SPS -> {
                sps = nalData
                configureSpsPs()
            }
            UsbProtocolParser.NAL_UNIT_TYPE_PPS -> {
                pps = nalData
                configureSpsPps()
            }
            UsbProtocolParser.NAL_UNIT_TYPE_IDR,
            UsbProtocolParser.NAL_UNIT_TYPE_NON_IDR -> {
                queueInputBuffer(nalData)
            }
        }
    }

    private fun configureSpsPps() {
        if (sps != null && pps != null) {
            // Reconfigure codec with SPS/PPS
            decoder?.stop()

            val csd = ByteBuffer.allocate(sps!!.size + pps!!.size)
            csd.put(sps)
            csd.put(pps)

            val format = MediaFormat.createVideoFormat(MediaFormat.MIMETYPE_VIDEO_AVC, 1280, 720)
            format.setByteBuffer("csd-0", ByteBuffer.wrap(sps))
            format.setByteBuffer("csd-1", ByteBuffer.wrap(pps))

            decoder?.configure(format, surface, null, 0)
            decoder?.start()
        }
    }

    private fun queueInputBuffer(data: ByteArray) {
        val decoder = this.decoder ?: return

        val inputBufferIndex = decoder.dequeueInputBuffer(10000)
        if (inputBufferIndex >= 0) {
            val inputBuffer = decoder.getInputBuffer(inputBufferIndex)
            inputBuffer?.clear()
            inputBuffer?.put(data)

            decoder.queueInputBuffer(inputBufferIndex, 0, data.size,
                System.nanoTime() / 1000, 0)
        }

        // Dequeue output buffer
        val bufferInfo = MediaCodec.BufferInfo()
        val outputBufferIndex = decoder.dequeueOutputBuffer(bufferInfo, 0)

        if (outputBufferIndex >= 0) {
            decoder.releaseOutputBuffer(outputBufferIndex, true)
        }
    }

    fun stop() {
        decoder?.stop()
        decoder?.release()
        decoder = null
    }
}
```

## Sending Control Commands

### Command Packet Format

```kotlin
fun buildControlCommand(command: Byte, param1: Short, param2: Int): ByteArray {
    val SYNC: Short = 0xA55A.toShort()
    val TYPE_CONTROL: Byte = 0x03

    val buffer = ByteBuffer.allocate(18).apply {
        order(ByteOrder.LITTLE_ENDIAN)
    }

    // Header
    buffer.putShort(SYNC)
    buffer.put(TYPE_CONTROL)
    buffer.put(0) // Flags
    buffer.putShort(6) // Payload size
    buffer.putShort(getNextSequence())
    buffer.putInt((System.nanoTime() / 1000).toInt())

    // Payload
    buffer.put(command)
    buffer.put(0) // Status
    buffer.putShort(param1)
    buffer.putInt(param2)

    // Calculate CRC
    val packetData = buffer.array().copyOfRange(0, 16)
    val crc = calculateCrc16(packetData)
    buffer.putShort(crc.toShort())

    return buffer.array()
}

// Example: Start streaming at 1280x720, 30 fps
val cmd = buildControlCommand(0x01, 0x04, 30)
serialPort?.write(cmd)

// Example: Request IDR frame
val cmd = buildControlCommand(0x05, 0, 0)
serialPort?.write(cmd)

// Example: Set bitrate to 8000 kbps
val cmd = buildControlCommand(0x03, 0, 8000)
serialPort?.write(cmd)
```

## Performance Optimization

### 1. Use Large Read Buffers

```kotlin
private const val READ_BUFFER_SIZE = 16384 // 16 KB
```

### 2. Use Dedicated Thread for USB I/O

```kotlin
private val usbThread = HandlerThread("UsbReader").apply { start() }
private val usbHandler = Handler(usbThread.looper)
```

### 3. Enable USB Bulk Transfer (if supported)

```kotlin
// Request bulk transfer instead of CDC
// Requires custom USB driver or libusb
```

### 4. Minimize Allocations

```kotlin
// Reuse buffers
private val readBuffer = ByteArray(READ_BUFFER_SIZE)

// Avoid creating new objects in hot path
parser?.parse(readBuffer, bytesRead)
```

## Troubleshooting

### USB Device Not Found

1. Check USB OTG cable is connected
2. Verify device permissions in AndroidManifest.xml
3. Check USB device filter matches ESP32 VID/PID
4. Try different USB cable (some are charge-only)

### CRC Errors

1. Check USB cable quality
2. Reduce bitrate/FPS on ESP32
3. Increase read buffer size
4. Check for electrical interference

### Video Freezing

1. Monitor buffer usage in metadata packets
2. Reduce bitrate if buffer >80% full
3. Request IDR frame if decoding fails
4. Check MediaCodec is configured correctly

### Low Throughput

1. Use USB 2.0 port (not USB 3.0 in compatibility mode)
2. Disable USB debugging if enabled
3. Close other apps using USB
4. Check ESP32 TX buffer isn't overflowing

## Statistics and Monitoring

```kotlin
// Get parser statistics
val stats = parser?.statistics

Log.i(TAG, "Packets received: ${stats?.packetsReceived}")
Log.i(TAG, "CRC errors: ${stats?.crcErrors}")
Log.i(TAG, "Sequence gaps: ${stats?.sequenceGaps}")
Log.i(TAG, "Bytes received: ${stats?.bytesReceived}")

// Get ESP32 statistics from metadata
metadata?.let {
    Log.i(TAG, "ESP32 RSSI: ${it.rssi}dBm")
    Log.i(TAG, "ESP32 FPS: ${it.fps / 10.0}fps")
    Log.i(TAG, "ESP32 Bitrate: ${it.bitrate}kbps")
    Log.i(TAG, "ESP32 Buffer: ${it.bufferUsage}%")
}
```

## License

This code is provided as part of the ESP32-S3 USB OTG streaming project.

## Support

For issues and questions:
- Check the protocol specification: `../USB_PROTOCOL_SPECIFICATION.md`
- Review ESP32-S3 firmware logs
- Test with example application first
- Enable verbose logging: `parser.setLogLevel(VERBOSE)`

## Example Projects

See `ExampleUsbActivity.kt` for a complete working example demonstrating:
- USB device enumeration
- Permission handling
- Data reception and parsing
- NAL unit extraction
- Statistics display
- Control command sending

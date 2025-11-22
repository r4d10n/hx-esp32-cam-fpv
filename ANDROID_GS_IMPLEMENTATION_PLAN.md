# Android Ground Station Implementation Plan

## Executive Summary

This document provides a comprehensive implementation plan for developing an Android-based ground station for the ESP32-CAM FPV system. The solution uses an ESP32-S3 as a WiFi receiver with USB OTG connectivity to an Android device, replacing the current Raspberry Pi/Linux desktop ground station.

---

## Table of Contents

1. [Current Desktop GS Architecture Analysis](#1-current-desktop-gs-architecture-analysis)
2. [Feature Inventory](#2-feature-inventory)
3. [Android GS System Architecture](#3-android-gs-system-architecture)
4. [ESP32-S3 Receiver Design](#4-esp32-s3-receiver-design)
5. [USB OTG Communication Protocol](#5-usb-otg-communication-protocol)
6. [Android App Architecture](#6-android-app-architecture)
7. [Feature Mapping](#7-feature-mapping)
8. [Development Phases](#8-development-phases)
9. [Testing Strategy](#9-testing-strategy)
10. [Bill of Materials](#10-bill-of-materials)
11. [Performance Requirements](#11-performance-requirements)

---

## 1. Current Desktop GS Architecture Analysis

### 1.1 High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Desktop Ground Station                    │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌───────────────┐  ┌──────────────┐  ┌─────────────────┐  │
│  │  Comms Layer  │  │ Video Decode │  │   OSD/Stats     │  │
│  │   (pcap)      │  │ (MJPEG/H264) │  │   Rendering     │  │
│  └───────┬───────┘  └──────┬───────┘  └────────┬────────┘  │
│          │                  │                    │            │
│          └──────────────────┴────────────────────┘            │
│                             │                                 │
│                    ┌────────▼─────────┐                       │
│                    │   Main Loop      │                       │
│                    │   - ImGui UI     │                       │
│                    │   - SDL2 Window  │                       │
│                    │   - Recording    │                       │
│                    └──────────────────┘                       │
│                                                               │
└─────────────────────────────────────────────────────────────┘
                             │
                    ┌────────▼─────────┐
                    │ WiFi Interface   │
                    │ (Monitor Mode)   │
                    └──────────────────┘
```

### 1.2 Key Components

#### 1.2.1 Comms Layer (`gs/src/Comms.cpp`)
- **WiFi Packet Capture**: Uses libpcap for monitor mode packet reception
- **Multi-Interface Support**: Supports diversity reception with 2+ WiFi adapters
- **FEC Decoding**: Reed-Solomon forward error correction (k=6-16, n=12-32)
- **Packet Filtering**: MAC address filtering and packet version validation
- **Radiotap Header Processing**: Extracts RSSI, channel, rate information
- **Block-based Reception**: Assembles packets into blocks for FEC processing
- **Bi-directional Communication**:
  - RX: Video, OSD, Telemetry, Config packets from air
  - TX: Config, Telemetry, Control packets to air

#### 1.2.2 Video Decoders

**MJPEG Decoder** (`gs/src/Video_Decoder.cpp`):
- Uses TurboJPEG library for hardware-accelerated decoding
- Multi-threaded decoding (4 threads)
- OpenGL texture upload via PBO (Pixel Buffer Objects)
- Supports resolutions: 320x240 to 1600x1200
- Fast decoding flags: `TJ_FASTUPSAMPLE | TJFLAG_FASTDCT`

**H.264 Decoder** (`gs/src/H264_Decoder.cpp`):
- Uses FFmpeg (libavcodec) for decoding
- Low-latency configuration: `AV_CODEC_FLAG_LOW_DELAY`
- Multi-threaded decoding (4 threads)
- YUV to RGB conversion via libswscale
- NAL unit parsing and frame assembly

#### 1.2.3 OSD System (`gs/src/osd.cpp`)
- Grid-based rendering: 53x20 character grid
- Walksnail font support (PNG-based)
- Overlay statistics and telemetry data
- Customizable fonts

#### 1.2.4 Recording System
- **Format**: AVI container with MJPEG video
- **Features**:
  - Start/stop recording via button
  - Free space checking
  - Frame indexing for proper AVI structure
  - Configurable FPS based on camera settings

### 1.3 Communication Protocol

#### Packet Types (Air to Ground):
```c
enum class Type : uint8_t {
    Video,      // MJPEG or H.264 frames
    Telemetry,  // MAVLink telemetry
    OSD,        // OSD data + statistics
    Config      // Camera/WiFi configuration
};
```

#### Packet Types (Ground to Air):
```c
enum class Type : uint8_t {
    Telemetry,  // MAVLink telemetry to FC
    Config,     // Camera/WiFi settings
    Connect     // Connection handshake
};
```

#### Packet Structure:
```c
// Air to Ground Video Packet
struct Air2Ground_Video_Packet {
    Air2Ground_Header header;      // 14 bytes
    Resolution resolution;          // 1 byte
    uint8_t part_index : 7;        // Frame part number
    uint8_t last_part : 1;         // Last part flag
    uint32_t frame_index;          // Frame counter
    // Payload follows (JPEG/H264 data)
};

// FEC Packet Header (from structures.h)
struct Packet_Header {
    uint32_t block_index : 24;     // FEC block number
    uint32_t packet_index : 8;     // Packet within block
    uint16_t size;                 // Payload size
};
```

---

## 2. Feature Inventory

### 2.1 Core Features (Must-Have)

| Feature | Current Implementation | Priority | Notes |
|---------|----------------------|----------|-------|
| WiFi Packet Reception | libpcap monitor mode | **P0** | ESP32-S3 will handle |
| FEC Decoding | fec_decode() in Comms.cpp | **P0** | ESP32-S3 handles |
| MJPEG Decoding | TurboJPEG (Video_Decoder.cpp) | **P0** | Android MediaCodec |
| H.264 Decoding | FFmpeg (H264_Decoder.cpp) | **P0** | Android MediaCodec |
| Video Display | OpenGL textures + ImGui | **P0** | Android SurfaceView |
| OSD Rendering | Custom grid renderer | **P0** | Android Canvas |
| Statistics Display | ImGui panels | **P0** | Android UI |
| Recording | AVI file writer | **P1** | MediaMuxer/MediaRecorder |
| Bi-directional Telemetry | Serial packets | **P1** | USB OTG |
| Camera Configuration | Config packets | **P1** | Android settings UI |

### 2.2 Advanced Features (Should-Have)

| Feature | Current Implementation | Priority | Notes |
|---------|----------------------|----------|-------|
| Diversity Reception | Multi-interface pcap | **P2** | Single ESP32-S3 initially |
| WiFi Channel Scanning | iwconfig commands | **P2** | ESP32-S3 handles |
| TX Power Control | iw commands | **P2** | ESP32-S3 handles |
| Latency Measurement | Ping/pong packets | **P2** | Maintain in protocol |
| MAVLink Support | HXMavlinkParser | **P2** | Android MAVLink library |
| Multiple Profiles | INI file | **P3** | SharedPreferences |

### 2.3 Features Removed/Modified

| Feature | Reason | Alternative |
|---------|--------|-------------|
| Multi-interface RX | Hardware complexity | Future: 2x ESP32-S3 |
| Monitor Mode Setup | Not needed on Android | ESP32-S3 handles |
| SDL2/ImGui UI | Desktop only | Native Android UI |

---

## 3. Android GS System Architecture

### 3.1 Overall System Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                         Android Device                           │
├─────────────────────────────────────────────────────────────────┤
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │                    Android App Layer                        │ │
│  ├────────────────────────────────────────────────────────────┤ │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │ │
│  │  │   UI Layer   │  │ Video Decode │  │  OSD Render  │    │ │
│  │  │  (Activity/  │  │ (MediaCodec) │  │   (Canvas)   │    │ │
│  │  │  Fragments)  │  │              │  │              │    │ │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘    │ │
│  │         │                  │                  │             │ │
│  │         └──────────────────┴──────────────────┘             │ │
│  │                            │                                │ │
│  │                   ┌────────▼────────┐                       │ │
│  │                   │  Service Layer  │                       │ │
│  │                   │  - Packet RX    │                       │ │
│  │                   │  - Telemetry    │                       │ │
│  │                   │  - Recording    │                       │ │
│  │                   └────────┬────────┘                       │ │
│  │                            │                                │ │
│  │                   ┌────────▼────────┐                       │ │
│  │                   │  USB OTG Layer  │                       │ │
│  │                   │  (UsbManager/   │                       │ │
│  │                   │   UsbDevice)    │                       │ │
│  │                   └────────┬────────┘                       │ │
│  └────────────────────────────┼────────────────────────────────┘ │
│                                │                                  │
└────────────────────────────────┼──────────────────────────────────┘
                                 │ USB-C Cable
                    ┌────────────▼────────────┐
                    │    ESP32-S3 Module      │
                    │  (WiFi RX + FEC Decode) │
                    └────────────┬────────────┘
                                 │ WiFi
                    ┌────────────▼────────────┐
                    │   ESP32-CAM Air Unit    │
                    └─────────────────────────┘
```

### 3.2 Data Flow

```
Air Unit (ESP32-CAM)
    │
    │ WiFi Packets (Monitor Mode)
    │ - Video frames (MJPEG/H264)
    │ - OSD data
    │ - Statistics
    │ - Telemetry
    ▼
ESP32-S3 Receiver
    │ 1. WiFi packet capture
    │ 2. FEC decoding (6/12, 8/16, 12/20)
    │ 3. Packet reassembly
    │ 4. Frame assembly
    │
    │ USB Bulk Transfer (480 Mbps)
    │ - Complete video frames
    │ - OSD buffers
    │ - Statistics
    │ - Telemetry packets
    ▼
Android App (USB Host)
    │ 1. USB packet reception
    │ 2. Frame parsing
    │ 3. Video decoding (MediaCodec)
    │ 4. OSD overlay rendering
    │ 5. UI updates
    │ 6. Optional recording
    │
    ▼
Display (60 FPS)
```

---

## 4. ESP32-S3 Receiver Design

### 4.1 Hardware Selection

**Recommended Module**: ESP32-S3-DevKitC-1 or similar

**Key Requirements**:
- ESP32-S3 with **USB OTG support** (native USB peripheral)
- **8MB Flash** minimum (for firmware + buffering)
- **8MB PSRAM** (for FEC buffers and packet queues)
- External antenna connector for better WiFi reception
- Power via USB (5V @ 500mA typical)

### 4.2 ESP32-S3 Firmware Architecture

```
┌─────────────────────────────────────────────────────────┐
│               ESP32-S3 Firmware Architecture             │
├─────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────────────────────────────────────────────┐   │
│  │            Main Task (Core 0)                     │   │
│  │  - USB communication                              │   │
│  │  - Packet routing                                 │   │
│  │  - Statistics                                     │   │
│  └──────────────────────────────────────────────────┘   │
│                          │                               │
│  ┌──────────────────────┼───────────────────────────┐   │
│  │                      ▼                            │   │
│  │         Dual Core Task Distribution              │   │
│  │                                                   │   │
│  │  ┌───────────────────────┐  ┌──────────────────┐│   │
│  │  │   Core 1: WiFi RX     │  │  Core 0: USB TX  ││   │
│  │  │                       │  │                  ││   │
│  │  │  - Monitor mode RX    │  │  - USB bulk TX   ││   │
│  │  │  - Promiscuous mode   │  │  - Frame queue   ││   │
│  │  │  - Packet filtering   │  │  - Flow control  ││   │
│  │  │  - FEC decoding       │  │                  ││   │
│  │  │  - Frame assembly     │  │                  ││   │
│  │  └───────────┬───────────┘  └──────────────────┘│   │
│  │              │                                   │   │
│  └──────────────┼───────────────────────────────────┘   │
│                 │                                        │
│  ┌──────────────▼─────────────┐                         │
│  │     Shared Memory Queues   │                         │
│  │  - Video frame queue       │                         │
│  │  - OSD queue               │                         │
│  │  - Telemetry queue         │                         │
│  │  - Config queue            │                         │
│  └────────────────────────────┘                         │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### 4.3 Key Firmware Components

#### 4.3.1 WiFi Monitor Mode Implementation

**File**: `esp32s3_receiver/main/wifi_monitor.c`

```c
// Pseudo-code structure
typedef struct {
    uint8_t protocol;
    uint8_t bandwidth;
    int8_t rssi;
    uint32_t timestamp;
    uint16_t length;
    uint8_t* data;
} wifi_promiscuous_pkt_t;

void wifi_promiscuous_callback(void* buf, wifi_promiscuous_pkt_type_t type) {
    // 1. Extract packet from WiFi driver
    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;

    // 2. Filter by MAC address (0x11223344556)
    if (!check_mac_filter(pkt->data)) return;

    // 3. Extract FEC header
    Packet_Header* hdr = parse_packet_header(pkt->data);

    // 4. Store in FEC block buffer
    store_fec_packet(hdr->block_index, hdr->packet_index, pkt);

    // 5. Try FEC decoding if block complete
    if (is_block_ready(hdr->block_index)) {
        fec_decode_block(hdr->block_index);
    }
}

void wifi_init_monitor_mode(uint8_t channel) {
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_NULL);
    esp_wifi_start();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_callback);
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}
```

#### 4.3.2 FEC Decoder

**File**: `esp32s3_receiver/main/fec_handler.c`

- Port existing FEC library from desktop GS
- Uses PSRAM for block buffers
- Configurable K/N parameters (6/12, 8/16, 12/20)
- Block timeout handling for missing packets

#### 4.3.3 USB Device Implementation

**File**: `esp32s3_receiver/main/usb_device.c`

```c
// USB Device Descriptor
#define USB_VENDOR_ID   0x303A  // Espressif VID
#define USB_PRODUCT_ID  0x1234  // Custom PID
#define USB_DEVICE_CLASS 0xFF   // Vendor specific

// USB Endpoints
#define EP_VIDEO_OUT    0x01  // Bulk OUT (ESP32 -> Android)
#define EP_CONTROL_IN   0x81  // Bulk IN (Android -> ESP32)

// USB Transfer Types
typedef enum {
    USB_TRANSFER_VIDEO,      // Video frame data
    USB_TRANSFER_OSD,        // OSD buffer
    USB_TRANSFER_TELEMETRY,  // Telemetry packet
    USB_TRANSFER_STATS,      // Statistics update
    USB_TRANSFER_CONFIG      // Configuration
} usb_transfer_type_t;

// Transfer frames to Android
void usb_send_frame(uint8_t* data, size_t len, usb_transfer_type_t type) {
    // Prepend transfer header
    usb_frame_header_t header = {
        .magic = 0xDEADBEEF,
        .type = type,
        .length = len,
        .timestamp_us = esp_timer_get_time()
    };

    // Send header + data
    tinyusb_cdcacm_write_queue(BULK_EP, &header, sizeof(header));
    tinyusb_cdcacm_write_queue(BULK_EP, data, len);
    tinyusb_cdcacm_write_flush(BULK_EP, 0);
}
```

### 4.4 Memory Management

**Flash Usage**:
- Firmware: ~1.5 MB
- FEC library: ~200 KB
- WiFi drivers: ~800 KB
- USB stack: ~100 KB
- **Total**: ~2.6 MB (fits in 4 MB easily)

**RAM Usage** (8 MB PSRAM):
- FEC block buffers: 1 MB (circular buffer)
- Video frame queue: 2 MB (4 frames @ 500 KB each)
- OSD buffers: 100 KB
- Telemetry buffers: 50 KB
- Stack/heap: 500 KB
- **Total**: ~3.65 MB

### 4.5 Power Consumption

- WiFi RX active: ~180 mA
- USB active: ~50 mA
- CPU processing: ~100 mA
- **Total**: ~330 mA @ 3.3V ≈ 1.1W

USB 2.0 provides 500 mA @ 5V = 2.5W, sufficient for operation.

---

## 5. USB OTG Communication Protocol

### 5.1 Physical Layer

**Connection**:
- USB-C to USB-C cable (or USB-A to USB-C with OTG adapter)
- USB 2.0 High Speed (480 Mbps)
- Bulk transfer endpoints

**Android USB Host API**:
```java
UsbManager usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);
UsbDevice device = // ... find device by VID/PID
UsbDeviceConnection connection = usbManager.openDevice(device);
UsbEndpoint epOut = // ... bulk OUT endpoint
UsbEndpoint epIn = // ... bulk IN endpoint
```

### 5.2 Packet Format

#### 5.2.1 Transfer Frame Header

```c
#pragma pack(push, 1)

typedef struct {
    uint32_t magic;           // 0xDEADBEEF
    uint8_t type;             // Transfer type
    uint8_t flags;            // Reserved
    uint16_t sequence;        // Sequence number
    uint32_t length;          // Payload length
    uint64_t timestamp_us;    // ESP32 timestamp
    uint16_t crc16;           // Header CRC
} usb_frame_header_t;  // 22 bytes

#pragma pack(pop)
```

#### 5.2.2 Transfer Types

```c
enum usb_transfer_type {
    TYPE_VIDEO_MJPEG     = 0x01,  // Complete MJPEG frame
    TYPE_VIDEO_H264      = 0x02,  // H.264 NAL unit
    TYPE_OSD_BUFFER      = 0x03,  // OSD character grid
    TYPE_STATISTICS      = 0x04,  // Air + GS statistics
    TYPE_TELEMETRY_DOWN  = 0x05,  // MAVLink from FC
    TYPE_CONFIG_DOWN     = 0x06,  // Camera/WiFi config from air

    // Android -> ESP32
    TYPE_TELEMETRY_UP    = 0x81,  // MAVLink to FC
    TYPE_CONFIG_UP       = 0x82,  // Camera/WiFi config to air
    TYPE_COMMAND         = 0x83   // Control commands
};
```

#### 5.2.3 Example: Video Frame Transfer

```
┌─────────────────────────────────────────────────────────┐
│  USB Transfer (Video Frame)                             │
├─────────────────────────────────────────────────────────┤
│  Header (22 bytes):                                     │
│    magic:      0xDEADBEEF                               │
│    type:       TYPE_VIDEO_MJPEG (0x01)                  │
│    flags:      0x00                                     │
│    sequence:   1234                                     │
│    length:     45678 bytes                              │
│    timestamp:  123456789 us                             │
│    crc16:      0xABCD                                   │
├─────────────────────────────────────────────────────────┤
│  Payload (45678 bytes):                                 │
│    JPEG SOI marker (0xFF 0xD8)                          │
│    JPEG compressed data                                 │
│    ...                                                  │
│    JPEG EOI marker (0xFF 0xD9)                          │
└─────────────────────────────────────────────────────────┘
```

### 5.3 Flow Control

**Buffering Strategy**:
1. ESP32-S3 maintains 4-frame circular buffer
2. Android signals "frame consumed" after decoding
3. If buffer full, ESP32 drops oldest frame

**Bandwidth Calculation**:
- Video: 800x600 MJPEG @ 60 FPS ≈ 60 MB/s
- USB 2.0 theoretical: 480 Mbps = 60 MB/s
- USB 2.0 practical: ~35 MB/s
- **Result**: Need frame compression or reduced resolution

**Optimization**:
- Use H.264 for higher resolutions (better compression)
- Limit to 640x480 @ 60 FPS or 800x600 @ 30 FPS for MJPEG

---

## 6. Android App Architecture

### 6.1 Project Structure

```
android_gs/
├── app/
│   ├── src/
│   │   ├── main/
│   │   │   ├── java/com/espressif/fpv/
│   │   │   │   ├── MainActivity.java
│   │   │   │   ├── service/
│   │   │   │   │   ├── UsbService.java
│   │   │   │   │   ├── VideoDecodeService.java
│   │   │   │   │   └── TelemetryService.java
│   │   │   │   ├── ui/
│   │   │   │   │   ├── VideoFragment.java
│   │   │   │   │   ├── StatsFragment.java
│   │   │   │   │   ├── SettingsFragment.java
│   │   │   │   │   └── OsdView.java
│   │   │   │   ├── decoder/
│   │   │   │   │   ├── MjpegDecoder.java
│   │   │   │   │   └── H264Decoder.java
│   │   │   │   ├── usb/
│   │   │   │   │   ├── UsbPacketParser.java
│   │   │   │   │   └── UsbTransferManager.java
│   │   │   │   └── util/
│   │   │   │       ├── FpsCounter.java
│   │   │   │       └── Statistics.java
│   │   │   └── res/
│   │   │       ├── layout/
│   │   │       ├── values/
│   │   │       └── xml/
│   │   │           └── device_filter.xml
│   │   └── AndroidManifest.xml
│   └── build.gradle
├── gradle.properties
└── settings.gradle
```

### 6.2 Key Android Components

#### 6.2.1 USB Service

**File**: `UsbService.java`

```java
public class UsbService extends Service {
    private UsbDeviceConnection connection;
    private UsbEndpoint epIn, epOut;
    private ExecutorService executor;

    private static final int VID = 0x303A;  // Espressif
    private static final int PID = 0x1234;  // Custom

    @Override
    public void onCreate() {
        super.onCreate();
        executor = Executors.newSingleThreadExecutor();
        connectToDevice();
    }

    private void connectToDevice() {
        UsbManager manager = (UsbManager) getSystemService(USB_SERVICE);

        for (UsbDevice device : manager.getDeviceList().values()) {
            if (device.getVendorId() == VID && device.getProductId() == PID) {
                requestPermission(device);
                break;
            }
        }
    }

    private void startReceiving() {
        executor.execute(() -> {
            byte[] buffer = new byte[512 * 1024];  // 512 KB buffer

            while (isRunning) {
                // Bulk transfer with 1 second timeout
                int bytesRead = connection.bulkTransfer(
                    epIn, buffer, buffer.length, 1000);

                if (bytesRead > 0) {
                    processUsbData(buffer, bytesRead);
                }
            }
        });
    }

    private void processUsbData(byte[] data, int length) {
        // Parse USB frame header
        UsbFrameHeader header = UsbFrameHeader.parse(data);

        // Route to appropriate handler
        switch (header.type) {
            case TYPE_VIDEO_MJPEG:
            case TYPE_VIDEO_H264:
                sendToVideoDecoder(data, header);
                break;
            case TYPE_OSD_BUFFER:
                sendToOsdRenderer(data, header);
                break;
            case TYPE_STATISTICS:
                updateStatistics(data, header);
                break;
            case TYPE_TELEMETRY_DOWN:
                forwardToTelemetry(data, header);
                break;
        }
    }
}
```

#### 6.2.2 Video Decoder Service

**File**: `VideoDecodeService.java`

```java
public class VideoDecodeService extends Service {
    private MediaCodec mjpegDecoder;
    private MediaCodec h264Decoder;
    private Surface outputSurface;

    @Override
    public void onCreate() {
        initializeDecoders();
    }

    private void initializeDecoders() {
        try {
            // MJPEG decoder
            mjpegDecoder = MediaCodec.createDecoderByType("video/mjpeg");
            MediaFormat mjpegFormat = MediaFormat.createVideoFormat(
                "video/mjpeg", 800, 600);
            mjpegDecoder.configure(mjpegFormat, outputSurface, null, 0);
            mjpegDecoder.start();

            // H.264 decoder
            h264Decoder = MediaCodec.createDecoderByType("video/avc");
            MediaFormat h264Format = MediaFormat.createVideoFormat(
                "video/avc", 1280, 720);
            // Low latency configuration
            h264Format.setInteger(MediaFormat.KEY_LOW_LATENCY, 1);
            h264Format.setInteger(MediaFormat.KEY_PRIORITY, 0);
            h264Decoder.configure(h264Format, outputSurface, null, 0);
            h264Decoder.start();

        } catch (IOException e) {
            Log.e(TAG, "Failed to initialize decoders", e);
        }
    }

    public void decodeFrame(byte[] frameData, boolean isH264) {
        MediaCodec decoder = isH264 ? h264Decoder : mjpegDecoder;

        int inputBufferId = decoder.dequeueInputBuffer(10000);
        if (inputBufferId >= 0) {
            ByteBuffer inputBuffer = decoder.getInputBuffer(inputBufferId);
            inputBuffer.clear();
            inputBuffer.put(frameData);

            decoder.queueInputBuffer(
                inputBufferId, 0, frameData.length,
                System.nanoTime() / 1000, 0);
        }

        // Output to surface
        MediaCodec.BufferInfo info = new MediaCodec.BufferInfo();
        int outputBufferId = decoder.dequeueOutputBuffer(info, 0);
        if (outputBufferId >= 0) {
            decoder.releaseOutputBuffer(outputBufferId, true);
        }
    }
}
```

#### 6.2.3 OSD View

**File**: `OsdView.java`

```java
public class OsdView extends View {
    private static final int OSD_COLS = 53;
    private static final int OSD_ROWS = 20;

    private Paint textPaint;
    private Bitmap fontBitmap;
    private byte[][] osdBuffer;

    public OsdView(Context context) {
        super(context);
        initPaint();
        loadFont();
    }

    private void loadFont() {
        // Load Walksnail font from assets
        try {
            InputStream is = getContext().getAssets().open("fonts/font.png");
            fontBitmap = BitmapFactory.decodeStream(is);
        } catch (IOException e) {
            Log.e(TAG, "Failed to load font", e);
        }
    }

    public void updateOsd(byte[] buffer) {
        // Parse OSD buffer (53x20 grid)
        for (int row = 0; row < OSD_ROWS; row++) {
            for (int col = 0; col < OSD_COLS; col++) {
                osdBuffer[row][col] = buffer[row * OSD_COLS + col];
            }
        }
        invalidate();  // Trigger redraw
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        int charWidth = getWidth() / OSD_COLS;
        int charHeight = getHeight() / OSD_ROWS;

        for (int row = 0; row < OSD_ROWS; row++) {
            for (int col = 0; col < OSD_COLS; col++) {
                int charCode = osdBuffer[row][col] & 0xFF;
                if (charCode != 0) {
                    drawCharacter(canvas, charCode,
                        col * charWidth, row * charHeight,
                        charWidth, charHeight);
                }
            }
        }
    }

    private void drawCharacter(Canvas canvas, int code,
                               int x, int y, int w, int h) {
        // Extract character from font bitmap
        int srcX = (code % 16) * 36;  // Font grid: 16x16
        int srcY = (code / 16) * 54;

        Rect src = new Rect(srcX, srcY, srcX + 36, srcY + 54);
        Rect dst = new Rect(x, y, x + w, y + h);

        canvas.drawBitmap(fontBitmap, src, dst, textPaint);
    }
}
```

#### 6.2.4 Main Activity

**File**: `MainActivity.java`

```java
public class MainActivity extends AppCompatActivity {
    private SurfaceView videoView;
    private OsdView osdView;
    private TextView statsText;

    private UsbService usbService;
    private VideoDecodeService videoService;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        setupViews();
        startServices();
    }

    private void setupViews() {
        videoView = findViewById(R.id.video_view);
        osdView = findViewById(R.id.osd_view);
        statsText = findViewById(R.id.stats_text);

        // Make OSD overlay video
        osdView.setZOrderOnTop(true);
        osdView.getHolder().setFormat(PixelFormat.TRANSLUCENT);
    }

    private void startServices() {
        Intent usbIntent = new Intent(this, UsbService.class);
        bindService(usbIntent, usbConnection, BIND_AUTO_CREATE);

        Intent videoIntent = new Intent(this, VideoDecodeService.class);
        videoIntent.putExtra("surface", videoView.getHolder().getSurface());
        bindService(videoIntent, videoConnection, BIND_AUTO_CREATE);
    }

    private void updateStats(Statistics stats) {
        runOnUiThread(() -> {
            String text = String.format(
                "FPS: %.1f  RSSI: %d dBm\n" +
                "Packets: %d  Lost: %d (%.1f%%)\n" +
                "Latency: %d ms",
                stats.fps, stats.rssi,
                stats.packetsReceived, stats.packetsLost,
                stats.packetLossPercent,
                stats.latencyMs
            );
            statsText.setText(text);
        });
    }
}
```

### 6.3 Layout Structure

**File**: `res/layout/activity_main.xml`

```xml
<?xml version="1.0" encoding="utf-8"?>
<RelativeLayout xmlns:android="http://schemas.android.com/apk/res/android"
    android:layout_width="match_parent"
    android:layout_height="match_parent"
    android:background="@android:color/black">

    <!-- Video Surface -->
    <SurfaceView
        android:id="@+id/video_view"
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        android:layout_centerInParent="true" />

    <!-- OSD Overlay -->
    <com.espressif.fpv.ui.OsdView
        android:id="@+id/osd_view"
        android:layout_width="match_parent"
        android:layout_height="match_parent"
        android:background="@android:color/transparent" />

    <!-- Statistics Overlay -->
    <TextView
        android:id="@+id/stats_text"
        android:layout_width="wrap_content"
        android:layout_height="wrap_content"
        android:layout_alignParentTop="true"
        android:layout_alignParentEnd="true"
        android:padding="16dp"
        android:textColor="@android:color/white"
        android:textSize="14sp"
        android:background="#80000000"
        android:fontFamily="monospace" />

    <!-- Controls -->
    <LinearLayout
        android:layout_width="match_parent"
        android:layout_height="wrap_content"
        android:layout_alignParentBottom="true"
        android:orientation="horizontal"
        android:padding="8dp"
        android:background="#80000000">

        <ImageButton
            android:id="@+id/btn_record"
            android:layout_width="48dp"
            android:layout_height="48dp"
            android:src="@drawable/ic_record"
            android:background="?attr/selectableItemBackground" />

        <ImageButton
            android:id="@+id/btn_settings"
            android:layout_width="48dp"
            android:layout_height="48dp"
            android:layout_marginStart="16dp"
            android:src="@drawable/ic_settings"
            android:background="?attr/selectableItemBackground" />
    </LinearLayout>

</RelativeLayout>
```

### 6.4 Permissions & Manifest

**File**: `AndroidManifest.xml`

```xml
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="com.espressif.fpv">

    <!-- USB Host support -->
    <uses-feature android:name="android.hardware.usb.host" />

    <!-- Permissions -->
    <uses-permission android:name="android.permission.USB_PERMISSION" />
    <uses-permission android:name="android.permission.WRITE_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.READ_EXTERNAL_STORAGE" />
    <uses-permission android:name="android.permission.WAKE_LOCK" />

    <application
        android:allowBackup="true"
        android:icon="@mipmap/ic_launcher"
        android:label="@string/app_name"
        android:theme="@style/AppTheme">

        <activity
            android:name=".MainActivity"
            android:screenOrientation="landscape"
            android:configChanges="orientation|screenSize"
            android:theme="@style/AppTheme.Fullscreen">
            <intent-filter>
                <action android:name="android.intent.action.MAIN" />
                <category android:name="android.intent.category.LAUNCHER" />
            </intent-filter>

            <!-- USB device attached intent -->
            <intent-filter>
                <action android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED" />
            </intent-filter>

            <meta-data
                android:name="android.hardware.usb.action.USB_DEVICE_ATTACHED"
                android:resource="@xml/device_filter" />
        </activity>

        <service android:name=".service.UsbService" />
        <service android:name=".service.VideoDecodeService" />

    </application>
</manifest>
```

**File**: `res/xml/device_filter.xml`

```xml
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <usb-device
        vendor-id="0x303A"
        product-id="0x1234" />
</resources>
```

---

## 7. Feature Mapping

### 7.1 Current Desktop GS → Android Implementation

| Desktop Feature | Desktop Implementation | Android Implementation | Status |
|-----------------|----------------------|----------------------|--------|
| **WiFi Packet Capture** | libpcap (Comms.cpp) | ESP32-S3 WiFi promiscuous mode | **Moved to ESP32** |
| **Monitor Mode Setup** | iwconfig/iw commands | ESP32-S3 firmware handles | **Moved to ESP32** |
| **FEC Decoding** | fec_decode() software | ESP32-S3 with PSRAM | **Moved to ESP32** |
| **Packet Filtering** | BPF filters in pcap | ESP32-S3 MAC filtering | **Moved to ESP32** |
| **Frame Assembly** | C++ deques in Comms | ESP32-S3 frame builder | **Moved to ESP32** |
| **MJPEG Decode** | TurboJPEG library | MediaCodec API | **Native Android** |
| **H.264 Decode** | FFmpeg (libavcodec) | MediaCodec API | **Native Android** |
| **Video Display** | OpenGL + SDL2 | SurfaceView | **Native Android** |
| **OSD Rendering** | Custom OpenGL | Canvas + Bitmap fonts | **Native Android** |
| **Statistics** | ImGui panels | TextView overlays | **Native Android** |
| **Recording** | AVI file writer | MediaMuxer/MediaRecorder | **Native Android** |
| **Settings UI** | ImGui menus | Fragments + Preferences | **Native Android** |
| **Telemetry** | Serial packets | USB bulk transfers | **USB Protocol** |
| **Config Sync** | INI files | SharedPreferences | **Native Android** |

### 7.2 Performance Comparison

| Metric | Desktop GS (Raspberry Pi 4) | Android GS (Snapdragon 8xx) |
|--------|----------------------------|----------------------------|
| Video Decode | Software (TurboJPEG/FFmpeg) | Hardware (MediaCodec) |
| Max Resolution | 1280x720 @ 60 FPS | 1920x1080 @ 60 FPS |
| Latency | 40-60 ms | 30-50 ms (target) |
| Power | 5W | 2-3W |
| Cost | $35-75 + WiFi adapter | Phone + $5 ESP32-S3 |

---

## 8. Development Phases

### Phase 1: ESP32-S3 Receiver Prototype (4 weeks)

**Week 1-2: Basic WiFi & USB**
- [ ] Set up ESP-IDF project
- [ ] Implement WiFi promiscuous mode packet capture
- [ ] Implement USB device stack (TinyUSB)
- [ ] Test basic packet reception and USB forwarding

**Week 3: FEC Integration**
- [ ] Port FEC library from desktop GS
- [ ] Implement block-based buffering in PSRAM
- [ ] Test FEC decoding with real air unit

**Week 4: Frame Assembly**
- [ ] Implement video frame assembly logic
- [ ] Add OSD packet handling
- [ ] Add statistics/telemetry packet handling
- [ ] Optimize memory usage and throughput

**Deliverables**:
- Working ESP32-S3 firmware
- USB communication validated with PC tool
- FEC decoding functional

---

### Phase 2: Android USB Communication (3 weeks)

**Week 5-6: USB Service**
- [ ] Create Android Studio project
- [ ] Implement USB device detection
- [ ] Implement bulk transfer reading
- [ ] Create packet parser for USB protocol
- [ ] Test with ESP32-S3 receiver

**Week 7: Data Routing**
- [ ] Implement video frame queue
- [ ] Implement OSD buffer queue
- [ ] Implement statistics updates
- [ ] Add logging and debugging

**Deliverables**:
- Android app receives packets from ESP32-S3
- Packet parsing and routing functional

---

### Phase 3: Video Decoding & Display (3 weeks)

**Week 8-9: MediaCodec Integration**
- [ ] Implement MJPEG decoder service
- [ ] Implement H.264 decoder service
- [ ] Connect decoder to SurfaceView
- [ ] Test with real video frames
- [ ] Measure latency

**Week 10: OSD Rendering**
- [ ] Create OsdView custom view
- [ ] Load Walksnail font assets
- [ ] Implement character grid rendering
- [ ] Test OSD overlay on video

**Deliverables**:
- Video display working
- OSD overlay rendering
- Latency < 60 ms

---

### Phase 4: UI & Settings (2 weeks)

**Week 11: Main UI**
- [ ] Design main activity layout
- [ ] Add statistics overlay
- [ ] Add record button
- [ ] Add settings button
- [ ] Implement fullscreen mode

**Week 12: Settings Fragment**
- [ ] Camera settings screen
- [ ] WiFi settings screen
- [ ] OSD settings screen
- [ ] Save/load preferences

**Deliverables**:
- Complete UI implementation
- User can configure camera/WiFi

---

### Phase 5: Recording & Telemetry (2 weeks)

**Week 13: Recording**
- [ ] Implement MediaMuxer recording
- [ ] Add start/stop recording logic
- [ ] Add storage space checking
- [ ] Test with long recordings

**Week 14: Telemetry**
- [ ] Implement telemetry forwarding (Android → ESP32)
- [ ] Implement telemetry reception (ESP32 → Android)
- [ ] Add MAVLink parsing (optional)
- [ ] Test with flight controller

**Deliverables**:
- Recording functional
- Telemetry passing through

---

### Phase 6: Testing & Optimization (2 weeks)

**Week 15: Performance Testing**
- [ ] Measure end-to-end latency
- [ ] Test at various resolutions/framerates
- [ ] Test WiFi range
- [ ] Test with different Android devices
- [ ] Profile CPU/memory usage

**Week 16: Bug Fixes & Polish**
- [ ] Fix critical bugs
- [ ] Optimize hotspots
- [ ] Add error handling
- [ ] Add user documentation

**Deliverables**:
- Production-ready app
- Performance validated
- User guide

---

## 9. Testing Strategy

### 9.1 Unit Testing

**ESP32-S3 Firmware**:
```c
// Test WiFi packet filtering
void test_packet_filter() {
    uint8_t valid_packet[] = {/* MAC: 0x112233445566 */};
    assert(check_mac_filter(valid_packet) == true);

    uint8_t invalid_packet[] = {/* MAC: 0xAABBCCDDEEFF */};
    assert(check_mac_filter(invalid_packet) == false);
}

// Test FEC decoding
void test_fec_decode() {
    // Simulate receiving 8 out of 12 packets
    simulate_packet_loss(4);
    bool result = fec_decode_block(0);
    assert(result == true);
}
```

**Android App**:
```java
@Test
public void testUsbPacketParsing() {
    byte[] testPacket = createMockVideoPacket();
    UsbFrameHeader header = UsbFrameHeader.parse(testPacket);
    assertEquals(TYPE_VIDEO_MJPEG, header.type);
    assertEquals(45678, header.length);
}

@Test
public void testOsdBufferUpdate() {
    byte[] osdData = createMockOsdBuffer();
    osdView.updateOsd(osdData);
    // Verify buffer updated correctly
}
```

### 9.2 Integration Testing

**Test Case 1: End-to-End Video**
1. ESP32-CAM transmits MJPEG frame
2. ESP32-S3 receives via WiFi
3. ESP32-S3 FEC decodes
4. ESP32-S3 sends via USB
5. Android receives and parses
6. Android decodes MJPEG
7. Android displays on screen
8. **Measure**: Total latency < 60 ms

**Test Case 2: OSD Overlay**
1. ESP32-CAM transmits OSD buffer
2. ESP32-S3 forwards via USB
3. Android updates OSD view
4. **Verify**: OSD matches air unit display

**Test Case 3: Bi-directional Telemetry**
1. Android sends config change
2. ESP32-S3 receives via USB
3. ESP32-S3 transmits via WiFi
4. ESP32-CAM receives and applies
5. ESP32-CAM sends ack
6. **Verify**: Config changed on air unit

### 9.3 Field Testing

**Range Test**:
- [ ] Test at 100m, 200m, 500m, 1km
- [ ] Measure RSSI and packet loss
- [ ] Compare with desktop GS

**Duration Test**:
- [ ] 30 minute continuous flight
- [ ] Monitor for frame drops, memory leaks
- [ ] Check battery drain on phone

**Multi-Device Test**:
- [ ] Test on Samsung Galaxy S20+
- [ ] Test on Google Pixel 7
- [ ] Test on OnePlus 9 Pro
- [ ] Test on budget Android (Snapdragon 660)

---

## 10. Bill of Materials

### 10.1 Hardware Components

| Item | Part Number | Quantity | Unit Price | Total | Notes |
|------|------------|----------|-----------|-------|-------|
| **ESP32-S3 Module** | ESP32-S3-DevKitC-1 (8MB) | 1 | $5.00 | $5.00 | With external antenna |
| **USB-C Cable** | USB 2.0 High Speed | 1 | $2.00 | $2.00 | OTG capable |
| **3D Printed Case** | Custom design | 1 | $1.00 | $1.00 | Material cost |
| **Optional: External WiFi Antenna** | 2.4GHz 5dBi | 1 | $3.00 | $3.00 | For extended range |
| **Optional: USB-C OTG Adapter** | For phones without OTG | 1 | $2.00 | $2.00 | If needed |

**Total Hardware Cost**: ~$13 (without optional items)

### 10.2 Development Tools

| Tool | Cost | Notes |
|------|------|-------|
| ESP-IDF | Free | Espressif SDK |
| Android Studio | Free | IDE |
| Android Device | $0-500 | Use existing phone |
| Logic Analyzer (optional) | $20 | For USB debugging |

---

## 11. Performance Requirements

### 11.1 Latency Budget

```
┌─────────────────────────────────────────────────────┐
│         End-to-End Latency Breakdown                │
├─────────────────────────────────────────────────────┤
│  Camera capture              :   5-10 ms            │
│  JPEG encoding               :  10-15 ms            │
│  WiFi transmission           :   5-10 ms            │
│  ──────────────────────────────────────             │
│  ESP32-S3 RX + FEC           :   5-8 ms             │
│  USB transfer                :   1-2 ms             │
│  Android USB read            :   1-2 ms             │
│  Android decode (MediaCodec) :   8-12 ms            │
│  Display render              :   1-2 ms             │
│  ──────────────────────────────────────             │
│  TOTAL LATENCY               :  36-61 ms            │
├─────────────────────────────────────────────────────┤
│  Target: < 60 ms  ✓                                 │
└─────────────────────────────────────────────────────┘
```

### 11.2 Throughput Requirements

**Video Bandwidth** (worst case: 800x600 MJPEG @ 60 FPS):
- Frame size: ~50 KB (compressed)
- Data rate: 50 KB × 60 FPS = 3 MB/s
- FEC overhead (12/20): 1.67x = 5 MB/s
- WiFi overhead: 1.2x = 6 MB/s
- **Required WiFi rate**: ~48 Mbps → Use MCS3 (26 Mbps) or higher

**USB Bandwidth**:
- Video: 3 MB/s
- OSD: 2 KB @ 10 Hz = 20 KB/s
- Stats: 100 bytes @ 10 Hz = 1 KB/s
- Telemetry: ~5 KB/s
- **Total**: ~3.03 MB/s (well within USB 2.0 35 MB/s limit)

### 11.3 Android Device Requirements

**Minimum**:
- Android 8.0 (API 26) or higher
- Snapdragon 660 or equivalent
- 4 GB RAM
- USB OTG support
- 720p display

**Recommended**:
- Android 11 (API 30) or higher
- Snapdragon 845 or equivalent
- 6 GB RAM
- USB 3.0 OTG
- 1080p display with 90+ Hz refresh rate

### 11.4 Power Consumption

**ESP32-S3 Receiver**:
- Average: 1.1W
- Powered by Android device via USB
- Battery drain on phone: ~500 mAh/hour

**Android App**:
- Screen on (720p): ~500 mW
- Video decode: ~300 mW
- USB: ~100 mW
- **Total**: ~900 mW additional drain

**Expected Battery Life** (5000 mAh phone):
- Normal phone drain: 500 mAh/h
- With FPV app: 1400 mAh/h
- **Runtime**: ~3.5 hours continuous

---

## 12. Risks & Mitigation

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| **USB bandwidth insufficient** | High | Medium | Use H.264 for high-res, limit MJPEG to 640x480 |
| **Android hardware decode latency** | High | Low | Profile on multiple devices, fallback to software decode |
| **ESP32-S3 WiFi sensitivity worse than desktop** | Medium | Medium | Add external antenna option, use diversity (2x ESP32) |
| **USB OTG compatibility issues** | Medium | Low | Test on wide range of devices, provide compatibility list |
| **FEC decoding too slow on ESP32** | High | Low | Optimize with PSRAM DMA, pre-validate performance |
| **Android app crashes on low-end devices** | Medium | Medium | Set minimum API level, test on budget devices |
| **Power drain too high** | Low | Low | Optimize USB polling, use hardware decode |

---

## 13. Future Enhancements

### Phase 2 Features (Post-MVP)

1. **Diversity Reception**: 2x ESP32-S3 modules with antenna diversity
2. **WiFi 6 Support**: Use ESP32-C6 for 802.11ax (future air unit upgrade)
3. **DVR Playback**: Play back recorded flights in app
4. **Cloud Upload**: Auto-upload recordings to cloud storage
5. **Flight Telemetry Overlay**: Integrate with MAVLink for real-time flight data
6. **Multi-Camera Support**: Switch between multiple air units
7. **Screen Recording**: Record Android screen with OSD overlays
8. **Wireless Charging**: Support charging phone while using FPV
9. **Head Tracking**: Use phone gyro for camera gimbal control
10. **AR Overlay**: Augmented reality markers and waypoints

---

## 14. Success Criteria

### Minimum Viable Product (MVP)

- ✅ Video streaming at 640x480 @ 60 FPS with < 60 ms latency
- ✅ OSD overlay rendering correctly
- ✅ Statistics display (FPS, RSSI, packet loss)
- ✅ Camera configuration via app
- ✅ WiFi channel/power configuration
- ✅ Recording to phone storage
- ✅ Stable for 30+ minute flights
- ✅ Works on 3+ different Android devices

### Performance Targets

- **Latency**: < 60 ms (glass-to-glass)
- **FPS**: 60 FPS sustained
- **Range**: Equal to desktop GS (500m+)
- **Packet Loss**: < 5% at 200m
- **Battery Life**: 3+ hours on 5000 mAh phone
- **App Size**: < 50 MB

---

## 15. References

### Desktop GS Source Files

- `/home/user/hx-esp32-cam-fpv/gs/src/main.cpp` - Main application
- `/home/user/hx-esp32-cam-fpv/gs/src/Comms.cpp` - WiFi communication
- `/home/user/hx-esp32-cam-fpv/gs/src/Video_Decoder.cpp` - MJPEG decoder
- `/home/user/hx-esp32-cam-fpv/gs/src/H264_Decoder.cpp` - H.264 decoder
- `/home/user/hx-esp32-cam-fpv/gs/src/osd.cpp` - OSD rendering
- `/home/user/hx-esp32-cam-fpv/components/common/packets.h` - Protocol definitions

### External Documentation

- ESP-IDF Programming Guide: https://docs.espressif.com/projects/esp-idf/
- Android USB Host: https://developer.android.com/guide/topics/connectivity/usb
- Android MediaCodec: https://developer.android.com/reference/android/media/MediaCodec
- TinyUSB: https://github.com/hathach/tinyusb
- FEC Library: https://github.com/catid/longhair

---

## Appendix A: ESP32-S3 Pin Assignment

```
┌──────────────────────────────────────────┐
│         ESP32-S3 Pin Assignment          │
├──────────────────────────────────────────┤
│  USB D+/D-      : GPIO 19/20 (internal)  │
│  WiFi Antenna   : Internal/External      │
│  Status LED     : GPIO 2                 │
│  Debug UART     : GPIO 43/44 (USB-JTAG)  │
│  Boot Button    : GPIO 0                 │
│  Reset Button   : EN pin                 │
└──────────────────────────────────────────┘
```

---

## Appendix B: USB Protocol Detailed Specification

### Frame Header (22 bytes)

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;        // 0xDEADBEEF (4 bytes)
    uint8_t type;          // Transfer type (1 byte)
    uint8_t flags;         // Reserved (1 byte)
    uint16_t sequence;     // Rolling sequence number (2 bytes)
    uint32_t length;       // Payload length (4 bytes)
    uint64_t timestamp_us; // Microsecond timestamp (8 bytes)
    uint16_t crc16;        // CRC16 of header (2 bytes)
} usb_frame_header_t;
```

### Transfer Types

```c
#define TYPE_VIDEO_MJPEG     0x01  // MJPEG frame
#define TYPE_VIDEO_H264_SPS  0x02  // H.264 SPS NAL
#define TYPE_VIDEO_H264_PPS  0x03  // H.264 PPS NAL
#define TYPE_VIDEO_H264_IDR  0x04  // H.264 IDR frame
#define TYPE_VIDEO_H264_P    0x05  // H.264 P frame
#define TYPE_OSD_BUFFER      0x10  // OSD 53x20 grid
#define TYPE_STATISTICS      0x11  // Stats packet
#define TYPE_TELEMETRY_DOWN  0x12  // Telemetry from FC
#define TYPE_CONFIG_DOWN     0x13  // Config from air unit

// Commands (Android → ESP32)
#define TYPE_TELEMETRY_UP    0x81  // Telemetry to FC
#define TYPE_CONFIG_UP       0x82  // Config to air unit
#define TYPE_COMMAND         0x83  // Control commands
```

### Statistics Packet Payload

```c
typedef struct __attribute__((packed)) {
    // WiFi statistics
    int8_t rssi_dbm;
    int8_t noise_floor_dbm;
    uint8_t channel;
    uint8_t wifi_rate;

    // Packet statistics
    uint16_t packets_received;
    uint16_t packets_lost;
    uint16_t fec_blocks_decoded;
    uint16_t fec_blocks_failed;

    // Video statistics
    uint16_t frames_received;
    uint16_t frames_dropped;
    uint8_t current_fps;
    uint8_t target_fps;

    // Latency
    uint16_t latency_ms;

} statistics_packet_t;
```

---

## Appendix C: Android Dependencies

### build.gradle (app level)

```gradle
dependencies {
    implementation 'androidx.appcompat:appcompat:1.6.1'
    implementation 'androidx.constraintlayout:constraintlayout:2.1.4'
    implementation 'androidx.preference:preference:1.2.1'

    // USB communication
    implementation 'com.github.felHR85:UsbSerial:6.1.0'

    // Video recording
    implementation 'androidx.media:media:1.6.0'

    // Logging
    implementation 'com.jakewharton.timber:timber:5.0.1'

    // Testing
    testImplementation 'junit:junit:4.13.2'
    androidTestImplementation 'androidx.test.ext:junit:1.1.5'
    androidTestImplementation 'androidx.test.espresso:espresso-core:3.5.1'
}
```

---

## Appendix D: Development Timeline Summary

```
Month 1: ESP32-S3 Firmware
  ├─ Week 1-2: WiFi + USB basics
  ├─ Week 3: FEC integration
  └─ Week 4: Frame assembly

Month 2: Android Core
  ├─ Week 5-6: USB service
  └─ Week 7: Data routing

Month 3: Android Features
  ├─ Week 8-9: Video decode
  └─ Week 10: OSD rendering

Month 4: UI & Polish
  ├─ Week 11-12: UI + Settings
  ├─ Week 13-14: Recording + Telemetry
  └─ Week 15-16: Testing

Total: 4 months to MVP
```

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-01-XX | AI Assistant | Initial comprehensive plan |

---

**END OF DOCUMENT**

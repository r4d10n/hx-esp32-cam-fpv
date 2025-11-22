# HX-ESP32 FPV Ground Station - Android Application Architecture

## Table of Contents
1. [Overview](#overview)
2. [Architecture Pattern](#architecture-pattern)
3. [Component Diagram](#component-diagram)
4. [Layer Architecture](#layer-architecture)
5. [Threading Model](#threading-model)
6. [Data Flow](#data-flow)
7. [Memory Management](#memory-management)
8. [Key Components](#key-components)
9. [Communication Protocol](#communication-protocol)
10. [Performance Optimizations](#performance-optimizations)

---

## Overview

The HX-ESP32 FPV Ground Station is an Android application designed to receive and display real-time H.264 video streams from an ESP32-S3 device via USB connection. The application follows modern Android development best practices using Jetpack Compose, Kotlin Coroutines, and the MVVM architectural pattern.

### Key Features
- **USB Communication**: Direct connection to ESP32-S3 via USB serial
- **H.264 Video Decoding**: Hardware-accelerated decoding using MediaCodec
- **Real-time Rendering**: Low-latency video display using SurfaceView
- **On-Screen Display (OSD)**: Telemetry overlay with statistics and flight data
- **Video Recording**: MP4 recording with MediaMuxer
- **Settings Management**: Configurable OSD and video parameters

---

## Architecture Pattern

The application follows the **MVVM (Model-View-ViewModel)** architecture pattern combined with **Clean Architecture** principles:

```
┌─────────────────────────────────────────────────────────┐
│                    Presentation Layer                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │  MainActivity │  │SettingsActivity│ │  Composables │  │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │
│         │                  │                  │          │
│         └──────────────────┴──────────────────┘          │
│                           │                              │
└───────────────────────────┼──────────────────────────────┘
                            │
┌───────────────────────────┼──────────────────────────────┐
│                    ViewModel Layer                       │
│                  ┌─────────────────┐                     │
│                  │  MainViewModel  │                     │
│                  └────────┬────────┘                     │
│                           │                              │
└───────────────────────────┼──────────────────────────────┘
                            │
┌───────────────────────────┼──────────────────────────────┐
│                      Domain Layer                        │
│  ┌────────────────┐  ┌────────────┐  ┌────────────────┐ │
│  │ ConnectionState│  │ VideoState │  │ RecordingState │ │
│  │   OsdData      │  │ Statistics │  │   OsdSettings  │ │
│  └────────────────┘  └────────────┘  └────────────────┘ │
└───────────────────────────┼──────────────────────────────┘
                            │
┌───────────────────────────┼──────────────────────────────┐
│                      Service Layer                       │
│  ┌──────────────────┐ ┌──────────────┐ ┌─────────────┐  │
│  │UsbCommunication  │ │VideoDecoder  │ │  Recording  │  │
│  │    Service       │ │   Service    │ │   Service   │  │
│  └────────┬─────────┘ └──────┬───────┘ └──────┬──────┘  │
└───────────┼────────────────────┼─────────────────┼────────┘
            │                    │                 │
┌───────────┼────────────────────┼─────────────────┼────────┐
│                      Hardware Layer                       │
│  ┌────────┴─────────┐ ┌───────┴────────┐ ┌──────┴──────┐ │
│  │  UsbDeviceManager│ │  H264Decoder   │ │ MediaMuxer  │ │
│  │  USB Serial Lib  │ │  MediaCodec    │ │   Encoder   │ │
│  └──────────────────┘ └────────────────┘ └─────────────┘ │
└──────────────────────────────────────────────────────────┘
```

### Design Principles
1. **Separation of Concerns**: Each layer has a specific responsibility
2. **Unidirectional Data Flow**: Data flows from services → ViewModel → UI
3. **Single Source of Truth**: ViewModel holds the UI state
4. **Reactive Programming**: StateFlow for reactive state updates
5. **Dependency Injection**: Services are injected via Android binding

---

## Component Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                          MainActivity                             │
│                                                                   │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │                      Jetpack Compose UI                     │  │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐ │  │
│  │  │ VideoSurface │  │  OSD Overlay │  │ Control Buttons  │ │  │
│  │  │ (SurfaceView)│  │   (Canvas)   │  │   (Compose)      │ │  │
│  │  └──────────────┘  └──────────────┘  └──────────────────┘ │  │
│  └────────────────────────────────────────────────────────────┘  │
│                              │                                    │
│                              ▼                                    │
│                      ┌──────────────┐                             │
│                      │ MainViewModel │                            │
│                      │  StateFlows  │                             │
│                      └──────────────┘                             │
└──────────────────────────────┬───────────────────────────────────┘
                               │
                 ┌─────────────┼─────────────┐
                 │             │             │
                 ▼             ▼             ▼
    ┌────────────────┐ ┌─────────────┐ ┌──────────────┐
    │      USB       │ │    Video    │ │  Recording   │
    │ Communication  │ │   Decoder   │ │   Service    │
    │    Service     │ │   Service   │ │              │
    └────────┬───────┘ └──────┬──────┘ └──────┬───────┘
             │                │               │
             ▼                ▼               ▼
    ┌────────────────┐ ┌─────────────┐ ┌──────────────┐
    │UsbDeviceManager│ │ H264Decoder │ │ MediaMuxer   │
    │                │ │ MediaCodec  │ │ MediaCodec   │
    └────────┬───────┘ └──────┬──────┘ └──────────────┘
             │                │
             ▼                ▼
    ┌────────────────┐ ┌─────────────┐
    │   ESP32-S3     │ │   Surface   │
    │  USB Device    │ │   Display   │
    └────────────────┘ └─────────────┘
```

---

## Layer Architecture

### 1. Presentation Layer (UI)
**Location**: `app/src/main/java/com/hxesp32/fpvgs/ui/`

- **MainActivity.kt**: Entry point, manages service lifecycle
- **SettingsActivity.kt**: Configuration screen
- **Composables**: Reusable UI components
- **Theme**: Material Design 3 theming

**Responsibilities**:
- Display video and OSD
- Handle user interactions
- Observe ViewModel state
- Manage activity lifecycle

### 2. ViewModel Layer
**Location**: `app/src/main/java/com/hxesp32/fpvgs/viewmodel/`

- **MainViewModel.kt**: Central state management

**Responsibilities**:
- Hold UI state using StateFlow
- Coordinate between services
- Business logic for UI state transformations
- Survive configuration changes

### 3. Domain Layer (Data Models)
**Location**: `app/src/main/java/com/hxesp32/fpvgs/data/model/`

- **ConnectionState.kt**: USB connection states
- **VideoState.kt**: Video streaming states
- **RecordingState.kt**: Recording states
- **OsdData.kt**: Telemetry data
- **OsdSettings.kt**: Configuration

**Responsibilities**:
- Define data structures
- Represent business entities
- Immutable data classes

### 4. Service Layer
**Location**: `app/src/main/java/com/hxesp32/fpvgs/service/`

#### UsbCommunicationService
- **Type**: Foreground Service
- **Responsibilities**:
  - Manage USB device connection
  - Read data from USB serial
  - Send commands to ESP32-S3
  - Emit connection state

#### VideoDecoderService
- **Type**: Foreground Service
- **Responsibilities**:
  - Parse H.264 NAL units
  - Decode video frames using MediaCodec
  - Render to Surface
  - Calculate statistics (FPS, bitrate, latency)

#### RecordingService
- **Type**: Foreground Service
- **Responsibilities**:
  - Encode video frames
  - Mux into MP4 container
  - Manage file I/O
  - Emit recording state

### 5. Hardware Abstraction Layer
**Location**: `app/src/main/java/com/hxesp32/fpvgs/usb/`, `video/`

- **UsbDeviceManager**: USB serial communication
- **H264Decoder**: MediaCodec wrapper
- **VideoStreamParser**: NAL unit parser

---

## Threading Model

The application uses Kotlin Coroutines for concurrent operations:

```
Main Thread (UI Thread)
├── Jetpack Compose UI rendering
├── ViewModel state collection
└── Activity lifecycle

IO Dispatcher (Background)
├── USB read operations
├── File I/O for recording
└── Settings persistence

Default Dispatcher (CPU-intensive)
├── Video frame parsing
├── Statistics calculation
└── Data transformations

MediaCodec Threads (Internal)
├── H.264 decoding
├── Frame rendering
└── Buffer management
```

### Thread Safety
- **StateFlow**: Thread-safe reactive state
- **Coroutine Scopes**: Structured concurrency
- **Service Lifecycle**: Bound to component lifecycle
- **Atomic Operations**: For counters and flags

### Synchronization Points
1. USB read → Video parser (IO → Default)
2. Decoded frames → Surface (MediaCodec → Main)
3. Statistics → UI (Default → Main)
4. Recording frames → File (Default → IO)

---

## Data Flow

### Video Streaming Flow

```
ESP32-S3 Device
    │
    │ [USB Serial @ 2 Mbps]
    ▼
UsbDeviceManager
    │
    │ [ByteArray (64KB chunks)]
    ▼
UsbCommunicationService
    │
    │ [dataCallback]
    ▼
VideoDecoderService
    │
    │ [H.264 Stream]
    ▼
VideoStreamParser
    │
    │ [NAL Units]
    ▼
H264Decoder (MediaCodec)
    │
    │ [Decoded Frames]
    ▼
SurfaceView
    │
    │ [Display]
    ▼
Screen (User)
```

### State Update Flow

```
Service Layer
    │
    │ [StateFlow emission]
    ▼
ViewModel
    │
    │ [State transformation]
    ▼
Compose UI
    │
    │ [collectAsStateWithLifecycle]
    ▼
Recomposition
    │
    ▼
Screen Update
```

### Recording Flow

```
H264Decoder
    │
    │ [Encoded Frame Data]
    ▼
RecordingService
    │
    │ [MediaCodec Buffer]
    ▼
MediaMuxer
    │
    │ [MP4 Container]
    ▼
File System
    │
    ▼
/Movies/HX-ESP32-FPV/recording.mp4
```

---

## Memory Management

### Buffer Management

#### USB Read Buffer
- **Size**: 64 KB
- **Type**: ByteArray (reused)
- **Allocation**: Per service instance
- **Lifetime**: Service lifecycle

#### NAL Unit Buffer
- **Size**: 1 MB
- **Type**: ByteBuffer
- **Allocation**: VideoStreamParser
- **Management**: Circular buffer with overflow protection

#### MediaCodec Buffers
- **Type**: Hardware-managed
- **Count**: Configured by MediaCodec (typically 4-8)
- **Lifecycle**: Automatically managed
- **Memory**: GPU memory (for Surface output)

### Memory Optimization Strategies

1. **Object Pooling**: Reuse byte arrays for USB reads
2. **Lazy Initialization**: Create decoders only when needed
3. **Resource Cleanup**: Release MediaCodec in onDestroy
4. **Bitmap Recycling**: For OSD rendering (if using bitmaps)
5. **Large Heap**: Enabled in manifest for video buffers

### Memory Pressure Handling

```kotlin
override fun onTrimMemory(level: Int) {
    when (level) {
        TRIM_MEMORY_RUNNING_LOW -> {
            // Reduce buffer sizes
            // Clear frame caches
        }
        TRIM_MEMORY_UI_HIDDEN -> {
            // Pause video if in background
            // Release non-critical resources
        }
    }
}
```

---

## Key Components

### 1. UsbDeviceManager

**Purpose**: Manages USB device connection and I/O

**Key Methods**:
```kotlin
fun findAndConnect()
fun disconnect()
fun read(buffer: ByteArray, timeout: Int): Int
fun write(data: ByteArray, timeout: Int): Int
```

**State Management**:
- Uses StateFlow<ConnectionState>
- Emits connection events
- Handles permissions via BroadcastReceiver

**Error Handling**:
- Permission denied
- Device disconnected
- Read/write timeouts
- Invalid device

### 2. H264Decoder

**Purpose**: Hardware-accelerated H.264 decoding

**Key Methods**:
```kotlin
fun initialize(width: Int, height: Int, surface: Surface)
fun decodeFrame(frameData: ByteArray, presentationTimeUs: Long): Boolean
fun flush()
fun release()
```

**Configuration**:
- **Codec**: `video/avc` (H.264)
- **Color Format**: `COLOR_FormatSurface`
- **Latency Mode**: Low latency enabled
- **Priority**: Real-time

**Performance Optimizations**:
- Input buffer timeout: 10ms
- Output buffer timeout: 10ms
- Immediate rendering (render=true)
- Statistics updated every 1 second

### 3. VideoStreamParser

**Purpose**: Parse H.264 NAL units from byte stream

**Algorithm**:
1. Accumulate bytes into buffer
2. Detect NAL start codes (0x00 0x00 0x01 or 0x00 0x00 0x00 0x01)
3. Extract complete NAL units
4. Emit to decoder

**NAL Unit Types**:
- **SPS (7)**: Sequence Parameter Set
- **PPS (8)**: Picture Parameter Set
- **IDR (5)**: Keyframe
- **Non-IDR (1)**: P-frame

### 4. OsdOverlay

**Purpose**: Render on-screen display elements

**Rendering Technology**: Jetpack Compose Canvas

**Elements**:
- FPS counter
- Bitrate indicator
- Latency display
- RSSI (signal strength)
- Battery voltage/percentage
- GPS data (satellites, position)
- Altitude, Speed
- Armed indicator
- Recording indicator

**Color Coding**:
- Green: Good (RSSI > 70%, Battery > 50%)
- Yellow: Warning (RSSI 40-70%, Battery 20-50%)
- Red: Critical (RSSI < 40%, Battery < 20%)

### 5. RecordingManager (RecordingService)

**Purpose**: Record video to MP4 file

**Recording Pipeline**:
```
Decoded Frames → MediaCodec Encoder → MediaMuxer → MP4 File
```

**Configuration**:
- **Format**: MP4 (MPEG-4 container)
- **Video Codec**: H.264
- **Bitrate**: 5 Mbps (configurable)
- **Frame Rate**: 30 fps (configurable)
- **Output**: `/Movies/HX-ESP32-FPV/fpv_recording_YYYYMMDD_HHMMSS.mp4`

**Features**:
- Start/stop recording
- Real-time file size and duration
- Frame counter
- Error recovery

---

## Communication Protocol

### USB Serial Protocol

**Configuration**:
- **Baud Rate**: 2,000,000 (2 Mbps)
- **Data Bits**: 8
- **Stop Bits**: 1
- **Parity**: None
- **Flow Control**: None

### Data Packet Structure

```
┌─────────────────────────────────────────────┐
│              Packet Header (4 bytes)         │
├──────┬──────┬──────┬──────────────────────┤
│ SYNC │ TYPE │ SIZE │      PAYLOAD         │
│ 0xAA │ 0xXX │ 0xNN │   (variable length)  │
└──────┴──────┴──────┴──────────────────────┘
```

**Packet Types**:
- `0x01`: Video data (H.264 NAL units)
- `0x02`: OSD/Telemetry data
- `0x03`: Command (GS → Air)
- `0x04`: Acknowledgment
- `0xFF`: Keep-alive/heartbeat

### Video Data Format

H.264 Annex B byte stream format:
```
[Start Code] [NAL Unit] [Start Code] [NAL Unit] ...
   (3-4 bytes)  (variable)   (3-4 bytes)  (variable)
```

Start codes:
- `0x00 0x00 0x01` (short form)
- `0x00 0x00 0x00 0x01` (long form)

### OSD Data Packet

```
struct OsdPacket {
    uint8_t rssi;           // Signal strength (0-100)
    float battery_voltage;  // Volts
    uint8_t battery_percent;// 0-100
    float temperature;      // Celsius
    char flight_mode[16];   // String
    bool armed;             // Boolean
    double gps_lat;         // Latitude
    double gps_lon;         // Longitude
    float gps_alt;          // Altitude (m)
    uint8_t gps_sats;       // Satellite count
    // ... additional fields
}
```

---

## Performance Optimizations

### 1. Low Latency Video Decoding

**Techniques**:
- MediaCodec low-latency mode enabled
- Minimal buffer queuing (2-4 frames)
- Immediate frame rendering
- No frame reordering

**Expected Latency**: 50-100ms glass-to-glass

### 2. Efficient USB Communication

**Optimizations**:
- Large read buffer (64 KB)
- Asynchronous I/O with coroutines
- Zero-copy where possible
- Batch processing of small packets

### 3. UI Rendering

**Compose Optimizations**:
- Immutable state for fewer recompositions
- Remember expensive calculations
- Lazy composition where applicable
- Hardware acceleration enabled

### 4. Memory Efficiency

**Strategies**:
- Reuse buffers for USB reads
- Limit NAL unit buffer size
- Release resources promptly
- Monitor memory pressure

### 5. Battery Optimization

**Techniques**:
- Wake lock only when streaming
- Pause in background (optional)
- Efficient coroutine usage
- Minimize wake-ups

---

## Build Configuration

### Gradle Configuration

**Minimum SDK**: 26 (Android 8.0)
- Required for modern MediaCodec features
- USB host API improvements
- Better coroutine support

**Target SDK**: 34 (Android 14)
- Latest Android features
- Security updates
- Performance improvements

### Dependencies

**Core Libraries**:
- Jetpack Compose (UI)
- Kotlin Coroutines (Concurrency)
- ViewModel & Lifecycle (State)
- DataStore (Settings persistence)

**Media Libraries**:
- MediaCodec (Video decoding)
- MediaMuxer (Recording)

**USB Library**:
- `usb-serial-for-android` (USB communication)

**Utilities**:
- Timber (Logging)

---

## Testing Strategy

### Unit Tests
**Location**: `app/src/test/`

- VideoStreamParser tests
- NAL unit parsing
- State transformations
- ViewModel logic

### Integration Tests
**Location**: `app/src/androidTest/`

- USB communication flow
- Video decoding pipeline
- Recording functionality
- OSD rendering

### Performance Tests
- Frame decode time
- USB throughput
- Memory usage
- Battery consumption

---

## Future Enhancements

1. **WiFi Support**: Add WiFi connectivity as alternative to USB
2. **Multi-camera**: Support multiple video sources
3. **Advanced OSD**: Customizable OSD layout
4. **Telemetry Logging**: Record telemetry to file
5. **Cloud Sync**: Upload recordings to cloud storage
6. **AI Features**: Object detection, tracking
7. **DVR Mode**: Automatic crash detection and saving
8. **Screen Recording**: Share-friendly output format

---

## References

- [Android MediaCodec Documentation](https://developer.android.com/reference/android/media/MediaCodec)
- [Jetpack Compose Documentation](https://developer.android.com/jetpack/compose)
- [Kotlin Coroutines Guide](https://kotlinlang.org/docs/coroutines-guide.html)
- [H.264 Specification](https://www.itu.int/rec/T-REC-H.264)
- [USB Serial for Android](https://github.com/mik3y/usb-serial-for-android)

---

## Conclusion

The HX-ESP32 FPV Ground Station Android application provides a robust, low-latency solution for receiving and displaying real-time video from ESP32-based FPV systems. The architecture is designed for performance, maintainability, and extensibility, following modern Android development best practices.

The modular design allows for easy addition of new features and supports future enhancements while maintaining code quality and performance.

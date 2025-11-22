# Android Ground Station - Project Summary

## Project Overview

**Project Name**: HX-ESP32 FPV Ground Station (Android)
**Package**: com.hxesp32.fpvgs
**Target Platform**: Android 8.0 (API 26) and above
**Language**: Kotlin
**UI Framework**: Jetpack Compose
**Architecture**: MVVM with Clean Architecture

## Project Statistics

- **Kotlin Source Files**: 38 files
- **XML Resource Files**: 9 files
- **Services**: 3 foreground services
- **ViewModels**: 1 main ViewModel
- **Activities**: 2 activities
- **Data Models**: 6 sealed classes/data classes

## Key Technologies

### Android Jetpack
- **Compose**: Modern declarative UI framework
- **ViewModel**: UI state management
- **Lifecycle**: Activity/service lifecycle handling
- **Navigation**: Screen navigation (future)
- **DataStore**: Settings persistence (future)

### Media
- **MediaCodec**: Hardware H.264 video decoding
- **MediaMuxer**: MP4 container creation for recording
- **Surface**: Video rendering surface

### Concurrency
- **Kotlin Coroutines**: Asynchronous programming
- **Flow**: Reactive data streams
- **StateFlow**: State management

### USB Communication
- **usb-serial-for-android**: USB serial communication library
- **USB Host API**: Android USB host support

### Logging
- **Timber**: Logging library

## Architecture Layers

### 1. Presentation Layer
**Files**: 12 files
**Location**: `app/src/main/java/com/hxesp32/fpvgs/ui/`

- MainActivity.kt
- SettingsActivity.kt
- MainScreen.kt (Composable)
- SettingsScreen.kt (Composable)
- StatisticsScreen.kt (Composable)
- Theme.kt
- Type.kt
- Components (future)

### 2. ViewModel Layer
**Files**: 1 file
**Location**: `app/src/main/java/com/hxesp32/fpvgs/viewmodel/`

- MainViewModel.kt

### 3. Domain Layer
**Files**: 4 files
**Location**: `app/src/main/java/com/hxesp32/fpvgs/data/model/`

- ConnectionState.kt
- VideoState.kt
- RecordingState.kt
- OsdData.kt

### 4. Service Layer
**Files**: 3 files
**Location**: `app/src/main/java/com/hxesp32/fpvgs/service/`

- UsbCommunicationService.kt
- VideoDecoderService.kt
- RecordingService.kt

### 5. Hardware Abstraction Layer
**Files**: 5+ files
**Locations**: `usb/`, `video/`, `osd/`

- UsbDeviceManager.kt
- H264Decoder.kt
- VideoStreamParser.kt
- MediaCodecHelper.kt
- VideoRenderer.kt
- OsdOverlay.kt

## Component Dependencies

```
MainActivity
    ├── MainViewModel
    │   ├── ConnectionState
    │   ├── VideoState
    │   ├── RecordingState
    │   ├── OsdData
    │   └── Statistics
    ├── UsbCommunicationService
    │   └── UsbDeviceManager
    ├── VideoDecoderService
    │   ├── H264Decoder
    │   └── VideoStreamParser
    └── RecordingService
        └── MediaMuxer
```

## Data Flow

### Video Pipeline
```
ESP32-S3 Device
    ↓ USB Serial (2 Mbps)
UsbDeviceManager (read)
    ↓ ByteArray (64KB chunks)
UsbCommunicationService
    ↓ dataCallback
VideoDecoderService
    ↓ H.264 byte stream
VideoStreamParser (parse NAL units)
    ↓ NAL units
H264Decoder (MediaCodec)
    ↓ Decoded frames
SurfaceView
    ↓
Display (Screen)
```

### State Flow
```
Service
    ↓ StateFlow.emit()
ViewModel
    ↓ StateFlow transformation
Composable UI
    ↓ collectAsStateWithLifecycle()
UI Recomposition
    ↓
Screen Update
```

## Services

### 1. UsbCommunicationService
- **Type**: Foreground Service
- **Lifecycle**: Bound service
- **Purpose**: USB device communication
- **Notification**: Persistent notification
- **Threading**: Coroutines on IO dispatcher

**Key Methods**:
- `connect()`: Connect to USB device
- `disconnect()`: Disconnect from device
- `sendData(data: ByteArray)`: Write to device
- `setDataCallback(callback)`: Set data receiver

### 2. VideoDecoderService
- **Type**: Foreground Service
- **Lifecycle**: Bound service
- **Purpose**: H.264 video decoding
- **Notification**: Persistent notification
- **Threading**: Coroutines on Default dispatcher

**Key Methods**:
- `initializeDecoder(width, height, surface)`: Setup decoder
- `processVideoData(data: ByteArray)`: Decode video
- `flush()`: Flush decoder buffers
- `releaseDecoder()`: Cleanup resources

### 3. RecordingService
- **Type**: Foreground Service
- **Lifecycle**: Bound service
- **Purpose**: Video recording to MP4
- **Notification**: Persistent notification
- **Threading**: Coroutines on IO dispatcher

**Key Methods**:
- `startRecording(width, height, bitrate, frameRate)`: Start recording
- `stopRecording()`: Stop and finalize recording
- `writeFrame(data, timestamp, isKeyFrame)`: Write encoded frame

## State Management

All states are managed using Kotlin StateFlow for reactive updates:

### ConnectionState
```kotlin
sealed class ConnectionState {
    object Disconnected
    object Connecting
    data class Connected(deviceInfo)
    data class Error(message, throwable)
}
```

### VideoState
```kotlin
sealed class VideoState {
    object Idle
    object Initializing
    data class Streaming(streamInfo)
    data class Error(message, throwable)
}
```

### RecordingState
```kotlin
sealed class RecordingState {
    object Idle
    object Starting
    data class Recording(info)
    object Stopping
    data class Error(message, throwable)
}
```

## Threading Model

### Dispatchers
- **Main**: UI updates, Compose recomposition
- **IO**: USB I/O, file operations, network
- **Default**: CPU-intensive work, parsing, calculations

### Coroutine Scopes
- **viewModelScope**: ViewModel lifecycle
- **serviceScope**: Service lifecycle
- **lifecycleScope**: Activity lifecycle

### Thread Safety
- StateFlow: Thread-safe state container
- Mutex: For critical sections (if needed)
- Atomic operations: For counters

## Performance Targets

### Latency
- **USB Read**: <10ms
- **Video Decode**: 15-30ms per frame
- **Total Glass-to-Glass**: 50-100ms

### Throughput
- **USB**: 2 Mbps sustained
- **Video**: Up to 10 Mbps bitrate
- **Frame Rate**: 30 FPS @ 640x480

### Resource Usage
- **CPU**: 15-25% on modern devices
- **RAM**: 100-200 MB active
- **Storage**: ~1 GB/hour recording
- **Battery**: 2-3 hours continuous use

## Build Configuration

### Gradle
- **Version**: 8.2
- **AGP**: 8.2.0
- **Kotlin**: 1.9.20

### SDK Versions
- **Min SDK**: 26 (Android 8.0)
- **Target SDK**: 34 (Android 14)
- **Compile SDK**: 34

### Build Types
- **Debug**: Development, logging enabled
- **Release**: Optimized, ProGuard enabled

### Dependencies
```kotlin
// Core
androidx.core:core-ktx:1.12.0
androidx.lifecycle:lifecycle-*:2.7.0

// Compose
androidx.compose:compose-bom:2024.01.00
androidx.activity:activity-compose:1.8.2

// Coroutines
kotlinx-coroutines-android:1.7.3

// USB
usb-serial-for-android:3.7.0

// Logging
timber:5.0.1
```

## Testing

### Unit Tests
**Location**: `app/src/test/`
- VideoStreamParser tests
- NAL unit parsing tests
- State transformation tests
- ViewModel logic tests

### Instrumentation Tests
**Location**: `app/src/androidTest/`
- USB communication tests
- Video decoding pipeline tests
- UI interaction tests
- Service lifecycle tests

## Documentation

### Files Created
1. **README.md**: Project overview and quick start
2. **ARCHITECTURE.md**: Detailed architecture documentation
3. **SETUP_GUIDE.md**: Development environment setup
4. **PROJECT_SUMMARY.md**: This file

### Additional Documentation (in main repo)
- USB_COMMUNICATION_GUIDE.md
- ANDROID_VIDEO_OSD_IMPLEMENTATION.md

## Future Enhancements

### Phase 1 (MVP)
- [x] USB communication
- [x] H.264 decoding
- [x] Video rendering
- [x] OSD overlay
- [x] Video recording
- [x] Basic settings

### Phase 2 (Features)
- [ ] WiFi connectivity support
- [ ] Customizable OSD layout
- [ ] Settings persistence (DataStore)
- [ ] Telemetry logging
- [ ] Multiple video resolutions
- [ ] Bitrate control

### Phase 3 (Advanced)
- [ ] Picture-in-picture mode
- [ ] Background recording
- [ ] Cloud upload
- [ ] AI features (object detection)
- [ ] Multi-camera support
- [ ] Bluetooth gamepad support

## Known Limitations

1. **USB Only**: No WiFi support yet
2. **Fixed Resolution**: 640x480 hardcoded
3. **No Settings Persistence**: Settings don't save
4. **Limited OSD Customization**: Fixed layout
5. **No Telemetry Logging**: In-memory only
6. **Single Camera**: One video source only

## Development Status

### Completed
- ✅ Project structure
- ✅ Gradle configuration
- ✅ Android manifest
- ✅ MVVM architecture
- ✅ USB communication layer
- ✅ Video decoding pipeline
- ✅ OSD overlay system
- ✅ Recording functionality
- ✅ UI (Jetpack Compose)
- ✅ State management
- ✅ Services implementation
- ✅ Documentation

### In Progress
- ⏳ Testing suite
- ⏳ Settings persistence
- ⏳ Performance optimization

### Not Started
- ❌ WiFi support
- ❌ Advanced OSD features
- ❌ Telemetry logging
- ❌ Cloud features

## Build Instructions

### Quick Build
```bash
cd android-gs
./gradlew assembleDebug
```

### Install
```bash
./gradlew installDebug
```

### Release Build
```bash
./gradlew assembleRelease
```

## Contact & Support

For questions, issues, or contributions:
- GitHub Issues
- GitHub Discussions
- Pull Requests welcome

## License

MIT License - See LICENSE file for details.

---

**Last Updated**: 2025-11-22
**Version**: 1.0.0-alpha
**Status**: Alpha - Ready for testing

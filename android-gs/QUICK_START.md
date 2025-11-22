# Android Ground Station - Quick Start Guide

## What Was Created

✅ **Complete Android Application Architecture**

### Project Structure
```
android-gs/
├── app/src/main/java/com/hxesp32/fpvgs/
│   ├── FpvApplication.kt           # Application entry point
│   ├── data/model/                 # Data models (4 files)
│   ├── service/                    # Services (3 files)
│   ├── usb/                        # USB communication
│   ├── video/                      # Video decoding (4 files)
│   ├── osd/                        # OSD overlay
│   ├── viewmodel/                  # MVVM ViewModel
│   └── ui/                         # Jetpack Compose UI (7 files)
├── docs/
│   ├── ARCHITECTURE.md             # Detailed architecture
│   ├── SETUP_GUIDE.md             # Setup instructions
│   └── PROJECT_SUMMARY.md         # Project overview
├── build.gradle.kts               # Build configuration
└── README.md                      # Main documentation
```

**Total**: 38 Kotlin files, 9 XML resources, 6 documentation files

## Getting Started in 5 Minutes

### 1. Open in Android Studio
```bash
cd hx-esp32-cam-fpv/android-gs
# Open this directory in Android Studio
```

### 2. Build the Project
```bash
./gradlew assembleDebug
```

### 3. Install on Device
```bash
./gradlew installDebug
```

### 4. Connect ESP32-S3
- Connect ESP32-S3 via USB OTG cable
- Grant USB permission when prompted
- Video stream should start automatically

## Key Features

✅ **USB Communication**
- Direct connection to ESP32-S3
- 2 Mbps serial communication
- Automatic device detection

✅ **Video Decoding**
- H.264 hardware decoding
- Low latency (<100ms)
- 30 FPS @ 640x480

✅ **OSD Overlay**
- FPS, bitrate, latency
- RSSI, battery status
- GPS, altitude, speed
- Recording indicator

✅ **Video Recording**
- MP4 format
- 5 Mbps bitrate
- Saved to /Movies/HX-ESP32-FPV/

## Architecture

**MVVM Pattern with Jetpack Compose**

```
UI (Compose) → ViewModel → Services → Hardware
```

**3 Foreground Services**:
1. UsbCommunicationService - USB I/O
2. VideoDecoderService - H.264 decoding
3. RecordingService - Video recording

## Documentation

📖 **Start Here**:
1. [README.md](README.md) - Overview and usage
2. [ARCHITECTURE.md](docs/ARCHITECTURE.md) - Technical details
3. [SETUP_GUIDE.md](docs/SETUP_GUIDE.md) - Development setup

## Next Steps

1. ✅ Project is created and ready
2. 🔧 Build with `./gradlew assembleDebug`
3. 📱 Install on Android device
4. 🎥 Connect ESP32-S3 and test
5. 🚀 Start developing new features!

## Requirements

- **Android Studio**: Hedgehog (2023.1.1) or later
- **Android Device**: 8.0+ with USB OTG
- **ESP32-S3**: With video streaming firmware

## Support

- 📚 Documentation in `docs/` directory
- 🐛 Issues: GitHub Issues
- 💬 Discussions: GitHub Discussions

---

**Project Status**: ✅ Ready for Development
**Created**: 2025-11-22

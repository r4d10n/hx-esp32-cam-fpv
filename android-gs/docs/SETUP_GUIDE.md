# Android Ground Station - Setup Guide

This guide will help you set up the development environment and build the Android Ground Station application.

## Prerequisites

### Required Software

1. **Android Studio** (Hedgehog 2023.1.1 or later)
   - Download: https://developer.android.com/studio
   - Includes Android SDK, Gradle, and emulator

2. **Java Development Kit (JDK) 17**
   - Included with Android Studio
   - Or download separately from: https://adoptium.net/

3. **Git**
   - For cloning the repository
   - Download: https://git-scm.com/

### Required Hardware

1. **Android Device**
   - Android 8.0 (API 26) or higher
   - USB OTG support
   - 2GB+ RAM recommended
   - Hardware video decoder (most modern devices)

2. **USB OTG Cable**
   - To connect ESP32-S3 to Android device

3. **Development Computer**
   - Windows, macOS, or Linux
   - 8GB+ RAM recommended
   - 10GB+ free disk space

## Step 1: Install Android Studio

### Windows/macOS

1. Download Android Studio from https://developer.android.com/studio
2. Run the installer
3. Follow the setup wizard
4. Install Android SDK components:
   - Android SDK Platform 34
   - Android SDK Build-Tools
   - Android SDK Platform-Tools
   - Android Emulator (optional)

### Linux

```bash
# Ubuntu/Debian
sudo apt update
sudo apt install android-studio

# Or download manually and extract
wget https://redirector.gvt1.com/edgedl/android/studio/ide-zips/2023.1.1.28/android-studio-2023.1.1.28-linux.tar.gz
tar -xzf android-studio-*.tar.gz
cd android-studio/bin
./studio.sh
```

## Step 2: Clone Repository

```bash
git clone https://github.com/yourusername/hx-esp32-cam-fpv.git
cd hx-esp32-cam-fpv/android-gs
```

## Step 3: Open Project in Android Studio

1. Launch Android Studio
2. Select "Open an Existing Project"
3. Navigate to `hx-esp32-cam-fpv/android-gs`
4. Click "OK"
5. Wait for Gradle sync (may take several minutes on first run)

### Gradle Sync Issues

If Gradle sync fails:

1. Check internet connection
2. Verify JDK 17 is configured:
   - File → Settings → Build, Execution, Deployment → Build Tools → Gradle
   - Gradle JDK: Select "jbr-17" or JDK 17

3. Invalidate caches:
   - File → Invalidate Caches → Invalidate and Restart

4. Manual Gradle sync:
   ```bash
   ./gradlew --refresh-dependencies
   ```

## Step 4: Configure Android Device

### Enable Developer Options

1. Go to Settings → About Phone
2. Tap "Build Number" 7 times
3. Developer options now enabled

### Enable USB Debugging

1. Go to Settings → Developer Options
2. Enable "USB Debugging"
3. Enable "Install via USB" (if available)

### Connect Device

1. Connect Android device to computer via USB
2. On device, allow USB debugging when prompted
3. In Android Studio, device should appear in device dropdown

Verify connection:
```bash
adb devices
```

## Step 5: Build the Application

### Debug Build (Command Line)

```bash
cd android-gs
./gradlew assembleDebug
```

Output: `app/build/outputs/apk/debug/app-debug.apk`

### Debug Build (Android Studio)

1. Build → Make Project (Ctrl+F9 / Cmd+F9)
2. Wait for build to complete
3. Check for errors in "Build" window

### Release Build

1. Generate signing key (first time only):
   ```bash
   keytool -genkey -v -keystore my-release-key.keystore \
           -alias my-key-alias -keyalg RSA -keysize 2048 -validity 10000
   ```

2. Configure signing in `app/build.gradle.kts`:
   ```kotlin
   android {
       signingConfigs {
           create("release") {
               storeFile = file("my-release-key.keystore")
               storePassword = "your_password"
               keyAlias = "my-key-alias"
               keyPassword = "your_password"
           }
       }
       buildTypes {
           release {
               signingConfig = signingConfigs.getByName("release")
           }
       }
   }
   ```

3. Build release APK:
   ```bash
   ./gradlew assembleRelease
   ```

## Step 6: Install on Device

### Install Debug Build

Via Android Studio:
1. Run → Run 'app' (Shift+F10 / Ctrl+R)
2. Select target device
3. App installs and launches automatically

Via Command Line:
```bash
./gradlew installDebug
```

Or manually:
```bash
adb install app/build/outputs/apk/debug/app-debug.apk
```

### Install Release Build

```bash
adb install app/build/outputs/apk/release/app-release.apk
```

Or transfer APK to device and install manually.

## Step 7: Configure USB Permissions

The app needs permission to access USB devices.

### Method 1: Automatic (Recommended)

1. Launch app
2. Connect ESP32-S3 via USB OTG
3. Tap "Connect" in app
4. Grant USB permission when prompted
5. Check "Remember this device"

### Method 2: Manual Permission

Add your device to `device_filter.xml`:

```xml
<usb-device vendor-id="YOUR_VID" product-id="YOUR_PID" />
```

Find VID/PID:
```bash
adb shell lsusb
# or
adb shell cat /proc/bus/usb/devices
```

## Step 8: Verify Installation

### Test Checklist

- [ ] App launches without crashes
- [ ] USB permission dialog appears when connecting ESP32-S3
- [ ] Video surface displays (black screen initially)
- [ ] OSD elements visible
- [ ] Control buttons responsive
- [ ] Settings screen accessible

### Logcat Monitoring

Monitor app logs:
```bash
adb logcat | grep FPV
```

Or in Android Studio:
- View → Tool Windows → Logcat
- Filter: "com.hxesp32.fpvgs"

## Troubleshooting

### Build Errors

#### "SDK location not found"

Create `local.properties`:
```properties
sdk.dir=/path/to/Android/Sdk
```

On Windows:
```properties
sdk.dir=C\:\\Users\\YourName\\AppData\\Local\\Android\\Sdk
```

#### "Gradle version incompatible"

Update Gradle wrapper:
```bash
./gradlew wrapper --gradle-version=8.2
```

#### "Unable to resolve dependency"

Check internet connection and proxy settings:
```bash
./gradlew --refresh-dependencies --debug
```

### Installation Errors

#### "INSTALL_FAILED_UPDATE_INCOMPATIBLE"

Uninstall existing version:
```bash
adb uninstall com.hxesp32.fpvgs
```

#### "Insufficient storage"

Free up space on device or install to SD card:
```bash
adb shell pm set-install-location 2
```

#### "INSTALL_FAILED_VERIFICATION_FAILURE"

Disable Play Protect:
- Open Play Store
- Menu → Play Protect → Settings
- Disable "Scan apps with Play Protect"

### Runtime Errors

#### "USB permission denied"

1. Check USB OTG cable
2. Restart app
3. Revoke USB permissions:
   ```bash
   adb shell pm clear com.hxesp32.fpvgs
   ```

#### "MediaCodec error"

Device may not support H.264 decoding. Check capabilities:
```bash
adb shell dumpsys media.codec_list
```

Look for "video/avc" decoder.

#### "No video stream"

1. Verify ESP32-S3 is transmitting
2. Check USB baud rate (2 Mbps)
3. Monitor logcat for USB read errors
4. Try different USB cable/adapter

## Development Setup

### Code Style

Install code style settings:
1. File → Settings → Editor → Code Style
2. Scheme → Import Scheme → Kotlin style guide

### Plugins (Recommended)

- Kotlin (pre-installed)
- Compose Multiplatform IDE Support
- Timber Logging
- ADB Idea (ADB tools integration)

### Emulator Setup (Testing without device)

1. Tools → Device Manager
2. Create Virtual Device
3. Select hardware profile (Pixel 5)
4. Select system image (API 34)
5. Finish and launch

Note: Emulator doesn't support USB OTG, use for UI testing only.

### Running Tests

Unit tests:
```bash
./gradlew test
```

Instrumentation tests (requires device):
```bash
./gradlew connectedAndroidTest
```

## Next Steps

1. Read [ARCHITECTURE.md](ARCHITECTURE.md) for system design
2. Review [README.md](../README.md) for usage instructions
3. Connect ESP32-S3 and test video streaming
4. Explore settings and OSD configuration
5. Try recording video to SD card

## Additional Resources

- [Android Developer Documentation](https://developer.android.com)
- [Jetpack Compose Tutorial](https://developer.android.com/jetpack/compose/tutorial)
- [Kotlin Coroutines Guide](https://kotlinlang.org/docs/coroutines-guide.html)
- [MediaCodec Guide](https://developer.android.com/reference/android/media/MediaCodec)

## Getting Help

If you encounter issues:

1. Check [Troubleshooting](#troubleshooting) section
2. Search GitHub issues
3. Create new issue with:
   - Android version
   - Device model
   - Error logs (logcat)
   - Steps to reproduce

## Contributing

See [README.md](../README.md) for contribution guidelines.

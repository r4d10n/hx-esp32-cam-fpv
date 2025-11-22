# Comprehensive Testing Plan
**Project:** ESP32-S3 Android Receiver - High-Resolution FPV System  
**Date:** 2025-11-22  
**Version:** 1.0  

---

## Executive Summary

This document outlines the comprehensive testing strategy for the ESP32-S3 WiFi receiver firmware and Android ground station application. The system receives H.264 video streams over WiFi, decodes FEC, and forwards the stream to Android via USB for display.

**Test Coverage Goals:**
- **Unit Tests:** 95%+ code coverage
- **Integration Tests:** All critical paths
- **Performance Tests:** Latency, throughput, packet loss
- **Hardware Tests:** Real-world RF conditions

---

## 1. ESP32-S3 Firmware Testing

### 1.1 WiFi Receiver Component (45 tests ✅ COMPLETE)

**Location:** `/esp32-s3-android-receiver/components/wifi_receiver/test/`

**Test Groups:**
1. **Initialization & Deinitialization (10 tests)**
   - Valid configuration
   - NULL parameters
   - Invalid channels (2.4GHz/5GHz)
   - Double initialization
   - Resource cleanup

2. **Start/Stop Operations (8 tests)**
   - Basic start/stop
   - State transitions
   - Multiple cycles
   - Promiscuous mode

3. **Channel Management (12 tests)**
   - Get/set channel
   - Channel hopping
   - Invalid channels
   - Band validation

4. **Statistics Tracking (10 tests)**
   - Packet counters
   - RSSI measurements
   - Throughput calculation
   - Buffer usage

5. **MAC Filtering (5 tests)**
   - Set/clear filters
   - Broadcast addresses
   - NULL handling

**Build Command:**
```bash
cd /home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver
idf.py build
idf.py flash monitor
```

### 1.2 FEC Decoder Component (40 tests - TO BE CREATED)

**Test Specification:**

```c
// Test file: components/fec_decoder/test/test_fec_decoder.c

// Group 1: Initialization (8 tests)
- test_fec_init_valid_config
- test_fec_init_null_params
- test_fec_init_invalid_k_n_ratio
- test_fec_deinit_cleanup

// Group 2: Block Encoding/Decoding (12 tests)
- test_fec_encode_single_block
- test_fec_decode_no_errors
- test_fec_decode_with_corrections
- test_fec_decode_uncorrectable
- test_fec_reed_solomon_6_12
- test_fec_reed_solomon_8_16

// Group 3: Packet Recovery (10 tests)
- test_fec_recover_single_packet_loss
- test_fec_recover_multiple_losses
- test_fec_recover_burst_errors
- test_fec_max_correctable_errors

// Group 4: Performance (5 tests)
- test_fec_encode_throughput
- test_fec_decode_latency
- test_fec_memory_usage

// Group 5: Edge Cases (5 tests)
- test_fec_zero_length_data
- test_fec_max_size_data
- test_fec_interleaving
```

**Key Features to Test:**
- Reed-Solomon FEC (6/12, 8/16 configurations)
- Packet interleaving
- Error correction limits
- Memory efficiency
- Encoding/decoding speed

### 1.3 USB Streamer Component (50 tests - TO BE CREATED)

**Test Specification:**

```c
// Test file: components/usb_streamer/test/test_usb_streamer.c

// Group 1: Initialization (10 tests)
- USB CDC vs Bulk mode
- Buffer allocation
- TinyUSB configuration
- Device descriptor

// Group 2: Packet Framing (15 tests)
- H.264 NAL unit framing
- Sync marker insertion
- Sequence numbering
- CRC16 validation
- Fragmentation

// Group 3: Data Transfer (15 tests)
- Bulk transfer
- CDC transfer
- Flow control
- Reconnection handling
- Buffer overrun

// Group 4: Control Commands (5 tests)
- Start/stop stream
- Set bitrate/FPS
- Request IDR frame
- Status queries

// Group 5: Performance (5 tests)
- Throughput measurement
- Latency tracking
- USB 2.0 HS speed (480 Mbps)
```

**Protocol Structure:**
```
[SYNC(2)] [TYPE(1)] [FLAGS(1)] [SIZE(2)] [SEQ(2)] [TS(4)] [PAYLOAD(N)] [CRC(2)]
```

### 1.4 Integration Tests - Full Pipeline (20 tests - TO BE CREATED)

**Test Specification:**

```c
// Test file: tests/integration/test_receiver_pipeline.c

// Group 1: WiFi → FEC → USB Pipeline (10 tests)
- test_e2e_single_frame_delivery
- test_e2e_continuous_streaming
- test_e2e_packet_loss_recovery
- test_e2e_channel_switching
- test_e2e_reconnection

// Group 2: Performance Tests (5 tests)
- test_latency_measurement (target: <50ms)
- test_throughput_measurement (target: >10 Mbps)
- test_packet_loss_tolerance (target: handle 20% loss)
- test_jitter_measurement

// Group 3: Stress Tests (5 tests)
- test_continuous_24hr_operation
- test_rapid_reconnection_cycles
- test_memory_leak_detection
- test_thermal_stability
```

**Performance Targets:**
- **Latency:** <50ms glass-to-glass
- **Throughput:** 10-15 Mbps sustained
- **Packet Loss:** Recover from <20% loss
- **Frame Rate:** 30 FPS sustained

---

## 2. Android Application Testing

### 2.1 USB Communication Layer (JUnit Tests - 25 tests)

**Test Specification:**

```kotlin
// File: android/app/src/test/java/UsbCommunicationTest.kt

class UsbCommunicationTest {
    // Group 1: Device Detection (5 tests)
    @Test fun testUsbDeviceDetection()
    @Test fun testDevicePermissions()
    @Test fun testConnectionEstablishment()
    @Test fun testReconnectionHandling()
    
    // Group 2: Data Transfer (10 tests)
    @Test fun testBulkTransferRead()
    @Test fun testBulkTransferWrite()
    @Test fun testControlTransfer()
    @Test fun testAsyncTransfer()
    @Test fun testBufferManagement()
    
    // Group 3: Protocol Parsing (10 tests)
    @Test fun testPacketHeaderParsing()
    @Test fun testSyncMarkerDetection()
    @Test fun testCrc16Validation()
    @Test fun testSequenceOrdering()
    @Test fun testFragmentReassembly()
}
```

**Build Command:**
```bash
cd /home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver/android
./gradlew test
```

### 2.2 Video Decoder (Instrumented Tests - 20 tests)

**Test Specification:**

```kotlin
// File: android/app/src/androidTest/java/VideoDecoderTest.kt

@RunWith(AndroidJUnit4::class)
class VideoDecoderTest {
    // Group 1: MediaCodec Setup (5 tests)
    @Test fun testH264CodecInitialization()
    @Test fun testSurfaceConfiguration()
    @Test fun testDecoderFormats()
    
    // Group 2: NAL Unit Processing (10 tests)
    @Test fun testSpsDecoding()
    @Test fun testPpsDecoding()
    @Test fun testIdrFrameDecoding()
    @Test fun testPFrameDecoding()
    @Test fun testNalUnitFragmentation()
    
    // Group 3: Performance (5 tests)
    @Test fun testDecodingLatency()
    @Test fun testFrameDropHandling()
    @Test fun test30FpsDecoding()
}
```

**Build Command:**
```bash
./gradlew connectedAndroidTest
```

### 2.3 UI Layer (Compose Testing - 15 tests)

**Test Specification:**

```kotlin
// File: android/app/src/androidTest/java/UiTest.kt

@RunWith(AndroidJUnit4::class)
class FpvUiTest {
    @get:Rule
    val composeTestRule = createComposeRule()
    
    // Group 1: Video Display (5 tests)
    @Test fun testVideoSurfaceRendering()
    @Test fun testFullscreenMode()
    @Test fun testAspectRatioHandling()
    
    // Group 2: OSD Elements (5 tests)
    @Test fun testRssiDisplay()
    @Test fun testLatencyDisplay()
    @Test fun testBitrateDisplay()
    @Test fun testFpsDisplay()
    
    // Group 3: Controls (5 tests)
    @Test fun testConnectionButton()
    @Test fun testSettingsDialog()
    @Test fun testRecordingControls()
}
```

### 2.4 Android Integration Tests (15 tests)

**Test Specification:**

```kotlin
// File: android/app/src/androidTest/java/IntegrationTest.kt

@RunWith(AndroidJUnit4::class)
@LargeTest
class IntegrationTest {
    // Group 1: USB → Decoder → Display (10 tests)
    @Test fun testEndToEndVideoPlayback()
    @Test fun testConnectionRecovery()
    @Test fun testFrameRateStability()
    @Test fun testLatencyMeasurement()
    
    // Group 2: User Scenarios (5 tests)
    @Test fun testAppStartupFlow()
    @Test fun testDeviceRotation()
    @Test fun testBackgroundHandling()
    @Test fun testMemoryManagement()
}
```

---

## 3. Hardware Testing Procedures

### 3.1 RF Performance Testing

**Test Setup:**
- ESP32-C5/C6 transmitter @ 20dBm
- ESP32-S3 receiver with diversity antennas
- Spectrum analyzer
- RF chamber (optional)

**Tests:**

1. **Range Test:**
   - Measure max distance with acceptable video quality
   - Test line-of-sight (LOS)
   - Test non-line-of-sight (NLOS)
   - Target: >500m LOS

2. **Interference Resistance:**
   - Co-channel WiFi interference
   - Bluetooth interference
   - Microwave oven test
   - Target: Maintain link in 10+ AP environment

3. **Antenna Diversity:**
   - Single vs dual antenna
   - RSSI switching performance
   - Packet loss comparison

### 3.2 USB Performance Testing

**Tests:**

1. **Throughput Test:**
   ```bash
   adb shell "cat /dev/video0" | pv > /dev/null
   ```
   Target: >15 MB/s (120 Mbps)

2. **Latency Test:**
   - Measure frame arrival time vs capture time
   - Target: <10ms USB transfer time

3. **Stability Test:**
   - 24-hour continuous operation
   - Monitor disconnections
   - Target: Zero unexpected disconnects

### 3.3 Thermal Testing

**Tests:**

1. **ESP32-S3 Temperature:**
   - Monitor during continuous RX
   - Target: <85°C junction temp

2. **Android Device:**
   - Monitor battery temperature
   - Test thermal throttling
   - Target: Maintain performance at 45°C ambient

---

## 4. Performance Benchmarks

### 4.1 Latency Benchmarks

| Component | Target | Measurement Method |
|-----------|--------|-------------------|
| WiFi Capture | <5ms | Timestamp at PHY layer |
| FEC Decoding | <10ms | Processing time per block |
| USB Transfer | <10ms | ESP32 → Android |
| H.264 Decode | <15ms | MediaCodec latency |
| Display | <10ms | Surface present time |
| **Total** | **<50ms** | Glass-to-glass |

**Test Procedure:**
```cpp
// Embed timestamp in video stream metadata
uint32_t capture_ts = esp_timer_get_time();
usb_streamer_send_metadata({.timestamp_us = capture_ts, ...});

// Measure on Android
long display_ts = SystemClock.elapsedRealtimeNanos() / 1000;
long latency = display_ts - packet.timestamp_us;
```

### 4.2 Throughput Benchmarks

| Resolution | FPS | Bitrate | Expected Throughput |
|------------|-----|---------|---------------------|
| 1280x720 | 30 | 4 Mbps | 10-12 Mbps (with FEC) |
| 1920x1080 | 30 | 6 Mbps | 14-16 Mbps (with FEC) |
| 640x480 | 60 | 2 Mbps | 5-7 Mbps (with FEC) |

### 4.3 Packet Loss Tolerance

| Loss Rate | Expected Behavior |
|-----------|-------------------|
| 0-5% | No visible artifacts |
| 5-10% | Occasional minor artifacts |
| 10-20% | Noticeable but acceptable |
| >20% | Significant degradation |

**Test Procedure:**
```python
# Simulate packet loss in test environment
iptables -A INPUT -m statistic --mode random --probability 0.1 -j DROP
```

---

## 5. Build Validation

### 5.1 ESP32-S3 Firmware Build

**Script:** `scripts/build_esp32s3.sh`

```bash
#!/bin/bash
set -e

echo "=== ESP32-S3 Receiver Build Validation ==="

cd /home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver

# Clean build
idf.py fullclean

# Configure for ESP32-S3
idf.py set-target esp32s3

# Build
idf.py build

# Check binary size
BUILD_SIZE=$(stat -c%s build/esp32-s3-receiver.bin)
if [ $BUILD_SIZE -gt 4194304 ]; then
    echo "ERROR: Binary size $BUILD_SIZE exceeds 4MB limit"
    exit 1
fi

# Run unit tests
idf.py build-tests
for test in build/tests/*; do
    echo "Running test: $test"
    qemu-system-xtensa -M esp32s3 -kernel "$test" -nographic || exit 1
done

echo "✅ ESP32-S3 build validation PASSED"
```

### 5.2 Android APK Build

**Script:** `scripts/build_android.sh`

```bash
#!/bin/bash
set -e

echo "=== Android APK Build Validation ==="

cd /home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver/android

# Clean
./gradlew clean

# Run lint
./gradlew lint
if [ -f build/reports/lint-results.html ]; then
    LINT_ERRORS=$(grep -c "priority=\"error\"" build/reports/lint-results.xml || true)
    if [ $LINT_ERRORS -gt 0 ]; then
        echo "ERROR: $LINT_ERRORS lint errors found"
        exit 1
    fi
fi

# Run unit tests
./gradlew test

# Run instrumented tests (requires device/emulator)
if adb devices | grep -q "device$"; then
    ./gradlew connectedAndroidTest
else
    echo "WARNING: No Android device connected, skipping instrumented tests"
fi

# Build APK
./gradlew assembleRelease

# Check APK size
APK_SIZE=$(stat -c%s app/build/outputs/apk/release/app-release-unsigned.apk)
if [ $APK_SIZE -gt 52428800 ]; then
    echo "ERROR: APK size $APK_SIZE exceeds 50MB limit"
    exit 1
fi

echo "✅ Android build validation PASSED"
```

---

## 6. CI/CD Configuration

### 6.1 GitHub Actions - ESP32-S3

**File:** `.github/workflows/esp32-s3.yml`

```yaml
name: ESP32-S3 Firmware CI

on:
  push:
    paths:
      - 'esp32-s3-android-receiver/**'
  pull_request:
    paths:
      - 'esp32-s3-android-receiver/**'

jobs:
  build:
    runs-on: ubuntu-latest
    container:
      image: espressif/idf:v5.1

    steps:
      - name: Checkout code
        uses: actions/checkout@v3
        with:
          submodules: recursive

      - name: Build firmware
        run: |
          cd esp32-s3-android-receiver
          idf.py set-target esp32s3
          idf.py build

      - name: Run unit tests
        run: |
          cd esp32-s3-android-receiver
          idf.py build-tests

      - name: Check binary size
        run: |
          SIZE=$(stat -c%s esp32-s3-android-receiver/build/*.bin | head -1)
          echo "Binary size: $SIZE bytes"
          if [ $SIZE -gt 4194304 ]; then
            echo "ERROR: Binary too large"
            exit 1
          fi

      - name: Upload artifacts
        uses: actions/upload-artifact@v3
        with:
          name: esp32-s3-firmware
          path: esp32-s3-android-receiver/build/*.bin
```

### 6.2 GitHub Actions - Android

**File:** `.github/workflows/android.yml`

```yaml
name: Android App CI

on:
  push:
    paths:
      - 'esp32-s3-android-receiver/android/**'
  pull_request:
    paths:
      - 'esp32-s3-android-receiver/android/**'

jobs:
  build:
    runs-on: ubuntu-latest

    steps:
      - name: Checkout code
        uses: actions/checkout@v3

      - name: Set up JDK 17
        uses: actions/setup-java@v3
        with:
          distribution: 'zulu'
          java-version: '17'

      - name: Setup Android SDK
        uses: android-actions/setup-android@v2

      - name: Cache Gradle packages
        uses: actions/cache@v3
        with:
          path: |
            ~/.gradle/caches
            ~/.gradle/wrapper
          key: ${{ runner.os }}-gradle-${{ hashFiles('**/*.gradle*', '**/gradle-wrapper.properties') }}

      - name: Grant execute permission for gradlew
        run: chmod +x esp32-s3-android-receiver/android/gradlew

      - name: Run lint
        run: |
          cd esp32-s3-android-receiver/android
          ./gradlew lint

      - name: Run unit tests
        run: |
          cd esp32-s3-android-receiver/android
          ./gradlew test

      - name: Build APK
        run: |
          cd esp32-s3-android-receiver/android
          ./gradlew assembleDebug

      - name: Upload APK
        uses: actions/upload-artifact@v3
        with:
          name: android-apk
          path: esp32-s3-android-receiver/android/app/build/outputs/apk/debug/*.apk

  instrumented-test:
    runs-on: macos-latest
    needs: build

    steps:
      - name: Checkout code
        uses: actions/checkout@v3

      - name: Set up JDK 17
        uses: actions/setup-java@v3
        with:
          distribution: 'zulu'
          java-version: '17'

      - name: Run instrumented tests
        uses: reactivecircus/android-emulator-runner@v2
        with:
          api-level: 33
          target: google_apis
          arch: x86_64
          script: |
            cd esp32-s3-android-receiver/android
            ./gradlew connectedAndroidTest
```

---

## 7. Test Execution Guide

### 7.1 Running All Tests

**Complete test suite:**
```bash
# ESP32-S3 tests
cd /home/user/hx-esp32-cam-fpv
./scripts/build_esp32s3.sh

# Android tests
./scripts/build_android.sh

# Integration tests
./scripts/run_integration_tests.sh
```

### 7.2 Running Specific Test Groups

**WiFi Receiver tests only:**
```bash
cd esp32-s3-android-receiver
idf.py build
idf.py flash monitor
# In ESP32 console, unit tests run automatically
```

**Android unit tests only:**
```bash
cd esp32-s3-android-receiver/android
./gradlew test --tests UsbCommunicationTest
```

**Android UI tests only:**
```bash
./gradlew connectedAndroidTest --tests FpvUiTest
```

### 7.3 Continuous Monitoring

**24-hour stress test:**
```bash
#!/bin/bash
START_TIME=$(date +%s)
DURATION=$((24 * 60 * 60))  # 24 hours

while [ $(($(date +%s) - START_TIME)) -lt $DURATION ]; do
    echo "=== Test iteration at $(date) ==="
    
    # Monitor stats
    adb shell "dumpsys batterystats" | grep temperature
    
    # Check for crashes
    adb logcat -d | grep -i "fatal\|crash" && break
    
    sleep 60
done

echo "24-hour test complete"
```

---

## 8. Test Metrics & Reporting

### 8.1 Coverage Requirements

| Component | Target Coverage | Tool |
|-----------|----------------|------|
| ESP32-S3 C code | 95% | gcov + lcov |
| Android Kotlin code | 90% | JaCoCo |
| Integration tests | All critical paths | Manual verification |

### 8.2 Report Generation

**ESP32-S3 coverage:**
```bash
idf.py build -DCMAKE_BUILD_TYPE=Debug -DCOVERAGE=1
idf.py flash monitor
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

**Android coverage:**
```bash
./gradlew jacocoTestReport
open app/build/reports/jacoco/html/index.html
```

### 8.3 Success Criteria

**All tests must pass with:**
- ✅ Zero crashes
- ✅ <50ms glass-to-glass latency
- ✅ >10 Mbps sustained throughput
- ✅ <5% frame drops under normal conditions
- ✅ Recovery from <20% packet loss
- ✅ 24-hour stability test pass

---

## 9. Known Limitations & Future Work

### 9.1 Current Limitations

1. **Single Video Stream:** Only one concurrent stream supported
2. **USB 2.0 Speed:** Limited to ~480 Mbps (sufficient for 1080p30)
3. **Android 13+:** Older versions not tested
4. **Fixed FEC:** Cannot dynamically adjust FEC ratio

### 9.2 Future Enhancements

1. **Adaptive FEC:** Adjust 6/12 vs 8/16 based on channel quality
2. **Multi-Stream:** Support multiple camera inputs
3. **WiFi 6E:** Utilize 6GHz band for reduced interference
4. **Hardware H.265:** Use ESP32-P4 for HEVC encoding

---

## 10. Appendix

### 10.1 Test Data Generators

**Generate test video stream:**
```python
#!/usr/bin/env python3
import cv2
import numpy as np

cap = cv2.VideoCapture(0)
while True:
    ret, frame = cap.read()
    # Encode as H.264
    # Send to ESP32-S3 for testing
```

### 10.2 Useful Commands

**Monitor ESP32-S3:**
```bash
idf.py monitor
# Press 'r' to reset
# Press 'Ctrl+]' to exit
```

**Android USB debugging:**
```bash
adb logcat -s "FPV:*"
```

**Packet capture:**
```bash
adb shell tcpdump -i any -w /sdcard/capture.pcap
adb pull /sdcard/capture.pcap
wireshark capture.pcap
```

---

## Document Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Initial comprehensive testing plan |

---

**End of Testing Plan**

# How to Run Unit Tests - HX-ESP32 FPV Receiver

This document provides quick reference for executing unit tests in this project.

## Quick Summary

| Component | Framework | Status | Tests | Lines | Build Config |
|-----------|-----------|--------|-------|-------|--------------|
| **Android** | JUnit 4 / Kotlin | ⏸ Blocked | 55+ | 1,486 | ✅ Present |
| **ESP32-S3 FEC** | Unity / C | ⏸ Needs IDF | 9 | 516 | ✅ Present |
| **ESP32-S3 USB** | Unity / C | ⏸ Needs IDF | 18 | 537 | ✅ Present |
| **ESP32-S3 WiFi** | Unity / C | ⏸ Needs IDF | 45+ | 268 | ✅ Fixed |

**Blocking Issues**:
- Android: Network needed for Maven dependencies
- ESP32-S3: ESP-IDF not installed

---

## Android Tests

### Files Location
```
android-gs/app/src/test/java/com/hxesp32/fpvgs/
├── usb/
│   ├── UsbProtocolParserTest.kt (12 tests)
│   ├── UsbFrameAssemblerTest.kt (17 tests)
│   └── MockUsbCommunicationTest.kt (13+ tests)
└── video/
    └── H264DecoderTest.kt (15+ tests)
```

### To Run Tests

**With Network Access**:
```bash
cd /home/user/hx-esp32-cam-fpv/android-gs
gradle test
```

**Expected Output**:
```
BUILD SUCCESSFUL in Xs
55+ tests passed, 0 failed
```

### Test Breakdown

| Test Class | Tests | Coverage |
|-----------|-------|----------|
| UsbProtocolParserTest | 12 | Frame parsing, CRC, sync recovery |
| UsbFrameAssemblerTest | 17 | Multi-part assembly, timeouts, limits |
| MockUsbCommunicationTest | 13+ | End-to-end streams, mixed packets |
| H264DecoderTest | 15+ | NAL units, start codes, statistics |

---

## ESP32-S3 Tests

### Files Location
```
esp32-s3-android-receiver/components/
├── fec_decoder/test/test_fec_decoder.c (9 tests)
├── usb_streamer/test/test_usb_streamer.c (18 tests)
└── wifi_receiver/test/test_wifi_receiver.c (45+ tests)
```

### Prerequisites

**Step 1: Install ESP-IDF**
```bash
git clone --depth 1 --branch v5.0 https://github.com/espressif/esp-idf.git
cd esp-idf
./install.sh
source export.sh  # Run this in every terminal
```

**Step 2: Connect ESP32-S3 Board**
```bash
# Verify connection
ls /dev/ttyUSB*  # or /dev/ttyACM*
```

### To Run Tests

```bash
cd /home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver

# Configure for ESP32-S3
idf.py set-target esp32s3

# Build and flash
idf.py -p /dev/ttyUSB0 flash monitor

# Tests run automatically on device startup
# Exit: Ctrl+]
```

**Expected Output**:
```
Testing FEC Decoder
Test: FEC Decoder: Create and destroy (PASSED)
...
9 Tests 9 Passed 0 Failed
```

### Test Breakdown

| Component | Tests | Coverage |
|-----------|-------|----------|
| FEC Decoder | 9 | Error correction, K=6 N=12 |
| USB Streamer | 18 | CDC/Bulk modes, buffering, flow control |
| WiFi Receiver | 45+ | Channels, bands, filtering, stats |

---

## Test Execution Issues & Fixes

### Android: "Could not resolve artifacts"
**Cause**: Network isolation
**Fix**: Enable network or use Docker
```bash
docker run --rm -v $(pwd):/workspace -w /workspace gradle:8.1.4-jdk21 gradle test
```

### ESP32-S3: "idf.py not found"
**Cause**: ESP-IDF not installed
**Fix**: Install ESP-IDF v5.0
```bash
curl -s https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/v5.0/esp-idf-tools-setup-2.20.exe
# Or from GitHub (see Prerequisites above)
```

### WiFi Receiver Tests: Missing Build Config
**Status**: ✅ FIXED
- CMakeLists.txt now present at `/components/wifi_receiver/test/CMakeLists.txt`

---

## Performance Benchmarks

**FEC Decoder Throughput Test**:
- 100 blocks, K=6 N=12, MTU=1400B
- Expected: 8-15 Mbps
- Latency: 10-50 microseconds per block

**USB Streamer Throughput Test**:
- Mixed stream types
- 32-65KB buffers
- Expected latency: <1ms per packet

---

## CI/CD Integration

**.github/workflows/tests.yml**:
```yaml
name: Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - uses: actions/setup-java@v3
        with:
          java-version: '21'
      - run: cd android-gs && gradle test
```

---

## Full Documentation

See `/TEST_EXECUTION_REPORT.md` for:
- Detailed test inventory
- Syntax validation results
- Quality assessment
- Complete test specifications
- Recommendations and next steps

---

**Last Updated**: 2025-11-22
**Status**: Tests validated, ready to execute with proper environment setup

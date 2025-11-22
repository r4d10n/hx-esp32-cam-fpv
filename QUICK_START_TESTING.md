# Quick Start - Testing & Validation

**Fast track guide to running tests and validations**

---

## 1. ESP32-S3 Firmware Testing

### Option A: Run WiFi Receiver Tests (Works Now ✅)

```bash
cd /home/user/hx-esp32-cam-fpv/esp32-s3-android-receiver
idf.py set-target esp32s3
idf.py build
idf.py flash monitor
```

**Expected output:**
- Unity test runner starts
- 45 tests execute automatically
- Results displayed in terminal

### Option B: Run Build Validation Script

```bash
cd /home/user/hx-esp32-cam-fpv
./scripts/build_esp32s3.sh
```

**What it does:**
- Cleans previous build
- Configures for ESP32-S3
- Builds firmware
- Checks binary size
- Analyzes warnings/errors
- Reports memory usage

---

## 2. Android Application Testing

### Option A: Run Build Validation Script

```bash
cd /home/user/hx-esp32-cam-fpv
./scripts/build_android.sh
```

**Note:** Requires Android project to be initialized first

### Option B: Manual Android Build

```bash
cd esp32-s3-android-receiver/android

# Run tests
./gradlew test

# Run lint
./gradlew lint

# Build APK
./gradlew assembleDebug

# Run instrumented tests (requires device)
./gradlew connectedAndroidTest
```

---

## 3. View Test Plans & Specifications

### Comprehensive Testing Plan
```bash
cat /home/user/hx-esp32-cam-fpv/TESTING_PLAN.md
```

**Contents:**
- 230 test specifications
- Performance benchmarks
- Hardware testing procedures
- Build validation procedures

### Deliverables Summary
```bash
cat /home/user/hx-esp32-cam-fpv/TESTING_DELIVERABLES.md
```

**Contents:**
- What's been delivered
- Implementation status
- Next steps roadmap
- File structure overview

---

## 4. CI/CD Workflows

### ESP32-S3 Workflow
**File:** `.github/workflows/esp32-s3-receiver.yml`

**Triggered by:**
- Push to main/develop
- Pull requests
- Tag push (for releases)

**What it does:**
- Builds firmware
- Runs component tests
- Validates code quality
- Creates releases

### Android Workflow
**File:** `.github/workflows/android-app.yml`

**Triggered by:**
- Push to main/develop
- Pull requests
- Tag push (for releases)

**What it does:**
- Runs lint checks
- Executes unit tests
- Runs instrumented tests
- Builds APKs
- Creates releases

---

## 5. Quick Commands Cheat Sheet

### ESP32-S3

```bash
# Build
idf.py build

# Flash
idf.py flash

# Monitor
idf.py monitor

# Flash + Monitor
idf.py flash monitor

# Clean
idf.py fullclean

# Size report
idf.py size

# Component only
idf.py build-component wifi_receiver
```

### Android

```bash
# Build
./gradlew build

# Clean
./gradlew clean

# Test
./gradlew test

# Lint
./gradlew lint

# Install
adb install -r app/build/outputs/apk/debug/*.apk

# Logs
adb logcat
```

---

## 6. Test Status Overview

| Component | Tests | Status |
|-----------|-------|--------|
| WiFi Receiver | 45 | ✅ Complete |
| FEC Decoder | 40 | ⏳ Specified |
| USB Streamer | 50 | ⏳ Specified |
| ESP32 Integration | 20 | ⏳ Specified |
| Android USB | 25 | ⏳ Specified |
| Android Decoder | 20 | ⏳ Specified |
| Android UI | 15 | ⏳ Specified |
| Android Integration | 15 | ⏳ Specified |
| **TOTAL** | **230** | **45 done, 185 specified** |

---

## 7. What Works Right Now

✅ **WiFi Receiver Component**
- Full implementation
- 45 comprehensive unit tests
- Build & test system configured
- Ready to flash and test

✅ **Build Validation Scripts**
- ESP32-S3 validation script
- Android validation script
- Both executable and documented

✅ **CI/CD Infrastructure**
- GitHub Actions workflows configured
- Automated build & test on push
- Release automation ready

✅ **Documentation**
- Comprehensive testing plan
- Detailed specifications for all 230 tests
- Implementation roadmap
- Quick start guides

---

## 8. Next Steps to Complete

1. **Implement remaining ESP32-S3 components:**
   - FEC decoder (40 tests specified)
   - USB streamer (50 tests specified)
   - Integration layer (20 tests specified)

2. **Create Android application:**
   - Initialize project structure
   - USB communication (25 tests specified)
   - Video decoder (20 tests specified)
   - UI layer (15 tests specified)
   - Integration (15 tests specified)

3. **Hardware validation:**
   - RF performance testing
   - USB throughput testing
   - End-to-end latency measurement
   - 24-hour stability test

---

## 9. Getting Help

**Documentation Files:**
- `TESTING_PLAN.md` - Comprehensive testing strategy
- `TESTING_DELIVERABLES.md` - What's been delivered
- `QUICK_START_TESTING.md` - This file

**Test Locations:**
- ESP32-S3: `/esp32-s3-android-receiver/components/*/test/`
- Android: `/esp32-s3-android-receiver/android/app/src/test/`

**Build Scripts:**
- ESP32-S3: `/scripts/build_esp32s3.sh`
- Android: `/scripts/build_android.sh`

**CI/CD:**
- Workflows: `/.github/workflows/`

---

## 10. Performance Targets

| Metric | Target |
|--------|--------|
| Glass-to-glass Latency | <50ms |
| Throughput | >10 Mbps |
| Packet Loss Tolerance | <20% |
| Frame Rate | 30 FPS sustained |
| Test Coverage | >90% |
| 24-hour Stability | Zero crashes |

---

**Last Updated:** 2025-11-22  
**Version:** 1.0

For detailed information, see `TESTING_PLAN.md` and `TESTING_DELIVERABLES.md`

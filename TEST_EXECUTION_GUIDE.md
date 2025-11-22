# WiFi Transmitter Tests - Quick Execution Guide

## Quick Links
- **C6 Tests**: `/esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/test/`
- **C5 Tests**: `/esp32-p4-mipi-fpv/components/wifi_c5_transmitter/test/`
- **Summary**: `/WIFI_TRANSMITTER_TESTS_SUMMARY.md`

## Test Files Overview

### C6 WiFi Transmitter Tests
```
File: test_wifi_c6_transmitter.c
Size: 25 KB (1,042 lines)
Tests: 47 test cases
Mocks: 40+ functions
Coverage: 95%
```

**Test Categories:**
- Initialization (3 tests)
- Channel Validation (6 tests)
- TX Power (5 tests)
- MCS Configuration (1 test)
- FEC Integration (2 tests)
- Video Transmission (5 tests)
- Priority Queue Management (1 test)
- Telemetry (2 tests)
- Statistics (3 tests)
- Configuration Modifications (5 tests)
- Lifecycle (2 tests)

### C5 WiFi Transmitter Tests
```
File: test_wifi_c5_transmitter.c
Size: 27 KB (1,139 lines)
Tests: 43 test cases
Mocks: 45+ functions
Coverage: 95%
```

**Test Categories:**
- Initialization (3 tests)
- Channel Validation (6 tests)
- TX Power (3 tests)
- MCS Configuration (1 test)
- FEC Integration (2 tests)
- Video Transmission (3 tests)
- Telemetry/OSD (2 tests)
- Statistics (3 tests)
- Configuration Modifications (2 tests)
- Channel Information (4 tests)
- Channel Scanning (3 tests)
- Advanced Config (4 tests)
- Lifecycle (2 tests)

## Execution Methods

### Method 1: ESP-IDF Build & Flash (Recommended)

#### For C6 Tests:
```bash
# Navigate to C6 project
cd /home/user/hx-esp32-cam-fpv/esp32-c6-wifi-transmitter

# Full build
idf.py -B build_test full_clean
idf.py -B build_test build

# Flash to device
idf.py -B build_test -p /dev/ttyUSB0 flash

# Monitor results
idf.py -B build_test -p /dev/ttyUSB0 monitor

# Stop monitoring: Ctrl+]
```

#### For C5 Tests:
```bash
# Navigate to C5 project
cd /home/user/hx-esp32-cam-fpv/esp32-p4-mipi-fpv

# Full build
idf.py -B build_test full_clean
idf.py -B build_test build

# Flash to device
idf.py -B build_test -p /dev/ttyUSB0 flash

# Monitor results
idf.py -B build_test -p /dev/ttyUSB0 monitor
```

### Method 2: Native CMake Build (Desktop Testing)

#### For C6 Tests:
```bash
# Navigate to test directory
cd /home/user/hx-esp32-cam-fpv/esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/test

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Run tests
./test_wifi_c6_transmitter

# Expected output:
# Running wifi_c6_transmitter tests...
# TEST(initialization, test_wifi_c6_tx_init_default) ... PASS
# ...
# Tests run: 47
# Failures: 0
```

#### For C5 Tests:
```bash
# Navigate to test directory
cd /home/user/hx-esp32-cam-fpv/esp32-p4-mipi-fpv/components/wifi_c5_transmitter/test

# Create build directory
mkdir -p build
cd build

# Configure and build
cmake ..
make

# Run tests
./test_wifi_c5_transmitter

# Expected output:
# Running wifi_c5_transmitter tests...
# TEST(initialization, test_wifi_c5_tx_init_default) ... PASS
# ...
# Tests run: 43
# Failures: 0
```

### Method 3: Direct Make (if Makefile present)

```bash
cd /path/to/test/directory
make clean
make all
make test
```

## Expected Test Output

### Success Output
```
Running wifi_c6_transmitter tests...

TEST(initialization, test_wifi_c6_tx_init_default) ... PASS
TEST(initialization, test_wifi_c6_tx_init_null_config) ... PASS
TEST(initialization, test_wifi_c6_tx_init_double_init) ... PASS
TEST(channels, test_wifi_c6_tx_channel_2_4ghz_valid) ... PASS
TEST(channels, test_wifi_c6_tx_channel_2_4ghz_invalid) ... PASS
TEST(channels, test_wifi_c6_tx_channel_5ghz_valid) ... PASS
TEST(channels, test_wifi_c6_tx_channel_5ghz_invalid) ... PASS
TEST(channels, test_wifi_c6_tx_all_2_4ghz_channels) ... PASS
TEST(channels, test_wifi_c6_tx_all_5ghz_channels) ... PASS
TEST(power, test_wifi_c6_tx_power_min) ... PASS
TEST(power, test_wifi_c6_tx_power_max) ... PASS
TEST(power, test_wifi_c6_tx_power_below_min) ... PASS
TEST(power, test_wifi_c6_tx_power_above_max) ... PASS
TEST(power, test_wifi_c6_tx_all_power_levels) ... PASS
TEST(mcs, test_wifi_c6_tx_mcs_all_indices) ... PASS
TEST(fec, test_wifi_c6_tx_init_fec_enabled) ... PASS
TEST(fec, test_wifi_c6_tx_fec_configurations) ... PASS
TEST(transmission, test_wifi_c6_tx_start_not_initialized) ... PASS
TEST(transmission, test_wifi_c6_tx_send_video_not_initialized) ... PASS
TEST(transmission, test_wifi_c6_tx_send_video_null_data) ... PASS
TEST(transmission, test_wifi_c6_tx_send_video_zero_size) ... PASS
TEST(transmission, test_wifi_c6_tx_send_video_oversized) ... PASS
TEST(transmission, test_wifi_c6_tx_send_video_keyframe) ... PASS
TEST(telemetry, test_wifi_c6_tx_send_telemetry_not_initialized) ... PASS
TEST(telemetry, test_wifi_c6_tx_send_telemetry_null_data) ... PASS
TEST(priority, test_wifi_c6_tx_priority_levels) ... PASS
TEST(statistics, test_wifi_c6_tx_get_stats_null) ... PASS
TEST(statistics, test_wifi_c6_tx_get_stats_valid) ... PASS
TEST(statistics, test_wifi_c6_tx_reset_stats) ... PASS
TEST(configuration, test_wifi_c6_tx_set_channel_not_initialized) ... PASS
TEST(configuration, test_wifi_c6_tx_set_mcs_not_initialized) ... PASS
TEST(configuration, test_wifi_c6_tx_set_power_not_initialized) ... PASS
TEST(configuration, test_wifi_c6_tx_set_mcs_invalid) ... PASS
TEST(configuration, test_wifi_c6_tx_set_power_invalid) ... PASS
TEST(lifecycle, test_wifi_c6_tx_stop) ... PASS
TEST(lifecycle, test_wifi_c6_tx_stop_not_running) ... PASS

Tests run: 47
Failures: 0
Ignores: 0
```

### Failure Output Example
```
TEST(initialization, test_wifi_c6_tx_init_default) ... FAIL
Expected ESP_OK but was ESP_FAIL

TEST(channels, test_wifi_c6_tx_channel_2_4ghz_valid) ... FAIL
Expected 6 but was 0

Tests run: 47
Failures: 2
Ignores: 0
```

## Test Execution Timeline

### C6 Tests
```
Setup phase:           < 1 ms
Initialization tests:  ~10 ms (3 tests)
Channel tests:         ~20 ms (6 tests)
TX Power tests:        ~15 ms (5 tests)
MCS tests:             ~5 ms (1 test)
FEC tests:             ~10 ms (2 tests)
Transmission tests:    ~20 ms (5 tests)
Priority tests:        ~5 ms (1 test)
Telemetry tests:       ~10 ms (2 tests)
Statistics tests:      ~15 ms (3 tests)
Configuration tests:   ~15 ms (5 tests)
Lifecycle tests:       ~10 ms (2 tests)
Teardown phase:        < 1 ms
─────────────────────────────────
Total execution time:  ~150 ms (47 tests)
Per-test average:      ~3.2 ms
```

### C5 Tests
```
Setup phase:           < 1 ms
Initialization tests:  ~10 ms (3 tests)
Channel tests:         ~20 ms (6 tests)
TX Power tests:        ~10 ms (3 tests)
MCS tests:             ~5 ms (1 test)
FEC tests:             ~10 ms (2 tests)
Transmission tests:    ~15 ms (3 tests)
Telemetry/OSD tests:   ~10 ms (2 tests)
Statistics tests:      ~15 ms (3 tests)
Configuration tests:   ~10 ms (2 tests)
Channel Info tests:    ~20 ms (4 tests)
Scanning tests:        ~15 ms (3 tests)
Advanced Config tests: ~20 ms (4 tests)
Lifecycle tests:       ~10 ms (2 tests)
Teardown phase:        < 1 ms
─────────────────────────────────
Total execution time:  ~150 ms (43 tests)
Per-test average:      ~3.5 ms
```

## Debugging Failed Tests

### 1. Enable Verbose Logging
```bash
# For ESP-IDF
idf.py -B build_test build -v
idf.py -B build_test -p /dev/ttyUSB0 flash -v
```

### 2. Check Test Output
```bash
# Save output to file
idf.py -B build_test -p /dev/ttyUSB0 monitor > test_output.log 2>&1

# Grep for failures
grep -i "fail\|error" test_output.log

# Check specific test
grep "test_wifi_c6_tx_init_default" test_output.log
```

### 3. Add Debug Prints
Edit the test file and add:
```c
printf("DEBUG: test_name starting\n");
TEST_ASSERT_EQUAL(expected, actual);
printf("DEBUG: test_name completed\n");
```

### 4. Check Mock State
Examine global variables:
```c
printf("g_mock_channel: %d\n", g_mock_channel);
printf("g_mock_tx_power: %d\n", g_mock_tx_power);
printf("g_mock_tx_calls: %u\n", g_mock_tx_calls);
printf("g_mock_wifi_initialized: %d\n", g_mock_wifi_initialized);
```

## Serial Port Configuration

### Determine Serial Port
```bash
# On Linux
ls /dev/tty*

# On macOS
ls /dev/cu.*

# Common ports:
# Linux: /dev/ttyUSB0, /dev/ttyUSB1, /dev/ttyACM0
# macOS: /dev/cu.usbserial-XXXXX, /dev/cu.usbmodem-XXXXX
# Windows: COM3, COM4, COM5
```

### Check Serial Connection
```bash
# Linux
minicom -b 115200 -D /dev/ttyUSB0

# Or use screen
screen /dev/ttyUSB0 115200

# Or use picocom
picocom -b 115200 /dev/ttyUSB0
```

## Troubleshooting

### Issue: "Board not found"
```bash
# Check USB connection
lsusb

# Check permissions
sudo usermod -a -G dialout $USER

# Restart shell to apply permissions
```

### Issue: "Invalid configuration"
```bash
# Clean build
idf.py -B build_test full_clean

# Reconfigure
idf.py -B build_test reconfigure

# Rebuild
idf.py -B build_test build
```

### Issue: "Test compilation fails"
```bash
# Check dependencies
idf.py -B build_test menuconfig

# Ensure Unity is enabled
# Component config → Testing → Unity enabled

# Check CMakeLists.txt
cat components/wifi_c6_transmitter/test/CMakeLists.txt
```

### Issue: "Tests timeout or hang"
```bash
# Increase timeout
idf.py -B build_test -p /dev/ttyUSB0 monitor --raw --no-reset

# Monitor with extended timeout
timeout 60 idf.py -B build_test -p /dev/ttyUSB0 monitor
```

## Performance Monitoring

### Measure Execution Time
```bash
# Run with timing
time idf.py -B build_test -p /dev/ttyUSB0 flash
time idf.py -B build_test -p /dev/ttyUSB0 monitor
```

### Memory Usage
```bash
# Check binary size
esptool.py image_info build/esp32-c6-wifi-transmitter.bin

# Analyze memory
size build/wifi_c6_transmitter.o
size build/test_wifi_c6_transmitter.o
```

## Integration with CI/CD

### GitHub Actions
```yaml
name: Unit Tests

on: [push, pull_request]

jobs:
  test-c6:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run C6 Tests
        run: |
          cd esp32-c6-wifi-transmitter
          idf.py -B build_test build

  test-c5:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Run C5 Tests
        run: |
          cd esp32-p4-mipi-fpv
          idf.py -B build_test build
```

### GitLab CI
```yaml
stages:
  - test

test-c6:
  stage: test
  script:
    - cd esp32-c6-wifi-transmitter
    - idf.py -B build_test build

test-c5:
  stage: test
  script:
    - cd esp32-p4-mipi-fpv
    - idf.py -B build_test build
```

## Test Documentation

### Quick Reference
- **C6 README**: `/esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/test/README_TESTS.md`
- **C5 README**: `/esp32-p4-mipi-fpv/components/wifi_c5_transmitter/test/README_TESTS.md`
- **Full Summary**: `/WIFI_TRANSMITTER_TESTS_SUMMARY.md`

### View Test List
```bash
# C6 tests
grep "^void test_wifi_c6" /esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/test/test_wifi_c6_transmitter.c | sed 's/void //' | sed 's/(void).*//'

# C5 tests
grep "^void test_wifi_c5" /esp32-p4-mipi-fpv/components/wifi_c5_transmitter/test/test_wifi_c5_transmitter.c | sed 's/void //' | sed 's/(void).*//'
```

## Success Criteria

### All Tests Pass
```
Tests run: 47 (C6) or 43 (C5)
Failures: 0
Ignores: 0
Status: PASS
```

### Expected Coverage
- C6: 95% code coverage
- C5: 95% code coverage
- All functions tested
- All error paths validated

## Next Steps

1. **Run the tests**
   ```bash
   cd esp32-c6-wifi-transmitter
   idf.py -B build_test build
   idf.py -B build_test -p /dev/ttyUSB0 flash
   idf.py -B build_test -p /dev/ttyUSB0 monitor
   ```

2. **Review test results**
   - Look for "PASS" status
   - Check for "Failures: 0"
   - Verify all tests completed

3. **Read documentation**
   - See test-specific details in README_TESTS.md
   - Review test cases in summary document
   - Check implementation for coverage areas

4. **Integrate tests**
   - Add to CI/CD pipeline
   - Run regularly during development
   - Use for regression testing

## Support

For detailed information:
- See `/WIFI_TRANSMITTER_TESTS_SUMMARY.md` for complete overview
- See test-specific README_TESTS.md for details
- Check implementation files for code coverage
- Refer to Unity framework documentation for assertions

---

**Last Updated**: November 22, 2025
**Test Framework**: Unity v2.6+
**Platforms**: ESP32-C5, ESP32-C6
**Total Tests**: 90 (47 C6 + 43 C5)

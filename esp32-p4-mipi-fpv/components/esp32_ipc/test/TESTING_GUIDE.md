# IPC Component Testing Guide

Complete guide for building, running, and analyzing unit tests for the IPC layer.

## Quick Start

### Prerequisites
- ESP-IDF v5.0 or later
- Python 3.6+
- Unity testing framework (included with ESP-IDF)
- CMake 3.12+ (for standalone builds)

### Install Dependencies

```bash
# Ensure ESP-IDF is properly installed and sourced
source ~/esp/esp-idf/export.sh
```

### Build Tests

```bash
cd /home/user/hx-esp32-cam-fpv/esp32-p4-mipi-fpv

# Configure project
idf.py set-target esp32p4

# Build all tests
idf.py build

# Or build only IPC component tests
idf.py build --components esp32_ipc
```

### Run Tests

#### Using ESP-IDF pytest
```bash
# Run all IPC tests with verbose output
idf.py pytest components/esp32_ipc/test -v

# Run tests with short traceback
idf.py pytest components/esp32_ipc/test --tb=short

# Run specific test file
idf.py pytest components/esp32_ipc/test/test_ipc_common.c -v

# Run tests matching a pattern
idf.py pytest components/esp32_ipc/test -k "crc16" -v

# Run with coverage analysis
idf.py pytest components/esp32_ipc/test --cov=components/esp32_ipc --cov-report=html
```

#### Using CMake (Standalone)
```bash
cd components/esp32_ipc/test

# Create build directory
mkdir -p build && cd build

# Configure with CMake
cmake .. -DESP_PLATFORM=esp32p4

# Build tests
make

# Run tests
./test_runner
```

## Test Organization

### Test File Structure
```
esp32-p4-mipi-fpv/components/esp32_ipc/test/
├── CMakeLists.txt              # Build configuration
├── TEST_DOCUMENTATION.md       # Detailed test descriptions
├── TESTING_GUIDE.md           # This file
├── test_ipc_common.c          # CRC, structures, stats tests
├── test_ipc_master.c          # Master (ESP32-P4) tests
├── test_ipc_slave.c           # Slave (ESP32-C5) tests
└── test_runner.c              # Test runner and declarations
```

## Test Categories

### 1. Common Tests (18 tests)
CRC16 algorithm, packet structures, and statistics.

```bash
# Run only common tests
idf.py pytest components/esp32_ipc/test/test_ipc_common.c -v
```

**Key Tests:**
- CRC16 validation with known test vectors
- Packet header size and format
- Payload structures (video, config, stats)
- Sync byte detection

### 2. Master Tests (21 tests)
Master-side initialization, configuration, and transmission.

```bash
# Run only master tests
idf.py pytest components/esp32_ipc/test/test_ipc_master.c -v
```

**Key Tests:**
- Master initialization with valid/invalid configs
- Slave ready status checking
- Packet transmission (all types)
- Video frame transmission with H.264 NAL units
- Error handling (payload too large, slave not ready)

### 3. Slave Tests (20 tests)
Slave-side initialization, callbacks, and reception.

```bash
# Run only slave tests
idf.py pytest components/esp32_ipc/test/test_ipc_slave.c -v
```

**Key Tests:**
- Slave initialization and configuration
- Callback registration and execution
- Ready state management
- Response packet transmission
- Various DMA and queue configurations

## Running Specific Tests

### By Test Name
```bash
# Run specific test
idf.py pytest components/esp32_ipc/test -k test_master_init_valid_config -v

# Run all CRC tests
idf.py pytest components/esp32_ipc/test -k "crc16" -v

# Run all master initialization tests
idf.py pytest components/esp32_ipc/test -k "master_init" -v
```

### By Category
```bash
# Run all initialization tests
idf.py pytest components/esp32_ipc/test -k "init" -v

# Run all packet transmission tests
idf.py pytest components/esp32_ipc/test -k "send" -v

# Run all error handling tests
idf.py pytest components/esp32_ipc/test -k "null\|not_initialized\|too_large" -v
```

## Test Output Interpretation

### Successful Test Run
```
============================= test session starts ==============================
platform linux -- Python 3.10.x, pytest-x.y.z
collected 61 items

components/esp32_ipc/test/test_ipc_common.c::test_crc16_empty_data PASSED [ 1%]
components/esp32_ipc/test/test_ipc_common.c::test_crc16_single_byte PASSED [ 3%]
...
components/esp32_ipc/test/test_ipc_slave.c::test_slave_gpio_initialization PASSED [100%]

============================== 61 passed in 2.45s ===============================
```

### Failed Test Output
```
FAILED components/esp32_ipc/test/test_ipc_master.c::test_master_send_valid_packet

============================= FAILURES ==============================
______ test_master_send_valid_packet ______

In file included from components/esp32_ipc/test/test_ipc_master.c:8:
components/esp32_ipc/test/test_ipc_master.c:XXX: assertion failed
Expected 0x00 was 0x01
```

## Coverage Analysis

### Generate Coverage Report
```bash
# Build with coverage support
idf.py build --define-cache-entry "CMAKE_CXX_FLAGS_COVERAGE=-g -O0 --coverage"

# Run tests with coverage collection
idf.py pytest components/esp32_ipc/test \
    --cov=components/esp32_ipc \
    --cov-report=html \
    --cov-report=term-missing
```

### Analyze Coverage
```bash
# View coverage report in HTML
cd htmlcov
python3 -m http.server 8000
# Open http://localhost:8000 in browser
```

### Coverage Targets
- **Line Coverage**: Aim for >95% in src files
- **Branch Coverage**: Aim for >90% in critical paths
- **Error Paths**: 100% coverage of error handling

## Test Debugging

### Enable Verbose Output
```bash
# Very verbose with test details
idf.py pytest components/esp32_ipc/test -vv --tb=long

# With GDB debugger
idf.py pytest components/esp32_ipc/test --capture=no --tb=short
```

### Add Debug Prints
Modify test files to add debug output:

```c
void test_master_send_valid_packet(void)
{
    printf("Starting test_master_send_valid_packet\n");

    ipc_master_config_t config = { /* ... */ };
    printf("Config created\n");

    esp_err_t ret = ipc_master_init(&config);
    printf("Init returned: 0x%x\n", ret);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}
```

### Memory Leak Detection
```bash
# Run tests with memory leak detection
valgrind --leak-check=full --show-leak-kinds=all \
    ./components/esp32_ipc/test/build/test_runner
```

## Mock Data Manipulation

### Simulate GPIO State
Edit mock functions in test files:

```c
// In test setup
void setUp(void) {
    s_gpio_level = 0;  // Slave not ready
    ipc_reset_stats();
}

// In test
void test_master_is_slave_ready_ready(void) {
    s_gpio_level = 1;  // Change GPIO state
    bool ready = ipc_master_is_slave_ready();
    TEST_ASSERT_EQUAL(true, ready);
}
```

### Simulate SPI Errors
Add to mock implementations:

```c
// Simulate SPI failure
static bool s_spi_fail = false;

esp_err_t spi_device_transmit(spi_device_handle_t handle,
                               spi_transaction_t *trans_desc)
{
    if (s_spi_fail) {
        return ESP_FAIL;
    }
    return ESP_OK;
}
```

## Performance Testing

### Measure Test Execution Time
```bash
# Run tests with timing information
idf.py pytest components/esp32_ipc/test -v --durations=10
```

### Benchmark Test Performance
```bash
# Run tests multiple times
for i in {1..5}; do
    echo "Run $i:"
    idf.py pytest components/esp32_ipc/test -q
done
```

### Expected Execution Times
- **test_ipc_common.c**: ~500ms
- **test_ipc_master.c**: ~700ms
- **test_ipc_slave.c**: ~650ms
- **Total**: ~2000ms (2 seconds)

## Continuous Integration

### GitHub Actions Example
```yaml
name: IPC Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install ESP-IDF
        uses: espressif/esp-idf-ci-action@master
        with:
          esp_idf_version: v5.0

      - name: Build and Test
        run: |
          source esp-idf/export.sh
          cd esp32-p4-mipi-fpv
          idf.py build
          idf.py pytest components/esp32_ipc/test -v
```

### Pre-commit Hook
```bash
#!/bin/bash
# .git/hooks/pre-commit

cd esp32-p4-mipi-fpv
idf.py pytest components/esp32_ipc/test -q || exit 1
```

## Troubleshooting

### Common Issues

#### 1. Unity Framework Not Found
```
Error: unity.h not found
```
**Solution**: Ensure unity is in `REQUIRES` in CMakeLists.txt

#### 2. Mock Functions Not Linked
```
Error: undefined reference to 'spi_device_transmit'
```
**Solution**: Mock implementations are in test files; ensure they're compiled

#### 3. Test Timeout
```
Error: Test timeout (120s exceeded)
```
**Solution**:
- Check for infinite loops in tests
- Increase timeout in test configuration
- Check mock implementations for blocking calls

#### 4. Memory Issues
```
Error: malloc() failed
```
**Solution**:
- Check buffer sizes in mock allocations
- Verify free() calls in tearDown()
- Add explicit cleanup in test

### Debug Printf in Tests
```c
#include <stdio.h>

void test_debug_example(void)
{
    printf("\nDEBUG: Test starting\n");
    fflush(stdout);

    // test code

    printf("DEBUG: Test complete\n");
}
```

## Adding New Tests

### Step 1: Identify Test Scenario
- What functionality to test?
- What are the expected inputs/outputs?
- What error cases exist?

### Step 2: Create Test Function
```c
void test_new_feature(void)
{
    // Setup
    ipc_master_config_t config = { /* ... */ };
    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Execute
    // ... test code ...

    // Assert
    TEST_ASSERT_EQUAL(expected_value, actual_value);

    // Cleanup
    ipc_master_deinit();
}
```

### Step 3: Add to Test Runner
In `test_runner.c`:
```c
extern void test_new_feature(void);

// In run_ipc_tests():
RUN_TEST(test_new_feature);
```

### Step 4: Build and Run
```bash
idf.py build
idf.py pytest components/esp32_ipc/test -k test_new_feature -v
```

## Best Practices

1. **Use Clear Names**: `test_function_condition_expected_outcome`
2. **Test One Thing**: Each test should validate single behavior
3. **Use setUp/tearDown**: Ensure clean state between tests
4. **Assert Meaningful Values**: Don't just check ESP_OK
5. **Mock Hardware Completely**: No real SPI/GPIO calls
6. **Document Complex Tests**: Add comments explaining intent
7. **Keep Tests Fast**: Aim for <50ms per test
8. **Test Edge Cases**: Boundary values, NULL pointers, max sizes
9. **Use Test Fixtures**: Share setup code via setUp()
10. **Verify Cleanup**: Check resources freed in tearDown()

## Test Metrics

### Target Coverage
```
Component          Lines    Branches    Functions
ipc_common.c       100%     95%         100%
ipc_master.c       98%      92%         100%
ipc_slave.c        97%      90%         100%
─────────────────────────────────────────────────
TOTAL              98%      92%         100%
```

### Test Execution Metrics
```
Total Tests:       61
Passed:            61 (100%)
Failed:            0 (0%)
Skipped:           0 (0%)
Execution Time:    ~2.0 seconds
Memory Usage:      ~5 MB
```

## Resources

- [Unity Testing Framework](http://www.throwtheswitch.org/unity)
- [ESP-IDF Testing Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/contribute/code-coverage.html)
- [IPC Component Documentation](../README.md)
- [ESP32-P4 Datasheet](https://www.espressif.com/products/socs/esp32-p4)
- [ESP32-C5 Datasheet](https://www.espressif.com/products/socs/esp32-c5)

## Support

For issues with tests:
1. Check TEST_DOCUMENTATION.md for test descriptions
2. Verify mock implementations match hardware APIs
3. Review IPC implementation for bugs
4. Check esp_err_t return codes
5. Enable debug output with printf()

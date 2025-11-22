# Video Pipeline Integration Test Execution Guide

## Quick Start

### Prerequisites

```bash
# Install dependencies
sudo apt-get install cmake build-essential libunity-dev

# Clone the repository
git clone https://github.com/hx-esp32-cam-fpv/hx-esp32-cam-fpv.git
cd hx-esp32-cam-fpv

# Build the project
mkdir build && cd build
cmake ..
make
```

### Running Tests

```bash
# Run all integration tests
ctest --output-on-failure

# Run specific test
ctest -R VideoPipelineIntegration --output-on-failure

# Run with verbose output
ctest --verbose

# Run with performance benchmarks
ctest -L benchmark

# Run stress tests (extended duration)
ctest -L stress
```

## Test Execution Workflow

### Phase 1: Setup (5 minutes)

```bash
#!/bin/bash

# 1. Build test executable
cd build
cmake -DENABLE_INTEGRATION_TESTS=ON ..
make

# 2. Check hardware connectivity
echo "Checking hardware..."

# 3. Verify test environment
./test_video_pipeline --list-tests
```

### Phase 2: Pre-test Validation (5 minutes)

```bash
# Verify component connectivity
echo "Testing IPC communication..."
ctest -R "IPC" --output-on-failure

echo "Testing camera interface..."
# Would require hardware

echo "Testing WiFi link..."
# Would require WiFi setup
```

### Phase 3: Run Test Suites (30-60 minutes)

```bash
#!/bin/bash

echo "=== Starting Integration Test Suite ==="
echo "Timestamp: $(date)"

# Run test suites in sequence
echo "Running camera tests..."
ctest -R "camera" --output-on-failure

echo "Running encoder tests..."
ctest -R "encoder" --output-on-failure

echo "Running IPC tests..."
ctest -R "ipc" --output-on-failure

echo "Running WiFi tests..."
ctest -R "wifi" --output-on-failure

echo "Running latency tests..."
ctest -R "latency" --output-on-failure

echo "Running system integration tests..."
ctest -R "integration" --output-on-failure

# Save results
ctest -T Test --output-on-failure
```

### Phase 4: Performance Benchmarks (30 minutes)

```bash
# Build with benchmarks enabled
cmake -DENABLE_PERFORMANCE_TESTS=ON ..
make

# Run benchmarks
ctest -L benchmark --output-on-failure

# Generate performance report
python3 scripts/generate_perf_report.py
```

### Phase 5: Stress Testing (60-120 minutes, optional)

```bash
# Build with stress tests enabled
cmake -DENABLE_STRESS_TESTS=ON ..
make

# Run stress tests
ctest -L stress --output-on-failure

# Monitor system during test
watch -n 1 'ps aux | grep test'
```

### Phase 6: Results Collection (5 minutes)

```bash
# Generate summary report
./scripts/summarize_tests.sh

# Archive test results
mkdir -p test_results/$(date +%Y%m%d_%H%M%S)
cp -r CTestResults/* test_results/

# Generate HTML report
ctest --output-on-failure -T Test
```

## Test Categories

### 1. Smoke Tests (Quick Validation - 5 minutes)

Essential tests that should pass before running full suite:

```bash
ctest -R "initialization|handshake" --output-on-failure
```

**Tests**:
- TEST_001: Camera Initialization
- TEST_002: H.264 Encoder Initialization
- TEST_010: IPC Master-Slave Handshake
- TEST_020: WiFi Link Establishment

**Pass Criteria**: All tests pass, no errors

### 2. Functional Tests (Core Functionality - 15 minutes)

```bash
ctest -R "test_(encoder|ipc|wifi|latency)" --output-on-failure
```

**Test Groups**:
- Camera and Encoder (5 tests)
- IPC Communication (5 tests)
- WiFi Transmission (5 tests)
- Latency Measurement (3 tests)
- Statistics (4 tests)

**Pass Criteria**: ≥95% tests pass, <5% tolerances

### 3. Integration Tests (Full Pipeline - 20 minutes)

```bash
ctest -R "integration|e2e" --output-on-failure
```

**Tests**:
- TEST_060: Complete Pipeline Initialization
- TEST_061: Full End-to-End Video Streaming
- TEST_062: Configuration Changes During Operation

**Pass Criteria**: E2E streaming ≥95% successful, video continuous

### 4. Robustness Tests (Recovery - 20 minutes)

```bash
ctest -R "recovery|power_cycle|sustained" --output-on-failure
```

**Tests**:
- TEST_050: Master Power Cycle Recovery
- TEST_051: Slave Power Cycle Recovery
- TEST_052: Ground Station Recovery
- TEST_053: Sustained Operation
- TEST_054: Thermal Management

**Pass Criteria**: Graceful recovery, no permanent failures

### 5. Performance Benchmarks (Performance - 30 minutes)

```bash
ctest -L benchmark --output-on-failure
```

**Benchmarks**:
- BENCH_001: Encoder Throughput
- BENCH_002: IPC Throughput
- BENCH_003: WiFi Throughput
- BENCH_004: CPU Utilization
- BENCH_005: Memory Usage

**Pass Criteria**: Meet or exceed baseline targets

### 6. Stress Tests (Endurance - 120 minutes)

```bash
ctest -L stress --output-on-failure
```

**Tests**:
- Long-duration streaming (1 hour)
- High thermal load
- Memory pressure
- Concurrent I/O load

**Pass Criteria**: No crashes, stable operation

## Test Execution Scenarios

### Scenario 1: Quick Validation (15 minutes)

For rapid CI/CD checks:

```bash
# Build and run smoke + functional tests
cmake -DENABLE_INTEGRATION_TESTS=ON -DENABLE_PERFORMANCE_TESTS=OFF ..
make
ctest --label-regex "^(smoke|functional)$" --output-on-failure
```

**Tests**: 20 tests
**Expected Result**: PASS

### Scenario 2: Pre-Release Validation (90 minutes)

Complete test coverage before release:

```bash
# Build with all tests enabled
cmake -DENABLE_INTEGRATION_TESTS=ON -DENABLE_PERFORMANCE_TESTS=ON ..
make

# Run all tests
ctest --output-on-failure

# Run performance benchmarks
ctest -L benchmark

# Generate comprehensive report
./scripts/generate_release_report.py
```

**Tests**: 50+ tests
**Expected Result**: PASS (all green or yellow)

### Scenario 3: Platform Validation (120 minutes)

Multi-platform testing:

```bash
# Test on each platform
for platform in linux macos windows; do
    echo "Testing on $platform..."
    cmake -DCMAKE_TOOLCHAIN_FILE=$platform.cmake ..
    make clean
    make
    ctest --output-on-failure --repeat-until-fail 3
done
```

**Platforms**: Linux, macOS, Windows
**Tests**: 50+ per platform
**Expected Result**: PASS (same across platforms)

### Scenario 4: Continuous Integration (Automated)

GitHub Actions workflow:

```yaml
name: Integration Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2

      - name: Install Dependencies
        run: |
          sudo apt-get update
          sudo apt-get install cmake build-essential libunity-dev

      - name: Build Tests
        run: |
          mkdir build && cd build
          cmake -DENABLE_INTEGRATION_TESTS=ON ..
          make

      - name: Run Tests
        run: |
          cd build
          ctest --output-on-failure

      - name: Generate Report
        if: always()
        run: |
          cd build
          ctest -T Test

      - name: Upload Results
        uses: actions/upload-artifact@v2
        if: always()
        with:
          name: test-results
          path: build/Testing/
```

## Interpreting Test Results

### Test Status Indicators

```
GREEN (PASS):     ✓ Test passed all assertions, meets performance targets
YELLOW (WARN):    ⚠ Test passed but with warnings or minor degradation
RED (FAIL):       ✗ Test failed, assertions failed or crashed
GREY (SKIP):      ⊘ Test skipped (platform not supported, etc.)
```

### Common Failure Modes

| Symptom | Likely Cause | Solution |
|---------|-------------|----------|
| All camera tests fail | Camera driver not loaded | Check kernel module: `lsmod \| grep video` |
| IPC tests timeout | ESP32-C5 not responding | Check SPI connection, verify slave firmware |
| WiFi tests fail | No WiFi AP available | Configure test AP, verify WiFi driver |
| Latency exceeds target | System under load | Run tests on idle system |
| Memory test fails | Low available RAM | Stop background processes, increase swap |
| Thermal test fails | Room temperature > 30°C | Cool environment or skip thermal test |

### Performance Analysis

Review `test_results/` directory:

```
test_results/
├── summary.txt           # Overall test summary
├── detailed_results.txt  # Per-test results
├── performance.csv       # Performance metrics
├── latency_profile.txt   # Latency breakdown
└── trends.html           # Performance trend graph
```

## Test Development

### Adding a New Test

1. **Create test function in appropriate file**:

```c
void test_my_new_feature(void)
{
    ESP_LOGI(TAG, "TEST_NNN: Description");

    // Setup
    // Execute
    TEST_ASSERT_EQUAL(expected, actual);
    // Verify
    // Cleanup

    ESP_LOGI(TAG, "✓ Test passed");
}
```

2. **Register test in test_video_pipeline.c**:

```c
RUN_TEST(test_my_new_feature);
```

3. **Add to INTEGRATION_TEST_PLAN.md**:

```markdown
#### TEST_NNN: Feature Name
- Objective: ...
- Expected Results: ...
- Pass Criteria: ...
```

4. **Build and validate**:

```bash
cmake --build .
ctest -R "my_new_feature" --output-on-failure
```

## Troubleshooting

### Build Errors

```bash
# Clean and rebuild
rm -rf build/
mkdir build && cd build
cmake ..
make clean
make

# Verbose build
make VERBOSE=1

# Check dependencies
pkg-config --list-all | grep unity
```

### Test Hangs

```bash
# Run with timeout
timeout 300 ctest --verbose

# Kill hung test
pkill -9 test_video_pipeline

# Check system resources
top
free -h
df -h
```

### Inconsistent Results

```bash
# Run test multiple times
ctest --repeat-until-fail 10

# Run in isolation
./test_video_pipeline 2>&1 | tee test_run_1.log

# Compare results
diff test_run_1.log test_run_2.log
```

### Platform-Specific Issues

```bash
# Check platform info
uname -a
lsb_release -a

# Verify architecture
file /usr/bin/libc.so.6

# Check available features
cat /proc/cpuinfo | grep flags
```

## Performance Optimization

### If Tests are Slow

1. **Check system load**:
   ```bash
   uptime
   iostat -x 1 5
   ```

2. **Stop background services**:
   ```bash
   systemctl stop bluetooth
   systemctl stop cups
   ```

3. **Disable power management**:
   ```bash
   sudo cpupower frequency-set -g performance
   ```

4. **Run with reduced precision**:
   ```bash
   ctest -R "quick" --output-on-failure
   ```

### If Latency Tests Fail

1. **Reduce system load**:
   ```bash
   nice -n 19 ctest
   ```

2. **Increase test tolerance**:
   ```bash
   # Modify tolerance in test code
   TEST_ASSERT_UINT32_WITHIN(tolerance_us, expected, actual);
   ```

3. **Run separately**:
   ```bash
   ctest -R "latency" --output-on-failure --repeat 1
   ```

## Continuous Integration Setup

### Jenkins Pipeline

```groovy
pipeline {
    agent any

    stages {
        stage('Build') {
            steps {
                sh '''
                    mkdir -p build
                    cd build
                    cmake -DENABLE_INTEGRATION_TESTS=ON ..
                    make
                '''
            }
        }

        stage('Test') {
            steps {
                sh '''
                    cd build
                    ctest --output-on-failure
                '''
            }
        }

        stage('Report') {
            steps {
                sh '''
                    cd build
                    ctest -T Test
                '''
                publishHTML([
                    reportDir: 'build/Testing/Temporary',
                    reportFiles: 'CTestResults.html',
                    reportName: 'CTest Report'
                ])
            }
        }
    }

    post {
        always {
            junit 'build/Testing/**/Test.xml'
            archiveArtifacts 'build/Testing/**/*'
        }
    }
}
```

### GitLab CI

```yaml
test:
  image: ubuntu:latest
  script:
    - apt-get update
    - apt-get install -y cmake build-essential libunity-dev
    - mkdir build && cd build
    - cmake -DENABLE_INTEGRATION_TESTS=ON ..
    - make
    - ctest --output-on-failure
  artifacts:
    paths:
      - build/Testing/
    when: always
  timeout: 1 hour
```

## Contact and Support

- **Test Failures**: Create issue with test logs
- **Feature Requests**: Discuss in team channels
- **Documentation**: Update this guide for others

---

**Last Updated**: 2024-11-22
**Test Framework Version**: 1.0
**Compatibility**: ESP32-P4, ESP32-C5/C6

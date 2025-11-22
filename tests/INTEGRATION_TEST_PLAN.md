# Video Pipeline Integration Test Plan

## 1. Overview

This document defines the comprehensive integration testing strategy for the ESP32-based high-resolution FPV system. The system consists of:

- **MIPI Camera Interface** (ESP32-P4): Captures video from IMX219/IMX477/OV5647 sensors
- **H.264 Hardware Encoder** (ESP32-P4): Encodes YUV frames to H.264 bitstream
- **Inter-Processor Communication (IPC)** (SPI): Master (ESP32-P4) to Slave (ESP32-C5/C6)
- **WiFi Transmitter** (ESP32-C5/C6): Transmits video over WiFi 6 with FEC
- **WiFi Receiver** (Ground Station): Receives and decodes video
- **H.264 Decoder** (Ground Station): Decodes H.264 bitstream
- **Display** (Ground Station): Renders received video

## 2. Test Architecture

### 2.1 Test Scope

```
┌─────────────────────────────────────────────────────────────────┐
│                    Integration Test Scope                       │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  Camera → Encoder → IPC Master → IPC Slave → WiFi TX            │
│    ↓        ↓         ↓           ↓          ↓                   │
│  [Test]   [Test]    [Test]      [Test]    [Test]                │
│                                                                 │
│  WiFi RX → H264 Decoder → Display                               │
│    ↓          ↓             ↓                                    │
│  [Test]     [Test]       [Test]                                 │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  Cross-layer Testing:                                   │   │
│  │  - Latency measurements across entire pipeline          │   │
│  │  - Frame drop detection and recovery                    │   │
│  │  - Packet loss simulation and FEC validation            │   │
│  │  - Statistics consistency across all components         │   │
│  │  - Performance degradation under load                   │   │
│  │  - Channel switching and mode changes                   │   │
│  │  - Power cycle recovery                                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

### 2.2 Test Environment

```
Physical Setup:
┌──────────────────┐                    ┌──────────────────┐
│   Air Unit       │                    │ Ground Station   │
│  ┌────────────┐  │                    │  ┌────────────┐  │
│  │ MIPI Camera│  │                    │  │  WiFi RX   │  │
│  └────────────┘  │                    │  └────────────┘  │
│       ↓          │                    │       ↓          │
│  ┌────────────┐  │                    │  ┌────────────┐  │
│  │  Encoder   │  │                    │  │  Decoder   │  │
│  └────────────┘  │                    │  └────────────┘  │
│       ↓          │      WiFi 6       │       ↓          │
│  ┌────────────┐  │   ─────────────   │  ┌────────────┐  │
│  │IPC Master  │──┼──  (Simulated)  ──┤  │   Display  │  │
│  └────────────┘  │   or Real Link     │  └────────────┘  │
│       ↓          │                    │                   │
│  ┌────────────┐  │                    │                   │
│  │IPC Slave   │  │                    │                   │
│  │WiFi TX     │  │                    │                   │
│  └────────────┘  │                    │                   │
└──────────────────┘                    └──────────────────┘

Test Execution:
- Real hardware integration tests
- Software simulation/mocking
- Hybrid testing (real components + mocks)
```

## 3. Test Scenarios

### 3.1 Camera to IPC Master Pipeline Tests

#### 3.1.1 Basic Frame Capture and Encoding
- **Objective**: Verify end-to-end frame capture to NAL unit generation
- **Steps**:
  1. Initialize MIPI camera with IMX219 sensor
  2. Initialize H.264 encoder
  3. Capture frames at specified FPS
  4. Verify encoder produces NAL units
  5. Validate NAL unit sequence (SPS, PPS, I-frames, P-frames)
- **Expected Results**:
  - ✓ Frame capture rate matches configured FPS (±2%)
  - ✓ NAL units contain valid H.264 data
  - ✓ Keyframe interval matches GOP size
  - ✓ No frame drops during steady-state capture
- **Pass Criteria**: 100% NAL unit generation success rate

#### 3.1.2 Multi-Resolution Testing
- **Objective**: Verify encoder supports multiple resolutions
- **Resolutions to test**:
  - 1280×720 (720p) @ 30fps
  - 1280×720 (720p) @ 60fps
  - 1920×1080 (1080p) @ 30fps
  - 1920×1080 (1080p) @ 60fps
  - 2560×1440 (1440p) @ 30fps (if supported)
- **Expected Results**:
  - ✓ All resolutions encode without errors
  - ✓ Frame rate stays within target (±5%)
  - ✓ Bitrate matches configured value (±10%)
- **Pass Criteria**: All tested resolutions produce valid H.264 output

#### 3.1.3 Multi-Frame Rate Testing
- **Objective**: Verify encoder handles different frame rates
- **Frame rates to test**: 15fps, 24fps, 30fps, 48fps, 60fps
- **Expected Results**:
  - ✓ Actual FPS matches target FPS (±2%)
  - ✓ I-frame spacing adapts correctly
  - ✓ NAL unit timestamp sequence is monotonic
- **Pass Criteria**: FPS accuracy within ±2%, valid timestamp progression

#### 3.1.4 Bitrate Control Testing
- **Objective**: Verify bitrate control modes (CBR, VBR, CQP)
- **Test Parameters**:
  - CBR: 2Mbps, 6Mbps, 10Mbps
  - VBR: Min 2Mbps, max 15Mbps
  - CQP: QP 18-35
- **Expected Results**:
  - ✓ CBR maintains bitrate within ±5%
  - ✓ VBR adapts to content complexity
  - ✓ CQP produces consistent quality
- **Pass Criteria**: Bitrate stability within tolerance, quality acceptable

#### 3.1.5 Keyframe Injection Testing
- **Objective**: Verify forced keyframe requests
- **Steps**:
  1. Run encoding at 30fps
  2. Request keyframe injection at frame 30
  3. Verify IDR NAL unit generated
  4. Measure keyframe latency
- **Expected Results**:
  - ✓ Keyframe generated within 1 frame period
  - ✓ Next frame after keyframe is not keyframe
  - ✓ Stream remains valid after injection
- **Pass Criteria**: Keyframe injection succeeds within 1 frame latency

#### 3.1.6 Encoder Statistics Validation
- **Objective**: Verify encoder stats accuracy
- **Expected Results**:
  - ✓ frames_encoded matches actual encoded frames (±1)
  - ✓ keyframes_encoded matches actual I-frames
  - ✓ total_bytes > 0 and growing monotonically
  - ✓ actual_fps within ±2% of target
  - ✓ average_qp between configured min and max
- **Pass Criteria**: All statistics accurate within tolerance

### 3.2 IPC Pipeline Tests

#### 3.2.1 IPC Master-Slave Handshake
- **Objective**: Verify correct initialization and synchronization
- **Steps**:
  1. Initialize IPC master
  2. Wait for slave ready signal
  3. Verify communication link
- **Expected Results**:
  - ✓ Master detects slave ready within 10s
  - ✓ Both endpoints report ready status
  - ✓ IPC link capacity verified
- **Pass Criteria**: Handshake succeeds and link ready

#### 3.2.2 NAL Unit Transmission
- **Objective**: Verify NAL units transmitted correctly over IPC
- **Steps**:
  1. Send various NAL types (SPS, PPS, I, P)
  2. Verify slave receives complete packets
  3. Validate packet integrity
- **Expected Results**:
  - ✓ 100% NAL unit delivery
  - ✓ Packets arrive in correct order
  - ✓ No data corruption (CRC valid)
  - ✓ Slave queue doesn't overflow
- **Pass Criteria**: Zero packet loss, correct ordering, valid CRC

#### 3.2.3 IPC Throughput Testing
- **Objective**: Verify throughput meets requirements
- **Test Matrix**:
  - 720p @ 30fps: ~500 Kbps-2 Mbps
  - 1080p @ 30fps: ~1-6 Mbps
  - 1080p @ 60fps: ~4-12 Mbps
- **Expected Results**:
  - ✓ Throughput matches calculated bitrate
  - ✓ Latency < 100ms (SPI-limited)
  - ✓ No packet drops under sustained load
- **Pass Criteria**: Throughput ≥95% of expected rate

#### 3.2.4 Configuration Packet Handling
- **Objective**: Verify config packets transmitted correctly
- **Test scenarios**:
  - WiFi channel change
  - Bitrate adjustment
  - FEC parameters change
  - TX power adjustment
- **Expected Results**:
  - ✓ Config packets delivered reliably
  - ✓ Slave applies configuration
  - ✓ No video stream interruption
- **Pass Criteria**: All config changes propagate within 1 second

#### 3.2.5 Statistics Packet Exchange
- **Objective**: Verify stats packet accuracy
- **Expected Results**:
  - ✓ WiFi RSSI, TX failures reported
  - ✓ Packet counts match master records
  - ✓ Bitrate calculations consistent
- **Pass Criteria**: Statistics match within ±2%

### 3.3 WiFi Transmission Tests

#### 3.3.1 WiFi Link Establishment
- **Objective**: Verify WiFi connection setup
- **Steps**:
  1. Configure WiFi parameters (channel, power)
  2. Establish link with ground station
  3. Verify connection stability
- **Expected Results**:
  - ✓ Connection succeeds within 5 seconds
  - ✓ RSSI stable (±3dBm variation)
  - ✓ Link rate negotiated correctly
- **Pass Criteria**: Stable WiFi link established

#### 3.3.2 Video Packet Transmission
- **Objective**: Verify video packets reach ground station
- **Test Matrix**:
  - 720p @ 30fps
  - 1080p @ 30fps
  - 1080p @ 60fps
- **Expected Results**:
  - ✓ Packet delivery rate ≥95%
  - ✓ Latency < 200ms (WiFi + decode)
  - ✓ No excessive packet loss
- **Pass Criteria**: ≥95% delivery, latency < 200ms

#### 3.3.3 FEC Encoding/Decoding
- **Objective**: Verify FEC improves reliability
- **FEC Test Parameters**:
  - k=4, n=6 (33% overhead)
  - k=8, n=12 (50% overhead)
  - k=16, n=20 (25% overhead)
- **Expected Results**:
  - ✓ FEC encodes all packets
  - ✓ Loss recovery works up to (n-k) packets
  - ✓ No performance degradation with FEC enabled
- **Pass Criteria**: FEC recovery succeeds, throughput maintained

#### 3.3.4 Channel Switching During Operation
- **Objective**: Verify seamless channel switching
- **Steps**:
  1. Stream video on channel 1
  2. Switch to channel 6 (overlapping)
  3. Switch to channel 11 (non-overlapping)
  4. Verify stream continues
- **Expected Results**:
  - ✓ Switch completes within 1 second
  - ✓ No frames lost during switch
  - ✓ Stream resumes immediately
- **Pass Criteria**: Seamless switching with zero frame loss

#### 3.3.5 WiFi Power Level Adjustment
- **Objective**: Verify TX power adjustment
- **Test Levels**: +20dBm, +10dBm, 0dBm, -10dBm
- **Expected Results**:
  - ✓ Power adjustment takes effect
  - ✓ RSSI changes proportionally
  - ✓ Link remains stable after adjustment
- **Pass Criteria**: Power adjustment verified by RSSI change

### 3.4 Packet Loss and Recovery Tests

#### 3.4.1 Simulated Packet Loss
- **Objective**: Verify FEC recovery from packet loss
- **Loss Scenarios**:
  - Single packet loss (0.5%)
  - Burst loss (3 consecutive packets)
  - Random loss (5%, 10%, 15%)
- **Expected Results**:
  - ✓ Loss ≤ FEC redundancy: 100% recovery
  - ✓ Loss > FEC redundancy: Controlled degradation
  - ✓ Frame drops minimized
- **Pass Criteria**: FEC recovery achieves target reliability

#### 3.4.2 Timeout and Retransmission
- **Objective**: Verify timeout handling
- **Test scenarios**:
  - Master waits for slave ACK
  - Slave waits for master packets
  - Ground station waits for WiFi packets
- **Expected Results**:
  - ✓ Timeouts occur when expected
  - ✓ Retransmission succeeds
  - ✓ No deadlocks
- **Pass Criteria**: Graceful timeout handling, no deadlocks

#### 3.4.3 Frame Drop Detection
- **Objective**: Detect and report frame drops
- **Steps**:
  1. Introduce packet loss exceeding FEC capability
  2. Verify frame index gaps detected
  3. Check statistics updated
- **Expected Results**:
  - ✓ Frame drops detected via sequence gap
  - ✓ Statistics reflect drops
  - ✓ Recovery occurs at next I-frame
- **Pass Criteria**: Frame drops detected and reported accurately

### 3.5 Latency Measurement Tests

#### 3.5.1 End-to-End Latency
- **Objective**: Measure complete pipeline latency
- **Measurement Points**:
  - Camera capture time
  - Encoding latency
  - IPC transmission
  - WiFi transmission
  - Decoding latency
  - Total E2E latency
- **Expected Results**:
  - ✓ Capture: < 30ms
  - ✓ Encoding: < 50ms @ 1080p/60fps
  - ✓ IPC: < 10ms
  - ✓ WiFi: < 50ms (varies by distance/interference)
  - ✓ Decode: < 30ms
  - ✓ Total E2E: < 200ms
- **Pass Criteria**: E2E latency < 200ms for FPV operation

#### 3.5.2 Latency Breakdown by Component
- **Objective**: Profile latency of each stage
- **Measure**:
  - Encoder encoding time per frame
  - IPC transmission time
  - WiFi packet transmission time
  - Decoder decode time
- **Expected Results**:
  - ✓ Bottleneck identification
  - ✓ Optimization opportunities found
  - ✓ Performance regression detection
- **Pass Criteria**: Latency profile established and baseline set

#### 3.5.3 Latency Under Load
- **Objective**: Measure latency with high utilization
- **Load conditions**:
  - 100% IPC throughput
  - High network traffic
  - System thermal load
- **Expected Results**:
  - ✓ Latency increases < 50%
  - ✓ Maintains FPV usability (< 300ms)
  - ✓ No catastrophic degradation
- **Pass Criteria**: Graceful latency degradation under load

### 3.6 Power Cycle and Recovery Tests

#### 3.6.1 Master (ESP32-P4) Power Cycle
- **Objective**: Verify recovery after power loss
- **Steps**:
  1. Stream video normally
  2. Power cycle ESP32-P4
  3. Verify recovery sequence
- **Expected Results**:
  - ✓ Boot time < 5 seconds
  - ✓ Camera initializes correctly
  - ✓ Encoder resumes operation
  - ✓ Video stream resumes
- **Pass Criteria**: Full recovery within 5 seconds

#### 3.6.2 Slave (ESP32-C5/C6) Power Cycle
- **Objective**: Verify WiFi transmitter recovery
- **Steps**:
  1. Stream video normally
  2. Power cycle ESP32-C5/C6
  3. Verify IPC restart sequence
- **Expected Results**:
  - ✓ Slave initializes within 3 seconds
  - ✓ Master detects recovery
  - ✓ Video transmission resumes
  - ✓ No permanent stream disruption
- **Pass Criteria**: Automatic recovery after slave restart

#### 3.6.3 Ground Station Recovery
- **Objective**: Verify receiver recovery
- **Steps**:
  1. Receive video normally
  2. Stop receiver application
  3. Restart receiver
  4. Verify recovery
- **Expected Results**:
  - ✓ Receiver reconnects within 3 seconds
  - ✓ First displayable frame within 1 second
  - ✓ Synchronization recovered
- **Pass Criteria**: Automatic resync after receiver restart

### 3.7 Statistics Validation Tests

#### 3.7.1 Frame Counter Consistency
- **Objective**: Verify frame counters across pipeline
- **Counters**:
  - Camera: frames_captured
  - Encoder: frames_encoded, keyframes_encoded
  - IPC: packets_sent
  - WiFi TX: packets_transmitted
  - Decoder: frames_decoded, keyframes_decoded
- **Expected Results**:
  - ✓ Encoder frames ≤ capture frames
  - ✓ Sent packets ≥ encoded frames
  - ✓ Transmitted packets ≤ sent packets
  - ✓ Gaps indicate drops (expected with packet loss)
- **Pass Criteria**: Frame counts consistent with expected data flow

#### 3.7.2 Bitrate Accuracy
- **Objective**: Verify bitrate calculations
- **Measure**:
  - Actual bitrate from encoder
  - Actual bitrate from IPC
  - Actual bitrate from WiFi TX
  - Actual bitrate from decoder
- **Expected Results**:
  - ✓ All measurements agree (±10%)
  - ✓ Bitrate stable over time
  - ✓ Variations due to frame complexity expected
- **Pass Criteria**: Bitrate measurements consistent

#### 3.7.3 Timing Information Accuracy
- **Objective**: Verify timestamps (PTS, DTS)
- **Expected Results**:
  - ✓ PTS monotonically increasing
  - ✓ DTS ≤ PTS (allows B-frames)
  - ✓ Timestamp accuracy: ±1ms
  - ✓ No timestamp resets
- **Pass Criteria**: Monotonic timestamps with correct ordering

### 3.8 System Load and Stress Tests

#### 3.8.1 Sustained Operation Test
- **Objective**: Verify system stability over extended operation
- **Duration**: 1 hour continuous streaming
- **Monitoring**:
  - Frame drops
  - CPU utilization
  - Memory usage
  - Thermal performance
- **Expected Results**:
  - ✓ Zero unrecovered frame drops
  - ✓ CPU stable (±5%)
  - ✓ Memory stable (no leaks)
  - ✓ Thermal throttling not triggered
- **Pass Criteria**: Stable operation for 1 hour

#### 3.8.2 Temperature Stress Test
- **Objective**: Verify operation under thermal load
- **Test Conditions**:
  - Ambient 40-50°C
  - Continuous streaming
  - Maximum bitrate
- **Expected Results**:
  - ✓ Chip temperature < 80°C
  - ✓ No thermal throttling
  - ✓ Quality maintained
- **Pass Criteria**: No thermal issues under load

#### 3.8.3 Memory Pressure Test
- **Objective**: Verify robust operation with memory pressure
- **Test Conditions**:
  - Allocate buffers up to 80% of available
  - Stream at maximum resolution/bitrate
- **Expected Results**:
  - ✓ Encoder adapts gracefully
  - ✓ No crashes or freezes
  - ✓ Frame drops possible but controlled
- **Pass Criteria**: Graceful degradation under memory pressure

#### 3.8.4 Interrupt and Concurrent Load
- **Objective**: Verify robustness with concurrent I/O
- **Test Conditions**:
  - Continuous streaming
  - WiFi scanning active
  - OTA updates occurring
- **Expected Results**:
  - ✓ Frame loss increases but manageable (< 5%)
  - ✓ Latency degrades gracefully
  - ✓ No system crash
- **Pass Criteria**: Robust operation with concurrent load

## 4. Performance Benchmark Specifications

### 4.1 Throughput Benchmarks

| Resolution | FPS | Expected Bitrate | IPC Link % | WiFi Link % |
|------------|-----|------------------|-----------|------------|
| 720p       | 30  | 1-2 Mbps         | 5-10%     | 10-20%    |
| 720p       | 60  | 2-4 Mbps         | 10-20%    | 20-40%    |
| 1080p      | 30  | 3-6 Mbps         | 15-30%    | 30-60%    |
| 1080p      | 60  | 6-12 Mbps        | 30-60%    | 60-95%    |
| 1440p      | 30  | 6-10 Mbps        | 30-50%    | 60-95%    |

### 4.2 Latency Benchmarks

| Component       | Target (ms) | Max Acceptable (ms) |
|-----------------|-------------|-------------------|
| Camera Capture  | 16-33       | 50                |
| H.264 Encoding  | 20-50       | 100               |
| IPC Transfer    | 5-10        | 20                |
| WiFi TX         | 20-50       | 100               |
| WiFi RX         | 20-50       | 100               |
| H.264 Decode    | 20-50       | 100               |
| E2E (Total)     | 100-150     | 200               |

### 4.3 CPU Usage Benchmarks

| Component         | Resolution | FPS | Target % | Max % |
|-------------------|-----------|-----|----------|-------|
| Camera Driver     | 1080p     | 60  | 5-10     | 15    |
| H.264 Encoder     | 1080p     | 60  | 30-40    | 50    |
| IPC Master        | 1080p     | 60  | 5-10     | 15    |
| IPC Slave         | 1080p     | 60  | 10-15    | 25    |
| WiFi TX           | 1080p     | 60  | 15-25    | 35    |
| System Total      | 1080p     | 60  | 60-80    | 90    |

### 4.4 Memory Usage Benchmarks

| Component         | Expected (KB) | Max (KB) |
|-------------------|---------------|----------|
| MIPI Camera       | 200-300       | 500      |
| H.264 Encoder     | 500-800       | 1000     |
| IPC Buffers       | 100-200       | 300      |
| WiFi TX Buffers   | 200-500       | 1000     |
| Statistics        | 50-100        | 200      |
| Total System      | 1500-2500     | 4000     |

### 4.5 Reliability Benchmarks

| Metric                          | Target    | Max Acceptable |
|---------------------------------|-----------|----------------|
| Frame Drop Rate                 | < 0.1%    | < 1%          |
| IPC Packet Loss Rate            | < 0.01%   | < 0.1%        |
| WiFi Packet Loss (good signal)  | < 2%      | < 5%          |
| WiFi Packet Loss (degraded)     | < 10%     | < 20%         |
| FEC Recovery Rate               | > 99%     | > 95%         |
| Mean Time Between Failures (MTBF) | > 24h   | > 8h          |

## 5. Test Execution Workflow

### 5.1 Test Setup Phase

```
1. Hardware Setup
   ├─ Flash ESP32-P4 with test firmware
   ├─ Flash ESP32-C5/C6 with test firmware
   ├─ Connect WiFi devices
   ├─ Configure test network
   └─ Verify hardware connections

2. Test Environment Initialization
   ├─ Start log collection
   ├─ Initialize test framework
   ├─ Start performance monitoring
   ├─ Calibrate latency measurements
   └─ Verify test data logging

3. Pre-test Checks
   ├─ Verify all components responsive
   ├─ Check IPC communication
   ├─ Verify WiFi connectivity
   ├─ Confirm camera sensor operational
   └─ Test log output functional
```

### 5.2 Test Execution Phase

```
Serial Test Execution:
Test1: Camera-Encoder Pipeline
  ├─ Initialize camera
  ├─ Initialize encoder
  ├─ Capture and encode frames
  ├─ Validate output
  └─ Collect statistics

Test2: IPC Communication
  ├─ Initialize master/slave
  ├─ Send test packets
  ├─ Verify reception
  └─ Measure throughput

Test3: WiFi Transmission
  ├─ Establish link
  ├─ Transmit video
  ├─ Measure signal quality
  └─ Validate packets

Test4: End-to-End Pipeline
  ├─ Full system initialization
  ├─ Stream video end-to-end
  ├─ Measure latencies
  ├─ Validate complete flow
  └─ Collect performance data

Test5: Stress and Recovery
  ├─ Simulate failures
  ├─ Verify recovery
  ├─ Long-duration streaming
  └─ Thermal management
```

### 5.3 Test Cleanup Phase

```
1. Stop Streaming
   ├─ Send stop commands
   ├─ Wait for graceful shutdown
   └─ Verify all threads stopped

2. Collect Results
   ├─ Save test logs
   ├─ Export performance data
   ├─ Generate statistics reports
   └─ Capture system snapshots

3. Cleanup Resources
   ├─ Deinitialize components
   ├─ Free allocated memory
   ├─ Reset hardware state
   └─ Prepare for next test
```

## 6. Test Results and Pass/Fail Criteria

### 6.1 Overall Test Status

A test suite passes if ALL of the following conditions are met:

1. **Functional Correctness**: 100% of functional tests pass
2. **Performance Targets**: ≥95% of performance tests meet targets
3. **No Crashes**: Zero segmentation faults or hard failures
4. **No Memory Leaks**: All allocated memory freed properly
5. **Stability**: No system lockups or watchdog timeouts
6. **Reproducibility**: Results consistent across multiple runs

### 6.2 Individual Test Pass Criteria

#### Pass (Green)
- Test completed without error
- All assertions passed
- Performance metrics within target
- Expected behavior observed
- No warnings or anomalies

#### Conditional Pass (Yellow)
- Test completed with warnings
- Minor degradation from target
- Recoverable from transient error
- Result acceptable but needs investigation

#### Fail (Red)
- Test did not complete
- Critical assertion failed
- Performance severely degraded
- Unexpected behavior observed
- System crash or reset

### 6.3 Performance Pass Thresholds

| Metric              | Pass (Green) | Conditional (Yellow) | Fail (Red)   |
|--------------------|-------------|-------------------|--------------|
| Throughput         | ≥95%        | 85-94%            | <85%        |
| Latency            | ≤100% of target | 100-150%      | >150%       |
| CPU Usage          | ≤100% of target | 100-120%      | >120%       |
| Memory Usage       | ≤100% of target | 100-120%      | >120%       |
| Frame Drop Rate    | <0.1%       | 0.1-1%            | >1%         |
| Packet Loss Rate   | <0.01%      | 0.01-0.1%         | >0.1%       |

### 6.4 Test Reporting

Each test generates a report containing:

```
┌─────────────────────────────────────────┐
│ Test Report Template                    │
├─────────────────────────────────────────┤
│ Test ID: TEST_001                       │
│ Name: Camera Frame Capture              │
│ Duration: 123.45 seconds                │
│ Status: PASS / CONDITIONAL / FAIL       │
│                                         │
│ Metrics:                                │
│ - Frames Captured: 3687                 │
│ - Capture FPS: 29.92 (target: 30)      │
│ - Frame Drop Count: 0                   │
│ - Latency (avg): 12.3ms                │
│                                         │
│ Assertions:                             │
│ ✓ Capture rate within ±2%              │
│ ✓ No frame drops                       │
│ ✓ Valid frame data                     │
│                                         │
│ Warnings: None                          │
│ Errors: None                            │
│ Notes: Baseline test for performance    │
│                                         │
│ Equipment:                              │
│ - Device: ESP32-P4 Rev 1.0             │
│ - Camera: IMX219                        │
│ - Ambient Temp: 25°C                   │
│                                         │
│ Timestamp: 2024-11-22 10:30:45 UTC    │
│ Test Operator: Integration Test Suite  │
└─────────────────────────────────────────┘
```

## 7. Test Data and Artifacts

### 7.1 Performance Baselines

Maintained in `/tests/benchmarks/baselines.csv`:

```
test_id, metric, target, baseline, tolerance, unit
TEST_001, capture_fps, 30.0, 29.95, 0.1, fps
TEST_002, encode_latency, 50.0, 45.2, 10.0, ms
TEST_003, ipc_throughput, 6.0, 5.98, 0.1, Mbps
...
```

### 7.2 Test Logs

- Real-time console logs: `/tests/logs/run_*.log`
- Formatted test reports: `/tests/reports/TEST_*.html`
- Raw measurement data: `/tests/data/measurements_*.csv`
- Performance graphs: `/tests/graphs/performance_*.png`

### 7.3 Video Capture Samples

Test video samples for validation:
- Reference bitstream: `/tests/fixtures/reference_720p_30fps.h264`
- Encoded test output: `/tests/output/actual_720p_30fps.h264`
- Decoded frame sequence: `/tests/output/decoded_frames/`

## 8. Continuous Integration

### 8.1 CI Pipeline Integration

Tests run automatically on:
- Every commit to development branch
- Pull request creation/update
- Nightly full test suite
- Release candidate validation

### 8.2 Test Badges

```
Integration Tests: [PASS] [CONDITIONAL] [FAIL]
Performance Regression: [OK] [DEGRADED] [CRITICAL]
Stability: [STABLE] [INTERMITTENT] [UNSTABLE]
Coverage: [95%] [80%] [<80%]
```

## 9. Test Configuration and Variants

### 9.1 Configuration Matrix

```
Dimensions:
1. Hardware:
   - Sensor: IMX219, IMX477, OV5647
   - Master: ESP32-P4
   - Slave: ESP32-C5, ESP32-C6
   - Ground: Linux, macOS, Windows

2. Resolution:
   - 720p, 1080p, 1440p

3. Frame Rate:
   - 30fps, 60fps

4. Network:
   - Ideal (0% loss), Degraded (5% loss), Poor (15% loss)
   - FEC On/Off

Total Combinations: 3 × 2 × 3 × 2 × 2 × 3 = 216 variants
Critical Path: 3 × 1 × 1 × 1 × 1 × 1 = 3 tests
```

### 9.2 Quick Test Suite (15 minutes)
Essential tests for CI:
- Camera initialization
- Encoder basic functionality
- IPC communication
- WiFi transmission
- E2E pipeline

### 9.3 Full Test Suite (2 hours)
All tests for release validation:
- All quick tests
- All stress tests
- Performance benchmarks
- Recovery scenarios
- All configuration variants

## 10. Known Limitations and Future Improvements

### 10.1 Current Limitations

1. WiFi link simulation in test harness may not fully represent real RF conditions
2. Packet loss simulation uniform, but real WiFi has bursty patterns
3. Thermal testing limited to ambient conditions
4. Cross-platform ground station testing limited to Linux initially

### 10.2 Future Enhancements

1. Add real RF testing with controlled interference
2. Implement bursty packet loss models
3. Add active thermal chamber for temperature testing
4. Expand ground station platform coverage
5. Add power consumption profiling
6. Implement video quality assessment (VMAF/SSIM)
7. Add active network traffic simulation

---

**Document Version**: 1.0
**Last Updated**: 2024-11-22
**Owner**: Video Pipeline Integration Team

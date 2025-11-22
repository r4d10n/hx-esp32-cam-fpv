# Video Pipeline Test Scenarios - Complete Reference

## Quick Index

- **[Camera & Encoder Tests](#camera--encoder-tests)** (5 tests)
- **[IPC Communication Tests](#ipc-communication-tests)** (5 tests)
- **[WiFi Transmission Tests](#wifi-transmission-tests)** (5 tests)
- **[Latency Measurement Tests](#latency-measurement-tests)** (3 tests)
- **[Statistics & Validation Tests](#statistics--validation-tests)** (4 tests)
- **[Recovery & Robustness Tests](#recovery--robustness-tests)** (5 tests)
- **[System Integration Tests](#system-integration-tests)** (3 tests)
- **[Performance Benchmarks](#performance-benchmarks)** (10 tests)

**Total Test Cases**: 40+

---

## Camera & Encoder Tests

### TEST_001: Camera Initialization
**Category**: Camera & Encoder | **Duration**: 2 min | **Type**: Functional

**Objective**: Verify MIPI camera initializes correctly with proper configuration

**Configuration**:
- Sensor: IMX219
- Resolution: 1920×1080
- FPS: 30
- Format: YUV422

**Steps**:
1. Initialize camera with test config
2. Verify sensor communication
3. Check frame buffer allocation
4. Confirm callback registration
5. Start/stop capture

**Expected Results**:
- ✓ Camera initialized without errors
- ✓ Frame capture starts within 1 second
- ✓ Frames arrive at expected rate (±2%)
- ✓ No frame corruption detected

**Pass Criteria**: All frames valid, no I2C errors

---

### TEST_002: H.264 Encoder Initialization
**Category**: Camera & Encoder | **Duration**: 2 min | **Type**: Functional

**Objective**: Verify H.264 encoder initializes with various configs

**Configurations to Test**:
- Profile: Baseline, Main, High
- Level: 3.0, 3.1, 4.0, 4.1
- Rate Control: CBR, VBR, CQP

**Steps**:
1. Create encoder config
2. Initialize encoder
3. Register NAL callback
4. Verify internal state
5. Cleanup resources

**Expected Results**:
- ✓ Encoder accepts all valid configs
- ✓ Rejects invalid configurations
- ✓ Memory allocated correctly
- ✓ Callback chain functions

**Pass Criteria**: All configs accepted/rejected correctly

---

### TEST_003: Multi-Resolution Support
**Category**: Camera & Encoder | **Duration**: 5 min | **Type**: Functional

**Resolutions**:
| Resolution | FPS | Bitrate | Status |
|------------|-----|---------|--------|
| 1280×720  | 30  | 2 Mbps  | ✓ |
| 1280×720  | 60  | 4 Mbps  | ✓ |
| 1920×1080 | 30  | 6 Mbps  | ✓ |
| 1920×1080 | 60  | 12 Mbps | ✓ |
| 2560×1440 | 30  | 10 Mbps | ✓ |

**Test Procedure**:
1. Generate YUV frame for each resolution
2. Encode 30 frames
3. Validate output bitrate
4. Measure encoding time
5. Verify frame drops

**Expected Results**:
- ✓ All resolutions encode successfully
- ✓ Bitrate within ±10% of target
- ✓ FPS accuracy ±2%
- ✓ No frame drops in 30-frame sequence

**Pass Criteria**: All resolutions produce valid H.264

---

### TEST_004: Bitrate Control Modes
**Category**: Camera & Encoder | **Duration**: 5 min | **Type**: Functional

**Modes**:

**CBR (Constant Bitrate)**:
- Target: 6 Mbps
- Tolerance: ±5%
- Expected: Stable bitrate throughout test

**VBR (Variable Bitrate)**:
- Min: 2 Mbps, Max: 15 Mbps
- Tolerance: Within range
- Expected: Adapts to content complexity

**CQP (Constant Quantization)**:
- QP: 25
- Tolerance: ±2 QP
- Expected: Consistent quality

**Steps**:
1. Encode 300 frames with each mode
2. Measure bitrate per second
3. Calculate variance
4. Assess quality (QP)
5. Verify stability

**Expected Results**:
- ✓ CBR: Bitrate ±5% of target
- ✓ VBR: Stays within configured range
- ✓ CQP: QP within ±2 of target
- ✓ No instability or oscillation

**Pass Criteria**: Bitrate control stable per mode

---

### TEST_005: Keyframe Injection
**Category**: Camera & Encoder | **Duration**: 2 min | **Type**: Functional

**Objective**: Verify forced keyframe requests work correctly

**Test Procedure**:
1. Start encoding at 30 fps
2. Encode 60 frames (2 seconds at natural 30 fps GOP)
3. Request keyframe at frame 45
4. Measure when IDR NAL appears
5. Verify stream integrity

**Expected Results**:
- ✓ Keyframe injected within 1 frame period
- ✓ IDR NAL valid H.264 format
- ✓ Stream remains decodable
- ✓ P-frames follow keyframe correctly

**Pass Criteria**: Keyframe appears within 1 frame, stream valid

---

## IPC Communication Tests

### TEST_010: IPC Master-Slave Handshake
**Category**: IPC | **Duration**: 3 min | **Type**: Functional

**Objective**: Verify master and slave establish communication

**Protocol**:
1. Master initializes SPI as master
2. Slave initializes SPI as slave
3. Master polls ready line
4. Slave asserts ready line
5. Exchange version/capability info

**Expected Results**:
- ✓ Master detects slave ready within 10s
- ✓ Both endpoints report synchronized state
- ✓ Version compatibility confirmed
- ✓ Ready to exchange video packets

**Pass Criteria**: Handshake succeeds, link ready

---

### TEST_011: NAL Unit Transmission
**Category**: IPC | **Duration**: 3 min | **Type**: Functional

**NAL Types**:
| Type | Size | Count |
|------|------|-------|
| SPS  | 100  | 1     |
| PPS  | 50   | 1     |
| IDR  | 5KB  | 30    |
| P    | 3KB  | 270   |

**Test Procedure**:
1. Create NAL units of each type
2. Send via IPC with sequence numbers
3. Slave receives and validates
4. Verify CRC16 of each packet
5. Check sequence continuity

**Expected Results**:
- ✓ All 302 NAL units delivered
- ✓ 100% delivery rate (no loss)
- ✓ Packets arrive in correct order
- ✓ CRC validation passes
- ✓ Slave buffer doesn't overflow

**Pass Criteria**: 100% NAL delivery, CRC valid, ordered

---

### TEST_012: IPC Throughput
**Category**: IPC | **Duration**: 5 min | **Type**: Performance

**Scenarios**:

| Scenario | Bitrate | Duration | Expected Throughput |
|----------|---------|----------|-------------------|
| 720p@30fps | 2 Mbps | 30s | ~2 Mbps |
| 1080p@30fps | 6 Mbps | 30s | ~6 Mbps |
| 1080p@60fps | 12 Mbps | 30s | ~12 Mbps |

**Measurement**:
1. Send continuous stream of packets
2. Measure bytes per second
3. Calculate achieved throughput
4. Compare to expected
5. Monitor for latency increase

**Expected Results**:
- ✓ Throughput ≥95% of expected
- ✓ SPI link not saturated
- ✓ Transfer latency stable
- ✓ No packet loss under sustained load

**Pass Criteria**: ≥95% of expected throughput

---

### TEST_013: IPC Configuration Packets
**Category**: IPC | **Duration**: 3 min | **Type**: Functional

**Configuration Changes**:

| Parameter | Change | Expected Time |
|-----------|--------|----------------|
| WiFi Channel | 1→6→11 | <1s |
| TX Power | 20→0→-10 dBm | <1s |
| FEC Ratio | 4/6→8/12→16/20 | <1s |
| Bitrate | 6→4→8 Mbps | <1s |

**Test Procedure**:
1. Send config change packet
2. Slave applies configuration
3. Master monitors effect
4. Measure convergence time
5. Verify video stream continuity

**Expected Results**:
- ✓ Config applied within 1 second
- ✓ No frame loss during change
- ✓ New settings effective immediately
- ✓ Stream resumes without artifacts

**Pass Criteria**: Config applied <1s, no frame drops

---

### TEST_014: IPC Error Handling
**Category**: IPC | **Duration**: 3 min | **Type**: Robustness

**Error Scenarios**:

| Error | Injection | Recovery |
|-------|-----------|----------|
| CRC Error | Flip bit in packet | Skip packet, sync next |
| Incomplete Packet | Cut transfer mid-packet | Timeout, resync |
| Queue Full | Flood with packets | Drop gracefully |
| Timeout | No response from slave | Retry with backoff |

**Test Procedure**:
1. Simulate each error condition
2. Measure system response
3. Verify graceful degradation
4. Check error logging
5. Confirm recovery mechanism

**Expected Results**:
- ✓ CRC errors detected and logged
- ✓ Incomplete packets recovered
- ✓ Queue overflow handled gracefully
- ✓ Timeouts trigger retry
- ✓ System resynchronizes

**Pass Criteria**: All errors handled gracefully

---

## WiFi Transmission Tests

### TEST_020: WiFi Link Establishment
**Category**: WiFi | **Duration**: 5 min | **Type**: Functional

**Test Procedure**:
1. Configure WiFi parameters
   - Channel: 6 (2.4 GHz)
   - Standard: WiFi 6 (802.11ax)
   - TX Power: 20 dBm
2. Scan for available networks
3. Connect to ground station
4. Wait for IP address
5. Verify UDP socket connectivity

**Expected Results**:
- ✓ Connection succeeds within 5 seconds
- ✓ RSSI stable (±3 dBm variation)
- ✓ Signal strength > -70 dBm
- ✓ Link rate negotiated (MCS 8+)
- ✓ Socket ready for data

**Pass Criteria**: Stable WiFi link established

---

### TEST_021: FEC Encoding/Decoding
**Category**: WiFi | **Duration**: 5 min | **Type**: Functional

**FEC Configurations**:

| k | n | Overhead | Recovery Capacity |
|---|---|----------|------------------|
| 4 | 6 | 50% | 2 packets |
| 8 | 12 | 50% | 4 packets |
| 16 | 20 | 25% | 4 packets |

**Test Procedure**:
1. Create FEC block with k data packets
2. Generate n-k parity packets
3. Verify parity calculation
4. Remove up to n-k packets
5. Decode and verify recovery

**Expected Results**:
- ✓ Parity packets generated correctly
- ✓ All losses ≤n-k recovered
- ✓ Decoding latency <10ms
- ✓ No performance penalty with FEC enabled
- ✓ Recovered data matches original

**Pass Criteria**: FEC recovery succeeds for n-k losses

---

### TEST_022: Packet Loss Recovery
**Category**: WiFi | **Duration**: 10 min | **Type**: Robustness

**Loss Scenarios**:

| Loss Rate | Pattern | Recovery Expected |
|-----------|---------|------------------|
| 0.5% | Random | >99% with FEC |
| 5% | Random | >95% with FEC |
| 10% | Burst (3 packets) | >85% with FEC |
| 15% | Random | >70% with FEC |

**Test Procedure**:
1. Configure FEC (8/12)
2. Transmit 1000 video packets
3. Simulate packet loss
4. Measure received vs expected
5. Verify frame reconstruction

**Expected Results**:
- ✓ Loss <FEC redundancy: 100% recovery
- ✓ Loss ≥FEC redundancy: Controlled degradation
- ✓ No cascading failures
- ✓ Recovery time <33ms (one frame)
- ✓ User sees minimal artifacts

**Pass Criteria**: FEC recovery meets targets

---

### TEST_023: WiFi Channel Switching
**Category**: WiFi | **Duration**: 5 min | **Type**: Functional

**Channel Sequences**:
1. Start streaming on channel 1
2. Switch to channel 6 (overlapping)
3. Switch to channel 11 (non-overlapping)
4. Return to channel 1

**Expected Results**:
- ✓ Switch completes <1 second
- ✓ No frame loss during switch
- ✓ Stream resumes immediately
- ✓ Video quality maintained
- ✓ No audio/video sync issues

**Measurement**:
- Switch latency: <1000ms
- Frames lost: 0
- Recovery time: <33ms

**Pass Criteria**: Seamless switching, zero frame loss

---

### TEST_024: WiFi TX Power Control
**Category**: WiFi | **Duration**: 3 min | **Type**: Functional

**TX Power Levels**:

| Power (dBm) | Expected RSSI | Tolerance |
|-------------|---------------|-----------|
| 20 | -40 | ±5 dBm |
| 10 | -50 | ±5 dBm |
| 0 | -60 | ±5 dBm |
| -10 | -70 | ±5 dBm |

**Test Procedure**:
1. Set TX power to each level
2. Measure received signal strength
3. Verify RSSI change
4. Confirm operation at each level
5. Return to maximum power

**Expected Results**:
- ✓ TX power adjustment takes effect
- ✓ RSSI changes proportionally
- ✓ Link remains stable after change
- ✓ No sudden disconnections
- ✓ Speed recovers at full power

**Pass Criteria**: TX power adjusts correctly, link stable

---

## Latency Measurement Tests

### TEST_030: End-to-End Latency
**Category**: Latency | **Duration**: 2 min | **Type**: Performance

**Measurement Method**:
1. Timestamp frame at camera input
2. Watermark frame with timestamp
3. Capture output display
4. Detect watermark in output
5. Calculate latency: output_time - input_time

**Measurement Points**:
- Camera input timestamp
- Encoder output timestamp
- IPC transmission timestamp
- WiFi transmission timestamp
- Decoder output timestamp
- Display arrival timestamp

**Expected Results**:
- ✓ E2E latency <200ms for FPV
- ✓ Latency stable (±10ms variation)
- ✓ No latency drift over time
- ✓ Latency increases gradually under load

**Pass Criteria**: E2E latency <200ms, stable

---

### TEST_031: Component Latency Breakdown
**Category**: Latency | **Duration**: 3 min | **Type**: Performance

**Components**:

| Component | Expected | Tolerance |
|-----------|----------|-----------|
| Camera Capture | 30ms | ±10ms |
| H.264 Encoding | 50ms | ±20ms |
| IPC Transfer | 10ms | ±5ms |
| WiFi TX/RX | 50ms | ±20ms |
| H.264 Decode | 30ms | ±10ms |
| Display Update | 10ms | ±5ms |
| **Total** | **180ms** | **±50ms** |

**Measurement**:
1. Insert timing markers in each stage
2. Measure stage latency
3. Sum component latencies
4. Compare with E2E measurement
5. Identify bottleneck

**Expected Results**:
- ✓ Component sum ~E2E latency
- ✓ All components within tolerance
- ✓ No unexpected latency sources
- ✓ Consistent across 100+ frames

**Pass Criteria**: All components within tolerance

---

### TEST_032: Latency Under Load
**Category**: Latency | **Duration**: 5 min | **Type**: Performance

**Load Levels**:

| Condition | CPU Load | Expected Latency Increase |
|-----------|----------|--------------------------|
| Idle | 10% | 0% (baseline) |
| Normal | 50% | +10% |
| High | 80% | +30% |
| Extreme | 95% | +50% |

**Test Procedure**:
1. Measure baseline latency (idle)
2. Increase CPU load (thread spawning)
3. Re-measure latency
4. Record latency vs load curve
5. Verify graceful degradation

**Expected Results**:
- ✓ Latency increases <50% at 95% load
- ✓ Maintains FPV usability (<300ms E2E)
- ✓ No catastrophic latency spikes
- ✓ Recovers quickly when load drops

**Pass Criteria**: Graceful latency degradation

---

## Statistics & Validation Tests

### TEST_040: Frame Counter Consistency
**Category**: Statistics | **Duration**: 5 min | **Type**: Functional

**Counters**:

| Counter | Source | Relationship |
|---------|--------|--------------|
| frames_captured | Camera | Baseline |
| frames_encoded | Encoder | ≤ captured |
| keyframes_encoded | Encoder | ≤ encoded |
| packets_sent | IPC | ≥ encoded |
| packets_transmitted | WiFi | ≤ sent |
| packets_received | Ground | ≤ transmitted |
| frames_decoded | Decoder | ≤ received |

**Test Procedure**:
1. Stream 1080p@30fps for 30 seconds
2. Capture all counters every second
3. Verify relationships hold
4. Check for gaps (frame drops)
5. Validate monotonic increase

**Expected Results**:
- ✓ All counters monotonically increasing
- ✓ Relationships hold throughout test
- ✓ Frame gaps detected and recorded
- ✓ No counter overflow/wraparound
- ✓ Timestamps synchronized

**Pass Criteria**: Counter consistency maintained

---

### TEST_041: Bitrate Accuracy
**Category**: Statistics | **Duration**: 5 min | **Type**: Functional

**Bitrate Measurement Points**:

| Point | Expected (6Mbps) | Tolerance |
|-------|-----------------|-----------|
| Encoder output | 6.0 Mbps | ±10% |
| IPC transmission | 6.0 Mbps | ±10% |
| WiFi transmission | 5.5 Mbps | ±10% (with FEC) |
| Decoder input | 5.5 Mbps | ±10% |

**Test Procedure**:
1. Configure 1080p@30fps, 6Mbps
2. Run for 60 seconds
3. Measure bytes at each point
4. Calculate Mbps = bytes * 8 / (seconds * 1e6)
5. Compare all measurements

**Expected Results**:
- ✓ All measurements agree (±10%)
- ✓ Bitrate stable over time
- ✓ Variations correlate with frame type
- ✓ No unexplained bitrate increases

**Pass Criteria**: Bitrate measurements consistent

---

### TEST_042: Timestamp Accuracy
**Category**: Statistics | **Duration**: 5 min | **Type**: Functional

**Timestamp Properties**:

| Property | Expected | Tolerance |
|----------|----------|-----------|
| PTS monotonic | Always increasing | Zero violations |
| DTS ≤ PTS | Always true | Zero violations |
| PTS spacing | 33.3ms @ 30fps | ±5% |
| No resets | Continuous increase | Zero resets |

**Test Procedure**:
1. Encode 150 frames (5 seconds @ 30fps)
2. Check PTS of each NAL unit
3. Verify DTS ≤ PTS
4. Calculate PTS deltas
5. Verify monotonic progression

**Expected Results**:
- ✓ PTS monotonically increases
- ✓ No DTS > PTS violations
- ✓ PTS delta avg 33.3ms
- ✓ No timestamp resets
- ✓ Microsecond precision maintained

**Pass Criteria**: All timestamp properties valid

---

### TEST_043: Statistics Computation
**Category**: Statistics | **Duration**: 5 min | **Type**: Functional

**Computed Statistics**:

| Statistic | Formula | Tolerance |
|-----------|---------|-----------|
| Actual FPS | frame_count / duration | ±2% |
| Actual Bitrate | bytes * 8 / duration | ±10% |
| Keyframe Rate | keyframes / duration | ±5% |
| Avg Encode Time | sum_encode_time / frames | ±10% |

**Test Procedure**:
1. Stream for known duration (30s)
2. Capture manual measurements
3. Retrieve reported statistics
4. Compute expected values
5. Compare

**Expected Results**:
- ✓ FPS accuracy within ±2%
- ✓ Bitrate accuracy within ±10%
- ✓ All computed stats match manual calc
- ✓ No integer overflow effects
- ✓ Consistent across multiple runs

**Pass Criteria**: Computed statistics accurate

---

## Recovery & Robustness Tests

### TEST_050: Master Power Cycle Recovery
**Category**: Recovery | **Duration**: 10 min | **Type**: Robustness

**Test Sequence**:
1. Stream 1080p@30fps normally
2. Power cycle ESP32-P4
3. Measure boot time
4. Verify reinitialization sequence
5. Confirm video resumes

**Boot Sequence**:
1. Bootloader: ~500ms
2. ESP-IDF startup: ~1 second
3. Camera init: ~1 second
4. Encoder init: ~500ms
5. IPC handshake: ~1 second
6. Video resume: <1 second

**Expected Results**:
- ✓ Total recovery <5 seconds
- ✓ All components reinitialize correctly
- ✓ Video stream resumes automatically
- ✓ No permanent corruption
- ✓ Statistics reset properly

**Pass Criteria**: Full recovery within 5 seconds

---

### TEST_051: Slave Power Cycle Recovery
**Category**: Recovery | **Duration**: 10 min | **Type**: Robustness

**Test Sequence**:
1. Stream 1080p@30fps
2. Power cycle ESP32-C5/C6
3. Master detects loss
4. Slave reinitializes
5. Handshake restablishes
6. Stream resumes

**Expected Results**:
- ✓ Master detects slave loss within 3s
- ✓ Slave recovers within 3s
- ✓ IPC link re-establishes
- ✓ Video transmission resumes
- ✓ No frame duplication/loss

**Pass Criteria**: Automatic recovery within 3 seconds

---

### TEST_052: Ground Station Recovery
**Category**: Recovery | **Duration**: 5 min | **Type**: Robustness

**Test Sequence**:
1. Receive video normally
2. Stop ground station application
3. Master continues streaming
4. Restart ground station
5. Reconnect and resync

**Expected Results**:
- ✓ Reconnection within 3 seconds
- ✓ First frame displayed <1s after connect
- ✓ Audio/video sync recovered
- ✓ No stream artifacts
- ✓ Seamless resume

**Pass Criteria**: Automatic resync within 3 seconds

---

### TEST_053: Sustained Operation
**Category**: Recovery | **Duration**: 60 min | **Type**: Stress

**Test Parameters**:
- Duration: 1 hour continuous
- Resolution: 1080p @ 30fps
- Bitrate: 6 Mbps
- Monitoring: Frame drops, CPU, memory, temp

**Success Criteria**:
- ✓ Zero unrecovered frame drops
- ✓ CPU stable ±5%
- ✓ Memory stable (no leaks)
- ✓ Temperature <75°C
- ✓ No watchdog resets
- ✓ Log file clean (no errors)

**Pass Criteria**: 1 hour stable operation

---

### TEST_054: Thermal Management
**Category**: Recovery | **Duration**: 30 min | **Type**: Stress

**Test Conditions**:
- Ambient: 40-50°C
- Streaming: 1080p@60fps, 12Mbps
- Duration: 30 minutes
- Monitoring: Temp, throttling, quality

**Expected Results**:
- ✓ Chip temperature <80°C
- ✓ No thermal throttling
- ✓ Quality maintained throughout
- ✓ No performance drops
- ✓ Stable operation at high temp

**Pass Criteria**: No thermal issues

---

## System Integration Tests

### TEST_060: Complete Pipeline Initialization
**Category**: Integration | **Duration**: 5 min | **Type**: Functional

**Initialization Sequence**:
1. NVS flash init
2. IPC master init (wait for slave)
3. H.264 encoder init
4. MIPI camera init
5. Register callbacks
6. Start camera capture
7. Verify first NAL arrives
8. Connect to WiFi ground station

**Expected Results**:
- ✓ All components initialized <10 seconds
- ✓ Handshake succeeds
- ✓ First frame encoded within 3s
- ✓ WiFi connection established
- ✓ System ready for streaming

**Pass Criteria**: Complete init <10 seconds

---

### TEST_061: Full End-to-End Streaming
**Category**: Integration | **Duration**: 30 min | **Type**: Functional

**Test Matrix**:

| Resolution | FPS | Duration | Bitrate |
|------------|-----|----------|---------|
| 720p | 30 | 30s | 2 Mbps |
| 720p | 60 | 30s | 4 Mbps |
| 1080p | 30 | 30s | 6 Mbps |
| 1080p | 60 | 30s | 12 Mbps |

**Verification**:
1. Capture video at source (camera)
2. Watermark with timestamp
3. Transmit through full pipeline
4. Receive at ground station
5. Verify frame integrity
6. Measure latency
7. Check bitrate accuracy
8. Validate audio sync

**Expected Results**:
- ✓ All frames arrive intact
- ✓ Latency <200ms
- ✓ No frame drops
- ✓ Bitrate accurate
- ✓ No artifacts or distortion
- ✓ Proper color reproduction

**Pass Criteria**: E2E streaming successful

---

### TEST_062: Configuration Changes During Operation
**Category**: Integration | **Duration**: 10 min | **Type**: Functional

**Configuration Changes**:

| Change | From | To | Expected Behavior |
|--------|------|----|--------------------|
| Channel | 1 | 6 | Switch <1s, no loss |
| Bitrate | 6 | 4 | Lower quality, no loss |
| FEC | 4/6 | 8/12 | More redundancy |
| TX Power | 20 | 10 | RSSI drops, stable |

**Test Procedure**:
1. Stream normally for 10 seconds
2. Send configuration change
3. Monitor for frame loss
4. Verify new settings take effect
5. Continue streaming 10 seconds
6. Repeat for each change

**Expected Results**:
- ✓ Configuration applied <1 second
- ✓ No frame loss during change
- ✓ Stream continues uninterrupted
- ✓ New settings effective immediately
- ✓ No image artifacts

**Pass Criteria**: Config changes seamless

---

## Performance Benchmarks

### BENCH_001-003: Encoder Throughput
**Category**: Benchmark | **Duration**: 5 min | **Type**: Performance

**Scenarios**:
- 720p@30fps: 2.0 Mbps (baseline)
- 1080p@30fps: 6.0 Mbps (baseline)
- 1080p@60fps: 12.0 Mbps (baseline)

**Measurement**: Encode 300 frames, measure total bytes/duration

---

### BENCH_004: IPC Throughput
**Category**: Benchmark | **Duration**: 5 min | **Type**: Performance

**Measurement**: 50 MHz SPI link throughput
**Expected**: 35-42 Mbps (85-90% of theoretical)

---

### BENCH_005-006: Latency Analysis
**Category**: Benchmark | **Duration**: 10 min | **Type**: Performance

**Measure**:
- End-to-end latency
- Component breakdown
- Latency under load

---

### BENCH_007-010: Resource Profiling
**Category**: Benchmark | **Duration**: 10 min | **Type**: Performance

**Profile**:
- CPU usage per component
- Memory usage
- Packet efficiency
- Frame rate consistency

---

## Summary Statistics

**Total Test Cases**: 40+
**Total Duration**: ~2 hours (full suite)
**Quick Test Duration**: ~15 minutes
**Pass Rate Target**: ≥95%
**Critical Path**: 5-10 tests

---

**Last Updated**: 2024-11-22
**Version**: 1.0
**Status**: Ready for Use

# ESP32-S3 Android WiFi Receiver - Architecture Design Document

## Table of Contents

1. [Overview](#overview)
2. [System Architecture](#system-architecture)
3. [Component Design](#component-design)
4. [Data Flow](#data-flow)
5. [Memory Management](#memory-management)
6. [Performance Targets](#performance-targets)
7. [Implementation Details](#implementation-details)
8. [Testing Strategy](#testing-strategy)

## Overview

### Purpose

The ESP32-S3 Android WiFi Receiver firmware enables Android devices to receive and decode high-quality FPV video streams transmitted over WiFi. The ESP32-S3 acts as a WiFi-to-USB bridge, receiving packets in monitor mode and streaming decoded video to Android via USB.

### Key Features

- **WiFi Monitor Mode Reception**: Captures raw WiFi packets on 2.4GHz/5GHz bands
- **Forward Error Correction**: Reed-Solomon FEC for packet loss recovery
- **USB Streaming**: High-throughput USB CDC/Bulk transfer to Android
- **Frame Assembly**: Reassembles fragmented video frames with jitter buffering
- **Real-time Statistics**: Comprehensive performance monitoring and health tracking

### Design Goals

1. **Low Latency**: End-to-end latency < 50ms
2. **High Throughput**: Support up to 20 Mbps video streams
3. **Robustness**: Graceful degradation with packet loss up to 30%
4. **Efficiency**: Optimized memory and CPU usage
5. **Modularity**: Clean component interfaces for maintainability

## System Architecture

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    ESP32-S3 WiFi Receiver                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────┐      ┌──────────────┐      ┌───────────┐ │
│  │WiFi Receiver │─────▶│Packet Handler│─────▶│USB Stream │ │
│  │  (Monitor)   │      │ (Assembly)   │      │  (CDC)    │ │
│  └──────────────┘      └──────────────┘      └───────────┘ │
│         │                      │                    │       │
│         │                      ▼                    │       │
│         │              ┌──────────────┐             │       │
│         │              │ FEC Decoder  │             │       │
│         │              │ (Reed-Solomon)│            │       │
│         │              └──────────────┘             │       │
│         │                      │                    │       │
│         └──────────────────────┴────────────────────┘       │
│                                │                            │
│                        ┌───────▼────────┐                   │
│                        │ Stats Tracker  │                   │
│                        │  (Monitoring)  │                   │
│                        └────────────────┘                   │
│                                                              │
└─────────────────────────────────────────────────────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │Android Device│
                    │   (USB OTG)  │
                    └──────────────┘
```

### Component Interactions

```
WiFi Packet Flow:
  WiFi → wifi_receiver → packet_handler → (optional FEC) → usb_streamer → Android

Statistics Flow:
  All Components → stats_tracker → Monitoring/Logging

Control Flow:
  Android → USB RX → Control Commands → Component Configuration
```

## Component Design

### 1. WiFi Receiver (`wifi_receiver`)

**Responsibility**: Capture raw WiFi packets in monitor mode

**Key Functions**:
- Configure ESP32-S3 WiFi in monitor mode
- Set channel and reception parameters
- Filter packets by MAC address and CRC
- Provide RSSI and noise floor measurements
- Support channel hopping for scanning

**Implementation Details**:
- Uses ESP-IDF WiFi promiscuous mode callbacks
- Runs in WiFi RX task (high priority, IRAM)
- Zero-copy packet handling where possible
- Circular buffer for packet queue (128 packets default)

**Performance**:
- Target: Process 5000+ packets/sec
- Latency: < 1ms from WiFi RX to callback
- Memory: ~128KB for RX buffers

**API Highlights**:
```c
wifi_rx_init()          // Initialize with config
wifi_rx_start()         // Start reception
wifi_rx_set_channel()   // Change channel
wifi_rx_get_stats()     // Get statistics
```

### 2. FEC Decoder (`fec_decoder`)

**Responsibility**: Reed-Solomon erasure code decoding for packet loss recovery

**Key Functions**:
- Manage FEC blocks (k data + n-k redundancy packets)
- Reconstruct missing packets using redundancy
- Track block completion and timeouts
- Provide recovery statistics

**Implementation Details**:
- Uses Reed-Solomon (n,k) erasure coding (default 12,6)
- Hash table for pending block management
- Timeout-based block purging
- SIMD optimizations for Galois field arithmetic

**Performance**:
- Target: Decode 1000+ blocks/sec
- Latency: < 2ms per block decode
- Memory: ~256KB for block buffers
- Recovery: Up to 50% packet loss

**API Highlights**:
```c
fec_decoder_create()         // Create decoder instance
fec_decoder_process_packet() // Add packet to block
fec_decoder_flush_block()    // Force reconstruction
fec_decoder_get_stats()      // Get statistics
```

### 3. USB Streamer (`usb_streamer`)

**Responsibility**: Stream decoded video to Android via USB

**Key Functions**:
- USB CDC device class implementation
- High-speed bulk transfers
- Flow control and backpressure handling
- Multiple stream multiplexing (video, telemetry, control)

**Implementation Details**:
- Uses TinyUSB stack (ESP-IDF component)
- Double-buffered TX for continuous streaming
- DMA transfers for efficiency
- Detects USB connection/disconnection events

**Performance**:
- Target: 20+ Mbps sustained throughput
- Latency: < 5ms buffering
- Memory: ~128KB for TX/RX buffers
- USB 2.0 High Speed (480 Mbps physical)

**API Highlights**:
```c
usb_streamer_init()              // Initialize USB
usb_streamer_send()              // Send data (blocking)
usb_streamer_send_nonblocking()  // Send data (non-blocking)
usb_streamer_is_ready()          // Check USB status
```

### 4. Packet Handler (`packet_handler`)

**Responsibility**: Frame assembly from fragmented packets with jitter buffering

**Key Functions**:
- Parse packet headers and extract metadata
- Reassemble fragmented frames
- Reorder packets based on sequence numbers
- Manage jitter buffer for smooth playback
- Extract H.264 NAL units

**Implementation Details**:
- Ring buffer for pending frames (16 frames default)
- Bitmap tracking for received fragments
- Timeout-based incomplete frame handling
- Priority queue for frame ordering

**Performance**:
- Target: Assemble 60+ frames/sec
- Latency: < 10ms jitter buffer delay
- Memory: ~1MB for frame buffers
- Max frame size: 64KB

**API Highlights**:
```c
packet_handler_create()      // Create handler
packet_handler_process()     // Process packet
packet_handler_flush()       // Flush pending frames
packet_handler_get_stats()   // Get statistics
```

### 5. Stats Tracker (`stats_tracker`)

**Responsibility**: System-wide performance monitoring and health tracking

**Key Functions**:
- Aggregate statistics from all components
- Monitor CPU and memory usage
- Calculate throughput and latency metrics
- Detect anomalies and health issues
- Export statistics (JSON, logging)

**Implementation Details**:
- Periodic sampling task (1 second interval)
- Lock-free counters for high-frequency updates
- Exponential moving averages for smoothing
- Threshold-based health classification

**Performance**:
- Overhead: < 1% CPU usage
- Memory: ~32KB for statistics buffers
- Update rate: 1 Hz

**API Highlights**:
```c
stats_tracker_init()          // Initialize tracker
stats_tracker_get_stats()     // Get all statistics
stats_tracker_record_latency() // Record latency
stats_tracker_print_summary()  // Print to console
```

## Data Flow

### Packet Reception Flow

```
1. WiFi Packet Arrives
   ├─ ESP32 WiFi hardware receives packet
   ├─ WiFi driver validates packet (CRC, MAC filter)
   └─ Promiscuous callback invoked (IRAM)

2. WiFi Receiver Processing
   ├─ Extract packet metadata (RSSI, timestamp, etc.)
   ├─ Apply additional filtering
   ├─ Queue packet in RX buffer
   └─ Invoke user callback

3. Packet Handler Processing
   ├─ Parse packet header
   ├─ Validate sequence number
   ├─ Add to appropriate frame buffer
   └─ Check if frame complete

4. (Optional) FEC Decoding
   ├─ Add packet to FEC block
   ├─ Check if k packets received
   ├─ Reconstruct missing packets
   └─ Forward complete block

5. Frame Assembly
   ├─ Collect all fragments
   ├─ Verify frame integrity
   ├─ Extract H.264 NAL unit
   └─ Invoke frame ready callback

6. USB Transmission
   ├─ Check USB connection status
   ├─ Apply flow control
   ├─ Queue data in TX buffer
   ├─ Initiate USB transfer
   └─ Update statistics

7. Statistics Update
   ├─ Record packet/frame counts
   ├─ Calculate throughput
   ├─ Measure latency
   └─ Update health status
```

### Control Flow (Android → ESP32)

```
1. Android sends control command via USB
2. USB RX callback receives data
3. Parse command (channel change, config update, etc.)
4. Apply configuration to appropriate component
5. Send acknowledgment/response to Android
```

## Memory Management

### Memory Layout

**Total RAM**: 512 KB (ESP32-S3)
**SPIRAM**: 2 MB (optional, recommended for buffering)

**Memory Allocation**:

| Component | Internal RAM | SPIRAM | Purpose |
|-----------|-------------|--------|---------|
| WiFi Stack | 80 KB | - | WiFi driver, buffers |
| WiFi RX Buffers | 128 KB | - | High-priority RX queue |
| Packet Handler | 64 KB | 1 MB | Frame assembly buffers |
| FEC Decoder | 32 KB | 256 KB | FEC block reconstruction |
| USB Stack | 64 KB | - | TinyUSB, TX/RX buffers |
| USB TX Buffer | - | 128 KB | USB streaming buffer |
| Stats Tracker | 32 KB | - | Statistics storage |
| FreeRTOS | 64 KB | - | Task stacks, kernel |
| **Total** | **464 KB** | **~1.4 MB** | |

### Buffer Management Strategy

**WiFi RX Buffer**: Circular buffer, fixed-size slots
- 128 packets × 1500 bytes = ~192 KB
- Implemented in internal RAM for speed
- Overflow handling: drop oldest packets

**Frame Assembly Buffer**: Dynamic allocation from SPIRAM
- 16 frame slots × 64 KB max = 1 MB
- Reference counting for memory reuse
- Timeout-based garbage collection

**USB TX Buffer**: Double-buffered DMA
- 2 × 64 KB = 128 KB
- Ping-pong buffering for continuous streaming
- Backpressure signaling when full

**FEC Block Buffer**: Hash table with fixed-size blocks
- 8 concurrent blocks × 32 KB = 256 KB
- Pre-allocated block pool
- LRU eviction policy

### Memory Optimization Techniques

1. **Zero-Copy Paths**: Minimize data copying between components
2. **DMA Transfers**: Use DMA for WiFi and USB where possible
3. **SPIRAM Offloading**: Store large buffers in external RAM
4. **Reference Counting**: Share buffers between components
5. **Pool Allocation**: Pre-allocate fixed-size buffers

## Performance Targets

### Latency Targets

| Stage | Target Latency | Budget |
|-------|---------------|--------|
| WiFi RX | < 1 ms | 2% |
| Packet Handler | < 5 ms | 10% |
| FEC Decode | < 2 ms | 4% |
| Frame Assembly | < 10 ms | 20% |
| USB TX | < 5 ms | 10% |
| Jitter Buffer | < 20 ms | 40% |
| Other | < 7 ms | 14% |
| **Total** | **< 50 ms** | **100%** |

### Throughput Targets

- **WiFi RX**: 25 Mbps (burst), 20 Mbps (sustained)
- **FEC Decode**: 30 Mbps processing capability
- **USB TX**: 25 Mbps (limited by USB 2.0 overhead)
- **End-to-End**: 20 Mbps sustained video stream

### Packet Loss Tolerance

- **0-10% Loss**: Full quality, no FEC needed
- **10-30% Loss**: FEC recovery, minimal quality impact
- **30-50% Loss**: Degraded quality, some frame drops
- **> 50% Loss**: Severe degradation, consider fallback

### Resource Utilization

- **CPU Usage**: < 80% average (dual-core)
- **Memory Usage**: < 90% of available RAM
- **WiFi Buffer**: < 80% utilization
- **USB Buffer**: < 70% utilization

## Implementation Details

### Task Architecture

```
Priority Levels (0 = lowest, 24 = highest):

24 - WiFi RX ISR (IRAM)
23 - WiFi RX Task (ESP-IDF)
20 - Packet Handler Task
18 - FEC Decoder Task
18 - USB TX Task
15 - Frame Assembly Task
10 - Stats Monitor Task
5  - Idle Tasks
```

### Critical Paths (IRAM Optimization)

Components/functions placed in IRAM for fast execution:
- WiFi RX callback
- Packet queue operations
- FEC hot paths (Galois field arithmetic)
- USB DMA setup

### Interrupt Handling

- **WiFi RX**: Handled by ESP-IDF WiFi driver
- **USB**: TinyUSB handles USB interrupts
- **Timers**: ESP Timer for periodic tasks

### Error Handling Strategy

1. **Packet-Level Errors**: Drop and log, rely on FEC
2. **Frame-Level Errors**: Mark incomplete, timeout recovery
3. **USB Errors**: Retry with exponential backoff
4. **Memory Errors**: Graceful degradation, log critical errors
5. **Fatal Errors**: System restart with error logging

### Configuration Parameters

Default configuration values (tunable via sdkconfig):

```c
// WiFi Configuration
#define WIFI_RX_BUFFER_COUNT 128
#define WIFI_CHANNEL_DEFAULT 6
#define WIFI_PROMISCUOUS_ENABLED true

// FEC Configuration
#define FEC_K 6    // Data blocks
#define FEC_N 12   // Total blocks (50% redundancy)
#define FEC_MAX_BLOCKS 8

// Packet Handler Configuration
#define JITTER_BUFFER_FRAMES 16
#define MAX_FRAME_SIZE (64 * 1024)
#define FRAME_TIMEOUT_MS 100

// USB Configuration
#define USB_TX_BUFFER_SIZE (64 * 1024)
#define USB_RX_BUFFER_SIZE (4 * 1024)
#define USB_MODE USB_MODE_CDC

// Stats Configuration
#define STATS_UPDATE_INTERVAL_MS 1000
```

### Packet Format Specification

**WiFi Packet Structure**:
```
┌──────────────────────────────────────────┐
│ 802.11 Header (24-30 bytes)              │
├──────────────────────────────────────────┤
│ Custom Payload:                          │
│  ┌────────────────────────────────────┐  │
│  │ Packet Header (20 bytes)           │  │
│  │  - Type (1 byte)                   │  │
│  │  - Sequence Number (4 bytes)       │  │
│  │  - Frame ID (4 bytes)              │  │
│  │  - Fragment Index (2 bytes)        │  │
│  │  - Total Fragments (2 bytes)       │  │
│  │  - Timestamp (8 bytes)             │  │
│  │  - Payload Size (2 bytes)          │  │
│  │  - Flags (1 byte)                  │  │
│  ├────────────────────────────────────┤  │
│  │ Payload Data (1-1400 bytes)        │  │
│  └────────────────────────────────────┘  │
├──────────────────────────────────────────┤
│ FCS (4 bytes)                            │
└──────────────────────────────────────────┘
```

**Packet Types**:
- 0x00: Video data
- 0x01: Telemetry
- 0x02: Control
- 0x03: Sync/heartbeat

**Flags**:
- Bit 0: Keyframe indicator
- Bit 1: FEC enabled
- Bit 2: Last fragment
- Bits 3-7: Reserved

## Testing Strategy

### Unit Tests

Each component will have comprehensive unit tests covering:
- Normal operation
- Edge cases (buffer full, timeouts, etc.)
- Error handling
- Performance benchmarks

### Integration Tests

Test component interactions:
- WiFi RX → Packet Handler flow
- Packet Handler → USB TX flow
- FEC Decoder integration
- Statistics aggregation

### Performance Tests

Measure and validate:
- Latency at each stage
- Throughput under various conditions
- Memory usage patterns
- CPU utilization
- Packet loss recovery effectiveness

### Field Tests

Real-world testing scenarios:
- Indoor/outdoor range tests
- Various packet loss conditions
- Different WiFi environments (congested, clean)
- Extended duration testing (thermal, stability)
- Multiple Android device compatibility

### Debugging and Diagnostics

Built-in debugging features:
- Verbose logging (compile-time configurable)
- Real-time statistics export
- Packet capture capability
- Performance profiling hooks
- Memory leak detection

## Future Enhancements

### Phase 1 (Core Functionality)
- ✅ Basic WiFi reception
- ✅ Frame assembly
- ✅ USB streaming
- ✅ Statistics tracking

### Phase 2 (Optimization)
- Advanced FEC algorithms (Raptor codes)
- Adaptive bitrate control
- Dynamic channel selection
- Power optimization modes

### Phase 3 (Features)
- Multi-antenna diversity
- Hardware-accelerated video decoding
- Encrypted video streams
- Over-the-air firmware updates

### Phase 4 (Advanced)
- AI-based packet loss prediction
- Mesh network support
- Multiple camera streams
- Cloud telemetry integration

## References

- [ESP32-S3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-s3_technical_reference_manual_en.pdf)
- [ESP-IDF WiFi Driver Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/network/esp_wifi.html)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [Reed-Solomon Error Correction](https://en.wikipedia.org/wiki/Reed%E2%80%93Solomon_error_correction)
- [H.264 Video Coding Standard](https://www.itu.int/rec/T-REC-H.264)

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-11-22 | System | Initial architecture design |

---

**Document Status**: Draft for Review
**Next Review Date**: TBD
**Owner**: Firmware Team

# USB OTG Streaming Protocol Specification

**Version:** 1.0
**Date:** 2025-11-22
**Target:** ESP32-S3 to Android USB OTG Communication

## 1. Overview

This document specifies the USB streaming protocol for high-throughput video and telemetry transmission from ESP32-S3 to Android devices via USB OTG connection. The protocol is designed for low-latency H.264 video streaming with metadata transmission and bidirectional control commands.

### 1.1 Design Goals

- **High Throughput:** Support >20 Mbps sustained video streaming
- **Low Latency:** Minimize protocol overhead (target <5ms protocol latency)
- **Reliability:** CRC16 error detection on all packets
- **Flexibility:** Support multiple stream types (video, telemetry, control, debug)
- **Efficiency:** Zero-copy where possible, minimal packet fragmentation

### 1.2 Transport Layers

The protocol supports two USB transport modes:

1. **CDC ACM Mode (Serial):**
   - Standard USB CDC device class
   - Maximum compatibility with Android devices
   - Throughput: ~12-20 Mbps
   - No special permissions required on Android 6.0+

2. **Bulk Transfer Mode:**
   - Higher performance custom USB bulk endpoints
   - Throughput: >40 Mbps
   - Requires custom USB driver or libusb on Android

**Default:** CDC ACM mode for maximum compatibility.

## 2. Packet Structure

### 2.1 Base Packet Format

All packets follow this structure:

```
+--------+--------+--------+--------+--------+--------+--------+--------+
|  SYNC  |  SYNC  |  TYPE  | FLAGS  |  SIZE  |  SIZE  |  SEQ   |  SEQ   |
|  0xA5  |  0x5A  | (1byte)| (1byte)| (LSB)  | (MSB)  | (LSB)  | (MSB)  |
+--------+--------+--------+--------+--------+--------+--------+--------+
|             TIMESTAMP (4 bytes, little-endian)                         |
+--------+--------+--------+--------+--------+--------+--------+--------+
|                   PAYLOAD (variable length)                            |
|                         (0 to 16384 bytes)                             |
+--------+--------+--------+--------+--------+--------+--------+--------+
|  CRC16 |  CRC16 |
|  (LSB) |  (MSB) |
+--------+--------+
```

**Total Header Size:** 12 bytes

### 2.2 Field Descriptions

| Field | Size | Description |
|-------|------|-------------|
| SYNC | 2 bytes | Synchronization marker: 0xA55A (little-endian) |
| TYPE | 1 byte | Packet type (see section 2.3) |
| FLAGS | 1 byte | Packet flags (see section 2.4) |
| SIZE | 2 bytes | Payload size in bytes (0-16384), little-endian |
| SEQ | 2 bytes | Sequence number (0-65535, wraps around), little-endian |
| TIMESTAMP | 4 bytes | Timestamp in microseconds (wraps at ~71 minutes), little-endian |
| PAYLOAD | variable | Packet payload (type-specific) |
| CRC16 | 2 bytes | CRC16-CCITT of entire packet (sync to end of payload), little-endian |

### 2.3 Packet Types

| Type | Value | Direction | Description |
|------|-------|-----------|-------------|
| VIDEO | 0x01 | ESP32→Android | H.264 NAL unit video data |
| METADATA | 0x02 | ESP32→Android | Telemetry and statistics |
| CONTROL | 0x03 | Android→ESP32 | Control commands |
| ACK | 0x04 | Bidirectional | Acknowledgment |
| HEARTBEAT | 0x05 | Bidirectional | Keep-alive ping |
| DEBUG | 0x06 | ESP32→Android | Debug log messages |

### 2.4 Packet Flags

| Bit | Name | Description |
|-----|------|-------------|
| 0 | FRAGMENT | Packet is part of a fragmented message |
| 1 | LAST_FRAGMENT | This is the last fragment |
| 2 | PRIORITY_HIGH | High priority packet (process immediately) |
| 3 | REQUIRE_ACK | Acknowledgment required |
| 4-7 | Reserved | Reserved for future use (must be 0) |

## 3. Stream Types

### 3.1 VIDEO Stream (Type 0x01)

Video stream carries H.264 NAL units with framing information.

**Payload Format:**
```
+--------+--------+--------+--------+
| NAL_TYPE| NAL_SIZE (3 bytes)      |
+--------+--------+--------+--------+
|     H.264 NAL Unit Data           |
|        (variable length)          |
+--------+--------+--------+--------+
```

**NAL Unit Types:**
- `0x05` - IDR frame (I-frame, keyframe)
- `0x01` - Non-IDR frame (P-frame)
- `0x07` - SPS (Sequence Parameter Set)
- `0x08` - PPS (Picture Parameter Set)

**Fragmentation:**
NAL units larger than 16KB are fragmented across multiple packets:
- First fragment: FLAGS = 0x01 (FRAGMENT)
- Middle fragments: FLAGS = 0x01 (FRAGMENT)
- Last fragment: FLAGS = 0x03 (FRAGMENT | LAST_FRAGMENT)

**Example:** 30fps H.264 stream at 8 Mbps
```
Frame 0 (IDR): SPS → PPS → NAL(0x05, 24KB) [fragmented into 2 packets]
Frame 1-29 (P): NAL(0x01, 8KB)
Frame 30 (IDR): NAL(0x05, 24KB)
...
```

### 3.2 METADATA Stream (Type 0x02)

Metadata carries telemetry, statistics, and system information.

**Payload Format:**
```
+--------+--------+--------+--------+--------+--------+
| RSSI   | FPS    | FPS    | BITRATE (4 bytes)        |
| (int8) | (LSB)  | (MSB)  | (little-endian)          |
+--------+--------+--------+--------+--------+--------+
|        FRAME_COUNT (4 bytes)                        |
+--------+--------+--------+--------+--------+--------+
|        BYTES_TRANSMITTED (8 bytes)                  |
|                (little-endian)                      |
+--------+--------+--------+--------+--------+--------+
| BUFFER | TEMP   | RESERVED                           |
| USAGE  | (°C)   |                                    |
+--------+--------+--------+--------+--------+--------+
```

**Field Descriptions:**
- **RSSI:** WiFi RSSI in dBm (-128 to 0, signed 8-bit)
- **FPS:** Current frame rate × 10 (e.g., 300 = 30.0 fps)
- **BITRATE:** Current encoding bitrate in kbps (32-bit)
- **FRAME_COUNT:** Total frames transmitted since boot
- **BYTES_TRANSMITTED:** Total bytes transmitted
- **BUFFER_USAGE:** TX buffer usage percentage (0-100)
- **TEMP:** ESP32 temperature in Celsius (optional)

**Update Rate:** Every 1 second or every 30 frames (whichever is first)

### 3.3 CONTROL Stream (Type 0x03)

Control commands from Android to ESP32.

**Payload Format:**
```
+--------+--------+--------+--------+--------+--------+
|COMMAND | STATUS | PARAM1 (2 bytes)| PARAM2 (4 bytes)|
| (1byte)| (1byte)| (little-endian) | (little-endian) |
+--------+--------+--------+--------+--------+--------+
```

**Command Types:**

| Command | Value | PARAM1 | PARAM2 | Description |
|---------|-------|--------|--------|-------------|
| START_STREAM | 0x01 | Resolution | FPS | Start video streaming |
| STOP_STREAM | 0x02 | - | - | Stop video streaming |
| SET_BITRATE | 0x03 | - | Bitrate (kbps) | Set encoding bitrate |
| SET_FPS | 0x04 | FPS × 10 | - | Set target frame rate |
| REQUEST_IDR | 0x05 | - | - | Request IDR frame |
| GET_STATUS | 0x06 | - | - | Request status (sends METADATA) |
| SET_QUALITY | 0x07 | Quality (0-100) | - | Set encoder quality |
| RESET | 0x08 | - | - | Reset encoder state |

**Resolution Encoding (PARAM1 for START_STREAM):**
- `0x01` - 640×480
- `0x02` - 800×600
- `0x03` - 1024×768
- `0x04` - 1280×720
- `0x05` - 1920×1080

### 3.4 ACK Stream (Type 0x04)

Acknowledgment for packets with REQUIRE_ACK flag.

**Payload Format:**
```
+--------+--------+--------+
| ACK_SEQ (2 bytes)| STATUS |
| (little-endian)  | (1byte)|
+--------+--------+--------+
```

**Status Codes:**
- `0x00` - Success
- `0x01` - CRC error
- `0x02` - Invalid packet
- `0x03` - Buffer overflow
- `0xFF` - General error

### 3.5 HEARTBEAT Stream (Type 0x05)

Keep-alive packets to maintain connection.

**Payload Format:**
```
+--------+--------+--------+--------+
| UPTIME (4 bytes, seconds)         |
+--------+--------+--------+--------+
```

**Interval:** Every 2 seconds (configurable)

### 3.6 DEBUG Stream (Type 0x06)

Debug log messages for development.

**Payload Format:**
```
+--------+--------+--------+--------+
| LEVEL  | LOG MESSAGE (null-terminated string) |
| (1byte)|                                      |
+--------+--------+--------+--------+--------+--+
```

**Log Levels:**
- `0` - Error
- `1` - Warning
- `2` - Info
- `3` - Debug
- `4` - Verbose

## 4. CRC Calculation

**Algorithm:** CRC16-CCITT
**Polynomial:** 0x1021
**Initial Value:** 0xFFFF
**Final XOR:** None

**C Implementation:**
```c
uint16_t crc16_ccitt(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc;
}
```

## 5. Flow Control

### 5.1 Buffer Management

- **ESP32 TX Buffer:** 128KB ring buffer
- **Android RX Buffer:** Recommended 256KB minimum
- **Backpressure:** When buffer >80% full, pause video transmission

### 5.2 Flow Control Signals

Android can send CONTROL commands to manage flow:
- `PAUSE_STREAM` (0x10) - Pause video temporarily
- `RESUME_STREAM` (0x11) - Resume video
- `REDUCE_BITRATE` (0x12) - Request lower bitrate

## 6. Connection Lifecycle

### 6.1 Connection Establishment

```
Android                                ESP32-S3
  |                                        |
  |--- USB Enumeration ------------------>|
  |<-- USB Descriptor ---------------------|
  |                                        |
  |--- GET_STATUS (CONTROL) ------------->|
  |<-- METADATA (status response) ---------|
  |                                        |
  |--- START_STREAM (CONTROL) ----------->|
  |<-- ACK ---------------------------------|
  |<-- SPS (VIDEO) ------------------------|
  |<-- PPS (VIDEO) ------------------------|
  |<-- NAL (VIDEO, IDR) -------------------|
  |<-- NAL (VIDEO, P-frame) ---------------|
  |    ...                                 |
```

### 6.2 Disconnection

```
Android                                ESP32-S3
  |                                        |
  |--- STOP_STREAM (CONTROL) ------------>|
  |<-- ACK ---------------------------------|
  |                                        |
  |--- USB Disconnect -------------------->|
```

### 6.3 Reconnection Handling

On USB disconnection:
1. ESP32 stops transmission immediately
2. Buffers are flushed
3. Statistics are preserved
4. Auto-reconnect when USB is re-established

## 7. Performance Specifications

### 7.1 Throughput

| Mode | Theoretical Max | Typical Sustained |
|------|-----------------|-------------------|
| CDC ACM | 12 Mbps | 8-10 Mbps |
| Bulk Transfer | 60 Mbps | 30-40 Mbps |

### 7.2 Latency

| Component | Latency |
|-----------|---------|
| USB transmission | 1-2 ms |
| Protocol overhead | <1 ms |
| Buffering | 2-5 ms |
| **Total** | **<10 ms** |

### 7.3 Packet Rate

- Video packets: 30-60 Hz (depends on frame rate)
- Metadata: 1 Hz
- Heartbeat: 0.5 Hz
- Control: As needed (low frequency)

## 8. Error Handling

### 8.1 CRC Error

1. Receiver detects CRC mismatch
2. If REQUIRE_ACK flag set, send NACK with ACK_SEQ
3. Sender retransmits packet (up to 3 retries)
4. If retries exhausted, log error and continue

### 8.2 Buffer Overflow

1. Sender detects buffer full
2. Drop oldest video P-frames (preserve I-frames, SPS, PPS)
3. Send METADATA with buffer overflow flag
4. Android may reduce bitrate or FPS

### 8.3 Sequence Gaps

1. Receiver detects missing sequence numbers
2. Request retransmission via CONTROL command (optional)
3. Or continue with next packet (video can tolerate some loss)

## 9. Security Considerations

### 9.1 No Encryption

This protocol does **not** include encryption. For sensitive applications:
- Implement TLS layer on top of USB transport
- Use Android's VPN API to encrypt payload
- Add HMAC authentication to packets

### 9.2 Input Validation

Android app **must** validate:
- Packet sync markers
- Payload sizes (prevent buffer overflows)
- CRC checksums
- Sequence number ranges

## 10. Implementation Notes

### 10.1 ESP32-S3 Considerations

- Use TinyUSB library for USB stack
- Allocate TX buffer in PSRAM (large buffers)
- Use DMA for USB transfers (zero-copy)
- Run USB task on dedicated core for real-time performance

### 10.2 Android Considerations

- Use UsbManager for USB CDC communication
- Allocate sufficiently large read buffer (>16KB)
- Use separate thread for USB I/O
- Implement packet parser with state machine
- Handle partial packet reads (USB may fragment packets)

## 11. Example Packet Traces

### 11.1 Heartbeat Packet

```
A5 5A 05 00 04 00 01 00 | 40 42 0F 00 78 56 34 12 | D4 3A
│  │  │  │  │  │  │  │    │  │  │  │  │  │  │  │    │  │
│  │  │  │  │  │  │  │    │  │  │  │  └─────────┘    └──┘
│  │  │  │  │  │  │  │    │  │  │  └─ Uptime (305,419,896s)
│  │  │  │  │  │  │  │    └──┴──┴─ Timestamp
│  │  │  │  │  │  └──┘
│  │  │  │  │  └─ SIZE = 4 bytes
│  │  │  │  └─ FLAGS = 0
│  │  │  └─ TYPE = HEARTBEAT
│  └──┘
└─ SYNC = 0xA55A
                                      CRC16 = 0x3AD4
```

### 11.2 Video Packet (NAL Unit)

```
A5 5A 01 00 10 00 02 00 | 80 96 98 00 05 00 10 00 | ...NAL data... | E2 7C
│  │  │  │  │  │  │  │    │  │  │  │  │  │  │  │                     │  │
│  │  │  │  │  │  │  │    │  │  │  │  │  └──┴──┘                     └──┘
│  │  │  │  │  │  │  │    │  │  │  │  └─ NAL size = 16 bytes
│  │  │  │  │  │  │  │    │  │  │  └─ NAL type = IDR (0x05)
│  │  │  │  │  │  │  │    └──┴──┴─ Timestamp = 10,000,000 µs
│  │  │  │  │  └──┘
│  │  │  │  └─ SIZE = 16 bytes (header+data)
│  │  │  └─ FLAGS = 0
│  │  └─ TYPE = VIDEO
│  └─ SYNC
                                                          CRC16 = 0x7CE2
```

## 12. Version History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2025-11-22 | Initial specification |

## 13. References

- [USB CDC Class Specification](https://www.usb.org/document-library/class-definitions-communication-devices-12)
- [H.264/AVC Standard](https://www.itu.int/rec/T-REC-H.264)
- [CRC16-CCITT](https://en.wikipedia.org/wiki/Cyclic_redundancy_check)
- [TinyUSB Documentation](https://docs.tinyusb.org/)
- [Android USB Host API](https://developer.android.com/guide/topics/connectivity/usb/host)

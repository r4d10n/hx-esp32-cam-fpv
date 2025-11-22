# Protocol Specification

## Overview

The hx-esp32-cam-fpv communication protocol is a custom bidirectional protocol built on top of WiFi 802.11 packet injection. It uses Forward Error Correction (FEC) to ensure reliable data delivery without requiring ACKs or retransmissions.

## Protocol Stack

```
┌─────────────────────────────────────┐
│   Application Layer                 │
│   (Video, Telemetry, Config, OSD)   │
├─────────────────────────────────────┤
│   FEC Layer                         │
│   (Reed-Solomon K/N encoding)       │
├─────────────────────────────────────┤
│   Packet Layer                      │
│   (Framing, CRC, Device ID)         │
├─────────────────────────────────────┤
│   WiFi Layer                        │
│   (802.11 Packet Injection)         │
└─────────────────────────────────────┘
```

## WiFi Layer

### Packet Injection

The system uses **WiFi monitor mode** with **packet injection** instead of traditional WiFi association:

**Advantages:**
- No association handshake delay
- No disconnection issues
- Works on any channel
- Multiple receivers can listen (broadcast)
- Lower latency

**Implementation:**
- Air unit: ESP32 WiFi driver in STA mode with raw packet injection
- Ground station: RTL8812AU/AR9271 in monitor mode
- No encryption (for lowest latency)

### WiFi Rates

Supported WiFi rates (from `packets.h`):

| Mode | Rate | Description |
|------|------|-------------|
| CCK | 2M, 5.5M, 11M | Legacy 802.11b |
| OFDM | 6M, 9M, 12M, 18M, 24M, 36M, 48M, 54M | 802.11g |
| MCS | MCS0-MCS7 | 802.11n (6.5M - 72M) |

**Default:** MCS3 (26Mbps nominal, ~10Mbps effective with FEC 6/12)

### Channel Selection

- **Default channel:** 7 (2.442 GHz)
- **Recommended:** Channels 3-7 (antenna tuned for mid-range)
- **Configurable:** 1-14 (region dependent)
- **Consideration:** Choose channel with least interference

## Packet Layer

### Packet Header Structure

All packets transmitted over WiFi include a common header (from `fec.h`):

```c
#pragma pack(push, 1)
struct Packet_Header {
    uint8_t  packet_version;      // PACKET_VERSION (2)
    uint8_t  packet_signature;    // PACKET_SIGNATURE (56)
    uint16_t fromDeviceId;        // Sender device ID
    uint16_t toDeviceId;          // Recipient device ID
    uint16_t size;                // Payload size
    uint32_t block_index : 24;    // FEC block number
    uint32_t packet_index : 8;    // Packet index within block
};
#pragma pack(pop)
// Size: 12 bytes (PACKET_OVERHEAD)
```

**Field Descriptions:**

- `packet_version`: Protocol version (currently 2)
- `packet_signature`: Magic number for packet identification (56)
- `fromDeviceId`: Unique ID of sender (assigned at boot)
- `toDeviceId`: Target device ID (0 = broadcast)
- `size`: Size of FEC-encoded payload
- `block_index`: Sequential FEC block number (wraps at 16M)
- `packet_index`: Index within FEC block (0 to N-1)

### Device ID Assignment

- **Air Unit:** Randomly generated on first boot, stored in NVS
- **Ground Station:** Randomly generated on first boot, stored in config file
- **Purpose:**
  - Pairing (air unit only responds to connected GS)
  - Multi-unit discrimination
  - Packet filtering

## FEC Layer

### Reed-Solomon Forward Error Correction

**Default Parameters:**
- K = 6 (data packets)
- N = 12 (total packets including parity)
- Overhead: 100% (doubles bandwidth)
- Recovery: Can lose up to 6 of 12 packets

**Configurable Options:**
- FEC 6/8 (33% overhead)
- FEC 6/10 (66% overhead)
- FEC 6/12 (100% overhead) - **Default**

### FEC Block Structure

```
Block 0:
┌──────────┬──────────┬──────────┬─────┬──────────┬──────────┬─────┬──────────┐
│ Packet 0 │ Packet 1 │ Packet 2 │ ... │ Packet 5 │ Parity 6 │ ... │ Parity 11│
│  (Data)  │  (Data)  │  (Data)  │     │  (Data)  │  (FEC)   │     │  (FEC)   │
└──────────┴──────────┴──────────┴─────┴──────────┴──────────┴─────┴──────────┘
 ◄────────────── K=6 data packets ──────────────►◄───── N-K=6 parity ────────►
```

**Encoding Process:**

1. Application data split into chunks
2. Each chunk becomes a data packet (max K packets)
3. FEC algorithm generates N-K parity packets
4. All N packets transmitted
5. Receiver needs any K of N packets to reconstruct

**Decoder Behavior:**

```
Received packets: [0, 1, _, 3, _, 5, 6, 7, _, 9, _, 11]
                   ◄──── 8 of 12 packets received ────►

Since 8 > K(6): ✓ Can reconstruct all data
Missing packets 2, 4, 8, 10 recovered using parity packets
```

### MTU Configuration

```
WLAN_MAX_PAYLOAD_SIZE = 1500 bytes (WiFi MTU)
PACKET_OVERHEAD = 12 bytes (Packet_Header)
AIR2GROUND_MTU = 1500 - 12 = 1488 bytes
GROUND2AIR_MTU = 64 bytes (smaller for config/telemetry)
```

## Application Layer

### Packet Types

#### Air-to-Ground Packets

```c
enum class Air2Ground_Header::Type : uint8_t {
    Video,      // MJPEG frame data
    Telemetry,  // Mavlink/MSP telemetry
    OSD,        // OSD buffer + stats
    Config      // Current configuration echo
};
```

#### Ground-to-Air Packets

```c
enum class Ground2Air_Header::Type : uint8_t {
    Telemetry,  // Mavlink RC commands
    Config,     // Configuration updates
    Connect     // Initial pairing request
};
```

### Connection Establishment

```
Time
  │
  │  [Ground Station Powers On]
  │
  ▼
  │  GS sends Connect Packet
  │  ├─ gsDeviceId: <random_id>
  │  ├─ airDeviceId: 0 (broadcast)
  │  └─ Repeats every 100ms
  │
  ▼
  │  [Air Unit receives Connect Packet]
  │
  │  Air Unit responds with Video/OSD packets
  │  ├─ airDeviceId: <air_id>
  │  ├─ gsDeviceId: <gs_id> (from Connect)
  │  └─ Connection established
  │
  ▼
  │  GS switches to Config Packet
  │  ├─ Stops sending Connect
  │  ├─ Sends configuration
  │  └─ Includes camera/WiFi settings
  │
  ▼
  │  Normal bidirectional communication
```

**Pairing Mechanism:**

1. Air unit boots with `gsDeviceId = 0` (unpaired)
2. GS sends Connect packets with its ID
3. Air unit accepts first GS and sets `gsDeviceId`
4. Air unit only processes packets from paired GS
5. Pairing persists until air unit reboot

### Packet Structures

#### Video Packet (Air → Ground)

```c
struct Air2Ground_Video_Packet {
    // Header (12 bytes)
    Type type = Type::Video;
    uint32_t size;
    uint8_t pong;              // Latency measurement
    uint8_t version;
    uint8_t crc;
    uint16_t airDeviceId;
    uint16_t gsDeviceId;

    // Video-specific (6 bytes)
    Resolution resolution;     // Enum
    uint8_t part_index : 7;    // Part number within frame
    uint8_t last_part : 1;     // Last part flag
    uint32_t frame_index;      // Monotonic frame counter

    // JPEG data follows (size bytes)
};
// Header size: 18 bytes
// Max payload: AIR2GROUND_MTU - 18 = 1470 bytes
```

**Multi-Part Frames:**

Large JPEG frames are split across multiple packets:

```
Frame 123 (3200 bytes) split into 3 parts:

Packet 1: part_index=0, last_part=0, data[0:1470]
Packet 2: part_index=1, last_part=0, data[1470:2940]
Packet 3: part_index=2, last_part=1, data[2940:3200]
```

#### OSD Packet (Air → Ground)

```c
struct Air2Ground_OSD_Packet {
    Air2Ground_Header header;
    AirStats stats;           // 30+ bytes of statistics
    OSDBuffer buffer;         // 53x20 character grid
};

struct OSDBuffer {
    uint8_t screenLow[20][53];   // Low byte of character
    uint8_t screenHigh[20][7];   // High byte (packed)
};
```

**OSD Update Frequency:** ~1 Hz (sent every second)

**Statistics Included:**
- WiFi RSSI, queue depth
- Frame rate, resolution
- Temperature, SD card status
- Telemetry rates
- Recording status

#### Config Packet (Ground → Air)

```c
struct Ground2Air_Config_Packet {
    Ground2Air_Header header;
    uint8_t ping;             // Latency measurement

    CameraConfig camera;      // Camera settings
    DataChannelConfig dataChannel;  // WiFi settings
};

struct CameraConfig {
    Resolution resolution;
    uint8_t fps_limit;
    uint8_t quality;          // 0=auto, 1-63=fixed
    int8_t brightness;        // -2 to +2
    int8_t contrast;
    int8_t saturation;
    int8_t sharpness;
    // ... many more sensor settings
};

struct DataChannelConfig {
    int8_t wifi_power;        // dBm
    WIFI_Rate wifi_rate;
    uint8_t wifi_channel;
    uint8_t fec_codec_k;
    uint8_t fec_codec_n;
    uint16_t fec_codec_mtu;
    // ... control flags
};
```

**Config Update Frequency:** ~10 Hz (sent every 100ms)

#### Telemetry Packets (Bidirectional)

```c
// Air → Ground
struct Air2Ground_Data_Packet {
    Air2Ground_Header header;
    uint8_t payload[AIR2GROUND_MTU - sizeof(header)];
    // Contains Mavlink/MSP messages
};

// Ground → Air
struct Ground2Air_Data_Packet {
    Ground2Air_Header header;
    uint8_t payload[GROUND2AIR_DATA_MAX_PAYLOAD_SIZE];
    // Contains Mavlink RC commands
};
```

**Telemetry FEC:**
- Air → Ground: Same as video (FEC 6/12)
- Ground → Air: FEC 2/3 (lower overhead for uplink)

## CRC and Data Integrity

### Header CRC

```c
uint8_t crc = compute_crc8(packet, size);
```

- **Algorithm:** CRC-8
- **Applied to:** Entire packet except CRC field
- **Purpose:** Detect header corruption
- **Action on failure:** Packet discarded

### JPEG Integrity

- JPEG format has internal markers (SOI, EOI)
- Decoder validates JPEG structure
- Corrupted JPEGs result in frame drop

## Flow Control

### Adaptive Compression

The air unit adjusts JPEG quality based on three factors:

```
┌──────────────────────────────────────────┐
│  Quality Factor 1: Bandwidth Limit       │
│  target_size = (wifi_rate * 0.5) /       │
│                (FEC_N/FEC_K) / fps       │
└──────────────────────────────────────────┘
              │
┌─────────────▼─────────────────────────────┐
│  Quality Factor 2: WiFi Queue Depth       │
│  if (queue_usage > 50%)                   │
│      decrease quality                     │
└──────────────────────────────────────────┘
              │
┌─────────────▼─────────────────────────────┐
│  Quality Factor 3: SD Card Write Speed    │
│  max_write_rate = 0.8 MB/s (ESP32)        │
│                   1.8 MB/s (ESP32-S3)     │
└──────────────────────────────────────────┘
```

**Quality Range:**
- **Minimum:** 8 (best quality, OV2640)
- **Maximum:** 63 (worst quality)
- **Auto mode:** Dynamically adjusts within range

### Congestion Control

```
if (wlan_queue_usage > 70%) {
    // Red zone - aggressive quality reduction
    quality += 3;
} else if (wlan_queue_usage > 50%) {
    // Yellow zone - moderate reduction
    quality += 1;
} else if (wlan_queue_usage < 30%) {
    // Green zone - improve quality
    quality -= 1;
}
quality = clamp(quality, 8, 63);
```

## Latency Sources

Total latency budget: **90-110ms**

```
┌─────────────────────────┬──────────┐
│ Camera capture          │  16-33ms │  (depends on FPS)
├─────────────────────────┼──────────┤
│ JPEG encoding (sensor)  │  10-20ms │
├─────────────────────────┼──────────┤
│ FEC encoding            │   5-10ms │
├─────────────────────────┼──────────┤
│ WiFi transmission       │   2-5ms  │
├─────────────────────────┼──────────┤
│ FEC decoding            │   5-10ms │
├─────────────────────────┼──────────┤
│ JPEG decoding           │   1-7ms  │
├─────────────────────────┼──────────┤
│ Rendering               │   8-16ms │  (60Hz display)
├─────────────────────────┼──────────┤
│ Buffering/jitter        │  10-20ms │
└─────────────────────────┴──────────┘
Total:                      90-110ms
```

## Error Handling

### Packet Loss

**FEC Recovery:**
```
if (received_packets >= K) {
    fec_decode();  // Reconstruct missing packets
    deliver_to_application();
} else {
    discard_block();  // Unrecoverable
}
```

**Frame Loss:**
- Entire frame discarded
- Next frame rendered
- No interpolation or concealment
- Visual artifact: momentary freeze/skip

### Connection Loss

**Air Unit Behavior:**
```
if (no_ground_packets_for > 5_seconds) {
    gsDeviceId = 0;  // Unpair
    accept_new_connections();
}
```

**Ground Station Behavior:**
```
if (no_air_packets_for > 1_second) {
    display_NO_SIGNAL_warning();
    continue_sending_config();
}
```

## Security Considerations

**Current State:**
- No encryption (performance/latency priority)
- Device ID provides basic pairing
- Packets transmitted in clear

**Implications:**
- Video can be intercepted
- Telemetry can be read
- Commands can be spoofed
- Suitable for hobbyist use only

**Future Considerations:**
- Optional lightweight encryption (ChaCha20)
- HMAC authentication
- Trade-off: increased latency

## Bandwidth Analysis

### Example Calculation (800x456 @ 30fps)

```
Average JPEG frame size: 8 KB
FPS: 30
Raw bandwidth: 8 KB × 30 = 240 KB/s = 1.92 Mbps

With FEC 6/12:
Total bandwidth: 1.92 × (12/6) = 3.84 Mbps

With packet overhead:
MTU per packet: 1488 bytes
Overhead per packet: 12 bytes
Efficiency: 1488/1500 = 99.2%

Actual WiFi bandwidth: 3.84 / 0.992 = 3.87 Mbps
```

**WiFi Rate MCS3 (26 Mbps nominal):**
- Practical throughput: ~10 Mbps
- Video: 3.87 Mbps
- Telemetry: ~0.1 Mbps
- Config/OSD: ~0.05 Mbps
- **Remaining headroom:** ~6 Mbps

## Protocol Evolution

**Version History:**

| Version | Changes |
|---------|---------|
| 1 | Initial protocol |
| 2 | Added device pairing, improved packet structure |

**Future Enhancements:**
- Retransmission for critical packets
- Variable FEC rates per packet type
- Congestion avoidance (CSMA/CA awareness)
- Multi-channel support

## See Also

- [03_PACKET_STRUCTURES.md](03_PACKET_STRUCTURES.md) - Binary layout details
- [04_VIDEO_PIPELINE.md](04_VIDEO_PIPELINE.md) - Video encoding specifics
- [05_FEC_IMPLEMENTATION.md](05_FEC_IMPLEMENTATION.md) - FEC algorithm details

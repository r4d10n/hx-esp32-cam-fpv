# ESP32-FPV-GS Architecture

## Overview

ESP32-FPV-GS is a low-latency digital FPV system designed specifically for ESP32/ESP32-S3
microcontrollers. It uses custom packet format optimized for memory-constrained embedded
systems with hardware JPEG encoding from OV2640/OV5640 cameras.

**Repository**: https://github.com/RomanLut/hx-esp32-cam-fpv

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    AIR UNIT (ESP32-S3 + Camera)                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    ESP32-S3 MCU                                      │    │
│  │                                                                      │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │           Camera Interface (I2C + DVP/DMA)               │       │    │
│  │  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐    │       │    │
│  │  │  │  OV2640/5640 │  │   Hardware   │  │  DMA Stream  │    │       │    │
│  │  │  │   Sensor     │──►│   JPEG Enc   │──►│  Callback    │    │       │    │
│  │  │  │  (SCCB/I2C)  │  │  (In-Camera) │  │  (20 MHz)    │    │       │    │
│  │  │  └──────────────┘  └──────────────┘  └──────┬───────┘    │       │    │
│  │  └─────────────────────────────────────────────┼────────────┘       │    │
│  │                                                │                     │    │
│  │                                                ▼                     │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              Video Processing Pipeline                    │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  JPEG Frame Detection (SOI/EOI markers)            │  │       │    │
│  │  │  │  0xFFD8 (Start) ... 0xFFD9 (End)                   │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  │                         │                                 │       │    │
│  │  │                         ▼                                 │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  Frame Chunker (1446 bytes/packet max)             │  │       │    │
│  │  │  │  Air2Ground_Video_Packet header (18 bytes)         │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  │                             │                                        │    │
│  │                             ▼                                        │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              FEC Codec (Reed-Solomon)                    │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  Encoder: K=6 primary, N=12 total (default)        │  │       │    │
│  │  │  │  Block accumulator → FEC generation → TX queue     │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  Packet_Header (12 bytes FEC wrapper)              │  │       │    │
│  │  │  │  version, signature, deviceId, size, block_idx,    │  │       │    │
│  │  │  │  packet_idx                                        │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  │                             │                                        │    │
│  │                             ▼                                        │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              WiFi TX (Packet Injection)                   │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  IEEE 802.11 Header (24 bytes)                     │  │       │    │
│  │  │  │  esp_wifi_80211_tx() - Raw frame injection         │  │       │    │
│  │  │  │  TX Queue: 85-90 KB circular buffer                │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  └─────────────────────────────┼───────────────────────────────────────┘    │
│                                │                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    ESP32 Internal WiFi                               │    │
│  │                    Monitor Mode + Injection                          │    │
│  │                    Channel 7, MCS3 (26 Mbps)                         │    │
│  └─────────────────────────────┬───────────────────────────────────────┘    │
└─────────────────────────────────┼───────────────────────────────────────────┘
                                  │
                        ══════════╪══════════
                           RADIO CHANNEL
                            2.4 GHz
                        ══════════╪══════════
                                  │
┌─────────────────────────────────┼───────────────────────────────────────────┐
│                    GROUND STATION (Raspberry Pi / PC)                        │
├─────────────────────────────────┼───────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │            WiFi Card(s) - RTL8812AU Recommended                      │    │
│  │            Monitor Mode + Promiscuous Reception                      │    │
│  └─────────────────────────────┬───────────────────────────────────────┘    │
│                                ▼                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    gs_fec_decoder Component                          │    │
│  │  ┌────────────────────────────────────────────────────────────────┐ │    │
│  │  │  IEEE 802.11 + Radiotap Parser                                 │ │    │
│  │  │  MAC Address Filter (configurable)                             │ │    │
│  │  └─────────────────────────┬──────────────────────────────────────┘ │    │
│  │                            ▼                                         │    │
│  │  ┌────────────────────────────────────────────────────────────────┐ │    │
│  │  │  BlockManager                                                  │ │    │
│  │  │  - 8 concurrent blocks max (MAX_BLOCKS_IN_FLIGHT)              │ │    │
│  │  │  - Packet pool: 128 buffers × 1464 bytes                       │ │    │
│  │  │  - Block timeout: 100 ms                                       │ │    │
│  │  └─────────────────────────┬──────────────────────────────────────┘ │    │
│  │                            ▼                                         │    │
│  │  ┌────────────────────────────────────────────────────────────────┐ │    │
│  │  │  FecDecoder                                                    │ │    │
│  │  │  - Reed-Solomon (zfec library)                                 │ │    │
│  │  │  - Recover missing packets when K+ available                   │ │    │
│  │  └─────────────────────────┬──────────────────────────────────────┘ │    │
│  │                            ▼                                         │    │
│  │  ┌────────────────────────────────────────────────────────────────┐ │    │
│  │  │  FrameAssembler                                                │ │    │
│  │  │  - Reassemble JPEG frames from parts                           │ │    │
│  │  │  - Max 150 KB frame buffer                                     │ │    │
│  │  │  - Parts tracking via bitmask (128 parts max)                  │ │    │
│  │  └─────────────────────────┬──────────────────────────────────────┘ │    │
│  └────────────────────────────┼────────────────────────────────────────┘    │
│                               ▼                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    Ground Station Application                        │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │    MJPEG     │  │  Telemetry   │  │     OSD      │               │    │
│  │  │   Display    │  │   Mavlink    │  │   Overlay    │               │    │
│  │  └──────────────┘  └──────────────┘  └──────────────┘               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Packet Structure

### Layer 1: IEEE 802.11 Header (24 bytes)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│              WLAN_IEEE_HEADER_AIR2GROUND (24 bytes)                         │
├──────────┬──────────────────────────────────────────────────────────────────┤
│  Bytes   │  Content                                                         │
├──────────┼──────────────────────────────────────────────────────────────────┤
│  0-1     │  0x08, 0x00 - Frame Control (Data frame)                         │
├──────────┼──────────────────────────────────────────────────────────────────┤
│  2-3     │  0x00, 0x00 - Duration/ID                                        │
├──────────┼──────────────────────────────────────────────────────────────────┤
│  4-9     │  FF:FF:FF:FF:FF:FF - Destination (Broadcast)                     │
├──────────┼──────────────────────────────────────────────────────────────────┤
│  10-15   │  11:22:33:44:55:66 - Source MAC (Spoofed/Fixed)                  │
├──────────┼──────────────────────────────────────────────────────────────────┤
│  16-21   │  11:22:33:44:55:66 - BSSID (Same as source)                      │
├──────────┼──────────────────────────────────────────────────────────────────┤
│  22-23   │  0x10, 0x86 - Sequence Control                                   │
└──────────┴──────────────────────────────────────────────────────────────────┘
```

### Layer 2: FEC Packet Header (12 bytes)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Packet_Header (12 bytes)                                 │
├─────────────────────┬───────────────────────────────────────────────────────┤
│  packet_version     │  1 byte  - Protocol version (PACKET_VERSION = 2)     │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  packet_signature   │  1 byte  - Magic byte (PACKET_SIGNATURE = 56)        │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  fromDeviceId       │  2 bytes - Source device ID                          │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  toDeviceId         │  2 bytes - Destination device ID                     │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  size               │  2 bytes - Payload size (max 1476)                   │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  block_index        │  24 bits - FEC block number                          │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  packet_index       │  8 bits  - Packet index (0-11 for K=6, N=12)         │
│                     │           0-5: Primary, 6-11: FEC redundancy         │
└─────────────────────┴───────────────────────────────────────────────────────┘
```

### Layer 3: Application Headers

#### Air2Ground_Header (Base, 2 bytes)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    Air2Ground_Header (2 bytes)                              │
├─────────────────────┬───────────────────────────────────────────────────────┤
│  type               │  1 byte  - PacketType enum                            │
│                     │           0=Video, 1=Telemetry, 2=OSD, 3=Config       │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  reserved           │  1 byte  - Reserved/padding                           │
└─────────────────────┴───────────────────────────────────────────────────────┘
```

#### Air2Ground_Video_Packet (18 bytes total)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                Air2Ground_Video_Packet (18 bytes)                           │
├─────────────────────┬───────────────────────────────────────────────────────┤
│  type               │  1 byte  - PacketType::Video (0)                      │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  reserved           │  1 byte  - Reserved                                   │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  resolution         │  1 byte  - Resolution enum (QVGA to UXGA)             │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  part_index         │  7 bits  - Part number within frame (0-127)           │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  last_part          │  1 bit   - Last part flag                             │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  frame_index        │  4 bytes - Frame sequence number                      │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  [JPEG payload]     │  Up to 1446 bytes of JPEG data                        │
└─────────────────────┴───────────────────────────────────────────────────────┘
```

#### Air2Ground_OSD_Packet (AirStats + OSDBuffer)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                 Air2Ground_OSD_Packet (~1250 bytes)                         │
├─────────────────────┬───────────────────────────────────────────────────────┤
│  type               │  1 byte  - PacketType::OSD (2)                        │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  AirStats           │  ~32 bytes - System metrics:                          │
│                     │    - SD card status, WiFi queue, RSSI                 │
│                     │    - FPS, temperature, resolution                     │
│                     │    - FEC parameters, brightness, etc.                 │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  OSDBuffer          │  ~1220 bytes - Character grid (53×20)                 │
│                     │    - 7-bit characters with high-bit flag              │
└─────────────────────┴───────────────────────────────────────────────────────┘
```

## FEC Implementation

### Parameters
| Parameter | Value | Notes |
|-----------|-------|-------|
| K (default) | 6 | Primary packets per block |
| N (default) | 12 | Total packets per block |
| MTU | 1464 | Max payload after FEC header |
| Block timeout | 100 ms | Expire incomplete blocks |
| Max blocks | 8 | Concurrent blocks in flight |
| Packet pool | 128 | Pre-allocated packet buffers |

### FEC Codec Classes
```cpp
class Fec_Codec {
    static const uint8_t MAX_CODING_K = 16;
    static const uint8_t MAX_CODING_N = 32;

    // Encoder state
    struct {
        std::vector<Packet> block_packets;      // K primary
        Packet block_fec_packet;                // Current FEC
        uint32_t last_block_index;
    } m_encoder;

    // Decoder state
    struct {
        std::vector<Packet> block_packets;      // Received primary
        std::vector<Packet> block_fec_packets;  // Received FEC
        uint32_t crt_block_index;
        std::vector<Packet> fec_decoded_packets;
    } m_decoder;
};
```

### Ground Station Decoder Classes
```cpp
class BlockManager {
    FecBlock m_blocks[MAX_BLOCKS_IN_FLIGHT];  // 8 blocks
    uint8_t* m_packet_pool[PACKET_POOL_SIZE]; // 128 buffers
    uint32_t m_next_expected_block;
};

class FecDecoder {
    fec_t* m_fec;                             // zfec context
    BlockManager m_block_manager;
    FrameAssembler m_frame_assembler;
};

class FrameAssembler {
    uint8_t* m_frame_buffer;                  // 150 KB
    FrameState m_current_frame;
    uint32_t m_frames_decoded;
    uint32_t m_frames_incomplete;
};
```

## Memory Budget (ESP32-S3 with 2MB PSRAM)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ESP32-S3 Memory Allocation                               │
├─────────────────────────────────────┬───────────────────────────────────────┤
│  Component                          │  Size                                 │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  WiFi TX Queue                      │  ~90 KB (Internal SRAM)               │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  WiFi RX Buffer                     │  ~1 KB (Internal SRAM)                │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  Camera DMA Buffer                  │  ~4 KB (streaming, no full frame)     │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  FEC Encoder Buffers                │  ~20 KB (PSRAM)                       │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  Telemetry/OSD Buffers              │  ~5 KB                                │
├─────────────────────────────────────┴───────────────────────────────────────┤
│  Ground Station (gs_fec_decoder):                                           │
├─────────────────────────────────────┬───────────────────────────────────────┤
│  Packet Pool (128 × 1464)           │  ~188 KB (PSRAM)                      │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  Block Management                   │  ~2 KB                                │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  Frame Buffer                       │  ~150 KB (PSRAM)                      │
├─────────────────────────────────────┼───────────────────────────────────────┤
│  Total GS Decoder                   │  ~340 KB                              │
└─────────────────────────────────────┴───────────────────────────────────────┘
```

## Key Features

| Feature | Status | Notes |
|---------|--------|-------|
| Video Streaming | Yes | MJPEG, hardware encoded |
| Resolutions | QVGA-1280×720 | Camera dependent |
| Frame Rate | 12-50 FPS | Resolution/camera dependent |
| FEC | Yes | Reed-Solomon K=6, N=12 |
| Bidirectional | Yes | Config + telemetry uplink |
| Telemetry | Yes | Mavlink via UART |
| OSD | Yes | 53×20 character grid |
| SD Recording | Yes | On air unit |
| Adaptive Quality | Yes | Auto JPEG quality 8-63 |
| Encryption | No | Not implemented |
| Multi-card RX | Yes | On ground station |

## WiFi Configuration

| Parameter | Value | Notes |
|-----------|-------|-------|
| Channel | 7 (default) | 2.4 GHz |
| Rate | MCS3 (26 Mbps) | 802.11n HT20 |
| TX Power | 20 dBm | Configurable |
| Mode | Monitor + Injection | Promiscuous RX |
| Max packet | 1500 bytes | Standard MTU |

## Performance

| Metric | ESP32 | ESP32-S3 | Notes |
|--------|-------|----------|-------|
| Latency | 90-110 ms | 80-100 ms | Glass-to-glass |
| Max bitrate | 2.3 MB/s | 2.9 MB/s | Practical limit |
| 640×360 FPS | 30-40 | 40-50 | OV2640 |
| 1024×576 FPS | 12 | 30 | OV5640 needed |
| 1280×720 FPS | - | 30 | HQ DVR mode |
| Range | ~1 km | ~1 km | With good antennas |

## Protocol Constants

```c
// Packet structure
#define PACKET_VERSION          2
#define PACKET_SIGNATURE        56
#define PACKET_OVERHEAD         12  // Packet_Header size

// MTU values
#define WLAN_MAX_PACKET_SIZE    1500
#define WLAN_MAX_PAYLOAD_SIZE   1476  // 1500 - 24 (IEEE header)
#define AIR2GROUND_MTU          1464  // 1476 - 12 (FEC header)
#define MAX_VIDEO_DATA_PAYLOAD  1446  // 1464 - 18 (video header)

// FEC defaults
#define FEC_K                   6
#define FEC_N                   12

// WiFi
#define DEFAULT_WIFI_CHANNEL    7
#define DEFAULT_WIFI_RATE       WIFI_Rate::RATE_N_26M_MCS3
```

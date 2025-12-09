# RubyFPV Architecture

## Overview

RubyFPV is a complete open-source digital FPV system with its own custom protocol.
Unlike WFB-NG-based systems, Ruby implements its own radio link layer with unique
features like multi-band redundancy, relay nodes, and automatic link adaptation.

**Repository**: https://github.com/RubyFPV/RubyFPV
**Website**: https://rubyfpv.com

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           VEHICLE (Air Unit)                                 │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    Ruby Vehicle Application                         │    │
│  │                                                                      │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │   Camera     │  │   Telemetry  │  │     RC       │               │    │
│  │  │  H.264 Enc   │  │  (Mavlink)   │  │  Receiver    │               │    │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │    │
│  │         │                 │                 │                        │    │
│  │         ▼                 ▼                 ▼                        │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              Stream Manager                               │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  Stream 0-3: Data/Telemetry/Audio/Data2            │  │       │    │
│  │  │  │  Stream 4+:  Video streams                         │  │       │    │
│  │  │  │  8 max radio streams, 4 video streams              │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  │                             │                                        │    │
│  │                             ▼                                        │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              Radio Link Manager                           │       │    │
│  │  │  ┌────────────────┐  ┌────────────────┐                  │       │    │
│  │  │  │  FEC Encoder   │  │  Packet Builder │                  │       │    │
│  │  │  │  (Reed-Solomon)│  │  (Ruby Format)  │                  │       │    │
│  │  │  └────────────────┘  └────────────────┘                  │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  Multi-Link Router (2.4/5.8 GHz + 433/868 MHz)     │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  └─────────────────────────────┼───────────────────────────────────────┘    │
│                                │                                             │
│           ┌────────────────────┼────────────────────┐                       │
│           ▼                    ▼                    ▼                       │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐             │
│  │ WiFi Card 1     │  │ WiFi Card 2     │  │ 433/868 MHz     │             │
│  │ 2.4 GHz         │  │ 5.8 GHz         │  │ SiK Radio       │             │
│  │ Monitor Mode    │  │ Monitor Mode    │  │ (Telemetry)     │             │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘             │
└───────────┼────────────────────┼────────────────────┼───────────────────────┘
            │                    │                    │
   ═════════╪════════   ═════════╪════════   ═════════╪════════
     2.4 GHz RADIO       5.8 GHz RADIO       433/868 MHz RADIO
   ═════════╪════════   ═════════╪════════   ═════════╪════════
            │                    │                    │
┌───────────┼────────────────────┼────────────────────┼───────────────────────┐
│           ▼                    ▼                    ▼                       │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────┐             │
│  │ WiFi Card 1     │  │ WiFi Card 2     │  │ 433/868 MHz     │             │
│  │ 2.4 GHz         │  │ 5.8 GHz         │  │ SiK Radio       │             │
│  └────────┬────────┘  └────────┬────────┘  └────────┬────────┘             │
│           │                    │                    │                       │
│           └────────────────────┼────────────────────┘                       │
│                                ▼                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │              Radio Link Manager (Aggregation)                       │    │
│  │  ┌────────────────────────────────────────────────────────────────┐ │    │
│  │  │  Multi-Link Aggregator + Duplicate Detection                   │ │    │
│  │  │  Best-Signal Selection per Packet                              │ │    │
│  │  └────────────────────────────────────────────────────────────────┘ │    │
│  │  ┌────────────────┐  ┌────────────────┐                             │    │
│  │  │  FEC Decoder   │  │  Packet Parser │                             │    │
│  │  │  (Reed-Solomon)│  │  (Ruby Format) │                             │    │
│  │  └────────────────┘  └────────────────┘                             │    │
│  └──────────────────────────┬──────────────────────────────────────────┘    │
│                             │                                                │
│                             ▼                                                │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    Ruby Controller Application                      │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │   Video      │  │  Telemetry   │  │     OSD      │               │    │
│  │  │   Decoder    │  │   Display    │  │   Renderer   │               │    │
│  │  └──────────────┘  └──────────────┘  └──────────────┘               │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
│                           GROUND STATION (Controller)                        │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Packet Structure

### Ruby Packet Header (t_packet_header)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    t_packet_header (26 bytes)                               │
├─────────────────────┬───────────────────────────────────────────────────────┤
│  uCRC               │  4 bytes - CRC32 of packet                            │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  packet_flags       │  1 byte  - Module type + flags                        │
│                     │    Bits 0-2: Module (video/telemetry/RC/command)      │
│                     │    Bit 3: Header-only CRC                             │
│                     │    Bit 4: Retransmitted packet                        │
│                     │    Bit 6: Encrypted                                   │
│                     │    Bit 7: High priority                               │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  packet_type        │  1 byte  - Specific packet type (3-72)                │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  stream_packet_idx  │  4 bytes - Stream sequence number                     │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  packet_flags_ext   │  2 bytes - Extended flags                             │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  total_length       │  2 bytes - Total packet length                        │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  radio_link_pkt_idx │  2 bytes - Radio link packet index                    │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  vehicle_id_src     │  4 bytes - Source vehicle/controller ID               │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  vehicle_id_dest    │  4 bytes - Destination ID (0 = broadcast)             │
└─────────────────────┴───────────────────────────────────────────────────────┘
```

### Video Segment Header (t_packet_header_video_segment)
```
┌─────────────────────────────────────────────────────────────────────────────┐
│              t_packet_header_video_segment (~28 bytes)                      │
├─────────────────────┬───────────────────────────────────────────────────────┤
│  uVideoStreamIdx    │  1 byte  - Stream index + type flags                  │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uVideoStatusFlags2 │  4 bytes - Video status flags                         │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uStreamInfoFlags   │  1 byte  - Stream info flags                          │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uStreamInfo        │  4 bytes - Stream information                         │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uCurrentVideoProfile│ 1 byte  - Current video link profile                 │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uKeyframeIntervalMs│  2 bytes - Keyframe interval in ms                    │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uCurrentBitrateBPS │  4 bytes - Current video bitrate                      │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uCurrentBlockIndex │  4 bytes - FEC block index                            │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uBlockPacketIndex  │  1 byte  - Packet index within block                  │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uBlockPacketSize   │  2 bytes - Packet size in block                       │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uBlockDataPackets  │  1 byte  - Data packets in block (K)                  │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uBlockECPackets    │  1 byte  - FEC packets in block (N-K)                 │
├─────────────────────┼───────────────────────────────────────────────────────┤
│  uH264FrameIndex    │  2 bytes - H.264 frame index                          │
└─────────────────────┴───────────────────────────────────────────────────────┘
```

### Packet Types
```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Ruby Packet Types                                   │
├────────────┬────────────────────────────────────────────────────────────────┤
│  Type 3-9  │  Control: Ping, radio reinit, model settings, pairing         │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 11-12│  Commands: Command request/response                            │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 13   │  Files: Log file segment transfer                              │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 18   │  Audio: Audio segment                                          │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 20   │  Video: Video request                                          │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 22   │  Video: Video data segment                                     │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 25-26│  RC: RC frames and download info                               │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 29-43│  Telemetry: Ruby short/extended, FC, MSP                       │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 45-46│  Data Links: Auxiliary data upload/download                    │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 60-61│  Adaptive: Video parameter negotiation                         │
├────────────┼────────────────────────────────────────────────────────────────┤
│  Type 72   │  Testing: Radio link negotiation                               │
└────────────┴────────────────────────────────────────────────────────────────┘
```

## FEC Implementation

### Parameters
| Parameter | Default | Max | Notes |
|-----------|---------|-----|-------|
| Data packets (K) | Variable | 64 | Per block |
| FEC packets (N-K) | Variable | 64 | Per block |
| Block buffer | 80-200 | - | Platform dependent |
| Max packet size | 1250 | 1500 | Payload bytes |

### Algorithm
- Reed-Solomon forward error correction
- Per-block encoding (not streaming)
- Automatic FEC adjustment based on link quality

## Multi-Link Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                         Multi-Link Radio System                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌───────────────┐  ┌───────────────┐  ┌───────────────┐                   │
│  │  Link 1       │  │  Link 2       │  │  Link 3       │                   │
│  │  2.4 GHz WiFi │  │  5.8 GHz WiFi │  │  433 MHz SiK  │                   │
│  │  High BW      │  │  High BW      │  │  Low BW       │                   │
│  │  Video+Telem  │  │  Video+Telem  │  │  Telem Only   │                   │
│  └───────┬───────┘  └───────┬───────┘  └───────┬───────┘                   │
│          │                  │                  │                            │
│          ▼                  ▼                  ▼                            │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    Link Aggregator / Router                         │   │
│  │  - Video only on high-bandwidth links                               │   │
│  │  - Telemetry on all available links                                 │   │
│  │  - Duplicate detection across links                                 │   │
│  │  - Best-signal packet selection                                     │   │
│  │  - Automatic failover                                               │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Relay Node Support

```
                    ┌──────────────┐
                    │   Vehicle    │
                    │  (Air Unit)  │
                    └──────┬───────┘
                           │
            ┌──────────────┼──────────────┐
            ▼              ▼              ▼
     ┌──────────┐   ┌──────────┐   ┌──────────┐
     │  Relay   │   │  Relay   │   │  Ground  │
     │  Node 1  │   │  Node 2  │   │ Station  │
     └────┬─────┘   └────┬─────┘   └──────────┘
          │              │
          └──────┬───────┘
                 ▼
          ┌──────────┐
          │  Ground  │
          │ Station  │
          │ (Remote) │
          └──────────┘
```

## Key Features

| Feature | Status | Notes |
|---------|--------|-------|
| Video Streaming | Yes | H.264, hardware encoding |
| Multi-Link | Yes | 2.4/5.8 GHz + 433/868 MHz |
| Relay Nodes | Yes | Extend range, BLOS |
| Encryption | Yes | End-to-end (temporarily disabled) |
| FEC | Yes | Reed-Solomon, adaptive |
| RC Control | Yes | Via radio link |
| Telemetry | Yes | Mavlink, LTM |
| OSD | Yes | Custom rendering |
| Adaptive Video | Yes | Auto bitrate/quality adjustment |
| Multi-Vehicle | Yes | Multiple vehicle support |

## Differences from WFB-NG

| Aspect | WFB-NG | RubyFPV |
|--------|--------|---------|
| Protocol | Standard wifibroadcast | Custom Ruby protocol |
| Multi-link | No (single band) | Yes (multi-band) |
| Relay support | No | Yes |
| Adaptive video | Manual | Automatic |
| Packet format | Simple | Rich metadata |
| Complexity | Low | High |
| Interoperability | High (standard) | Low (proprietary) |

## Radio Configuration

### WiFi Parameters
- Radio ports: 0x0E (uplink), 0x0F (downlink)
- Monitor mode with injection
- Configurable MCS rates
- Power control

### Stream Allocation
| Stream ID | Purpose |
|-----------|---------|
| 0 | Data channel 1 |
| 1 | Telemetry |
| 2 | Audio |
| 3 | Data channel 2 |
| 4+ | Video streams |

## Protocol Constants

```c
// Packet sizes
#define MAX_PACKET_PAYLOAD       1250
#define MAX_PACKET_TOTAL         1500

// Streams
#define MAX_RADIO_STREAMS        8
#define MAX_VIDEO_STREAMS        4

// FEC
#define MAX_BLOCKS_BUFFERS       200  // RPi/Radxa
#define MAX_BLOCKS_BUFFERS_OTHER 80   // Other platforms
#define MAX_PACKETS_IN_BLOCK     64

// Radio ports
#define RADIO_PORT_UPLINK        0x0E
#define RADIO_PORT_DOWNLINK      0x0F
```

# OpenHD Architecture

## Overview

OpenHD is a comprehensive digital FPV ecosystem built on top of wifibroadcast technology.
It provides a complete solution for HD video, telemetry, and RC control over WiFi.

**Repository**: https://github.com/OpenHD/OpenHD
**Wifibroadcast Library**: https://github.com/OpenHD/wifibroadcast

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           AIR UNIT                                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    OpenHD Air Application                           │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │   Camera     │  │   Mavlink    │  │     RC       │               │    │
│  │  │   Manager    │  │   Handler    │  │   Receiver   │               │    │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │    │
│  │         │                 │                 │                        │    │
│  │         ▼                 ▼                 ▼                        │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │                 Stream Multiplexer                        │       │    │
│  │  │   Radio Port 0: Video (Primary)                          │       │    │
│  │  │   Radio Port 1: Video (Secondary)                        │       │    │
│  │  │   Radio Port 2: Telemetry Down                           │       │    │
│  │  │   Radio Port 3: Audio                                    │       │    │
│  │  │   ...                                                    │       │    │
│  │  │   Radio Port 127: Session Keys                           │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  └─────────────────────────────┼───────────────────────────────────────┘    │
│                                ▼                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    WBTxRx Library                                   │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │  FEC Encoder │  │  Encryption  │  │   Radiotap   │               │    │
│  │  │  (per stream)│  │  ChaCha20    │  │   Builder    │               │    │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │    │
│  │         │                 │                 │                        │    │
│  │         └─────────────────┼─────────────────┘                        │    │
│  │                           ▼                                          │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              Packet Injection (raw socket)                │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  └─────────────────────────────┼───────────────────────────────────────┘    │
│                                ▼                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    WiFi Card (RTL8812AU/AR9271)                      │    │
│  │                    Monitor Mode + Injection                          │    │
│  └─────────────────────────────┬───────────────────────────────────────┘    │
└─────────────────────────────────┼───────────────────────────────────────────┘
                                  │
                        ══════════╪══════════
                           RADIO CHANNEL
                         2.4/5.8/6 GHz
                        ══════════╪══════════
                                  │
┌─────────────────────────────────┼───────────────────────────────────────────┐
│                           GROUND STATION                                     │
├─────────────────────────────────┼───────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    WiFi Card(s) - Diversity                         │    │
│  │                    Monitor Mode + Promiscuous                       │    │
│  └─────────────────────────────┬───────────────────────────────────────┘    │
│                                ▼                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    WBTxRx Library (RX Thread)                       │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │    libpcap   │  │  Decryption  │  │  FEC Decoder │               │    │
│  │  │    Capture   │  │  Validation  │  │  (per stream)│               │    │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │    │
│  │         │                 │                 │                        │    │
│  │         └─────────────────┼─────────────────┘                        │    │
│  │                           ▼                                          │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              Stream Demultiplexer (by Radio Port)         │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  └─────────────────────────────┼───────────────────────────────────────┘    │
│                                ▼                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    OpenHD Ground Application                        │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │    Video     │  │   Telemetry  │  │     OSD      │               │    │
│  │  │   Decoder    │  │   Handler    │  │   Renderer   │               │    │
│  │  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘               │    │
│  │         │                 │                 │                        │    │
│  │         └─────────────────┼─────────────────┘                        │    │
│  │                           ▼                                          │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │                  QOpenHD Application                      │       │    │
│  │  │              (Qt-based, Multi-platform)                   │       │    │
│  │  └──────────────────────────────────────────────────────────┘       │    │
│  └─────────────────────────────────────────────────────────────────────┘    │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Packet Structure

### IEEE 802.11 Header (OpenHD Custom)
```
┌─────────────────────────────────────────────────────────────────────────┐
│              Ieee80211HeaderOpenHD (24 bytes)                           │
├──────────┬──────────────────────────────────────────────────────────────┤
│ Frame    │ 0x08 0x01 (Data frame, To DS)                                │
│ Control  │ 2 bytes                                                      │
├──────────┼──────────────────────────────────────────────────────────────┤
│ Duration │ 0x00 0x00                                                    │
│          │ 2 bytes                                                      │
├──────────┼──────────────────────────────────────────────────────────────┤
│ Addr1    │ FF:FF:FF:FF:FF:FF (Broadcast)                                │
│ (Dest)   │ 6 bytes                                                      │
├──────────┼──────────────────────────────────────────────────────────────┤
│ Addr2    │ [UniqueID][Nonce Part1 (4 bytes)][RadioPort]                 │
│ (Source) │ 6 bytes - Encodes 64-bit nonce + port                        │
├──────────┼──────────────────────────────────────────────────────────────┤
│ Addr3    │ [UniqueID][Nonce Part2 (4 bytes)][RadioPort]                 │
│ (BSSID)  │ 6 bytes - Continues nonce encoding                           │
├──────────┼──────────────────────────────────────────────────────────────┤
│ Seq Ctrl │ Sequence number                                              │
│          │ 2 bytes                                                      │
└──────────┴──────────────────────────────────────────────────────────────┘

UniqueID: 0x01 = Air Unit, 0x02 = Ground Station
RadioPort: 0-126 for streams, 127 for session keys
```

### Radio Port Byte
```
┌─────────────────────────────────────────────────────────────────┐
│                   RadioPort (1 byte)                            │
├─────────┬───────────────────────────────────────────────────────┤
│  Bit 7  │  Encryption flag (1 = encrypted)                      │
├─────────┼───────────────────────────────────────────────────────┤
│ Bits 0-6│  Stream index (0-126), 127 = session key              │
└─────────┴───────────────────────────────────────────────────────┘
```

### Session Key Packet
```
┌─────────────────────────────────────────────────────────────────┐
│              SessionKeyPacket (72 bytes)                        │
├─────────────────┬───────────────────────────────────────────────┤
│     Nonce       │  24 bytes (crypto_box_NONCEBYTES)             │
├─────────────────┼───────────────────────────────────────────────┤
│  Encrypted Key  │  48 bytes (32-byte key + 16-byte MAC)         │
│  + MAC          │                                               │
└─────────────────┴───────────────────────────────────────────────┘
```

### FEC Payload Header
```
┌─────────────────────────────────────────────────────────────────┐
│                 FECPayloadHdr (variable)                        │
├─────────────────┬───────────────────────────────────────────────┤
│  block_idx      │  32-bit block index                           │
├─────────────────┼───────────────────────────────────────────────┤
│  fragment_idx   │  8-bit fragment index within block            │
├─────────────────┼───────────────────────────────────────────────┤
│  n_primary      │  8-bit count of primary fragments (K)         │
├─────────────────┼───────────────────────────────────────────────┤
│  n_secondary    │  8-bit count of FEC fragments (N-K)           │
├─────────────────┼───────────────────────────────────────────────┤
│  payload_size   │  16-bit payload size                          │
└─────────────────┴───────────────────────────────────────────────┘
```

## FEC Implementation

### Class Structure
```cpp
class FECEncoder {
    // Encodes data into FEC-protected blocks
    void encode_block(data_packets);      // Pre-formed packets
    void fragment_and_encode(data, k);    // Auto-fragment
private:
    std::array<uint8_t, MAX_PAYLOAD>[MAX_FRAGMENTS] m_block_buffer;
    uint32_t m_curr_block_idx;
};

class FECDecoder {
    // Decodes and recovers FEC blocks
    void process_valid_packet(packet);
private:
    std::queue<RxBlock> rx_ring;  // Searchable ring buffer
    size_t rx_queue_max_depth;    // 1-19 blocks
};
```

### FEC Parameters
| Parameter | Value | Notes |
|-----------|-------|-------|
| Max fragments/block | 128 | Configurable |
| Default K | Variable | Per-stream |
| Default overhead | 20% | N = K * 1.2 |
| Block timeout | Configurable | Auto-expire incomplete |
| SIMD | Yes | NEON (ARM), SSSE3 (x86) |

### Zero-Latency FEC
OpenHD implements "zero-latency" FEC by:
1. Forwarding primary packets immediately as received
2. Only using FEC recovery when gaps detected
3. Not waiting for complete block before output

## Key Features

| Feature | Status | Description |
|---------|--------|-------------|
| HD Video | Yes | H.264/H.265, hardware encoding |
| Bidirectional Link | Yes | Video down, telemetry/RC up |
| Multi-stream | Yes | Up to 126 simultaneous streams |
| Encryption | Yes | ChaCha20-Poly1305, per-packet optional |
| FEC | Yes | Reed-Solomon, SIMD accelerated |
| Multi-card RX | Yes | Diversity combining, RSSI-based |
| Auto TX switch | Yes | Best card selection by RSSI |
| QOpenHD App | Yes | Qt-based, Android/Linux/Windows |
| OSD | Yes | Customizable overlay |
| DVR Recording | Yes | Local recording on GS |
| Adaptive Bitrate | Partial | Manual adjustment |

## Differences from WFB-NG

| Aspect | WFB-NG | OpenHD |
|--------|--------|--------|
| Architecture | Separate TX/RX binaries | Unified library |
| FEC Location | In wfb_tx/wfb_rx | Separate layer (WBStreamTx/Rx) |
| Multiplexing | Via radio_port in MAC | Same + encryption flag |
| Nonce encoding | data_nonce in header | Split across MAC addresses |
| Application | CLI tools | Complete FPV ecosystem |
| Configuration | Command-line/files | QOpenHD GUI |

## Protocol Constants

```cpp
// Maximum sizes
#define MAX_PCAP_PACKET_SIZE      1510
#define MAX_RAW_FRAME_PAYLOAD     1473
#define MAX_USER_PAYLOAD          1457  // After encryption overhead

// Encryption
#define ENCRYPTION_OVERHEAD       16    // ChaCha20-Poly1305 tag

// Session management
#define SESSION_KEY_ANNOUNCE_INTERVAL_MS  1000
#define SESSION_KEY_RADIO_PORT            127

// FEC
#define MAX_TOTAL_FRAGMENTS_PER_BLOCK     128
#define MAX_PAYLOAD_BEFORE_FEC            1466
#define RX_QUEUE_MAX_DEPTH_DEFAULT        5
```

## Supported Hardware

### Air Unit
- Raspberry Pi 3/4/Zero 2W
- Radxa Zero 3W
- Custom OpenIPC cameras

### Ground Station
- Raspberry Pi 4
- x86 Linux PC
- Android (via USB WiFi)

### WiFi Adapters
- **Recommended**: RTL8812AU, RTL8812BU
- **Alternative**: AR9271, MT7612U
- Must support monitor mode + injection

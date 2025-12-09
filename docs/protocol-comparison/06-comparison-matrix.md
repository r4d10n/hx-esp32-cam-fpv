# FPV Protocol Comparison Matrix

## Feature Comparison

### Core Features

| Feature | WFB-NG | OpenHD | OpenIPC | RubyFPV | ESP32-FPV-GS |
|---------|--------|--------|---------|---------|--------------|
| **Video Codec** | Any (RTP) | H.264/H.265 | H.264/H.265 | H.264 | MJPEG |
| **Video Source** | UDP input | Camera | IP Camera | Camera | OV2640/5640 |
| **Max Resolution** | Unlimited | 4K | 4K | 1080p | 1280×720 |
| **Typical Latency** | 80-120ms | 80-120ms | 60-100ms | 100-150ms | 80-110ms |
| **FEC** | Yes | Yes | Yes (via wfb) | Yes | Yes |
| **Encryption** | ChaCha20 | ChaCha20 | ChaCha20 | Yes* | No |
| **Bidirectional** | Yes | Yes | Yes | Yes | Yes |
| **Multi-card RX** | Yes | Yes | Yes | Yes | Yes |

*RubyFPV encryption temporarily disabled

### Protocol Details

| Aspect | WFB-NG | OpenHD | OpenIPC | RubyFPV | ESP32-FPV-GS |
|--------|--------|--------|---------|---------|--------------|
| **FEC Header** | 9 bytes | ~8 bytes | 9 bytes | 28+ bytes | 12 bytes |
| **App Header** | 3 bytes | Variable | RTP | 26 bytes | 2-18 bytes |
| **Max MTU** | 4045 | 1510 | 4045 | 1500 | 1500 |
| **FEC Default K** | 8 | Variable | 8 | Variable | 6 |
| **FEC Default N** | 12 | K×1.2 | 12 | Variable | 12 |
| **Block Index** | 64-bit | 32-bit | 64-bit | 32-bit | 24-bit |
| **Packet Index** | 8-bit | 8-bit | 8-bit | 8-bit | 8-bit |

### Hardware Support

| Platform | WFB-NG | OpenHD | OpenIPC | RubyFPV | ESP32-FPV-GS |
|----------|--------|--------|---------|---------|--------------|
| **Raspberry Pi** | Yes | Yes | GS only | Yes | GS only |
| **x86 Linux** | Yes | Yes | GS only | Partial | GS only |
| **ESP32** | No | No | No | No | Yes (Air) |
| **ESP32-S3** | No | No | No | No | Yes (Air+GS) |
| **IP Camera SoC** | Via UDP | Partial | Yes | Partial | No |
| **Android** | No | Yes | Via UDP | No | No |

### WiFi Card Support

| Card | WFB-NG | OpenHD | OpenIPC | RubyFPV | ESP32-FPV-GS |
|------|--------|--------|---------|---------|--------------|
| **RTL8812AU** | Yes | Yes | Yes | Yes | GS only |
| **RTL8812BU** | Yes | Yes | Yes | Yes | GS only |
| **AR9271** | Yes | Yes | Partial | Yes | GS only |
| **MT7612U** | Yes | Yes | No | Yes | GS only |
| **ESP32 Internal** | No | No | No | No | Yes (Air) |

## Packet Structure Comparison

### WFB-NG Packet
```
┌────────────────────────────────────────────────────────────────┐
│  IEEE 802.11 Header (24 bytes)                                 │
├────────────────────────────────────────────────────────────────┤
│  wblock_hdr_t (9 bytes) - packet_type + data_nonce             │
├────────────────────────────────────────────────────────────────┤
│  [Encrypted with ChaCha20-Poly1305]                            │
│  ├─ wpacket_hdr_t (3 bytes) - flags + size                     │
│  └─ Payload (UDP packet)                                       │
├────────────────────────────────────────────────────────────────┤
│  AEAD Tag (16 bytes)                                           │
└────────────────────────────────────────────────────────────────┘
Total overhead: 52 bytes minimum
Max payload: ~3993 bytes
```

### OpenHD Packet
```
┌────────────────────────────────────────────────────────────────┐
│  IEEE 802.11 Header (24 bytes) - Nonce in MAC fields           │
├────────────────────────────────────────────────────────────────┤
│  RadioPort (1 byte) - Stream index + encryption flag           │
├────────────────────────────────────────────────────────────────┤
│  FECPayloadHdr (8+ bytes) - block_idx, fragment_idx, K, N      │
├────────────────────────────────────────────────────────────────┤
│  [Optional: ChaCha20-Poly1305 encrypted]                       │
│  └─ Payload                                                    │
├────────────────────────────────────────────────────────────────┤
│  [Optional: AEAD Tag (16 bytes)]                               │
└────────────────────────────────────────────────────────────────┘
Total overhead: 33-49 bytes
Max payload: ~1457 bytes
```

### RubyFPV Packet
```
┌────────────────────────────────────────────────────────────────┐
│  IEEE 802.11 Header (24 bytes)                                 │
├────────────────────────────────────────────────────────────────┤
│  t_packet_header (26 bytes)                                    │
│  ├─ CRC (4), flags (1), type (1), stream_idx (4)               │
│  ├─ flags_ext (2), length (2), radio_link_idx (2)              │
│  └─ vehicle_id_src (4), vehicle_id_dest (4)                    │
├────────────────────────────────────────────────────────────────┤
│  [Type-specific header: Command/Video/Telemetry]               │
│  └─ Video: t_packet_header_video_segment (~28 bytes)           │
├────────────────────────────────────────────────────────────────┤
│  Payload                                                       │
└────────────────────────────────────────────────────────────────┘
Total overhead: 50-78 bytes
Max payload: ~1250 bytes
```

### ESP32-FPV-GS Packet
```
┌────────────────────────────────────────────────────────────────┐
│  IEEE 802.11 Header (24 bytes)                                 │
├────────────────────────────────────────────────────────────────┤
│  Packet_Header (12 bytes)                                      │
│  ├─ version (1), signature (1), fromDeviceId (2)               │
│  ├─ toDeviceId (2), size (2)                                   │
│  └─ block_index (3), packet_index (1)                          │
├────────────────────────────────────────────────────────────────┤
│  Air2Ground_Video_Packet (18 bytes for video)                  │
│  ├─ type (1), reserved (1), resolution (1)                     │
│  ├─ part_index:7 + last_part:1                                 │
│  └─ frame_index (4)                                            │
├────────────────────────────────────────────────────────────────┤
│  Payload (JPEG data, max 1446 bytes)                           │
└────────────────────────────────────────────────────────────────┘
Total overhead: 54 bytes (video)
Max payload: 1446 bytes
```

## Architecture Comparison

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                          PROTOCOL STACK COMPARISON                          │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  WFB-NG:          OpenHD:          RubyFPV:        ESP32-FPV-GS:           │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐    ┌──────────┐            │
│  │ UDP/RTP  │     │  QOpenHD │     │  Ruby UI │    │   MJPEG  │            │
│  │ Streams  │     │  App     │     │  System  │    │  Stream  │            │
│  └────┬─────┘     └────┬─────┘     └────┬─────┘    └────┬─────┘            │
│       │                │                │               │                   │
│       ▼                ▼                ▼               ▼                   │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐    ┌──────────┐            │
│  │  wfb_tx  │     │ WBStream │     │  Stream  │    │   FEC    │            │
│  │  wfb_rx  │     │   Tx/Rx  │     │  Manager │    │  Codec   │            │
│  └────┬─────┘     └────┬─────┘     └────┬─────┘    └────┬─────┘            │
│       │                │                │               │                   │
│       ▼                ▼                ▼               ▼                   │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐    ┌──────────┐            │
│  │   FEC    │     │   FEC    │     │   FEC    │    │  Packet  │            │
│  │ (zfec)   │     │ (custom) │     │   (RS)   │    │ Builder  │            │
│  └────┬─────┘     └────┬─────┘     └────┬─────┘    └────┬─────┘            │
│       │                │                │               │                   │
│       ▼                ▼                ▼               ▼                   │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐    ┌──────────┐            │
│  │ ChaCha20 │     │ ChaCha20 │     │Encryption│    │   (No    │            │
│  │ Poly1305 │     │ Optional │     │ (paused) │    │  Encrypt)│            │
│  └────┬─────┘     └────┬─────┘     └────┬─────┘    └────┬─────┘            │
│       │                │                │               │                   │
│       ▼                ▼                ▼               ▼                   │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐    ┌──────────┐            │
│  │ Raw WiFi │     │  WBTxRx  │     │  Radio   │    │ WiFi TX  │            │
│  │ Injection│     │  Library │     │  Link    │    │ Injection│            │
│  └──────────┘     └──────────┘     └──────────┘    └──────────┘            │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Implementation Plan: Multi-Protocol Support for ESP32-FPV-GS

### Phase 1: WFB-NG Compatibility (High Priority)

**Goal**: Enable ESP32-FPV-GS ground station to receive WFB-NG streams

**Tasks**:
1. Implement WFB-NG packet parser
   - Parse wblock_hdr_t (9-byte header)
   - Extract data_nonce for block/fragment identification
   - Handle session key packets (type 0x02)

2. Add ChaCha20-Poly1305 decryption
   - Port libsodium/TweetNaCl to ESP32
   - Implement AEAD decryption for data packets
   - Session key management

3. Adapt FEC decoder
   - Support variable K/N from session packets
   - Handle WFB-NG block numbering (64-bit nonce)

**Estimated complexity**: Medium
**Memory impact**: ~20 KB for crypto library

### Phase 2: OpenHD Compatibility

**Goal**: Receive OpenHD streams on ESP32-GS

**Tasks**:
1. Parse OpenHD IEEE header format
   - Extract nonce from MAC address fields
   - Handle RadioPort byte

2. Support FECPayloadHdr format
   - Variable-size header parsing
   - Different K/N per stream

3. Stream demultiplexing
   - Handle multiple simultaneous streams
   - Route video vs telemetry

**Estimated complexity**: Medium
**Memory impact**: ~5 KB additional

### Phase 3: OpenIPC Integration

**Goal**: Direct integration with OpenIPC cameras

**Tasks**:
1. RTP/H.264 depacketization
   - NAL unit reconstruction
   - Handle fragmented NAL units

2. H.264 to MJPEG transcoding (optional)
   - For ESP32 display compatibility
   - Or pass-through to capable displays

3. Majestic configuration tool
   - UDP output configuration
   - OSD overlay support

**Estimated complexity**: High (codec complexity)
**Memory impact**: Significant for H.264 decode

### Phase 4: RubyFPV Compatibility (Lower Priority)

**Goal**: Basic RubyFPV stream reception

**Tasks**:
1. Implement t_packet_header parser
   - CRC validation
   - Packet type routing

2. Video segment reconstruction
   - Parse t_packet_header_video_segment
   - Block/packet assembly

3. Telemetry pass-through
   - Mavlink extraction
   - Relay to flight controller

**Estimated complexity**: High (proprietary protocol)
**Memory impact**: ~10 KB for larger headers

### Protocol Detection Layer

```cpp
// Proposed multi-protocol detector
class ProtocolDetector {
public:
    enum Protocol {
        PROTOCOL_UNKNOWN,
        PROTOCOL_ESP32_FPV,    // Current native protocol
        PROTOCOL_WFB_NG,       // wblock_hdr_t with packet_type
        PROTOCOL_OPENHD,       // RadioPort in MAC addr
        PROTOCOL_RUBY          // t_packet_header with CRC
    };

    Protocol detect(const uint8_t* packet, size_t len) {
        // Check ESP32-FPV signature first (fast path)
        if (len >= 36 && packet[24] == PACKET_VERSION &&
            packet[25] == PACKET_SIGNATURE) {
            return PROTOCOL_ESP32_FPV;
        }

        // Check WFB-NG packet type
        if (len >= 33 && (packet[24] == 0x01 || packet[24] == 0x02)) {
            return PROTOCOL_WFB_NG;
        }

        // Check Ruby CRC header
        if (len >= 50) {
            uint32_t crc = *(uint32_t*)(packet + 24);
            if (validate_ruby_crc(packet + 24, len - 24, crc)) {
                return PROTOCOL_RUBY;
            }
        }

        // OpenHD detection via MAC pattern
        if (len >= 24 && (packet[10] == 0x01 || packet[10] == 0x02)) {
            return PROTOCOL_OPENHD;
        }

        return PROTOCOL_UNKNOWN;
    }
};
```

### Memory Budget for Multi-Protocol Support

| Component | Current | With WFB-NG | With All |
|-----------|---------|-------------|----------|
| FEC Decoder | 340 KB | 360 KB | 380 KB |
| Crypto (ChaCha20) | 0 | 20 KB | 20 KB |
| Protocol Parsers | 5 KB | 10 KB | 25 KB |
| RTP Depacketizer | 0 | 0 | 15 KB |
| **Total** | **345 KB** | **390 KB** | **440 KB** |

This fits within ESP32-S3's 2 MB PSRAM budget.

## Recommendations

### For Maximum Compatibility
1. **Implement WFB-NG support first** - Most widely used, standard protocol
2. **Add OpenHD support** - Growing ecosystem, similar to WFB-NG
3. **Consider OpenIPC as air unit** - Better performance than ESP32 for HD

### For ESP32-S3 Ground Station
1. Focus on reception (RX) for all protocols
2. Keep ESP32-FPV protocol for ESP32 air units
3. Support hybrid setups (ESP32 Air + Linux GS, or OpenIPC Air + ESP32 GS)

### Protocol Selection Guide

| Use Case | Recommended Protocol |
|----------|---------------------|
| ESP32 air unit | ESP32-FPV-GS (native) |
| Raspberry Pi air unit | WFB-NG or OpenHD |
| IP camera air unit | OpenIPC |
| Multi-vehicle | RubyFPV |
| Long range (>5 km) | WFB-NG/OpenIPC |
| Low latency (<100 ms) | ESP32-FPV-GS or OpenIPC |
| Encrypted link | WFB-NG or OpenHD |

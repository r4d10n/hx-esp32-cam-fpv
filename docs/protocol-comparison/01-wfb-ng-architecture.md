# WFB-NG (WiFiBroadcast Next Generation) Architecture

## Overview

WFB-NG is the foundational protocol that many other FPV systems are built upon. It provides
a broadcast-style radio link using WiFi hardware in monitor mode with packet injection.

**Repository**: https://github.com/svpcom/wfb-ng

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           AIR UNIT (TX)                                      │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐                   │
│  │   Camera     │───►│  GStreamer   │───►│  UDP Socket  │                   │
│  │  (H.264/5)   │    │  (Encoder)   │    │  Port 5600   │                   │
│  └──────────────┘    └──────────────┘    └──────┬───────┘                   │
│                                                  │                           │
│  ┌──────────────┐    ┌──────────────┐           │                           │
│  │   Mavlink    │───►│  UDP Socket  │───────────┼───────────┐               │
│  │   FC UART    │    │  Port 14550  │           │           │               │
│  └──────────────┘    └──────────────┘           │           │               │
│                                                  ▼           ▼               │
│                                          ┌──────────────────────┐           │
│                                          │      wfb_tx          │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ Session Key    │  │           │
│                                          │  │ Management     │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          │          ▼           │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ FEC Encoder    │  │           │
│                                          │  │ (Reed-Solomon) │  │           │
│                                          │  │ K=8, N=12 def  │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          │          ▼           │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ ChaCha20-Poly  │  │           │
│                                          │  │ Encryption     │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          │          ▼           │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ IEEE 802.11    │  │           │
│                                          │  │ Frame Builder  │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          └──────────┼───────────┘           │
│                                                     ▼                        │
│                                          ┌──────────────────────┐           │
│                                          │   WiFi Card          │           │
│                                          │   (Monitor Mode)     │           │
│                                          │   Packet Injection   │           │
│                                          └──────────┬───────────┘           │
└─────────────────────────────────────────────────────┼───────────────────────┘
                                                      │
                                            ══════════╪══════════
                                               RADIO CHANNEL
                                            ══════════╪══════════
                                                      │
┌─────────────────────────────────────────────────────┼───────────────────────┐
│                           GROUND STATION (RX)       │                        │
├─────────────────────────────────────────────────────┼───────────────────────┤
│                                          ┌──────────▼───────────┐           │
│                                          │   WiFi Card(s)       │           │
│                                          │   (Monitor Mode)     │           │
│                                          │   Promiscuous RX     │           │
│                                          └──────────┬───────────┘           │
│                                                     ▼                        │
│                                          ┌──────────────────────┐           │
│                                          │      wfb_rx          │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ BPF Filter     │  │           │
│                                          │  │ (Magic + ID)   │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          │          ▼           │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ ChaCha20-Poly  │  │           │
│                                          │  │ Decryption     │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          │          ▼           │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ FEC Decoder    │  │           │
│                                          │  │ (Reed-Solomon) │  │           │
│                                          │  │ Ring Buffer    │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          │          ▼           │           │
│                                          │  ┌────────────────┐  │           │
│                                          │  │ UDP Output     │  │           │
│                                          │  │ Port 5600      │  │           │
│                                          │  └───────┬────────┘  │           │
│                                          └──────────┼───────────┘           │
│                                                     │                        │
│                     ┌───────────────────────────────┼─────────┐              │
│                     ▼                               ▼         ▼              │
│          ┌──────────────────┐            ┌─────────────┐  ┌───────┐         │
│          │   Video Player   │            │  QGroundCtl │  │  OSD  │         │
│          │   (GStreamer)    │            │  (Mavlink)  │  │       │         │
│          └──────────────────┘            └─────────────┘  └───────┘         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Packet Structure

### Layer 1: IEEE 802.11 Frame
```
┌────────────────────────────────────────────────────────────────────┐
│                    IEEE 802.11 Data Frame (24 bytes)               │
├──────────┬──────────┬──────────┬──────────┬──────────┬────────────┤
│ Frame    │ Duration │ Addr1    │ Addr2    │ Addr3    │ Seq Ctrl   │
│ Control  │          │ (Dest)   │ (Src)    │ (BSSID)  │            │
│ 2 bytes  │ 2 bytes  │ 6 bytes  │ 6 bytes  │ 6 bytes  │ 2 bytes    │
├──────────┴──────────┴──────────┴──────────┴──────────┴────────────┤
│ Frame Control: 0x0801 (Data frame, To DS)                         │
│ Addr format: W:B:X:X:X:X where channel_id = (link_id<<8)+radio_port│
└────────────────────────────────────────────────────────────────────┘
```

### Layer 2: WFB-NG Block Header
```
┌─────────────────────────────────────────────────────────────────┐
│                   wblock_hdr_t (9 bytes)                        │
├─────────────────┬───────────────────────────────────────────────┤
│  packet_type    │               data_nonce                      │
│   (1 byte)      │               (8 bytes)                       │
├─────────────────┼───────────────────────────────────────────────┤
│  0x01 = DATA    │  (block_idx << 8) | fragment_idx              │
│  0x02 = SESSION │  Used as AEAD nonce for encryption            │
└─────────────────┴───────────────────────────────────────────────┘
```

### Layer 3: WFB-NG Packet Header (after decryption)
```
┌─────────────────────────────────────────────────────────────────┐
│                   wpacket_hdr_t (3 bytes)                       │
├─────────────────┬───────────────────────────────────────────────┤
│     flags       │              packet_size                      │
│   (1 byte)      │              (2 bytes, BE)                    │
├─────────────────┼───────────────────────────────────────────────┤
│  Bit 0: FEC_ONLY│  Size of payload following this header        │
└─────────────────┴───────────────────────────────────────────────┘
```

### Session Key Packet
```
┌─────────────────────────────────────────────────────────────────┐
│                wsession_hdr_t + wsession_data_t                 │
├─────────────────┬───────────────────────────────────────────────┤
│  packet_type    │  session_nonce (32 bytes)                     │
│  0x02           │  [crypto_box_NONCEBYTES]                      │
├─────────────────┴───────────────────────────────────────────────┤
│                    Encrypted Session Data:                       │
├─────────────────┬───────────────────────────────────────────────┤
│  epoch          │  8 bytes - Session epoch for replay protection │
├─────────────────┼───────────────────────────────────────────────┤
│  channel_id     │  4 bytes - Link identifier                     │
├─────────────────┼───────────────────────────────────────────────┤
│  fec_type       │  1 byte  - 0x01 = Reed-Solomon                 │
├─────────────────┼───────────────────────────────────────────────┤
│  k              │  1 byte  - Data fragments per block            │
├─────────────────┼───────────────────────────────────────────────┤
│  n              │  1 byte  - Total fragments per block           │
├─────────────────┼───────────────────────────────────────────────┤
│  session_key    │  32 bytes - ChaCha20-Poly1305 key              │
├─────────────────┼───────────────────────────────────────────────┤
│  tags[]         │  Variable - TLV-encoded optional data          │
└─────────────────┴───────────────────────────────────────────────┘
```

## FEC Implementation

### Parameters
| Parameter | Default | Range | Description |
|-----------|---------|-------|-------------|
| K | 8 | 1-128 | Data packets per block |
| N | 12 | K+1-128 | Total packets per block |
| MTU | 1466 | - | Max payload after headers |
| WIFI_MTU | 4045 | - | Max injected packet size |

### Algorithm
- **Type**: Reed-Solomon (Vandermonde matrices)
- **Library**: zfec with SIMD acceleration (NEON/SSSE3)
- **Recovery**: Any K of N packets recovers block
- **Overhead**: (N-K)/K redundancy ratio

### Block Processing
```
TX Flow:
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│ UDP Pkt │───►│ Collect │───►│  FEC    │───►│ Encrypt │───► WiFi
│  Input  │    │ K pkts  │    │ Encode  │    │  N pkts │
└─────────┘    └─────────┘    └─────────┘    └─────────┘

RX Flow:
┌─────────┐    ┌─────────┐    ┌─────────┐    ┌─────────┐
│  WiFi   │───►│ Decrypt │───►│  FEC    │───►│  UDP    │───► Output
│   RX    │    │ Validate│    │ Decode  │    │ Forward │
└─────────┘    └─────────┘    └─────────┘    └─────────┘
```

## Key Features

| Feature | Status | Notes |
|---------|--------|-------|
| Video streaming | Yes | UDP/RTP input, any codec |
| Bidirectional telemetry | Yes | Mavlink, custom data |
| Encryption | Yes | ChaCha20-Poly1305 AEAD |
| FEC | Yes | Reed-Solomon, configurable K/N |
| Multi-card RX | Yes | Diversity combining |
| Dynamic FEC | Yes | Runtime K/N changes |
| Distributed operation | Yes | Multiple hosts |
| Session management | Yes | Key rotation, epoch tracking |

## WiFi Configuration

- **Mode**: Monitor mode with packet injection
- **Rates**: MCS0-MCS7 (HT20/HT40), OFDM, CCK
- **Channels**: Any supported by hardware
- **Power**: Configurable TX power
- **Cards**: Realtek RTL8812AU recommended

## Protocol Constants

```c
#define WIFI_MTU                4045
#define MAX_PAYLOAD_SIZE        3976  // After all headers
#define MAX_FEC_PAYLOAD        (WIFI_MTU - sizeof(ieee80211_header) -
                                sizeof(wblock_hdr_t) -
                                crypto_aead_chacha20poly1305_ABYTES)
#define WFB_PACKET_DATA         0x1
#define WFB_PACKET_SESSION      0x2
#define WFB_FEC_VDM_RS          0x1
#define SESSION_KEY_ANNOUNCE_MSEC 1000
#define RX_ANT_MAX              4
#define MAX_RX_INTERFACES       8
```

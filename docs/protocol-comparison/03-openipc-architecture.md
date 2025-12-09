# OpenIPC FPV Architecture

## Overview

OpenIPC is an alternative open firmware for IP cameras that includes FPV capabilities.
It combines the Majestic streamer with WFB-NG for a complete digital FPV solution
without requiring a Raspberry Pi on the air unit.

**Website**: https://openipc.org
**Wiki**: https://github.com/OpenIPC/wiki
**Majestic**: https://github.com/OpenIPC/majestic (binary only)

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    AIR UNIT (IP Camera + WiFi Module)                        │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    IP Camera SoC                                    │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │   Image      │  │   Hardware   │  │    RTP       │               │    │
│  │  │   Sensor     │──►│   H.264/265  │──►│   Streamer   │               │    │
│  │  │  (IMX307)    │  │   Encoder    │  │ (smolrtsp)   │               │    │
│  │  └──────────────┘  └──────────────┘  └──────┬───────┘               │    │
│  │                                             │                        │    │
│  │  ┌──────────────┐                           │                        │    │
│  │  │   Mavlink    │                           │                        │    │
│  │  │    UART      │───────────────────────────┼────────┐               │    │
│  │  └──────────────┘                           │        │               │    │
│  │                                             │        │               │    │
│  │                    ┌────────────────────────┘        │               │    │
│  │                    ▼                                 ▼               │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              Majestic Streamer                           │       │    │
│  │  │  ┌────────────────────────────────────────────────────┐  │       │    │
│  │  │  │  Video: UDP localhost:5600 (RTP/H.264 or H.265)    │  │       │    │
│  │  │  │  Telemetry: MSP/Mavlink passthrough                │  │       │    │
│  │  │  │  OSD: Betaflight/INAV font rendering               │  │       │    │
│  │  │  └────────────────────────────────────────────────────┘  │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  └─────────────────────────────┼───────────────────────────────────────┘    │
│                                │                                             │
│                                ▼ UDP Port 5600 (Video)                       │
│                                ▼ UDP Port 14550 (Mavlink)                    │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    WFB-NG TX (wfb_tx)                                │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │  UDP Input   │  │  FEC Encode  │  │  Encryption  │               │    │
│  │  │  Listener    │──►│  K=8, N=12   │──►│  ChaCha20    │               │    │
│  │  └──────────────┘  └──────────────┘  └──────┬───────┘               │    │
│  │                                             │                        │    │
│  │                                             ▼                        │    │
│  │  ┌──────────────────────────────────────────────────────────┐       │    │
│  │  │              WiFi Module (RTL8812AU USB)                  │       │    │
│  │  │              Monitor Mode + Packet Injection              │       │    │
│  │  └──────────────────────────┬───────────────────────────────┘       │    │
│  └─────────────────────────────┼───────────────────────────────────────┘    │
└─────────────────────────────────┼───────────────────────────────────────────┘
                                  │
                        ══════════╪══════════
                           RADIO CHANNEL
                          5.8 GHz typical
                        ══════════╪══════════
                                  │
┌─────────────────────────────────┼───────────────────────────────────────────┐
│                           GROUND STATION                                     │
├─────────────────────────────────┼───────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    WiFi Card(s) (RTL8812AU)                          │    │
│  │                    Monitor Mode                                      │    │
│  └─────────────────────────────┬───────────────────────────────────────┘    │
│                                ▼                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐    │
│  │                    WFB-NG RX (wfb_rx)                                │    │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐               │    │
│  │  │  Decryption  │  │  FEC Decode  │  │  UDP Output  │               │    │
│  │  │  + Validate  │──►│  Recovery    │──►│  Port 5600   │               │    │
│  │  └──────────────┘  └──────────────┘  └──────┬───────┘               │    │
│  └─────────────────────────────────────────────┼───────────────────────┘    │
│                                                │                             │
│                    ┌───────────────────────────┼─────────────┐               │
│                    ▼                           ▼             ▼               │
│  ┌──────────────────────┐  ┌──────────────────────┐  ┌───────────────┐      │
│  │   GStreamer Pipeline │  │   QGroundControl     │  │  FPV-VR App   │      │
│  │   H.264/H.265 Decode │  │   Mission Planner    │  │  (Android)    │      │
│  └──────────────────────┘  └──────────────────────┘  └───────────────┘      │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Key Components

### Majestic Streamer
Majestic is the heart of OpenIPC FPV, providing:
- Hardware-accelerated H.264/H.265 encoding
- RTP streaming output
- OSD overlay (Betaflight/INAV fonts)
- MSP/Mavlink telemetry handling
- Low-latency optimization

**Note**: Majestic source code is NOT open source (Prosperity Public License).
The smolrtsp library it's based on is open source.

### Configuration (majestic.yaml)
```yaml
video0:
  enabled: true
  codec: h264        # or h265
  rcMode: cbr        # Constant bitrate
  bitrate: 4096      # kbps
  gopSize: 1.0       # Keyframe interval (seconds)

outgoing:
  - udp://127.0.0.1:5600  # Local to wfb-ng

osd:
  enabled: true
  font: betaflight
```

### WFB-NG Configuration (wfb.conf)
```ini
[wfb]
channel=149      # 5.8 GHz channel
bandwidth=20     # MHz
mcs_index=3      # MCS rate
stbc=1           # Space-time block coding
ldpc=1           # Low-density parity check
fec_k=8          # Data packets
fec_n=12         # Total packets
key=/etc/gs.key  # Encryption key
```

## Packet Flow

### Video Pipeline
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│   Camera    │───►│   H.264/5   │───►│  RTP/UDP    │───►│   wfb_tx    │
│   Sensor    │    │   Encoder   │    │  Port 5600  │    │   FEC+Enc   │
└─────────────┘    └─────────────┘    └─────────────┘    └──────┬──────┘
                                                                │
                                                         ┌──────▼──────┐
                                                         │    WiFi     │
                                                         │  Injection  │
                                                         └──────┬──────┘
                                                                │
                                                         ══════╪══════
                                                           RADIO
                                                         ══════╪══════
                                                                │
┌─────────────┐    ┌─────────────┐    ┌─────────────┐    ┌──────▼──────┐
│   Display   │◄───│   Decode    │◄───│  RTP/UDP    │◄───│   wfb_rx    │
│  (GStreamer)│    │ (H.264/5)   │    │  Port 5600  │    │   FEC Dec   │
└─────────────┘    └─────────────┘    └─────────────┘    └─────────────┘
```

### GStreamer RX Pipeline Example
```bash
# H.264
gst-launch-1.0 udpsrc port=5600 ! \
  application/x-rtp ! rtph264depay ! \
  avdec_h264 ! videoconvert ! autovideosink

# H.265
gst-launch-1.0 udpsrc port=5600 buffer-size=32768 ! \
  application/x-rtp ! rtph265depay ! \
  avdec_h265 ! videoconvert ! \
  video/x-raw,format=BGRA ! autovideosink
```

## Supported Hardware

### IP Camera SoCs
| Chipset | Resolution | FPS | H.265 | Notes |
|---------|------------|-----|-------|-------|
| GK7205V200 | 1080p | 30 | Yes | Budget option |
| GK7205V300 | 1080p | 60 | Yes | Better quality |
| SSC338Q | 4K | 30 | Yes | High-end |
| Hi3516EV200 | 1080p | 30 | Yes | Hisilicon |
| Hi3516EV300 | 1080p | 60 | Yes | Better Hisilicon |

### Image Sensors
- IMX307 (2MP, excellent low-light)
- IMX335 (5MP)
- IMX415 (8MP)
- SC2239 (2MP, budget)

### WiFi Modules
- RTL8812AU (recommended)
- RTL8812EU
- RTL8811CU

## Performance

| Metric | Value | Conditions |
|--------|-------|------------|
| Glass-to-Glass Latency | 60-100 ms | 720p/1080p @ 30-60 fps |
| Video Bitrate | 2-12 Mbps | Configurable |
| Range | 20+ km | With good antennas |
| Power Consumption | ~2W | Camera + WiFi module |

## Key Features

| Feature | Status | Notes |
|---------|--------|-------|
| Video Streaming | Yes | H.264/H.265, hardware encoded |
| Bidirectional Telemetry | Yes | Mavlink, MSP |
| OSD | Yes | Betaflight/INAV fonts |
| Encryption | Yes | Via wfb-ng |
| FEC | Yes | Via wfb-ng |
| Recording | GS only | Not on air unit |
| RC Control | Yes | Via Mavlink RC override |
| Adaptive Bitrate | Manual | Via wfb-ng CLI |

## Differences from Standalone WFB-NG

| Aspect | WFB-NG Standalone | OpenIPC + WFB-NG |
|--------|-------------------|------------------|
| Air Unit | Raspberry Pi | IP Camera SoC |
| Video Source | USB/CSI Camera | Integrated sensor |
| Encoding | Software (Pi) or Camera | Hardware (SoC) |
| Power | ~5W (Pi) | ~2W |
| Size/Weight | Larger | Very compact |
| Latency | 80-120 ms | 60-100 ms |
| Cost | Higher | Lower |
| Complexity | More components | Integrated |

## Protocol Compatibility

OpenIPC uses **standard WFB-NG protocol** for radio transmission:
- Same packet format
- Same FEC implementation
- Same encryption (ChaCha20-Poly1305)
- Interoperable with WFB-NG ground stations

The video payload is RTP-encapsulated H.264/H.265, standard format that
works with any RTP-compatible player (VLC, GStreamer, QGroundControl, etc.)

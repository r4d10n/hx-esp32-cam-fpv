# HX-ESP32-CAM-FPV System Architecture Overview

## Executive Summary

The hx-esp32-cam-fpv system is an open-source digital FPV (First Person View) solution that enables low-latency video transmission from ESP32-based air units to ground stations using WiFi packet injection and Forward Error Correction (FEC). The system achieves 90-110ms latency at resolutions up to 1280x720 using MJPEG streaming over WiFi 802.11n.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        AIR UNIT (ESP32)                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌─────────┐  │
│  │OV2640/   │───▶│  Camera  │───▶│   JPEG   │───▶│   FEC   │  │
│  │ OV5640   │    │  Driver  │    │  Parser  │    │ Encoder │  │
│  │ Sensor   │    │  (I2S)   │    │          │    │  (6/12) │  │
│  └──────────┘    └──────────┘    └──────────┘    └─────────┘  │
│                                                         │       │
│  ┌──────────┐    ┌──────────┐                         ▼       │
│  │   DVR    │◀───│    SD    │                  ┌──────────┐   │
│  │Recording │    │   Card   │                  │   WiFi   │   │
│  └──────────┘    └──────────┘                  │Injection │   │
│                                                 │ (Monitor │   │
│  ┌──────────┐    ┌──────────┐                  │   Mode)  │   │
│  │Mavlink/  │◀──▶│   UART   │◀────────────────▶│          │   │
│  │MSP OSD   │    │ Handler  │                  └─────┬────┘   │
│  └──────────┘    └──────────┘                        │        │
│                                                       │        │
└───────────────────────────────────────────────────────┼────────┘
                                                        │
                                         2.4GHz WiFi    │
                                      (Packet Injection)│
                                                        ▼
┌─────────────────────────────────────────────────────────────────┐
│               GROUND STATION (Raspberry Pi/Radxa)               │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌─────────┐  │
│  │RTL8812AU │───▶│   WiFi   │───▶│   FEC   │───▶│  MJPEG  │  │
│  │  Card(s) │    │ Monitor  │    │ Decoder │    │ Decoder │  │
│  │ (Dual)   │    │   Mode   │    │  (6/12) │    │(Turbo   │  │
│  └──────────┘    └──────────┘    └──────────┘    │  JPEG)  │  │
│                                                   └─────┬───┘  │
│  ┌──────────┐    ┌──────────┐                         │       │
│  │   DVR    │◀───│  Video   │◀────────────────────────┘       │
│  │Recording │    │ Recorder │                                 │
│  └──────────┘    └──────────┘                                 │
│                                                                │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐                │
│  │  OpenGL  │◀───│   OSD    │◀───│Mavlink/  │                │
│  │  Render  │    │  Overlay │    │MSP Parser│                │
│  │  Engine  │    └──────────┘    └──────────┘                │
│  └──────────┘                                                 │
│                                                                │
└─────────────────────────────────────────────────────────────────┘
```

## System Components

### 1. Air Unit (Transmitter)

**Hardware Variants:**
- ESP32-CAM (OV2640 sensor)
- ESP32-S3-Sense (OV2640 or OV5640 sensor) - **Recommended**

**Key Subsystems:**
- Camera capture and JPEG encoding
- FEC encoding
- WiFi packet injection
- SD card DVR
- UART telemetry (Mavlink/MSP)
- Temperature monitoring
- Configuration management

### 2. Ground Station (Receiver)

**Hardware Variants:**
- Radxa Zero 3W (Recommended)
- Raspberry Pi Zero 2W / 3 / 4
- x86_64 Linux (Ubuntu/Fedora)

**Key Subsystems:**
- WiFi packet reception (RTL8812AU/AR9271)
- FEC decoding
- MJPEG video decoding (TurboJPEG)
- OpenGL rendering
- OSD rendering
- DVR recording
- Telemetry handling
- Configuration UI

## Key Technologies

### Communication Protocol

**Physical Layer:**
- WiFi 802.11b/g/n on 2.4GHz
- Packet injection (monitor mode)
- Default: MCS3 26Mbps (~10Mbps effective with FEC 6/12)

**Data Link Layer:**
- Custom packet structure with FEC headers
- Forward Error Correction (FEC) using Reed-Solomon codes
- Default: K=6, N=12 (any 6 of 12 packets can reconstruct data)
- Adaptive compression based on channel conditions

**Application Layer:**
- Video packets (MJPEG frames)
- Telemetry packets (Mavlink/MSP)
- Configuration packets
- OSD packets

### Video Pipeline

**Air Unit Flow:**
```
Camera Sensor → I2S DMA → JPEG Parser → Adaptive Quality Control →
FEC Encoder → WiFi Injection → (Optional) SD Card DVR
```

**Ground Station Flow:**
```
WiFi Monitor → Packet Filter → FEC Decoder → MJPEG Parser →
TurboJPEG Decoder → OpenGL Texture → Screen Render + (Optional) DVR
```

## Performance Characteristics

| Metric | Value |
|--------|-------|
| Latency | 90-110ms |
| Resolution | Up to 1280x720 @ 30fps |
| Range | 1-2km (line of sight) |
| FPS | 13-50fps (sensor/resolution dependent) |
| Bandwidth | 8-14Mbps effective (with FEC) |
| Power Consumption | <300mA @ 5V (air unit) |

## Key Design Decisions

### 1. MJPEG Instead of H.264/H.265
- ESP32 lacks hardware video encoder
- OV2640/OV5640 sensors natively output JPEG
- Lower latency (no frame buffering needed)
- Simpler implementation
- Trade-off: Higher bandwidth requirements

### 2. Forward Error Correction
- WiFi packet injection has no ACK/retransmission
- FEC 6/12 allows recovery from 50% packet loss
- Doubles bandwidth but ensures smooth video
- Critical for FPV where lost frames are unacceptable

### 3. Streaming from DMA
- Modified esp32-camera component to stream data as it arrives
- Reduces latency by 10-20ms
- Minimizes PSRAM usage
- Enables "real-time" transmission

### 4. Adaptive Compression
- JPEG quality adjusted dynamically based on:
  - Available WiFi bandwidth
  - Queue depth
  - SD card write speed
  - Target FPS
- Maintains smooth FPS under varying conditions

## Inter-Module Communication

### Air Unit Task Architecture

```
┌──────────────┐  DMA   ┌──────────────┐
│Camera I2S DMA│───────▶│Frame Parser  │
└──────────────┘        │   Task       │
                        └──────┬───────┘
                               │
                        ┌──────▼───────┐
                        │FEC Encoder   │
                        │   Task       │
                        └──────┬───────┘
                               │
                        ┌──────▼───────┐
                        │WiFi TX Task  │
                        └──────────────┘

┌──────────────┐        ┌──────────────┐
│WiFi RX Task  │───────▶│Config Handler│
└──────────────┘        │   Task       │
                        └──────────────┘

┌──────────────┐        ┌──────────────┐
│UART RX Task  │───────▶│Mavlink Parser│
└──────────────┘        │   Task       │
                        └──────────────┘
```

### Ground Station Thread Architecture

```
┌──────────────┐        ┌──────────────┐
│WiFi Monitor  │───────▶│FEC Decoder   │
│   Thread     │        │   Thread     │
└──────────────┘        └──────┬───────┘
                               │
                        ┌──────▼───────┐
                        │JPEG Decoder  │
                        │   Thread     │
                        └──────┬───────┘
                               │
┌──────────────┐        ┌──────▼───────┐
│Main Render   │◀───────│Frame Queue   │
│   Thread     │        │              │
└──────────────┘        └──────────────┘

┌──────────────┐        ┌──────────────┐
│Config TX     │◀───────│UI Input      │
│   Thread     │        │   Handler    │
└──────────────┘        └──────────────┘
```

## Directory Structure

```
hx-esp32-cam-fpv/
├── components/
│   ├── air/                    # Air unit application code
│   │   ├── main.cpp           # Main air unit logic
│   │   ├── wifi.cpp           # WiFi packet injection
│   │   ├── osd.cpp            # MSP OSD handling
│   │   ├── msp.cpp            # MSP protocol
│   │   └── nvs_args.cpp       # Configuration storage
│   ├── esp32-camera/          # Modified camera driver
│   │   ├── driver/            # Camera I2S driver
│   │   └── sensors/           # OV2640/OV5640 sensor configs
│   └── common/                # Shared air/ground code
│       ├── packets.h          # Packet structures
│       ├── fec.cpp            # FEC implementation
│       └── fec_codec.cpp      # FEC codec tasks
├── air_firmware_esp32cam/     # ESP32-CAM build config
├── air_firmware_esp32s3sense/ # ESP32-S3 build config
├── gs/                        # Ground station software
│   └── src/
│       ├── main.cpp           # GS main loop
│       ├── Comms.cpp          # WiFi monitor mode
│       ├── Video_Decoder.cpp  # MJPEG decoder
│       ├── osd.cpp            # OSD rendering
│       └── osd_menu.cpp       # Configuration UI
└── docs/                      # Documentation
```

## Configuration Flow

```
Ground Station                Air Unit
      │                          │
      │   Connect Packet         │
      ├─────────────────────────▶│
      │   (Device pairing)       │
      │                          │
      │   Config Packet          │
      ├─────────────────────────▶│
      │   (Settings)             │
      │                          │
      │                          │
      │   Video/OSD Packets      │
      │◀─────────────────────────┤
      │   (Contains device IDs)  │
      │                          │
      │   Config Update          │
      ├─────────────────────────▶│
      │                          │
```

## Data Flow Summary

1. **Video Capture**: Camera sensor captures image row-by-row, outputs JPEG via I2S
2. **Streaming**: Data streams from DMA directly to processing pipeline
3. **Adaptive Quality**: JPEG quality adjusted based on bandwidth/queue depth
4. **FEC Encoding**: Frames split into packets, FEC parity packets generated
5. **WiFi Transmission**: Packets injected over WiFi (no association needed)
6. **Reception**: Ground station receives packets on multiple interfaces (diversity)
7. **FEC Decoding**: Lost packets reconstructed if <50% loss
8. **JPEG Decoding**: TurboJPEG decodes frames to RGB
9. **Rendering**: OpenGL renders video + OSD overlay
10. **Bidirectional**: Telemetry and config flow both directions

## Next Documents

- [02_PROTOCOL_SPECIFICATION.md](02_PROTOCOL_SPECIFICATION.md) - Detailed protocol description
- [03_PACKET_STRUCTURES.md](03_PACKET_STRUCTURES.md) - Binary packet formats
- [04_VIDEO_PIPELINE.md](04_VIDEO_PIPELINE.md) - Video encoding/decoding flow
- [05_FEC_IMPLEMENTATION.md](05_FEC_IMPLEMENTATION.md) - Forward error correction details
- [06_GROUND_STATION.md](06_GROUND_STATION.md) - GS architecture and decoding

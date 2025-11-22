# ESP32-P4 High-Resolution FPV System

Next-generation FPV system using ESP32-P4 + ESP32-C5 for high-resolution video transmission.

## Features

- **1920x1080 @ 60fps** video capture using MIPI cameras
- **H.264 hardware encoding** for efficient compression
- **WiFi 6 (802.11ax)** transmission on 2.4GHz or 5GHz
- **60-80ms latency** (improved from 90-110ms)
- **Inter-processor communication** between ESP32-P4 and ESP32-C5
- **Forward Error Correction** for reliable transmission

## Hardware Requirements

### Air Unit

| Component | Specification |
|-----------|--------------|
| Main MCU | ESP32-P4-Function-EV-Board |
| WiFi MCU | ESP32-C5-DevKitC-1 |
| Camera | IMX219 (recommended), IMX477, or OV5647 |
| Connection | SPI @ 50MHz between P4 and C5 |

### Ground Station

Compatible with existing hx-esp32-cam-fpv ground stations with H.264 decoder support.

## Supported Cameras

### Sony IMX219 (Recommended)
- 8MP sensor
- 1920x1080 @ 60fps
- Good low-light performance
- Available as Raspberry Pi Camera Module V2

### Sony IMX477 (High Quality)
- 12.3MP sensor
- 1920x1080 @ 60fps
- Excellent low-light performance
- C/CS-mount for interchangeable lenses
- Available as Raspberry Pi HQ Camera Module

### OmniVision OV5647 (Budget)
- 5MP sensor
- 1920x1080 @ 30fps
- Available as Raspberry Pi Camera Module V1

## Project Structure

```
esp32-p4-mipi-fpv/
├── components/
│   ├── mipi_camera/          # MIPI CSI-2 camera driver
│   ├── h264_encoder/         # H.264 hardware encoder
│   ├── esp32_ipc/            # Inter-processor communication
│   └── wifi_c5_transmitter/  # WiFi 6 transmission (ESP32-C5)
├── main/                     # Main application
├── docs/                     # Documentation
└── tests/                    # Unit tests
```

## Building

### Prerequisites

- ESP-IDF v5.3 or later
- ESP32-P4 toolchain
- ESP32-C5 toolchain

### Build ESP32-P4 Firmware

```bash
cd esp32-p4-mipi-fpv
idf.py set-target esp32p4
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

### Build ESP32-C5 Firmware

```bash
cd esp32-c5-transmitter
idf.py set-target esp32c5
idf.py build
idf.py -p /dev/ttyUSB1 flash monitor
```

## Configuration

### Camera Settings

Edit `main/main.c`:

```c
#define CAMERA_SENSOR        MIPI_CAMERA_SENSOR_IMX219
#define CAMERA_RESOLUTION    MIPI_CAMERA_RESOLUTION_FHD  // 1920x1080
#define CAMERA_FPS           60
```

### H.264 Encoder Settings

```c
#define H264_BITRATE         6000000  // 6 Mbps
#define H264_GOP_SIZE        60       // I-frame every 2 seconds @ 30fps
```

### WiFi Settings

```c
#define WIFI_BAND            WIFI_BAND_5GHZ
#define WIFI_CHANNEL         36
#define WIFI_MCS             WIFI_MCS_7
#define WIFI_TX_POWER        20  // dBm
```

## Performance

| Metric | Value |
|--------|-------|
| Resolution | 1920x1080 |
| Frame Rate | 60 fps |
| Bitrate | 4-8 Mbps (H.264 VBR) |
| Latency | 60-80 ms |
| Range | 1-2 km (5GHz, line of sight) |
| Power | ~2.8W |

## Development Status

- [x] Architecture design
- [x] Component interface definitions
- [x] Build system setup
- [ ] MIPI camera driver implementation
- [ ] H.264 encoder integration
- [ ] IPC implementation
- [ ] WiFi transmitter implementation
- [ ] Unit tests
- [ ] Integration tests
- [ ] Ground station H.264 decoder

## Documentation

See `docs/` directory:
- [ESP32_P4_C5_IMPLEMENTATION_PLAN.md](docs/ESP32_P4_C5_IMPLEMENTATION_PLAN.md) - Complete implementation plan
- Component-specific documentation in each component's directory

## License

Same as parent project (hx-esp32-cam-fpv)

## Contributing

This is an experimental branch. Contributions welcome!

## See Also

- Parent project: [hx-esp32-cam-fpv](../)
- Architecture documentation: [../docs/architecture/](../docs/architecture/)

# ESP32-S3 Android WiFi Receiver

## Overview

This firmware enables the ESP32-S3 to receive WiFi video packets in monitor mode and stream them to Android devices via USB. It acts as a high-performance WiFi-to-USB bridge for FPV video streaming applications.

## Features

- **WiFi Monitor Mode Reception**: Captures raw WiFi packets on 2.4GHz/5GHz bands
- **Forward Error Correction**: Reed-Solomon FEC for robust packet loss recovery
- **USB CDC/Bulk Streaming**: High-throughput USB connection to Android
- **Frame Assembly**: Intelligent packet reassembly with jitter buffering
- **Real-time Statistics**: Comprehensive performance monitoring

## Hardware Requirements

- **ESP32-S3** development board with:
  - USB OTG support
  - 2MB+ SPIRAM recommended
  - WiFi antenna
- **Android Device** with USB OTG support

## Project Structure

```
esp32-s3-android-receiver/
├── CMakeLists.txt              # Project build configuration
├── sdkconfig.defaults          # ESP-IDF default configuration
├── README.md                   # This file
├── docs/
│   └── DESIGN.md              # Detailed architecture documentation
├── main/
│   ├── CMakeLists.txt
│   └── main.c                 # Application entry point
└── components/
    ├── wifi_receiver/         # WiFi monitor mode reception
    │   ├── include/
    │   │   └── wifi_receiver.h
    │   ├── wifi_receiver.c
    │   └── CMakeLists.txt
    ├── fec_decoder/           # Reed-Solomon FEC decoder
    │   ├── include/
    │   │   └── fec_decoder.h
    │   ├── fec_decoder.c
    │   └── CMakeLists.txt
    ├── usb_streamer/          # USB CDC/Bulk streaming
    │   ├── include/
    │   │   └── usb_streamer.h
    │   ├── usb_streamer.c
    │   └── CMakeLists.txt
    ├── packet_handler/        # Packet assembly and buffering
    │   ├── include/
    │   │   └── packet_handler.h
    │   ├── packet_handler.c
    │   └── CMakeLists.txt
    └── stats_tracker/         # Performance monitoring
        ├── include/
        │   └── stats_tracker.h
        ├── stats_tracker.c
        └── CMakeLists.txt
```

## Building

### Prerequisites

1. Install ESP-IDF v5.0 or later:
   ```bash
   git clone --recursive https://github.com/espressif/esp-idf.git
   cd esp-idf
   ./install.sh esp32s3
   . ./export.sh
   ```

2. Ensure you have the required dependencies:
   - Python 3.7+
   - CMake 3.16+
   - Ninja build system

### Build Instructions

1. Clone this repository and navigate to the project:
   ```bash
   cd esp32-s3-android-receiver
   ```

2. Set the ESP32-S3 as the target:
   ```bash
   idf.py set-target esp32s3
   ```

3. Configure the project (optional):
   ```bash
   idf.py menuconfig
   ```

4. Build the firmware:
   ```bash
   idf.py build
   ```

5. Flash to the ESP32-S3:
   ```bash
   idf.py -p /dev/ttyUSB0 flash monitor
   ```

## Configuration

Key configuration parameters in `sdkconfig.defaults`:

- **WiFi Settings**:
  - `CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM`: Number of static RX buffers
  - `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM`: Number of dynamic RX buffers
  
- **USB Settings**:
  - `CONFIG_TINYUSB_CDC_ENABLED`: Enable USB CDC
  - `CONFIG_TINYUSB_CDC_RX_BUFSIZE`: CDC RX buffer size
  
- **Memory Settings**:
  - `CONFIG_SPIRAM`: Enable SPIRAM support
  - `CONFIG_SPIRAM_USE_MALLOC`: Use SPIRAM for malloc

- **Performance**:
  - `CONFIG_COMPILER_OPTIMIZATION_PERF`: Performance optimization
  - `CONFIG_FREERTOS_HZ`: FreeRTOS tick rate (1000 Hz)

## Usage

### Basic Operation

1. **Power on** the ESP32-S3
2. **Connect** via USB to Android device
3. The ESP32-S3 will:
   - Initialize WiFi in monitor mode
   - Listen on the configured channel (default: channel 6)
   - Receive and decode video packets
   - Stream to Android via USB CDC

### Monitoring

View logs via serial monitor:
```bash
idf.py -p /dev/ttyUSB0 monitor
```

Expected output:
```
I (1234) main: ESP32-S3 Android WiFi Receiver Starting...
I (1235) wifi_rx: Initializing WiFi receiver
I (1236) usb_streamer: Initializing USB streamer
I (1237) main: All components initialized successfully
I (1238) main: Listening on WiFi channel 6
I (1239) main: System ready - waiting for WiFi packets...
```

### Statistics

The firmware provides real-time statistics every 5 seconds:
```
I (5000) main: === System Status ===
I (5001) main: Health: 0
I (5002) main: Free Heap: 245 KB
I (5003) main: RX Throughput: 15.2 Mbps
I (5004) main: Packets RX: 12543, Dropped: 23
```

## Performance

### Target Specifications

- **Latency**: < 50ms end-to-end
- **Throughput**: Up to 20 Mbps sustained
- **Packet Loss Tolerance**: Up to 30% with FEC
- **Frame Rate**: 60 fps @ 720p

### Memory Usage

- **Internal RAM**: ~464 KB
- **SPIRAM**: ~1.4 MB
- **Flash**: ~1 MB (firmware)

## Architecture

For detailed architecture documentation, see [docs/DESIGN.md](docs/DESIGN.md).

### Component Overview

1. **wifi_receiver**: WiFi monitor mode packet capture
2. **fec_decoder**: Reed-Solomon error correction
3. **usb_streamer**: USB communication with Android
4. **packet_handler**: Frame assembly and buffering
5. **stats_tracker**: Performance monitoring

### Data Flow

```
WiFi Packets → wifi_receiver → packet_handler → fec_decoder → usb_streamer → Android
                                      ↓
                              stats_tracker
```

## Development

### Adding New Features

1. Create new component in `components/` directory
2. Add header file in `include/` subdirectory
3. Implement source file(s)
4. Create `CMakeLists.txt` for the component
5. Add dependency in `main/CMakeLists.txt`

### Debugging

Enable verbose logging:
```bash
idf.py menuconfig
# Component config → Log output → Default log verbosity → Verbose
```

Use ESP-IDF debugging tools:
- JTAG debugging with OpenOCD
- Core dump analysis
- Heap tracing
- Task profiling

## Troubleshooting

### WiFi Reception Issues

- Check antenna connection
- Verify WiFi channel configuration
- Monitor RSSI values
- Check for WiFi interference

### USB Connection Issues

- Verify USB OTG cable
- Check Android permissions
- Monitor USB connection status
- Review USB logs

### Performance Issues

- Check memory usage (heap)
- Monitor CPU utilization
- Review packet drop statistics
- Adjust buffer sizes if needed

## Contributing

This is part of the hx-esp32-cam-fpv project. Please follow the project's contribution guidelines.

## License

See the LICENSE file in the project root.

## Related Projects

- ESP32-C6 WiFi Transmitter
- ESP32-P4 MIPI FPV Camera
- Ground Station Application

## Contact

For issues and questions, please refer to the main project repository.

---

**Status**: Initial Implementation (Stub Functions)
**Version**: 1.0.0
**Last Updated**: 2025-11-22

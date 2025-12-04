# ESP32 FPV Ground Station

ESP32-S3 based FPV ground station that captures video packets via WiFi promiscuous mode and provides multiple output options:
- **USB UVC (Webcam)** - Appears as a standard USB camera for use with VLC, OBS, or Android camera apps
- **USB NCM (Network)** - Provides a web interface over USB network connection
- **WiFi AP** - Optional wireless access to the web interface

## Features

- **WiFi Promiscuous Mode Capture** - Receives FPV video packets on configurable channel
- **USB CDC+NCM+UVC Composite Device** - Single USB connection provides:
  - **CDC ACM** - Serial port for status/debug output
  - **NCM** - Network interface with DHCP server (web interface access)
  - **UVC** - USB Video Class webcam (MJPEG streaming)
- **Runtime Mode Switching** - Switch between NCM and UVC modes without reboot
- **Web Interface** - MJPEG streaming via WebSocket
- **Optional WiFi AP** - Secondary access via WiFi SoftAP
- **On-the-fly Channel Switching** - Change capture channel via web UI

## USB Modes

The ESP32-S3 supports two mutually exclusive USB streaming modes (only one active at a time):

### UVC Mode (USB Webcam)

In UVC mode, the device appears as a standard USB webcam. Connect to any camera application:
- **Windows**: Camera app, VLC, OBS Studio
- **Linux**: VLC, Cheese, OBS Studio, guvcview
- **macOS**: QuickTime Player, VLC, OBS Studio
- **Android**: USB Camera apps (requires OTG adapter)

Default UVC settings:
- Resolution: 640x480
- Frame Rate: 15 fps
- Format: MJPEG
- Transfer Mode: Isochronous (~512 KB/s)

### NCM Mode (USB Network)

In NCM mode, the device appears as a USB network adapter with a built-in DHCP server:
- ESP32 IP: 192.168.7.1
- Client IP: 192.168.7.2 (assigned via DHCP)
- Access web interface at: http://192.168.7.1/

## Hardware Requirements

- ESP32-S3 based board with USB OTG support (GPIO19/20)
- USB-C/USB connection to host PC
- WiFi antenna for packet capture

## USB Interface

When connected via USB, the ESP32-S3 appears as a composite device:

| Interface | Linux Device | Description |
|-----------|--------------|-------------|
| CDC ACM   | `/dev/ttyACM0` | Serial port - stats output every 3 seconds |
| NCM       | `usb0` | Network interface - DHCP client gets 192.168.7.x |
| UVC       | `/dev/video*` | Video device - MJPEG webcam |

### CDC Serial Output

Every 3 seconds, the CDC serial port outputs:
```
=== FPV Ground Station Stats ===
USB Mode: UVC | Connected: yes
UVC: 150 frames sent, 2 dropped
Channel: 6 | RSSI: -45 dBm
Packets: 12345 recv, 12300 valid
Frames: 100 complete, 5 incomplete
WebSocket clients: 0
```

### Switching USB Modes

Via Web API (when in NCM mode or via WiFi AP):
```bash
# Get current mode
curl http://192.168.7.1/api/usb_mode

# Switch to UVC mode
curl -X POST -d '{"mode":"UVC"}' http://192.168.7.1/api/usb_mode

# Switch to NCM mode
curl -X POST -d '{"mode":"NCM"}' http://192.168.7.1/api/usb_mode

# Toggle mode
curl -X POST -d '{"mode":"toggle"}' http://192.168.7.1/api/usb_mode
```

## Building

### Prerequisites

- ESP-IDF v5.0 or later
- ESP32-S3 target

### Build Commands

```bash
# Source ESP-IDF environment
source $IDF_PATH/export.sh

# Build
cd esp32-gs
idf.py build

# Flash
idf.py flash

# Monitor UART console (if using UART for console)
idf.py monitor
```

### Configuration

Key settings in `sdkconfig.defaults`:

```
# USB CDC+NCM+UVC Composite
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_NET_MODE_NCM=y
CONFIG_TINYUSB_VIDEO_ENABLED=y

# Enable USB networking
CONFIG_FPV_GS_ENABLE_USB_NET=y

# Enable USB UVC (webcam)
CONFIG_FPV_GS_ENABLE_USB_UVC=y
CONFIG_FPV_GS_UVC_WIDTH=640
CONFIG_FPV_GS_UVC_HEIGHT=480
CONFIG_FPV_GS_UVC_FPS=15
CONFIG_FPV_GS_UVC_BULK_MODE=n
```

### UVC Configuration Options

Use `idf.py menuconfig` to configure UVC settings:

| Option | Description | Default |
|--------|-------------|---------|
| `FPV_GS_UVC_WIDTH` | Frame width (320-1280) | 640 |
| `FPV_GS_UVC_HEIGHT` | Frame height (240-720) | 480 |
| `FPV_GS_UVC_FPS` | Target frame rate (5-30) | 15 |
| `FPV_GS_UVC_BULK_MODE` | Use bulk transfer (higher throughput, Linux issues) | n |
| `FPV_GS_UVC_DEFAULT_ACTIVE` | Start in UVC mode (otherwise NCM) | n |

### Recommended Resolutions

| Resolution | FPS | Bandwidth | Notes |
|------------|-----|-----------|-------|
| 320x240 | 30 | ~300 KB/s | Low latency, good compatibility |
| 480x320 | 30 | ~400 KB/s | Balanced |
| 640x480 | 15 | ~450 KB/s | **Recommended** |
| 800x456 | 15 | ~500 KB/s | Wide aspect ratio |
| 1280x720 | 10 | ~800 KB/s | HD, bulk mode recommended |

## Architecture

```
        FPV TX (Air)
             │
             │ FPV Video Packets
             ▼
┌───────────────────────────────────────────────────────────────┐
│                          ESP32-S3                             │
│                                                               │
│  ┌──────────────────┐                                         │
│  │  WiFi Promiscuous │                                        │
│  │      Mode         │                                        │
│  └─────────┬─────────┘                                        │
│            │                                                  │
│            ▼                                                  │
│  ┌──────────────────┐                                         │
│  │   Frame Buffer   │◄────────────────────────┐               │
│  │  (JPEG Frames)   │                         │               │
│  └────┬─────────┬───┘                         │               │
│       │         │                             │               │
│       ▼         ▼                             │               │
│  ┌─────────┐  ┌─────────┐                     │               │
│  │   UVC   │  │   Web   │                     │               │
│  │ Stream  │  │ Server  │                     │               │
│  └────┬────┘  └────┬────┘                     │               │
│       │            │                          │               │
│       ▼            ▼                          │               │
│  ┌─────────┐  ┌─────────┐  ┌─────────────────┐│               │
│  │   UVC   │  │   NCM   │  │  WiFi SoftAP    ││ (optional)    │
│  │(Webcam) │  │(Network)│  │  192.168.4.1    ││               │
│  └────┬────┘  └────┬────┘  └────────┬────────┘│               │
│       │            │                │         │               │
│       └──────┬─────┘                │         │               │
│              │ TinyUSB              │         │               │
│              │ Composite            │         │               │
│              │ + CDC ACM            │         │               │
└──────────────┼──────────────────────┼─────────┘               │
               │                      │                         │
               ▼                      ▼                         │
        USB to Host PC          WiFi Clients                    │
        ├─ /dev/video* (UVC)   (Phone/Laptop)                   │
        ├─ usb0 (NCM)                                           │
        └─ /dev/ttyACM0 (CDC)                                   │
```

## Files

| File | Description |
|------|-------------|
| `main/main.c` | Application entry, main loop with stats output |
| `main/usb_device.c` | USB composite device manager (CDC+NCM+UVC) |
| `main/usb_uvc.c` | UVC webcam implementation |
| `main/usb_network.h` | USB network compatibility API |
| `main/wifi_manager.c` | WiFi promiscuous mode and optional AP |
| `main/packet_rx.c` | Packet reception and parsing |
| `main/frame_buffer.c` | Frame assembly and buffering |
| `main/web_server.c` | HTTP server and WebSocket streaming |
| `main/config_manager.c` | NVS configuration storage |

## API Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Web interface |
| `/ws` | WebSocket | MJPEG video stream |
| `/api/config` | GET/POST | Configuration |
| `/api/stats` | GET | Statistics |
| `/api/channel` | POST | Change WiFi channel |
| `/api/usb_mode` | GET/POST | Get/Set USB mode (NCM/UVC) |

## Troubleshooting

### UVC Not Working

1. Check USB enumeration:
   ```bash
   dmesg | grep -i uvc
   # Should see: uvcvideo driver loading
   ```

2. List video devices:
   ```bash
   v4l2-ctl --list-devices
   ```

3. Test with VLC:
   ```bash
   vlc v4l2:///dev/video0
   ```

4. If using Linux, try isochronous mode (default) as bulk mode may have issues.

### USB Network Not Working

1. Check `dmesg` for USB enumeration:
   ```bash
   dmesg | tail -20
   # Should see: cdc_acm, cdc_ncm drivers loading
   ```

2. Verify network interface:
   ```bash
   ip link show usb0
   ```

3. If no IP assigned, manually configure:
   ```bash
   sudo ip addr add 192.168.7.2/24 dev usb0
   sudo ip link set usb0 up
   ```

### CDC Serial Not Visible

1. Check device exists:
   ```bash
   ls /dev/ttyACM*
   ```

2. Open with terminal:
   ```bash
   picocom -b 115200 /dev/ttyACM0
   # or
   screen /dev/ttyACM0 115200
   ```

### No Packets Received

1. Verify channel matches transmitter
2. Check antenna connection
3. Monitor stats for RSSI values

### Mode Switching Issues

1. If stuck, access via WiFi AP (192.168.4.1) if enabled
2. Or use CDC serial to monitor current mode
3. Reset device to return to default mode

## Platform Compatibility

| Platform | UVC Isochronous | UVC Bulk | NCM |
|----------|-----------------|----------|-----|
| Windows 10/11 | Yes | Yes | Yes |
| macOS | Yes | Yes | Yes |
| Linux | Yes | Partial* | Yes |
| Android (OTG) | Yes | Yes | No |

*Bulk mode may have compatibility issues on some Linux systems.

## License

MIT License

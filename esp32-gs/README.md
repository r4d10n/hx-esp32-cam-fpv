# ESP32 FPV Ground Station

ESP32-S3 based FPV ground station that captures video packets via WiFi promiscuous mode and provides a web interface over USB network (CDC+NCM composite device).

## Features

- **WiFi Promiscuous Mode Capture** - Receives FPV video packets on configurable channel
- **USB CDC+NCM Composite Device** - Single USB connection provides:
  - CDC ACM serial port for status/debug output
  - NCM network interface with DHCP server
- **Web Interface** - MJPEG streaming via WebSocket
- **Optional WiFi AP** - Secondary access via WiFi SoftAP
- **On-the-fly Channel Switching** - Change capture channel via web UI

## Hardware Requirements

- ESP32-S3 based board with USB OTG support
- USB-C/USB connection to host PC

## USB Interface

When connected via USB, the ESP32-S3 appears as a composite device:

| Interface | Linux Device | Description |
|-----------|--------------|-------------|
| CDC ACM   | `/dev/ttyACM0` | Serial port - stats output every 3 seconds |
| NCM       | `usb0` | Network interface - DHCP client gets 192.168.7.x |

### CDC Serial Output

Every 3 seconds, the CDC serial port outputs:
```
=== FPV Ground Station Stats ===
ESP32 IP: 192.168.7.1 | Client IP: 192.168.7.2
Channel: 6 | RSSI: -45 dBm
Packets: 12345 recv, 12300 valid
Frames: 100 complete, 5 incomplete
WebSocket clients: 1
```

### Network Access

After connecting USB:
```bash
# Check network interface
ip addr show usb0

# Access web interface
curl http://192.168.7.1/
# or open in browser: http://192.168.7.1/
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
# USB CDC+NCM Composite
CONFIG_TINYUSB_CDC_ENABLED=y
CONFIG_TINYUSB_NET_MODE_NCM=y

# USB Network IP (different from WiFi AP)
CONFIG_USB_NETIF_DEFAULT_IP="192.168.7.1"

# Enable USB networking
CONFIG_FPV_GS_ENABLE_USB_NET=y
```

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32-S3                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐     │
│  │   WiFi      │  │  TinyUSB    │  │  Web Server │     │
│  │ Promiscuous │  │  CDC + NCM  │  │  (HTTP/WS)  │     │
│  └──────┬──────┘  └──────┬──────┘  └──────┬──────┘     │
│         │                │                │             │
│         ▼                ▼                ▼             │
│  ┌─────────────────────────────────────────────────┐   │
│  │              Frame Buffer + Stats                │   │
│  └─────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
         │                │
         ▼                ▼
   FPV Packets      USB to Host PC
   (Air)            (CDC Serial + NCM Network)
```

## Files

| File | Description |
|------|-------------|
| `main/main.c` | Application entry, main loop with stats output |
| `main/usb_network.c` | USB CDC+NCM composite initialization |
| `main/wifi_manager.c` | WiFi promiscuous mode and optional AP |
| `main/packet_rx.c` | Packet reception and parsing |
| `main/frame_buffer.c` | Frame assembly and buffering |
| `main/web_server.c` | HTTP server and WebSocket streaming |
| `main/config_manager.c` | NVS configuration storage |

## Troubleshooting

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

## License

MIT License

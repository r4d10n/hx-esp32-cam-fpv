# ESP32-C5 WiFi Transmitter Firmware

This firmware runs on the ESP32-C5 and handles WiFi 6 transmission for the high-resolution FPV system.

## Features

- **SPI Slave:** Receives video data from ESP32-P4 via high-speed SPI
- **FEC Encoding:** Forward error correction (6/12 default)
- **WiFi 6 Transmission:** 802.11ax packet injection on 2.4GHz or 5GHz
- **Statistics:** Real-time monitoring of throughput and packet loss

## Hardware Requirements

- ESP32-C5-DevKitC-1
- SPI connection to ESP32-P4 (MOSI, MISO, CLK, CS, Handshake)

## Wiring

### SPI Connection (ESP32-C5 <-> ESP32-P4)

| ESP32-C5 Pin | ESP32-P4 Pin | Function |
|--------------|--------------|----------|
| GPIO6 | MOSI | SPI MOSI |
| GPIO2 | MISO | SPI MISO |
| GPIO4 | CLK | SPI CLK |
| GPIO5 | CS | SPI CS |
| GPIO3 | Handshake | Ready signal |
| GND | GND | Ground |

## Building

```bash
cd esp32-c5-wifi-transmitter
idf.py set-target esp32c5
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

Edit `main/main.c` to configure:

- WiFi band (2.4GHz/5GHz)
- WiFi channel
- TX power
- MCS index
- FEC parameters

## Statistics

The firmware outputs statistics every 5 seconds:

```
RX: 60.0 pkt/s (300 total)
TX: 60.0 pkt/s (300 total)
WiFi: 12.50 Mbps, 300 packets, queue: 15%
IPC: 50.00 Mbps, 300 packets, 0 errors
```

## Performance

| Metric | Value |
|--------|-------|
| SPI Speed | 50 MHz |
| WiFi 6 MCS7 | ~86 Mbps |
| Throughput | 10-15 Mbps (with FEC 6/12) |
| Latency | <10ms (IPC + WiFi) |

## Troubleshooting

**IPC not ready:**
- Check SPI wiring
- Verify ESP32-P4 is running and initialized
- Check handshake pin state

**WiFi TX errors:**
- Check channel availability
- Verify TX power settings
- Ensure antenna is connected

**High packet loss:**
- Reduce bitrate
- Increase FEC redundancy (e.g., 6/16)
- Check for WiFi interference

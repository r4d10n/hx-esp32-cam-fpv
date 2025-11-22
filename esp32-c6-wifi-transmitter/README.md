# ESP32-C6 WiFi Transmitter Firmware

This firmware runs on the ESP32-C6 and handles WiFi 6 transmission for the high-resolution FPV system.

## Why ESP32-C6?

The ESP32-C6 is integrated on the **ESP32-P4 evaluation board**, making it a convenient and cost-effective choice for development. It offers similar capabilities to the ESP32-C5:

- **WiFi 6 (802.11ax)** support with high throughput
- **Bluetooth 5.3** for control and telemetry
- **RISC-V 32-bit processor** @ 160 MHz
- **Low power consumption**
- **Integrated on ESP32-P4 eval board** - no additional hardware needed

## Features

- **SPI Slave:** Receives video data from ESP32-P4 via high-speed SPI
- **FEC Encoding:** Forward error correction (6/12 default)
- **WiFi 6 Transmission:** 802.11ax packet injection on 2.4GHz or 5GHz
- **Priority Queue:** Separate queues for keyframes, normal frames, and telemetry
- **Statistics:** Real-time monitoring of throughput and packet loss
- **Dynamic Configuration:** Runtime channel, MCS, and power adjustments

## Hardware Requirements

- **ESP32-C6-DevKitC-1** or **ESP32-P4 Evaluation Board** (includes ESP32-C6)
- SPI connection to ESP32-P4 (MOSI, MISO, CLK, CS, Handshake)

## ESP32-P4 Evaluation Board Setup

If using the ESP32-P4 evaluation board, the ESP32-C6 is already present on the board. You'll need to:

1. Connect the SPI pins between ESP32-P4 and ESP32-C6 (see wiring below)
2. Flash the ESP32-C6 with this firmware
3. Flash the ESP32-P4 with the main FPV firmware

## Wiring

### SPI Connection (ESP32-C6 <-> ESP32-P4)

| ESP32-C6 Pin | ESP32-P4 Pin | Function | Description |
|--------------|--------------|----------|-------------|
| GPIO6 | MOSI | SPI MOSI | Data from P4 to C6 |
| GPIO2 | MISO | SPI MISO | Data from C6 to P4 |
| GPIO4 | CLK | SPI CLK | Clock signal |
| GPIO5 | CS | SPI CS | Chip select |
| GPIO3 | Handshake | Ready | Ready/handshake signal |
| GND | GND | Ground | Common ground |

### WiFi Antenna

Connect a 5GHz-capable antenna to the ESP32-C6's antenna connector for optimal range and performance.

## Building

```bash
cd esp32-c6-wifi-transmitter
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Configuration

Edit `main/main.c` to configure:

```c
// WiFi Configuration
#define WIFI_BAND           WIFI_C6_BAND_5GHZ    // 2.4GHz or 5GHz
#define WIFI_CHANNEL        149                  // Channel number
#define WIFI_MCS            WIFI_C6_MCS_7        // MCS0-MCS11
#define WIFI_TX_POWER_DBM   20                   // 5-20 dBm

// FEC Configuration
#define ENABLE_FEC          true
#define FEC_K               6                     // Data blocks
#define FEC_N               12                    // Total blocks (50% redundancy)

// Queue Configuration
#define TX_QUEUE_SIZE       128                   // Packet queue size
```

### WiFi Channels

**2.4GHz Band:**
- Channels: 1-14
- Recommended: 1, 6, 11 (non-overlapping)
- Range: Better penetration through obstacles
- Bandwidth: Lower (~50 Mbps max with WiFi 6)

**5GHz Band:**
- Channels: 36, 40, 44, 48, 149, 153, 157, 161, 165
- Recommended: 149, 153, 157, 161 (DFS-free)
- Range: Lower, but less interference
- Bandwidth: Higher (~200+ Mbps with WiFi 6)

### MCS Selection

| MCS | Modulation | Code Rate | 20MHz BW | 40MHz BW | Use Case |
|-----|------------|-----------|----------|----------|----------|
| 0 | BPSK | 1/2 | 8.6 Mbps | 18 Mbps | Max range |
| 3 | 16-QAM | 1/2 | 34 Mbps | 72 Mbps | Balanced |
| 7 | 64-QAM | 5/6 | 86 Mbps | 180 Mbps | Good quality |
| 9 | 256-QAM | 5/6 | 129 Mbps | 270 Mbps | Excellent quality |
| 11 | 1024-QAM | 5/6 | 143 Mbps | 300 Mbps | Optimal conditions |

**Recommendation:** Start with MCS 7 for best quality/range balance.

## Statistics

The firmware outputs statistics every 5 seconds:

```
=== Statistics (5s interval) ===
IPC RX: 60.0 pkt/s, 12.50 Mbps, 300 total, 0 errors
Packets: video=280, config=10, telemetry=10
WiFi TX: 15.20 Mbps, 300 packets, 0 failed, queue=15%
FEC: 150 redundancy blocks sent
```

**Metrics:**
- **IPC RX:** Packets and data received from ESP32-P4 via SPI
- **Packets:** Breakdown by type (video, config, telemetry)
- **WiFi TX:** Throughput, success rate, and queue usage
- **FEC:** Redundancy blocks sent for error correction

## Performance

### Expected Throughput

| Configuration | Video Bitrate | FEC Overhead | WiFi Throughput | Latency |
|---------------|---------------|--------------|-----------------|---------|
| 1080p60 MCS7 | 6 Mbps | 6 Mbps (50%) | 12 Mbps | <10ms |
| 1080p60 MCS9 | 10 Mbps | 10 Mbps (50%) | 20 Mbps | <8ms |
| 1440p60 MCS9 | 15 Mbps | 15 Mbps (50%) | 30 Mbps | <12ms |
| 4K30 MCS11 | 20 Mbps | 20 Mbps (50%) | 40 Mbps | <15ms |

### Latency Breakdown

| Stage | Latency | Notes |
|-------|---------|-------|
| SPI Transfer (P4→C6) | 1-2 ms | @ 50 MHz |
| FEC Encoding | 0.5-1 ms | Hardware accelerated |
| WiFi TX Queue | 1-3 ms | Depends on queue size |
| WiFi Transmission | 2-5 ms | Depends on MCS/channel |
| **Total (P4→GS)** | **5-11 ms** | Excellent for FPV |

## Troubleshooting

### IPC not ready

**Symptoms:**
```
Waiting for IPC connection from ESP32-P4...
```

**Solutions:**
- Check SPI wiring (MOSI, MISO, CLK, CS, GND)
- Verify ESP32-P4 is running and initialized
- Check handshake pin (GPIO3) state
- Ensure both devices share common ground
- Verify SPI frequency matches on both sides (50 MHz)

### WiFi TX errors

**Symptoms:**
```
Failed to send video packet: -1
WiFi TX: 0.00 Mbps, 0 packets, 100 failed
```

**Solutions:**
- Check WiFi channel availability (use WiFi analyzer app)
- Verify TX power settings (5-20 dBm)
- Ensure antenna is properly connected
- Check for WiFi interference (switch channel)
- Verify MCS is appropriate for distance

### High packet loss at ground station

**Symptoms:**
- Video stuttering or artifacts
- Packet loss > 10%

**Solutions:**
- **Reduce bitrate:** Lower video bitrate on ESP32-P4
- **Increase FEC redundancy:** Change to FEC 6/16 or 6/18
- **Lower MCS:** Use MCS 5 or MCS 7 instead of MCS 9/11
- **Increase TX power:** Set to maximum (20 dBm)
- **Change channel:** Use less congested channel
- **Improve antenna:** Use directional or higher-gain antenna
- **Reduce distance:** Move closer to ground station

### Queue overflow

**Symptoms:**
```
TX queue full, packet dropped
WiFi TX: queue=95%
```

**Solutions:**
- Increase `TX_QUEUE_SIZE` (e.g., 256)
- Lower video bitrate on ESP32-P4
- Increase WiFi MCS for higher throughput
- Check if WiFi transmission is bottlenecked

## Advanced Configuration

### Dynamic Channel Switching

```c
// Switch channel at runtime
wifi_c6_tx_set_channel(153);
```

### Adaptive MCS

Implement adaptive MCS based on packet loss:

```c
wifi_c6_tx_stats_t stats;
wifi_c6_tx_get_stats(&stats);

float loss_rate = (float)stats.packets_failed / stats.packets_sent;

if (loss_rate > 0.1) {
    // Reduce MCS for better reliability
    wifi_c6_tx_set_mcs(WIFI_C6_MCS_5);
} else if (loss_rate < 0.01) {
    // Increase MCS for higher throughput
    wifi_c6_tx_set_mcs(WIFI_C6_MCS_9);
}
```

### Power Saving

For battery-powered systems, reduce TX power when close to ground station:

```c
// Reduce power to save battery
wifi_c6_tx_set_power(10); // 10 dBm instead of 20 dBm
```

## Comparison: ESP32-C5 vs ESP32-C6

| Feature | ESP32-C5 | ESP32-C6 |
|---------|----------|----------|
| CPU | RISC-V @ 240 MHz | RISC-V @ 160 MHz |
| WiFi | 802.11ax (WiFi 6) | 802.11ax (WiFi 6) |
| Bluetooth | BLE 5.3 | BLE 5.3 |
| Max WiFi Throughput | ~300 Mbps | ~250 Mbps |
| On P4 Eval Board | ❌ No | ✅ Yes |
| Power Consumption | Lower | Lower |
| Cost | Similar | Similar |
| Availability | Limited | Widely available |

**Recommendation:** Use ESP32-C6 if you have the ESP32-P4 evaluation board. Use ESP32-C5 for standalone designs with maximum performance.

## Integration with ESP32-P4

The ESP32-C6 receives video data from the ESP32-P4 via the IPC (Inter-Processor Communication) layer:

1. **ESP32-P4** captures video from MIPI camera
2. **ESP32-P4** encodes to H.264 using hardware encoder
3. **ESP32-P4** sends NAL units to ESP32-C6 via SPI (IPC master)
4. **ESP32-C6** receives NAL units (IPC slave)
5. **ESP32-C6** applies FEC encoding
6. **ESP32-C6** transmits over WiFi 6
7. **Ground Station** receives, decodes, and displays

## Next Steps

After flashing this firmware:

1. Flash ESP32-P4 with main FPV firmware
2. Connect MIPI camera to ESP32-P4
3. Power on both ESP32-P4 and ESP32-C6
4. Verify IPC connection in serial monitor
5. Start ground station receiver
6. Enjoy low-latency HD FPV!

## License

See main project LICENSE file.

## Support

For issues and questions, please refer to the main project documentation or open an issue on GitHub.

# FEC Decoder Component for ESP32-S3

Reed-Solomon Forward Error Correction decoder optimized for ESP32-S3 WiFi video streaming.

## Features

- **Reed-Solomon FEC decoding** compatible with transmitter encoding (6/12, 8/16, etc.)
- **Variable K/N ratios** for flexible error correction
- **Block assembly** with automatic packet ordering
- **Performance optimized** for ESP32-S3 (84 Mbps throughput with FEC)
- **Statistics tracking** (corrected blocks, uncorrectable errors, etc.)
- **Thread-safe** operation with mutex protection
- **Memory efficient** (~13.5 KB RAM footprint)

## Quick Start

### 1. Include Component

Add to your `CMakeLists.txt`:
```cmake
idf_component_register(
    ...
    REQUIRES fec_decoder
)
```

### 2. Basic Usage

```c
#include "fec_decoder.h"

// Callback for decoded data
void decoded_data_callback(const void *data, size_t size, void *user_ctx)
{
    // Process decoded packet
    printf("Received %d bytes\n", size);
}

void app_main(void)
{
    // Get default configuration
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = 6;   // 6 data packets per block
    config.coding_n = 12;  // 12 total packets (6 data + 6 FEC)
    config.mtu = 1400;     // Packet payload size
    
    // Create decoder
    fec_decoder_handle_t decoder;
    esp_err_t ret = fec_decoder_create(&config, decoded_data_callback, NULL, &decoder);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create decoder");
        return;
    }
    
    // Process incoming packets (from WiFi receiver)
    while (1) {
        uint8_t packet[1500];
        size_t len = receive_packet(packet, sizeof(packet));
        
        ret = fec_decoder_process_packet(decoder, packet, len);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Packet processing failed");
        }
    }
    
    // Cleanup
    fec_decoder_destroy(decoder);
}
```

## API Reference

### Configuration

```c
typedef struct {
    uint8_t coding_k;           // Number of data packets per block
    uint8_t coding_n;           // Total packets per block (data + FEC)
    uint16_t mtu;               // Maximum transmission unit
    uint8_t max_blocks_pending; // Maximum blocks to buffer
    bool enable_stats;          // Enable statistics tracking
} fec_decoder_config_t;

// Get default configuration (K=6, N=12, MTU=1400)
fec_decoder_config_t fec_decoder_get_default_config(void);
```

### Decoder Management

```c
// Create decoder
esp_err_t fec_decoder_create(
    const fec_decoder_config_t *config,
    fec_decoder_data_cb_t data_callback,
    void *user_ctx,
    fec_decoder_handle_t *handle
);

// Destroy decoder
esp_err_t fec_decoder_destroy(fec_decoder_handle_t handle);

// Process incoming packet
esp_err_t fec_decoder_process_packet(
    fec_decoder_handle_t handle,
    const void *data,
    size_t size
);
```

### Statistics

```c
typedef struct {
    uint32_t blocks_received;        // Total blocks received
    uint32_t blocks_decoded;         // Blocks decoded with FEC
    uint32_t blocks_complete;        // Blocks received complete
    uint32_t blocks_uncorrectable;   // Uncorrectable errors
    uint32_t packets_received;       // Total packets
    uint32_t packets_corrected;      // Packets recovered via FEC
    uint32_t packets_duplicate;      // Duplicate packets
    uint32_t packets_old;            // Old/late packets
    uint64_t total_bytes_decoded;    // Total bytes decoded
} fec_decoder_stats_t;

// Get statistics
esp_err_t fec_decoder_get_stats(
    fec_decoder_handle_t handle,
    fec_decoder_stats_t *stats
);

// Reset statistics
esp_err_t fec_decoder_reset_stats(fec_decoder_handle_t handle);
```

### Advanced Functions

```c
// Update coding parameters dynamically
esp_err_t fec_decoder_update_coding(
    fec_decoder_handle_t handle,
    uint8_t coding_k,
    uint8_t coding_n
);

// Flush pending blocks
esp_err_t fec_decoder_flush(fec_decoder_handle_t handle);

// Get current block index
esp_err_t fec_decoder_get_current_block(
    fec_decoder_handle_t handle,
    uint32_t *block_index
);
```

## Packet Format

The decoder expects packets with the following header (must match transmitter):

```c
typedef struct __attribute__((packed)) {
    uint8_t packet_version;     // Must be 2
    uint8_t packet_signature;   // Must be 56
    uint16_t from_device_id;    // Source device ID
    uint16_t to_device_id;      // Destination device ID
    uint16_t size;              // Payload size in bytes
    uint32_t block_index : 24;  // Block sequence number
    uint32_t packet_index : 8;  // Packet index within block
} fec_packet_header_t;
```

Header size: 12 bytes

## Performance

- **Throughput**: 84 Mbps with FEC decoding
- **Latency**: <1 ms per block
- **CPU Usage**: ~25% @ 240 MHz
- **Memory**: 13.5 KB RAM

See [PERFORMANCE_ANALYSIS.md](PERFORMANCE_ANALYSIS.md) for detailed benchmarks.

## Configuration Examples

### Low Latency (FPV Racing)
```c
config.coding_k = 4;
config.coding_n = 8;
config.mtu = 1024;
```

### Balanced (Video Streaming)
```c
config.coding_k = 6;
config.coding_n = 12;
config.mtu = 1400;
```

### High Reliability (Long Range)
```c
config.coding_k = 8;
config.coding_n = 16;
config.mtu = 1400;
```

## Testing

Run unit tests:
```bash
cd esp32-s3-android-receiver
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

## Troubleshooting

### High Packet Loss
- Check `blocks_uncorrectable` in statistics
- Consider increasing N (more FEC packets)
- Verify WiFi signal strength

### High Latency
- Reduce K (smaller blocks)
- Check CPU utilization
- Disable statistics if not needed

### Memory Issues
- Reduce `max_blocks_pending`
- Use PSRAM for buffers (with performance trade-off)
- Check for memory leaks in callback

## Integration with WiFi Receiver

Example integration:

```c
#include "esp_wifi.h"
#include "fec_decoder.h"

static fec_decoder_handle_t s_decoder;

void wifi_rx_callback(void *buffer, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_DATA) return;
    
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buffer;
    
    // Process packet through FEC decoder
    fec_decoder_process_packet(s_decoder, pkt->payload, pkt->rx_ctrl.sig_len);
}

void app_main(void)
{
    // Initialize WiFi
    esp_wifi_init(&wifi_init_config);
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(wifi_rx_callback);
    
    // Initialize FEC decoder
    fec_decoder_config_t config = fec_decoder_get_default_config();
    fec_decoder_create(&config, decoded_callback, NULL, &s_decoder);
    
    esp_wifi_start();
}
```

## License

See main project LICENSE file.

## Contributing

Please report issues or submit pull requests to the main repository.

## See Also

- [PERFORMANCE_ANALYSIS.md](PERFORMANCE_ANALYSIS.md) - Detailed performance analysis
- [../../components/common/fec.h](../../components/common/fec.h) - Reed-Solomon implementation
- WiFi transmitter FEC encoder in `esp32-c5-wifi-transmitter`

# WiFi Receiver Component for ESP32-S3

A comprehensive WiFi receiver component for ESP32-S3 that provides packet reception using promiscuous mode (monitor mode). This component is designed for FPV video reception and telemetry applications.

## Features

- **WiFi Promiscuous Mode**: Full packet capture on configured channels
- **Multi-Band Support**: 2.4GHz and 5GHz band operation
- **Channel Management**: 
  - Single channel monitoring
  - Multi-channel scanning
  - Automatic channel hopping
- **Packet Filtering**:
  - MAC address filtering
  - CRC error filtering
  - Packet type filtering (Data/Management/Control)
- **Signal Quality Tracking**:
  - RSSI monitoring (min/avg/max)
  - Noise floor measurement
  - Per-packet signal metrics
- **Statistics**:
  - Packet counts (received/valid/dropped/CRC errors)
  - Throughput measurement (Mbps)
  - Buffer usage monitoring
- **Flexible Callback System**: User-defined packet handlers
- **Thread-Safe Operation**: FreeRTOS queues and mutexes

## Architecture

```
┌─────────────────┐
│  WiFi Hardware  │
│   (ESP32-S3)    │
└────────┬────────┘
         │ Promiscuous Mode
         ▼
┌─────────────────┐
│  RX Callback    │◄─── WiFi Interrupt
│   (ISR Safe)    │
└────────┬────────┘
         │ Queue
         ▼
┌─────────────────┐
│   RX Task       │◄─── FreeRTOS Task
│ (Packet Process)│
└────────┬────────┘
         │ User Callback
         ▼
┌─────────────────┐
│ Application     │
│   Handler       │
└─────────────────┘
```

## API Overview

### Initialization

```c
wifi_rx_config_t config = {
    .band = WIFI_RX_BAND_2_4GHZ,
    .channel = 6,
    .filter_mac = {0, 0, 0, 0, 0, 0},  // No MAC filtering
    .enable_promiscuous = true,
    .filter_crc_errors = true,
    .callback = packet_handler,
    .callback_ctx = NULL,
    .rx_buffer_size = 32,
};

esp_err_t ret = wifi_rx_init(&config);
```

### Start/Stop Reception

```c
esp_err_t wifi_rx_start(void);
esp_err_t wifi_rx_stop(void);
esp_err_t wifi_rx_deinit(void);
```

### Channel Management

```c
esp_err_t wifi_rx_set_channel(uint8_t channel);
esp_err_t wifi_rx_get_channel(uint8_t *channel);

// Channel hopping
uint8_t channels[] = {1, 6, 11};
esp_err_t wifi_rx_enable_channel_hopping(channels, 3, 100);  // 100ms dwell
esp_err_t wifi_rx_disable_channel_hopping(void);
```

### Statistics

```c
wifi_rx_stats_t stats;
esp_err_t wifi_rx_get_stats(&stats);
esp_err_t wifi_rx_reset_stats(void);
```

### Signal Quality

```c
int8_t rssi;
esp_err_t wifi_rx_get_rssi(&rssi);

int8_t noise_floor;
esp_err_t wifi_rx_get_noise_floor(&noise_floor);
```

## Usage Examples

### Example 1: Simple Packet Monitor

```c
#include "wifi_receiver.h"
#include "esp_log.h"

static const char *TAG = "WIFI_MON";

static void packet_callback(const wifi_rx_packet_info_t *pkt, void *ctx)
{
    ESP_LOGI(TAG, "Packet: len=%zu, rssi=%d dBm, channel=%d",
             pkt->payload_len, pkt->rssi, pkt->channel);
}

void app_main(void)
{
    wifi_rx_config_t config = {
        .band = WIFI_RX_BAND_2_4GHZ,
        .channel = 6,
        .filter_mac = {0, 0, 0, 0, 0, 0},
        .enable_promiscuous = true,
        .filter_crc_errors = true,
        .callback = packet_callback,
        .callback_ctx = NULL,
        .rx_buffer_size = 64,
    };

    ESP_ERROR_CHECK(wifi_rx_init(&config));
    ESP_ERROR_CHECK(wifi_rx_start());

    // Monitor for 60 seconds
    vTaskDelay(pdMS_TO_TICKS(60000));

    wifi_rx_stats_t stats;
    wifi_rx_get_stats(&stats);
    ESP_LOGI(TAG, "Packets: %llu, RSSI: %d dBm, Throughput: %.2f Mbps",
             stats.packets_received, stats.avg_rssi_dbm, stats.throughput_mbps);

    wifi_rx_stop();
    wifi_rx_deinit();
}
```

### Example 2: Channel Scanner

```c
#include "wifi_receiver.h"
#include "esp_log.h"

static const char *TAG = "CHANNEL_SCAN";

static void packet_callback(const wifi_rx_packet_info_t *pkt, void *ctx)
{
    // Count packets per channel
    uint32_t *counts = (uint32_t *)ctx;
    counts[pkt->channel]++;
}

void app_main(void)
{
    uint32_t channel_counts[14] = {0};

    wifi_rx_config_t config = {
        .band = WIFI_RX_BAND_2_4GHZ,
        .channel = 1,
        .filter_mac = {0, 0, 0, 0, 0, 0},
        .enable_promiscuous = true,
        .filter_crc_errors = true,
        .callback = packet_callback,
        .callback_ctx = channel_counts,
        .rx_buffer_size = 128,
    };

    ESP_ERROR_CHECK(wifi_rx_init(&config));
    ESP_ERROR_CHECK(wifi_rx_start());

    // Enable channel hopping across all 2.4GHz channels
    uint8_t channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    ESP_ERROR_CHECK(wifi_rx_enable_channel_hopping(channels, 13, 500));

    // Scan for 30 seconds
    vTaskDelay(pdMS_TO_TICKS(30000));

    // Print results
    for (int i = 1; i <= 13; i++) {
        ESP_LOGI(TAG, "Channel %d: %lu packets", i, channel_counts[i]);
    }

    wifi_rx_disable_channel_hopping();
    wifi_rx_stop();
    wifi_rx_deinit();
}
```

### Example 3: MAC Address Filter

```c
#include "wifi_receiver.h"
#include "esp_log.h"

static const char *TAG = "MAC_FILTER";

static void packet_callback(const wifi_rx_packet_info_t *pkt, void *ctx)
{
    ESP_LOGI(TAG, "Packet from target: len=%zu, rssi=%d dBm",
             pkt->payload_len, pkt->rssi);
}

void app_main(void)
{
    // Target MAC address
    uint8_t target_mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

    wifi_rx_config_t config = {
        .band = WIFI_RX_BAND_2_4GHZ,
        .channel = 6,
        .filter_mac = {0, 0, 0, 0, 0, 0},
        .enable_promiscuous = true,
        .filter_crc_errors = true,
        .callback = packet_callback,
        .callback_ctx = NULL,
        .rx_buffer_size = 32,
    };

    ESP_ERROR_CHECK(wifi_rx_init(&config));

    // Set MAC filter
    ESP_ERROR_CHECK(wifi_rx_set_mac_filter(target_mac));

    ESP_ERROR_CHECK(wifi_rx_start());

    // Monitor indefinitely
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

### Example 4: FPV Video Reception

```c
#include "wifi_receiver.h"
#include "esp_log.h"

static const char *TAG = "FPV_RX";

typedef struct {
    uint8_t frame_data[65536];
    size_t frame_size;
    uint32_t packets_received;
} fpv_context_t;

static void fpv_packet_callback(const wifi_rx_packet_info_t *pkt, void *ctx)
{
    fpv_context_t *fpv = (fpv_context_t *)ctx;

    // Only accept data packets with good RSSI
    if (pkt->pkt_type == WIFI_RX_PKT_DATA && pkt->rssi > -70) {
        // Extract video payload (skip WiFi headers)
        if (pkt->payload_len > 24) {
            size_t video_len = pkt->payload_len - 24;
            memcpy(&fpv->frame_data[fpv->frame_size],
                   &pkt->payload[24], video_len);
            fpv->frame_size += video_len;
            fpv->packets_received++;
        }
    }
}

void app_main(void)
{
    fpv_context_t fpv = {0};

    wifi_rx_config_t config = {
        .band = WIFI_RX_BAND_5GHZ,
        .channel = 149,  // 5GHz channel
        .filter_mac = {0, 0, 0, 0, 0, 0},
        .enable_promiscuous = true,
        .filter_crc_errors = true,
        .callback = fpv_packet_callback,
        .callback_ctx = &fpv,
        .rx_buffer_size = 128,
    };

    ESP_ERROR_CHECK(wifi_rx_init(&config));
    ESP_ERROR_CHECK(wifi_rx_start());

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));

        wifi_rx_stats_t stats;
        wifi_rx_get_stats(&stats);

        ESP_LOGI(TAG, "Video: %zu bytes, %lu pkts, RSSI: %d dBm, %.2f Mbps",
                 fpv.frame_size, fpv.packets_received,
                 stats.avg_rssi_dbm, stats.throughput_mbps);
    }
}
```

## Channel Configuration

### 2.4GHz Channels (WIFI_RX_BAND_2_4GHZ)
- Channels: 1-14
- Common channels: 1, 6, 11 (non-overlapping)
- Typical range: ~100m outdoor, ~30m indoor

### 5GHz Channels (WIFI_RX_BAND_5GHZ)
- Channels: 36, 40, 44, 48, 52, 56, 60, 64, 100-144, 149-165
- Higher bandwidth, less interference
- Shorter range than 2.4GHz

## Performance Characteristics

### Throughput
- 2.4GHz: Up to 54 Mbps (802.11g/n)
- 5GHz: Up to 866 Mbps (802.11ac)
- Actual throughput depends on MCS, channel width, and signal quality

### Latency
- Packet capture: <1ms
- Queue processing: ~10ms (configurable)
- Total end-to-end: ~10-20ms

### Memory Usage
- Static: ~2KB
- Queue buffer: `rx_buffer_size * 2.5KB`
- Example: 32 buffers = ~80KB RAM

## Thread Safety

- All API functions are thread-safe
- Statistics protected by mutex
- Callbacks executed in RX task context
- User callbacks should not block

## Error Handling

```c
esp_err_t ret = wifi_rx_init(&config);
if (ret != ESP_OK) {
    if (ret == ESP_ERR_INVALID_ARG) {
        ESP_LOGE(TAG, "Invalid configuration");
    } else if (ret == ESP_ERR_NO_MEM) {
        ESP_LOGE(TAG, "Out of memory");
    } else if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Already initialized");
    }
}
```

## Best Practices

1. **Buffer Sizing**: Set `rx_buffer_size` based on expected packet rate
   - Low traffic: 16-32 buffers
   - High traffic: 64-128 buffers
   - FPV video: 128-256 buffers

2. **Callback Performance**: Keep callbacks fast and non-blocking
   - Avoid heavy processing
   - Use queues to defer work
   - Don't call blocking functions

3. **Channel Selection**:
   - Use WiFi analyzer to find least congested channel
   - 2.4GHz: Prefer channels 1, 6, or 11
   - 5GHz: Use DFS channels when possible

4. **RSSI Thresholds**:
   - Good: > -50 dBm
   - Fair: -50 to -70 dBm
   - Poor: < -70 dBm

5. **Error Handling**:
   - Check return values
   - Monitor statistics for dropped packets
   - Adjust buffer size if drops occur

## Testing

Run unit tests:
```bash
cd esp32-s3-android-receiver
idf.py menuconfig  # Enable testing
idf.py build
idf.py flash monitor
```

## Dependencies

- ESP-IDF v4.4 or later
- ESP32-S3 with WiFi support
- FreeRTOS
- NVS Flash

## License

See project root LICENSE file.

## Support

For issues and questions, please refer to the main project documentation.

/**
 * @file main.c
 * @brief ESP32-C6 WiFi Transmitter Main Application
 *
 * This application receives video data from ESP32-P4 via SPI (IPC slave)
 * and transmits it over WiFi 6 (802.11ax).
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp32_ipc.h"
#include "wifi_c6_transmitter.h"

static const char *TAG = "main";

// Configuration
#define WIFI_BAND           WIFI_C6_BAND_5GHZ
#define WIFI_CHANNEL        149              // 5GHz channel
#define WIFI_MCS            WIFI_C6_MCS_7    // 64-QAM 5/6
#define WIFI_TX_POWER_DBM   20               // Maximum power
#define ENABLE_FEC          true
#define FEC_K               6                // Data blocks
#define FEC_N               12               // Total blocks (6 data + 6 redundancy)
#define TX_QUEUE_SIZE       128              // Packets

// SPI IPC Configuration (ESP32-C6 as slave)
#define IPC_MOSI_PIN        GPIO_NUM_6
#define IPC_MISO_PIN        GPIO_NUM_2
#define IPC_CLK_PIN         GPIO_NUM_4
#define IPC_CS_PIN          GPIO_NUM_5
#define IPC_HANDSHAKE_PIN   GPIO_NUM_3
#define IPC_SPI_FREQ_HZ     (50 * 1000 * 1000)  // 50 MHz

// Statistics
typedef struct {
    uint64_t ipc_packets_received;
    uint64_t ipc_bytes_received;
    uint64_t ipc_errors;
    uint64_t video_packets;
    uint64_t config_packets;
    uint64_t telemetry_packets;
} app_stats_t;

static app_stats_t s_stats = {0};

/**
 * @brief IPC receive callback - handles packets from ESP32-P4
 */
static void ipc_receive_callback(
    ipc_packet_type_t type,
    const uint8_t *payload,
    size_t payload_size,
    void *user_data)
{
    s_stats.ipc_packets_received++;
    s_stats.ipc_bytes_received += payload_size;

    switch (type) {
        case IPC_PACKET_TYPE_VIDEO: {
            // Extract video packet header
            if (payload_size < sizeof(ipc_video_header_t)) {
                ESP_LOGE(TAG, "Invalid video packet size: %zu", payload_size);
                s_stats.ipc_errors++;
                return;
            }

            ipc_video_header_t *header = (ipc_video_header_t *)payload;
            const uint8_t *nalu_data = payload + sizeof(ipc_video_header_t);
            size_t nalu_size = payload_size - sizeof(ipc_video_header_t);

            // Determine priority based on NAL type
            wifi_c6_priority_t priority = WIFI_C6_PRIORITY_NORMAL;
            if (header->is_keyframe) {
                priority = WIFI_C6_PRIORITY_HIGH;
            }

            // Send video packet via WiFi
            esp_err_t ret = wifi_c6_tx_send_video(
                nalu_data,
                nalu_size,
                header->nal_type,
                header->is_keyframe,
                header->frame_index,
                header->pts_us,
                header->dts_us,
                priority
            );

            if (ret == ESP_OK) {
                s_stats.video_packets++;
            } else {
                ESP_LOGW(TAG, "Failed to send video packet: %d", ret);
                s_stats.ipc_errors++;
            }
            break;
        }

        case IPC_PACKET_TYPE_CONFIG: {
            s_stats.config_packets++;
            // Handle configuration updates (channel, MCS, power, etc.)
            ESP_LOGI(TAG, "Config packet received (%zu bytes)", payload_size);
            // TODO: Parse and apply configuration
            break;
        }

        case IPC_PACKET_TYPE_TELEMETRY: {
            // Forward telemetry to ground station
            esp_err_t ret = wifi_c6_tx_send_telemetry(payload, payload_size);
            if (ret == ESP_OK) {
                s_stats.telemetry_packets++;
            } else {
                ESP_LOGW(TAG, "Failed to send telemetry: %d", ret);
            }
            break;
        }

        default:
            ESP_LOGW(TAG, "Unknown packet type: %d", type);
            s_stats.ipc_errors++;
            break;
    }
}

/**
 * @brief Statistics task - prints statistics every 5 seconds
 */
static void stats_task(void *arg)
{
    static uint64_t last_ipc_packets = 0;
    static uint64_t last_ipc_bytes = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        // Calculate rates
        uint64_t ipc_packets_delta = s_stats.ipc_packets_received - last_ipc_packets;
        uint64_t ipc_bytes_delta = s_stats.ipc_bytes_received - last_ipc_bytes;

        float ipc_pkt_rate = ipc_packets_delta / 5.0f;
        float ipc_mbps = (ipc_bytes_delta * 8.0f) / (5.0f * 1024.0f * 1024.0f);

        // Get WiFi statistics
        wifi_c6_tx_stats_t wifi_stats;
        wifi_c6_tx_get_stats(&wifi_stats);

        // Print statistics
        ESP_LOGI(TAG, "=== Statistics (5s interval) ===");
        ESP_LOGI(TAG, "IPC RX: %.1f pkt/s, %.2f Mbps, %llu total, %llu errors",
                 ipc_pkt_rate, ipc_mbps,
                 s_stats.ipc_packets_received,
                 s_stats.ipc_errors);

        ESP_LOGI(TAG, "Packets: video=%llu, config=%llu, telemetry=%llu",
                 s_stats.video_packets,
                 s_stats.config_packets,
                 s_stats.telemetry_packets);

        ESP_LOGI(TAG, "WiFi TX: %.2f Mbps, %llu packets, %llu failed, queue=%u%%",
                 wifi_stats.throughput_mbps,
                 wifi_stats.packets_sent,
                 wifi_stats.packets_failed,
                 wifi_stats.queue_usage_percent);

        if (wifi_stats.fec_blocks_sent > 0) {
            ESP_LOGI(TAG, "FEC: %u redundancy blocks sent", wifi_stats.fec_blocks_sent);
        }

        // Update for next interval
        last_ipc_packets = s_stats.ipc_packets_received;
        last_ipc_bytes = s_stats.ipc_bytes_received;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32-C6 WiFi Transmitter starting...");
    ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi transmitter
    wifi_c6_tx_config_t wifi_config = {
        .band = WIFI_BAND,
        .channel = WIFI_CHANNEL,
        .mcs = WIFI_MCS,
        .tx_power_dbm = WIFI_TX_POWER_DBM,
        .enable_fec = ENABLE_FEC,
        .fec_k = FEC_K,
        .fec_n = FEC_N,
        .tx_queue_size = TX_QUEUE_SIZE
    };

    ESP_ERROR_CHECK(wifi_c6_tx_init(&wifi_config));
    ESP_ERROR_CHECK(wifi_c6_tx_start());

    ESP_LOGI(TAG, "WiFi 6 transmitter started: band=%s, channel=%d, MCS=%d, power=%d dBm",
             (WIFI_BAND == WIFI_C6_BAND_2_4GHZ) ? "2.4GHz" : "5GHz",
             WIFI_CHANNEL, WIFI_MCS, WIFI_TX_POWER_DBM);

    // Initialize IPC slave (SPI slave to receive from ESP32-P4)
    ipc_slave_config_t ipc_config = {
        .mosi_pin = IPC_MOSI_PIN,
        .miso_pin = IPC_MISO_PIN,
        .clk_pin = IPC_CLK_PIN,
        .cs_pin = IPC_CS_PIN,
        .handshake_pin = IPC_HANDSHAKE_PIN,
        .spi_freq_hz = IPC_SPI_FREQ_HZ,
        .rx_callback = ipc_receive_callback,
        .user_data = NULL
    };

    ESP_ERROR_CHECK(ipc_slave_init(&ipc_config));
    ESP_ERROR_CHECK(ipc_slave_start());

    ESP_LOGI(TAG, "IPC slave started: SPI @ %u MHz", IPC_SPI_FREQ_HZ / 1000000);

    // Wait for IPC to be ready
    ESP_LOGI(TAG, "Waiting for IPC connection from ESP32-P4...");
    while (!ipc_slave_is_ready()) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "IPC connected to ESP32-P4");

    // Create statistics task
    xTaskCreate(stats_task, "stats", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "ESP32-C6 WiFi Transmitter ready");
    ESP_LOGI(TAG, "Pipeline: ESP32-P4 [SPI] -> ESP32-C6 [WiFi 6] -> Ground Station");

    // Main loop - just monitor
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));

        // Check if IPC is still connected
        if (!ipc_slave_is_ready()) {
            ESP_LOGW(TAG, "IPC connection lost! Waiting for reconnection...");
            while (!ipc_slave_is_ready()) {
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            ESP_LOGI(TAG, "IPC reconnected");
        }
    }
}

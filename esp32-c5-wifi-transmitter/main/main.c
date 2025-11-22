/**
 * @file main.c
 * @brief ESP32-C5 WiFi Transmitter - Main Application
 *
 * This application runs on ESP32-C5 and handles:
 * - Receiving video data from ESP32-P4 via SPI (IPC slave)
 * - FEC encoding
 * - WiFi 6 packet injection and transmission
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "esp32_ipc.h"
#include "wifi_c5_transmitter.h"

static const char *TAG = "C5_MAIN";

// Statistics
static volatile uint32_t packets_received = 0;
static volatile uint32_t packets_transmitted = 0;

/**
 * @brief IPC packet receive callback
 *
 * Called when a packet is received from ESP32-P4 via SPI.
 */
static void ipc_receive_callback(
    ipc_packet_type_t packet_type,
    const void *payload,
    size_t payload_size,
    void *user_data)
{
    packets_received++;

    switch (packet_type) {
        case IPC_PACKET_TYPE_VIDEO: {
            // Extract video payload
            const ipc_video_payload_t *video = (const ipc_video_payload_t *)payload;

            if (payload_size < sizeof(ipc_video_payload_t)) {
                ESP_LOGE(TAG, "Invalid video packet size");
                break;
            }

            const uint8_t *nalu_data = (const uint8_t *)payload + sizeof(ipc_video_payload_t);
            size_t nalu_size = video->nalu_size;

            // Transmit NAL unit over WiFi
            esp_err_t ret = wifi_tx_send_video(
                nalu_data,
                nalu_size,
                video->nal_type,
                video->is_keyframe,
                video->frame_index,
                video->pts_us
            );

            if (ret == ESP_OK) {
                packets_transmitted++;
            } else {
                ESP_LOGW(TAG, "WiFi TX failed");
            }

            break;
        }

        case IPC_PACKET_TYPE_CONFIG: {
            // Handle configuration updates
            const ipc_config_payload_t *config = (const ipc_config_payload_t *)payload;

            ESP_LOGI(TAG, "Config update: ch=%d, pwr=%d dBm, fec=%d/%d, bitrate=%u",
                     config->wifi_channel,
                     config->wifi_tx_power_dbm,
                     config->fec_k,
                     config->fec_n,
                     config->bitrate_bps);

            // Update WiFi settings
            wifi_tx_set_channel(config->wifi_channel);
            wifi_tx_set_power(config->wifi_tx_power_dbm);

            break;
        }

        case IPC_PACKET_TYPE_TELEMETRY:
            // Handle telemetry data
            wifi_tx_send_telemetry((const uint8_t *)payload, payload_size);
            break;

        case IPC_PACKET_TYPE_STATS:
            // Handle statistics request
            break;

        default:
            ESP_LOGW(TAG, "Unknown packet type: %d", packet_type);
            break;
    }
}

/**
 * @brief Initialize IPC slave
 */
static esp_err_t init_ipc(void)
{
    ESP_LOGI(TAG, "Initializing IPC slave...");

    ipc_slave_config_t ipc_config = {
        // SPI pins (adjust for your ESP32-C5 board)
        .mosi_pin = 6,
        .miso_pin = 2,
        .clk_pin = 4,
        .cs_pin = 5,
        .handshake_pin = 3,

        // Configuration
        .dma_buffer_size = 4096,
        .dma_channel = 1,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&ipc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IPC slave init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register receive callback
    ret = ipc_slave_register_callback(ipc_receive_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register IPC callback");
        return ret;
    }

    // Signal ready to master
    ret = ipc_slave_set_ready(true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set ready signal");
        return ret;
    }

    ESP_LOGI(TAG, "IPC slave initialized and ready");
    return ESP_OK;
}

/**
 * @brief Initialize WiFi transmitter
 */
static esp_err_t init_wifi(void)
{
    ESP_LOGI(TAG, "Initializing WiFi transmitter...");

    wifi_tx_config_t wifi_config = {
        .band = WIFI_BAND_5GHZ,         // 5GHz for higher bandwidth
        .channel = 36,                   // Channel 36 (5.18 GHz)
        .mcs = WIFI_MCS_7,              // MCS7 (~86 Mbps)
        .tx_power_dbm = 20,             // 20 dBm (100mW)

        // FEC settings
        .enable_fec = true,
        .fec_k = 6,
        .fec_n = 12,

        // Packet settings
        .mtu = 1500,
        .retry_count = 0,               // No retries for low latency

        // Device IDs
        .air_device_id = 0x1234,        // Should match ESP32-P4
        .gs_device_id = 0,              // Broadcast

        // Performance
        .tx_queue_size = 20,
        .tx_task_priority = configMAX_PRIORITIES - 1,
        .tx_task_core = 0,
    };

    esp_err_t ret = wifi_tx_init(&wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi TX init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start transmission
    ret = wifi_tx_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "WiFi TX start failed");
        return ret;
    }

    ESP_LOGI(TAG, "WiFi transmitter initialized and running");
    return ESP_OK;
}

/**
 * @brief Statistics task
 */
static void stats_task(void *pvParameters)
{
    uint32_t last_rx = 0;
    uint32_t last_tx = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));  // Every 5 seconds

        uint32_t rx = packets_received;
        uint32_t tx = packets_transmitted;

        float rx_rate = (rx - last_rx) / 5.0f;
        float tx_rate = (tx - last_tx) / 5.0f;

        ESP_LOGI(TAG, "=== Statistics ===");
        ESP_LOGI(TAG, "RX: %.1f pkt/s (%" PRIu32 " total)", rx_rate, rx);
        ESP_LOGI(TAG, "TX: %.1f pkt/s (%" PRIu32 " total)", tx_rate, tx);

        // Get WiFi stats
        wifi_tx_stats_t wifi_stats;
        if (wifi_tx_get_stats(&wifi_stats) == ESP_OK) {
            ESP_LOGI(TAG, "WiFi: %.2f Mbps, %" PRIu32 " packets, queue: %" PRIu32 "%%",
                     wifi_stats.throughput_mbps,
                     (uint32_t)wifi_stats.packets_sent,
                     wifi_stats.queue_usage_percent);
        }

        // Get IPC stats
        ipc_stats_t ipc_stats;
        if (ipc_get_stats(&ipc_stats) == ESP_OK) {
            ESP_LOGI(TAG, "IPC: %.2f Mbps, %llu packets, %" PRIu32 " errors",
                     ipc_stats.throughput_mbps,
                     ipc_stats.packets_received,
                     ipc_stats.crc_errors);
        }

        ESP_LOGI(TAG, "=================");

        last_rx = rx;
        last_tx = tx;
    }
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, " ESP32-C5 WiFi Transmitter");
    ESP_LOGI(TAG, "=================================");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi transmitter
    ESP_ERROR_CHECK(init_wifi());

    // Initialize IPC slave
    ESP_ERROR_CHECK(init_ipc());

    // Start statistics task
    xTaskCreate(stats_task, "stats", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System running!");

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

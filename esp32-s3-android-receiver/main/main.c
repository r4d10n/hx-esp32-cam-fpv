/**
 * @file main.c
 * @brief ESP32-S3 Android WiFi Receiver Main Application
 *
 * This is the main entry point for the ESP32-S3 WiFi receiver firmware.
 * It coordinates all components to receive WiFi video packets and stream
 * them to Android via USB.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "wifi_receiver.h"
#include "fec_decoder.h"
#include "usb_streamer.h"
#include "packet_handler.h"
#include "stats_tracker.h"

static const char *TAG = "main";

// Configuration
#define WIFI_CHANNEL 6
#define FEC_K 6
#define FEC_N 12
#define MAX_FRAME_SIZE (64 * 1024)
#define JITTER_BUFFER_SIZE 16

/**
 * @brief WiFi packet received callback
 */
static void wifi_packet_received_cb(const wifi_rx_packet_info_t *pkt_info, void *user_ctx) {
    packet_handler_t *pkt_handler = (packet_handler_t *)user_ctx;
    
    if (pkt_info && pkt_info->crc_valid) {
        // Process packet through packet handler
        packet_handler_process(pkt_handler, pkt_info->payload, pkt_info->payload_len);
        
        // Update stats
        stats_tracker_record_packet_event(1, 0, 0);
    }
}

/**
 * @brief Frame ready callback from packet handler
 */
static void frame_ready_cb(const assembled_frame_t *frame, void *user_ctx) {
    if (frame && frame->is_complete) {
        ESP_LOGI(TAG, "Frame %lu ready (%zu bytes)", 
                 (unsigned long)frame->frame_id, frame->data_len);
        
        // Send frame to USB
        usb_streamer_send(USB_STREAM_VIDEO, frame->data, frame->data_len);
    }
}

/**
 * @brief FEC decoded block callback
 */
static void fec_decoded_cb(uint32_t block_id, const uint8_t *data, 
                          size_t data_len, fec_block_status_t status, 
                          void *user_ctx) {
    ESP_LOGI(TAG, "FEC block %lu decoded (status: %d)", 
             (unsigned long)block_id, status);
}

/**
 * @brief USB connection state callback
 */
static void usb_connection_cb(usb_connection_status_t status, void *user_ctx) {
    ESP_LOGI(TAG, "USB connection status: %d", status);
}

/**
 * @brief Statistics monitoring task
 */
static void stats_monitor_task(void *arg) {
    system_stats_t stats;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Update every 5 seconds
        
        if (stats_tracker_get_stats(&stats) == ESP_OK) {
            ESP_LOGI(TAG, "=== System Status ===");
            ESP_LOGI(TAG, "Health: %d", stats.health);
            ESP_LOGI(TAG, "Free Heap: %lu KB", (unsigned long)stats.memory.free_heap / 1024);
            ESP_LOGI(TAG, "RX Throughput: %.2f Mbps", stats.network.rx_throughput_mbps);
            ESP_LOGI(TAG, "Packets RX: %llu, Dropped: %llu", 
                     stats.network.packets_received, stats.network.packets_dropped);
        }
    }
}

/**
 * @brief Initialize NVS
 */
static esp_err_t init_nvs(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

/**
 * @brief Main application entry point
 */
void app_main(void) {
    ESP_LOGI(TAG, "ESP32-S3 Android WiFi Receiver Starting...");
    
    // Initialize NVS
    ESP_ERROR_CHECK(init_nvs());
    
    // Initialize statistics tracker
    stats_tracker_config_t stats_config = {
        .update_interval_ms = 1000,
        .enable_cpu_profiling = true,
        .enable_memory_tracking = true,
        .enable_logging = true
    };
    ESP_ERROR_CHECK(stats_tracker_init(&stats_config, NULL, NULL));
    ESP_ERROR_CHECK(stats_tracker_start());
    
    // Initialize USB streamer
    usb_streamer_config_t usb_config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 64 * 1024,
        .rx_buffer_size = 4 * 1024,
        .tx_timeout_ms = 100,
        .enable_flow_control = true
    };
    ESP_ERROR_CHECK(usb_streamer_init(&usb_config, usb_connection_cb, NULL, NULL));
    
    // Initialize FEC decoder
    fec_decoder_config_t fec_config = {
        .k = FEC_K,
        .n = FEC_N,
        .max_block_size = 2048,
        .max_pending_blocks = 8,
        .block_timeout_ms = 100
    };
    fec_decoder_t *fec_decoder = fec_decoder_create(&fec_config, fec_decoded_cb, NULL);
    if (!fec_decoder) {
        ESP_LOGE(TAG, "Failed to create FEC decoder");
        return;
    }
    
    // Initialize packet handler
    packet_handler_config_t pkt_config = {
        .jitter_buffer_size = JITTER_BUFFER_SIZE,
        .frame_timeout_ms = 100,
        .max_frame_size = MAX_FRAME_SIZE,
        .enable_reordering = true,
        .enable_validation = true
    };
    packet_handler_t *pkt_handler = packet_handler_create(&pkt_config, frame_ready_cb, NULL);
    if (!pkt_handler) {
        ESP_LOGE(TAG, "Failed to create packet handler");
        return;
    }
    
    // Initialize WiFi receiver
    wifi_rx_config_t wifi_config = {
        .band = WIFI_RX_BAND_2_4GHZ,
        .channel = WIFI_CHANNEL,
        .filter_mac = {0, 0, 0, 0, 0, 0}, // No MAC filtering
        .enable_promiscuous = true,
        .filter_crc_errors = true,
        .callback = wifi_packet_received_cb,
        .callback_ctx = pkt_handler,
        .rx_buffer_size = 128
    };
    ESP_ERROR_CHECK(wifi_rx_init(&wifi_config));
    ESP_ERROR_CHECK(wifi_rx_start());
    
    ESP_LOGI(TAG, "All components initialized successfully");
    ESP_LOGI(TAG, "Listening on WiFi channel %d", WIFI_CHANNEL);
    
    // Create statistics monitoring task
    xTaskCreate(stats_monitor_task, "stats_monitor", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "System ready - waiting for WiFi packets...");
}

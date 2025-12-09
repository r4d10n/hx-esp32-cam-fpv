/**
 * @file main.cpp
 * @brief ESP32-S3 Ground Station firmware entry point
 *
 * Ground station for ESP32-FPV and WFB-NG FPV systems.
 * Features:
 * - USB NCM network connectivity
 * - HTTP web interface
 * - WebSocket video streaming
 * - Multi-protocol support (ESP32-FPV + WFB-NG)
 * - Browser-based video decoding (MJPEG/H.264/H.265)
 */

#include <stdio.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gs_main.h"

static const char* TAG = "main";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "ESP32-S3 FPV Ground Station v0.1");
    ESP_LOGI(TAG, "=================================");

    // Print memory info
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free internal: %lu bytes",
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes",
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Initialize ground station with default config
    gs_config_t config = {
        .http_port = 80,
        .ws_port = 80,
        .wifi_channel = 7,
        .device_id = 0,
        .gs_key_path = "/sdcard/gs.key",
        .fec_k = 6,
        .fec_n = 12,
        .target_latency_ms = 100
    };

    if (gs_init(&config) != 0) {
        ESP_LOGE(TAG, "Failed to initialize ground station");
        return;
    }

    // Start ground station
    if (gs_start() != 0) {
        ESP_LOGE(TAG, "Failed to start ground station");
        return;
    }

    ESP_LOGI(TAG, "Ground station running");
    ESP_LOGI(TAG, "Connect USB to access web interface at http://192.168.7.1/");

    // Main loop - just print stats periodically
    while (1) {
        gs_stats_t stats;
        gs_get_stats(&stats);

        ESP_LOGI(TAG, "FPS:%.1f BR:%luK LAT:%dms RSSI:%d Clients:%lu Proto:%d Codec:%d",
                 stats.fps,
                 (unsigned long)stats.bitrate_kbps,
                 stats.latency_ms,
                 stats.rssi,
                 (unsigned long)stats.ws_clients,
                 stats.active_protocol,
                 stats.active_codec);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

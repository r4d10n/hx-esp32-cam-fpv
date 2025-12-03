/**
 * ESP32 FPV Ground Station
 *
 * Captures FPV video packets in promiscuous mode while serving
 * a web interface via SoftAP for live video streaming.
 *
 * Features:
 * - Promiscuous mode packet capture on same channel as AP
 * - WebSocket MJPEG streaming to browser clients
 * - On-the-fly channel switching
 * - Configuration via web interface
 */

#include "fpv_gs.h"
#include "config_manager.h"
#include "wifi_manager.h"
#include "packet_rx.h"
#include "frame_buffer.h"
#include "web_server.h"

static const char *TAG = "main";

// Global configuration and statistics
fpv_gs_config_t g_config = {0};
fpv_gs_stats_t g_stats = {0};

void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ESP32 FPV Ground Station Starting");
    ESP_LOGI(TAG, "========================================");

    // Initialize configuration (loads from NVS)
    ESP_ERROR_CHECK(config_manager_init());

    ESP_LOGI(TAG, "Config: SSID=%s, Channel=%d, FEC=%s",
             g_config.ap_ssid, g_config.channel,
             g_config.fec_enabled ? "enabled" : "disabled");

    // Initialize frame buffer
    ESP_ERROR_CHECK(frame_buffer_init());

    // Initialize packet RX
    ESP_ERROR_CHECK(packet_rx_init());

    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_manager_init());

    // Initialize web server
    ESP_ERROR_CHECK(web_server_init());

    // Start WiFi (AP + promiscuous)
    ESP_ERROR_CHECK(wifi_manager_start());

    // Start packet processing
    ESP_ERROR_CHECK(packet_rx_start());

    // Start web server
    ESP_ERROR_CHECK(web_server_start());

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Ground Station Ready!");
    ESP_LOGI(TAG, "  Connect to WiFi: %s", g_config.ap_ssid);
    ESP_LOGI(TAG, "  Password: %s", g_config.ap_password);
    ESP_LOGI(TAG, "  Open: http://192.168.4.1/");
    ESP_LOGI(TAG, "  Channel: %d", g_config.channel);
    ESP_LOGI(TAG, "========================================");

    // Main loop - print stats periodically
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        ESP_LOGI(TAG, "Stats: pkts=%lu valid=%lu frames=%lu incomplete=%lu rssi=%d clients=%lu",
                 (unsigned long)g_stats.packets_received,
                 (unsigned long)g_stats.packets_valid,
                 (unsigned long)g_stats.frames_complete,
                 (unsigned long)g_stats.frames_incomplete,
                 g_stats.rssi_dbm,
                 (unsigned long)g_stats.websocket_clients);
    }
}

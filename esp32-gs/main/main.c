/**
 * ESP32 FPV Ground Station
 *
 * Captures FPV video packets in promiscuous mode while serving
 * a web interface via USB network (NCM/RNDIS) or WiFi SoftAP.
 *
 * Features:
 * - USB network interface (NCM/RNDIS) for tethered operation
 * - Optional WiFi AP + promiscuous mode for packet capture
 * - WebSocket MJPEG streaming to browser clients
 * - On-the-fly channel switching (when WiFi enabled)
 * - Configuration via web interface
 */

#include "fpv_gs.h"
#include "config_manager.h"
#include "frame_buffer.h"
#include "web_server.h"
#include "usb_network.h"

#ifdef CONFIG_FPV_GS_ENABLE_WIFI
#include "wifi_manager.h"
#include "packet_rx.h"
#endif

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

    ESP_LOGI(TAG, "Config: Channel=%d, FEC=%s",
             g_config.channel,
             g_config.fec_enabled ? "enabled" : "disabled");

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
    ESP_LOGI(TAG, "Mode: USB Network enabled");
#endif
#ifdef CONFIG_FPV_GS_ENABLE_WIFI
    ESP_LOGI(TAG, "Mode: WiFi enabled (SSID=%s)", g_config.ap_ssid);
#endif

    // Initialize frame buffer
    ESP_ERROR_CHECK(frame_buffer_init());

#ifdef CONFIG_FPV_GS_ENABLE_WIFI
    // Initialize packet RX (only needed with WiFi for promiscuous capture)
    ESP_ERROR_CHECK(packet_rx_init());

    // Initialize WiFi
    ESP_ERROR_CHECK(wifi_manager_init());
#endif

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
    // Initialize USB network
    ESP_ERROR_CHECK(usb_network_init());
#endif

    // Initialize web server (works with any network interface)
    ESP_ERROR_CHECK(web_server_init());

#ifdef CONFIG_FPV_GS_ENABLE_WIFI
    // Start WiFi (AP + promiscuous)
    ESP_ERROR_CHECK(wifi_manager_start());

    // Start packet processing
    ESP_ERROR_CHECK(packet_rx_start());
#endif

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
    // Start USB network
    ESP_ERROR_CHECK(usb_network_start());
#endif

    // Start web server
    ESP_ERROR_CHECK(web_server_start());

    // Print connection info
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Ground Station Ready!");
#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
    ESP_LOGI(TAG, "  USB Network: http://%s/", usb_network_get_ip());
#endif
#ifdef CONFIG_FPV_GS_ENABLE_WIFI
    ESP_LOGI(TAG, "  WiFi: %s (no password)", g_config.ap_ssid);
    ESP_LOGI(TAG, "  WiFi URL: http://192.168.4.1/");
    ESP_LOGI(TAG, "  Channel: %d", g_config.channel);
#endif
    ESP_LOGI(TAG, "========================================");

    // Main loop - print stats periodically
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

#ifdef CONFIG_FPV_GS_ENABLE_WIFI
        ESP_LOGI(TAG, "Stats: pkts=%lu valid=%lu frames=%lu incomplete=%lu rssi=%d clients=%lu",
                 (unsigned long)g_stats.packets_received,
                 (unsigned long)g_stats.packets_valid,
                 (unsigned long)g_stats.frames_complete,
                 (unsigned long)g_stats.frames_incomplete,
                 g_stats.rssi_dbm,
                 (unsigned long)g_stats.websocket_clients);
#else
        ESP_LOGI(TAG, "Stats: frames=%lu incomplete=%lu clients=%lu usb=%s",
                 (unsigned long)g_stats.frames_complete,
                 (unsigned long)g_stats.frames_incomplete,
                 (unsigned long)g_stats.websocket_clients,
                 usb_network_is_connected() ? "connected" : "waiting");
#endif
    }
}

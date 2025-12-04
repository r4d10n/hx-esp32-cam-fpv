/**
 * ESP32 FPV Ground Station
 *
 * Captures FPV video packets in promiscuous mode while serving
 * a web interface via USB network (NCM/RNDIS) and/or WiFi SoftAP.
 *
 * Features:
 * - Promiscuous mode packet capture (always enabled)
 * - USB network interface (NCM/RNDIS) for tethered operation
 * - Optional WiFi AP for wireless web interface access
 * - WebSocket MJPEG streaming to browser clients
 * - On-the-fly channel switching
 * - Configuration via web interface
 */

#include "fpv_gs.h"
#include "config_manager.h"
#include "frame_buffer.h"
#include "web_server.h"
#include "wifi_manager.h"
#include "packet_rx.h"
#include "usb_network.h"

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
#ifdef CONFIG_FPV_GS_ENABLE_WIFI_AP
    ESP_LOGI(TAG, "Mode: WiFi AP enabled (SSID=%s)", g_config.ap_ssid);
#else
    ESP_LOGI(TAG, "Mode: WiFi AP disabled (promiscuous only)");
#endif

    // Initialize frame buffer
    ESP_ERROR_CHECK(frame_buffer_init());

    // Initialize packet RX (always needed for promiscuous capture)
    ESP_ERROR_CHECK(packet_rx_init());

    // Initialize WiFi (always needed for promiscuous mode)
    ESP_ERROR_CHECK(wifi_manager_init());

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
    // Initialize USB network
    ESP_ERROR_CHECK(usb_network_init());
#endif

    // Initialize web server (works with any network interface)
    ESP_ERROR_CHECK(web_server_init());

    // Start WiFi (promiscuous mode, optionally with AP)
    ESP_ERROR_CHECK(wifi_manager_start());

    // Start packet processing
    ESP_ERROR_CHECK(packet_rx_start());

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
#ifdef CONFIG_FPV_GS_ENABLE_WIFI_AP
    ESP_LOGI(TAG, "  WiFi: %s (open)", g_config.ap_ssid);
    ESP_LOGI(TAG, "  WiFi URL: http://192.168.4.1/");
#endif
    ESP_LOGI(TAG, "  Capture Channel: %d", g_config.channel);
    ESP_LOGI(TAG, "========================================");

    // Main loop - print stats periodically (every 3 seconds)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));

        // Log to UART console
        ESP_LOGI(TAG, "Stats: pkts=%lu valid=%lu frames=%lu incomplete=%lu rssi=%d ws_clients=%lu",
                 (unsigned long)g_stats.packets_received,
                 (unsigned long)g_stats.packets_valid,
                 (unsigned long)g_stats.frames_complete,
                 (unsigned long)g_stats.frames_incomplete,
                 g_stats.rssi_dbm,
                 (unsigned long)g_stats.websocket_clients);

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
        // Output to USB CDC serial with IP addresses
        usb_cdc_printf("\r\n=== FPV Ground Station Stats ===\r\n");
        usb_cdc_printf("ESP32 IP: %s | Client IP: %s\r\n",
                       usb_network_get_ip(),
                       usb_network_get_client_ip());
        usb_cdc_printf("Channel: %d | RSSI: %d dBm\r\n",
                       g_stats.channel,
                       g_stats.rssi_dbm);
        usb_cdc_printf("Packets: %lu recv, %lu valid\r\n",
                       (unsigned long)g_stats.packets_received,
                       (unsigned long)g_stats.packets_valid);
        usb_cdc_printf("Frames: %lu complete, %lu incomplete\r\n",
                       (unsigned long)g_stats.frames_complete,
                       (unsigned long)g_stats.frames_incomplete);
        usb_cdc_printf("WebSocket clients: %lu\r\n",
                       (unsigned long)g_stats.websocket_clients);
#endif
    }
}

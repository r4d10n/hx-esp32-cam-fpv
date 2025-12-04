/**
 * ESP32 FPV Ground Station
 *
 * Captures FPV video packets in promiscuous mode while serving
 * a web interface via USB network (NCM/RNDIS) and/or WiFi SoftAP.
 * Also supports USB UVC (webcam) mode for direct video output.
 *
 * Features:
 * - Promiscuous mode packet capture (always enabled)
 * - USB network interface (NCM/RNDIS) for tethered web interface
 * - USB UVC (webcam) for direct video streaming to host apps
 * - Runtime switching between NCM and UVC modes
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
#include "usb_device.h"
#include "usb_network.h"
#include "usb_uvc.h"

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
    ESP_LOGI(TAG, "Mode: USB Network (NCM) enabled");
#endif
#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
    ESP_LOGI(TAG, "Mode: USB UVC (Webcam) enabled");
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

#if defined(CONFIG_FPV_GS_ENABLE_USB_NET) || defined(CONFIG_FPV_GS_ENABLE_USB_UVC)
    // Initialize USB device (composite: CDC + NCM + UVC)
    ESP_ERROR_CHECK(usb_device_init());
#endif

    // Initialize web server (works with any network interface)
    ESP_ERROR_CHECK(web_server_init());

    // Start WiFi (promiscuous mode, optionally with AP)
    ESP_ERROR_CHECK(wifi_manager_start());

    // Start packet processing
    ESP_ERROR_CHECK(packet_rx_start());

#if defined(CONFIG_FPV_GS_ENABLE_USB_NET) || defined(CONFIG_FPV_GS_ENABLE_USB_UVC)
    // Start USB device
    ESP_ERROR_CHECK(usb_device_start());
#endif

    // Start web server
    ESP_ERROR_CHECK(web_server_start());

    // Print connection info
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Ground Station Ready!");
#if defined(CONFIG_FPV_GS_ENABLE_USB_NET) || defined(CONFIG_FPV_GS_ENABLE_USB_UVC)
    ESP_LOGI(TAG, "  USB Mode: %s", usb_device_mode_str(usb_device_get_mode()));
#endif
#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
    ESP_LOGI(TAG, "  USB Network: http://%s/", usb_network_get_ip());
#endif
#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
    const usb_uvc_config_t *uvc_cfg = usb_uvc_get_config();
    if (uvc_cfg) {
        ESP_LOGI(TAG, "  UVC: %dx%d @ %dfps (%s)",
                 uvc_cfg->width, uvc_cfg->height, uvc_cfg->fps,
                 uvc_cfg->bulk_mode ? "bulk" : "isochronous");
    }
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

#if defined(CONFIG_FPV_GS_ENABLE_USB_NET) || defined(CONFIG_FPV_GS_ENABLE_USB_UVC)
        // Output to USB CDC serial
        usb_cdc_printf("\r\n=== FPV Ground Station Stats ===\r\n");
        usb_cdc_printf("USB Mode: %s | Connected: %s\r\n",
                       usb_device_mode_str(usb_device_get_mode()),
                       usb_device_is_connected() ? "yes" : "no");
#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
        usb_cdc_printf("ESP32 IP: %s | Client IP: %s\r\n",
                       usb_network_get_ip(),
                       usb_network_get_client_ip());
#endif
#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
        if (usb_device_get_mode() == USB_MODE_UVC) {
            usb_uvc_stats_t uvc_stats;
            usb_uvc_get_stats(&uvc_stats);
            usb_cdc_printf("UVC: %lu frames sent, %lu dropped\r\n",
                           (unsigned long)uvc_stats.frames_sent,
                           (unsigned long)uvc_stats.frames_dropped);
        }
#endif
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

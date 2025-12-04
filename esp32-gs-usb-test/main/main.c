/**
 * USB CDC + NCM Composite Test
 * ESP32-S3 as USB composite device: CDC (console) + NCM (network)
 * Using chegewara/usb-netif component for esp_netif integration
 */

#include <stdio.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_http_server.h"

#include "usb_netif.h"

static const char *TAG = "USB_CDC_NCM";

static httpd_handle_t s_server = NULL;

// Simple HTML page
static const char *html_page =
    "<!DOCTYPE html><html><head><title>USB CDC+NCM Test</title>"
    "<style>body{font-family:sans-serif;margin:40px;}</style></head>"
    "<body><h1>USB CDC+NCM Composite Working!</h1>"
    "<p>ESP32-S3 USB Composite Device:</p>"
    "<ul><li>CDC ACM - Serial Console</li>"
    "<li>NCM - Network (IP: 192.168.4.1)</li></ul>"
    "</body></html>";

// HTTP handler
static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP request received");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 7;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting server on port: %d", config.server_port);
    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
        };
        httpd_register_uri_handler(s_server, &root);
        ESP_LOGI(TAG, "HTTP server started");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== USB CDC+NCM Composite Test ===");

    // Initialize event loop and network interface stack
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_init());

    // Initialize USB CDC+NCM composite device via esp_tinyusb
    // Console is on USB Serial JTAG (separate peripheral)
    // USB OTG is used for CDC ACM + NCM composite
    ESP_LOGI(TAG, "Initializing USB OTG with CDC+NCM composite...");

    esp_netif_t *netif = usb_ip_init_default_config();
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to initialize USB network interface");
        return;
    }

    // Start web server
    start_webserver();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "USB Composite Device initialized!");
    ESP_LOGI(TAG, "Console: USB Serial JTAG");
    ESP_LOGI(TAG, "CDC ACM: /dev/ttyACMx (data port)");
    ESP_LOGI(TAG, "NCM: http://192.168.4.1/");
    ESP_LOGI(TAG, "========================================");
}

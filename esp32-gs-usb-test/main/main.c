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

#ifdef CONFIG_ESP_CONSOLE_USB_CDC
    // When CDC console is enabled, TinyUSB is already initialized by ESP-IDF
    // We only need to add NCM to the existing USB stack
    ESP_LOGI(TAG, "CDC console enabled - TinyUSB already initialized");

    // Initialize NCM on existing TinyUSB stack (skip init_tinyusb)
    ESP_ERROR_CHECK(usb_net_create(NULL));

    // Create network interface with DHCP server
    esp_netif_t *netif = netif_create(NULL, NULL, NULL);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to create network interface");
        return;
    }
    esp_netif_action_start(netif, 0, 0, 0);

    // Set MAC address
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_netif_set_mac(netif, mac));
#else
    // No CDC console - use full initialization
    ESP_LOGI(TAG, "NCM only mode");
    esp_netif_t *netif = usb_ip_init_default_config();
    if (netif == NULL) {
        ESP_LOGE(TAG, "Failed to initialize USB network interface");
        return;
    }
#endif

    // Start web server
    start_webserver();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "USB Composite Device initialized!");
#ifdef CONFIG_ESP_CONSOLE_USB_CDC
    ESP_LOGI(TAG, "CDC: Serial console on /dev/ttyACM0");
#endif
    ESP_LOGI(TAG, "NCM: http://192.168.4.1/");
    ESP_LOGI(TAG, "========================================");
}

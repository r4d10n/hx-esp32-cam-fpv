/**
 * USB NCM Network Test - Simplified
 * Based on ESP-IDF tusb_ncm example structure
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "tinyusb.h"
#include "tinyusb_net.h"
#include "lwip/ip4_addr.h"

static const char *TAG = "usb_test";

#define USB_NET_IP      "192.168.8.1"
#define USB_NET_MASK    "255.255.255.0"

static esp_netif_t *s_netif = NULL;
static httpd_handle_t s_server = NULL;

// Simple HTML page
static const char *html_page =
    "<!DOCTYPE html><html><head><title>USB NCM Test</title></head>"
    "<body><h1>USB NCM Network Working!</h1>"
    "<p>ESP32-S3 USB NCM is operational.</p>"
    "<p>IP: " USB_NET_IP "</p>"
    "</body></html>";

// HTTP handler
static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

// Start web server
static void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&s_server, &config) == ESP_OK) {
        httpd_uri_t root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
        };
        httpd_register_uri_handler(s_server, &root);
        ESP_LOGI(TAG, "Web server started");
    }
}

// USB receive callback
static esp_err_t tusb_recv_cb(void *buffer, uint16_t len, void *ctx)
{
    if (s_netif) {
        esp_netif_receive(s_netif, buffer, len, NULL);
    }
    return ESP_OK;
}

// Transmit for esp_netif
static esp_err_t netif_transmit(void *h, void *buffer, size_t len)
{
    return tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100));
}

static void l2_free(void *h, void *buffer)
{
    // No-op: TinyUSB manages the buffer
}

// esp_netif driver config
static const esp_netif_driver_ifconfig_t driver_cfg = {
    .handle = (void*)1,
    .transmit = netif_transmit,
    .driver_free_rx_buffer = l2_free,
};

// USB init callback
static void tusb_init_cb(void *arg)
{
    ESP_LOGI(TAG, "USB device configured by host");
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== USB NCM Test ===");

    // NVS init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Network stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // TinyUSB driver
    ESP_LOGI(TAG, "Installing TinyUSB driver...");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // TinyUSB NCM
    ESP_LOGI(TAG, "Initializing NCM...");
    uint8_t mac[6] = {0x02, 0x02, 0x11, 0x22, 0x33, 0x44};
    const tinyusb_net_config_t net_cfg = {
        .mac_addr = {mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]},
        .on_recv_callback = tusb_recv_cb,
        .on_init_callback = tusb_init_cb,
    };
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));

    // Create esp_netif
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.if_desc = "usb0";
    base.route_prio = 50;
    esp_netif_config_t cfg = {
        .base = &base,
        .driver = &driver_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };
    s_netif = esp_netif_new(&cfg);
    esp_netif_attach(s_netif, (void*)1);

    // Static IP
    esp_netif_dhcps_stop(s_netif);
    esp_netif_ip_info_t ip = {0};
    ip4addr_aton(USB_NET_IP, (ip4_addr_t*)&ip.ip);
    ip4addr_aton(USB_NET_MASK, (ip4_addr_t*)&ip.netmask);
    ip.gw.addr = ip.ip.addr;
    esp_netif_set_ip_info(s_netif, &ip);
    esp_netif_dhcps_start(s_netif);

    // Start interface
    esp_netif_action_start(s_netif, NULL, 0, NULL);

    // Web server
    start_webserver();

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "Ready! Connect USB, then open:");
    ESP_LOGI(TAG, "  http://%s/", USB_NET_IP);
    ESP_LOGI(TAG, "=================================");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Running...");
    }
}

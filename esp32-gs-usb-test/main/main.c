/**
 * USB NCM Network Test
 *
 * Simple test to verify USB-OTG NCM networking works.
 * Serves a basic web page over USB network.
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

static esp_netif_t *s_usb_netif = NULL;
static httpd_handle_t s_server = NULL;
static uint8_t s_mac_addr[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x01};

// Simple HTML page
static const char *html_page =
    "<!DOCTYPE html><html><head><title>USB NCM Test</title></head>"
    "<body><h1>USB NCM Network Working!</h1>"
    "<p>ESP32-S3 USB Network Control Model is operational.</p>"
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
        ESP_LOGI(TAG, "Web server started on http://%s/", USB_NET_IP);
    }
}

// Receive callback from USB
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    if (s_usb_netif && buffer && len > 0) {
        esp_netif_receive(s_usb_netif, buffer, len, NULL);
    }
    return ESP_OK;
}

// Transmit function for lwIP
static esp_err_t usb_netif_transmit(void *h, void *buffer, size_t len)
{
    if (tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100)) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

static void usb_netif_free_rx_buffer(void *h, void *buffer)
{
    // Managed by TinyUSB
}

static const esp_netif_driver_ifconfig_t s_driver_config = {
    .handle = (void *)1,
    .transmit = usb_netif_transmit,
    .driver_free_rx_buffer = usb_netif_free_rx_buffer,
};

// Called when USB is ready
static void usb_net_init_cb(void *ctx)
{
    ESP_LOGI(TAG, "USB network connected to host!");
    if (s_usb_netif) {
        esp_netif_action_start(s_usb_netif, NULL, 0, NULL);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== USB NCM Network Test ===");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Initialize TCP/IP and event loop
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize TinyUSB
    ESP_LOGI(TAG, "Initializing TinyUSB...");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // Initialize TinyUSB NCM
    ESP_LOGI(TAG, "Initializing USB NCM...");
    const tinyusb_net_config_t net_cfg = {
        .mac_addr = {s_mac_addr[0], s_mac_addr[1], s_mac_addr[2],
                     s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]},
        .on_recv_callback = usb_recv_callback,
        .on_init_callback = usb_net_init_cb,
    };
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));

    // Create network interface
    ESP_LOGI(TAG, "Creating network interface...");
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base_cfg.if_desc = "usb_ncm";
    base_cfg.route_prio = 50;

    esp_netif_config_t netif_cfg = {
        .base = &base_cfg,
        .driver = &s_driver_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };

    s_usb_netif = esp_netif_new(&netif_cfg);
    if (s_usb_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create netif");
        return;
    }

    // Set MAC
    esp_netif_set_mac(s_usb_netif, s_mac_addr);

    // Configure static IP
    esp_netif_dhcps_stop(s_usb_netif);

    esp_netif_ip_info_t ip_info = {0};
    ip4addr_aton(USB_NET_IP, (ip4_addr_t *)&ip_info.ip);
    ip4addr_aton(USB_NET_MASK, (ip4_addr_t *)&ip_info.netmask);
    ip_info.gw.addr = ip_info.ip.addr;

    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_usb_netif, &ip_info));

    // Start DHCP server
    esp_netif_dhcps_start(s_usb_netif);

    // Attach driver
    esp_netif_attach(s_usb_netif, (void *)1);

    // Start web server
    start_webserver();

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "USB NCM Test Ready!");
    ESP_LOGI(TAG, "Connect USB-OTG port to PC");
    ESP_LOGI(TAG, "Open http://%s/ in browser", USB_NET_IP);
    ESP_LOGI(TAG, "=================================");

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Waiting for USB connection...");
    }
}

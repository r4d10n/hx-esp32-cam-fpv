/**
 * USB NCM Network Test
 * ESP32-S3 as USB network device with web server
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "lwip/ip4_addr.h"

#include "tinyusb.h"
#include "tinyusb_net.h"

static const char *TAG = "USB_NCM";

#define USB_NET_IP      "192.168.8.1"
#define USB_NET_MASK    "255.255.255.0"

static esp_netif_t *s_netif = NULL;
static httpd_handle_t s_server = NULL;

// Simple HTML page
static const char *html_page =
    "<!DOCTYPE html><html><head><title>USB NCM Test</title>"
    "<style>body{font-family:sans-serif;margin:40px;}</style></head>"
    "<body><h1>USB NCM Network Working!</h1>"
    "<p>ESP32-S3 USB Network Control Model is operational.</p>"
    "<p>Device IP: " USB_NET_IP "</p>"
    "</body></html>";

// HTTP handler
static esp_err_t root_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP request received");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, strlen(html_page));
    return ESP_OK;
}

// USB receive callback - forward to network stack
static esp_err_t usb_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    ESP_LOGD(TAG, "USB RX: %d bytes", len);
    if (s_netif) {
        esp_netif_receive(s_netif, buffer, len, NULL);
    }
    return ESP_OK;
}

// Free TX buffer callback
static void usb_free_buffer(void *buf, void *ctx)
{
    // ESP-NETIF manages buffers, nothing to free here
    ESP_LOGD(TAG, "Free buffer called");
}

// Transmit callback for esp_netif
static esp_err_t netif_transmit(void *h, void *buffer, size_t len)
{
    ESP_LOGD(TAG, "TX: %d bytes", (int)len);
    return tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100));
}

static void netif_free_rx_buffer(void *h, void *buffer)
{
    // Nothing to do
}

// esp_netif driver
static const esp_netif_driver_ifconfig_t driver_cfg = {
    .handle = (void*)1,
    .transmit = netif_transmit,
    .driver_free_rx_buffer = netif_free_rx_buffer,
};

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
        ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== USB NCM Network Test ===");

    // NVS init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize TinyUSB
    ESP_LOGI(TAG, "Installing TinyUSB driver...");
    const tinyusb_config_t tusb_cfg = {
        .external_phy = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // Initialize USB NCM - use correct struct fields from official example
    ESP_LOGI(TAG, "Initializing USB NCM...");
    tinyusb_net_config_t net_config = {
        .on_recv_callback = usb_recv_callback,
        .free_tx_buffer = usb_free_buffer,
        .user_context = NULL,
    };
    // Get MAC from device
    esp_read_mac(net_config.mac_addr, ESP_MAC_WIFI_STA);
    ESP_LOGI(TAG, "MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             net_config.mac_addr[0], net_config.mac_addr[1], net_config.mac_addr[2],
             net_config.mac_addr[3], net_config.mac_addr[4], net_config.mac_addr[5]);

    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_config));

    // Create network interface
    ESP_LOGI(TAG, "Creating network interface...");
    esp_netif_inherent_config_t base = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base.if_desc = "usb0";
    base.route_prio = 50;
    base.flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP;

    esp_netif_config_t cfg = {
        .base = &base,
        .driver = &driver_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH,
    };
    s_netif = esp_netif_new(&cfg);
    if (s_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create netif");
        return;
    }

    // Attach driver
    esp_netif_attach(s_netif, (void*)1);

    // Set MAC on interface
    esp_netif_set_mac(s_netif, net_config.mac_addr);

    // Configure static IP
    esp_netif_dhcps_stop(s_netif);
    esp_netif_ip_info_t ip_info = {0};
    ip4addr_aton(USB_NET_IP, (ip4_addr_t*)&ip_info.ip);
    ip4addr_aton(USB_NET_MASK, (ip4_addr_t*)&ip_info.netmask);
    ip_info.gw.addr = ip_info.ip.addr;
    ESP_ERROR_CHECK(esp_netif_set_ip_info(s_netif, &ip_info));

    // Start DHCP server to assign IP to connected host
    ESP_ERROR_CHECK(esp_netif_dhcps_start(s_netif));
    ESP_LOGI(TAG, "DHCP server started");

    // Bring interface up
    esp_netif_action_start(s_netif, NULL, 0, NULL);

    // Start web server
    start_webserver();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "USB NCM initialized!");
    ESP_LOGI(TAG, "Connect USB to host PC");
    ESP_LOGI(TAG, "On Linux: sudo dhclient <interface>");
    ESP_LOGI(TAG, "Then open: http://%s/", USB_NET_IP);
    ESP_LOGI(TAG, "========================================");

    // Main loop
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Waiting for connection...");
    }
}

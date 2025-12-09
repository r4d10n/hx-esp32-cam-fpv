/**
 * @file usb_ncm.c
 * @brief USB NCM device implementation using TinyUSB
 */

#include "usb_ncm.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"

#include "tinyusb.h"
#include "tinyusb_net.h"
#include "dhcpserver/dhcpserver.h"

static const char *TAG = "usb_ncm";

// Default configuration
#define DEFAULT_IP_ADDR     "192.168.7.1"
#define DEFAULT_NETMASK     "255.255.255.0"
#define DEFAULT_GW_ADDR     "192.168.7.1"
#define DEFAULT_HOSTNAME    "esp32-fpv-gs"

// State
static struct {
    usb_ncm_config_t config;
    usb_ncm_stats_t stats;
    esp_netif_t* netif;
    bool initialized;
    bool running;
} g_ncm = {0};

// Network interface receive callback
static esp_err_t netif_recv_cb(void* buffer, uint16_t len, void* ctx)
{
    if (g_ncm.netif) {
        esp_netif_receive(g_ncm.netif, buffer, len, NULL);
        g_ncm.stats.packets_received++;
        g_ncm.stats.bytes_received += len;
    }
    return ESP_OK;
}

// Network interface transmit callback
static esp_err_t netif_transmit(void* h, void* buffer, size_t len)
{
    if (tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100)) == ESP_OK) {
        g_ncm.stats.packets_sent++;
        g_ncm.stats.bytes_sent += len;
        return ESP_OK;
    }
    return ESP_FAIL;
}

// Free TX buffer callback
static void netif_free_tx_buffer(void* h, void* buffer)
{
    free(buffer);
}

// USB connect/disconnect callback
static void usb_connection_cb(tinyusb_net_state_t state, void* ctx)
{
    switch (state) {
        case TINYUSB_NET_STATE_CONNECTED:
            ESP_LOGI(TAG, "USB host connected");
            g_ncm.stats.connected = true;
            esp_netif_action_connected(g_ncm.netif, 0, 0, NULL);
            break;
        case TINYUSB_NET_STATE_DISCONNECTED:
            ESP_LOGI(TAG, "USB host disconnected");
            g_ncm.stats.connected = false;
            esp_netif_action_disconnected(g_ncm.netif, 0, 0, NULL);
            break;
        default:
            break;
    }
}

int usb_ncm_init(const usb_ncm_config_t* config)
{
    if (g_ncm.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return -1;
    }

    // Apply configuration
    if (config) {
        g_ncm.config = *config;
    } else {
        g_ncm.config.ip_addr = DEFAULT_IP_ADDR;
        g_ncm.config.netmask = DEFAULT_NETMASK;
        g_ncm.config.gw_addr = DEFAULT_GW_ADDR;
        g_ncm.config.hostname = DEFAULT_HOSTNAME;
    }

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create network interface config
    const esp_netif_ip_info_t ip_info = {
        .ip = { .addr = ESP_IP4TOADDR(192, 168, 7, 1) },
        .gw = { .addr = ESP_IP4TOADDR(192, 168, 7, 1) },
        .netmask = { .addr = ESP_IP4TOADDR(255, 255, 255, 0) }
    };

    // Setup inherent config for USB NCM
    const esp_netif_inherent_config_t base_cfg = {
        .flags = ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP,
        .ip_info = &ip_info,
        .route_prio = 10,
        .if_key = "USB_NCM",
        .if_desc = "USB NCM Network Interface"
    };

    // Custom driver config
    esp_netif_driver_ifconfig_t driver_cfg = {
        .handle = (void*)1,  // Dummy handle
        .transmit = netif_transmit,
        .driver_free_rx_buffer = netif_free_tx_buffer
    };

    const esp_netif_config_t netif_config = {
        .base = &base_cfg,
        .driver = &driver_cfg,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_WIFI_AP
    };

    g_ncm.netif = esp_netif_new(&netif_config);
    if (!g_ncm.netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        return -1;
    }

    // Initialize TinyUSB
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,  // Use default
        .string_descriptor = NULL,  // Use default
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = NULL,
        .hs_configuration_descriptor = NULL,
#else
        .configuration_descriptor = NULL,
#endif
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    // Configure NCM
    tinyusb_net_config_t net_cfg = {
        .on_recv_callback = netif_recv_cb,
        .user_context = NULL
    };

    // Get MAC address
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    memcpy(net_cfg.mac_addr, mac, sizeof(mac));

    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));

    // Register connection callback
    tinyusb_net_set_state_callback(usb_connection_cb, NULL);

    memset(&g_ncm.stats, 0, sizeof(g_ncm.stats));
    g_ncm.initialized = true;

    ESP_LOGI(TAG, "USB NCM initialized, IP: %s", g_ncm.config.ip_addr);
    return 0;
}

int usb_ncm_start(void)
{
    if (!g_ncm.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return -1;
    }

    if (g_ncm.running) {
        ESP_LOGW(TAG, "Already running");
        return 0;
    }

    // Start DHCP server
    esp_netif_dhcps_start(g_ncm.netif);

    g_ncm.running = true;
    ESP_LOGI(TAG, "USB NCM started");

    return 0;
}

void usb_ncm_stop(void)
{
    if (!g_ncm.running) return;

    esp_netif_dhcps_stop(g_ncm.netif);
    g_ncm.running = false;

    ESP_LOGI(TAG, "USB NCM stopped");
}

bool usb_ncm_is_connected(void)
{
    return g_ncm.stats.connected;
}

void usb_ncm_get_stats(usb_ncm_stats_t* stats)
{
    if (stats) {
        *stats = g_ncm.stats;
    }
}

const char* usb_ncm_get_ip_addr(void)
{
    return g_ncm.config.ip_addr;
}

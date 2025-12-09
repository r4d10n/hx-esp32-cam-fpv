/**
 * @file usb_ncm.c
 * @brief USB NCM device implementation using ESP TinyUSB
 *
 * This is a simplified stub implementation for ESP-IDF 5.x
 * Full NCM support requires additional tusb_config.h configuration
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
} g_ncm;

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
    esp_netif_inherent_config_t base_cfg = ESP_NETIF_INHERENT_DEFAULT_ETH();
    base_cfg.flags = (esp_netif_flags_t)(ESP_NETIF_DHCP_SERVER | ESP_NETIF_FLAG_AUTOUP);
    base_cfg.ip_info = &ip_info;
    base_cfg.route_prio = 10;
    base_cfg.if_key = "USB_NCM";
    base_cfg.if_desc = "USB NCM Network Interface";

    esp_netif_config_t netif_config = {
        .base = &base_cfg,
        .driver = NULL,  // No driver for stub
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };

    g_ncm.netif = esp_netif_new(&netif_config);
    if (!g_ncm.netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        return -1;
    }

    // Initialize TinyUSB with default config
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = NULL,
        .string_descriptor_count = 0,
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    esp_err_t ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %d", ret);
        return -1;
    }

    memset(&g_ncm.stats, 0, sizeof(g_ncm.stats));
    g_ncm.initialized = true;

    ESP_LOGI(TAG, "USB NCM initialized (stub), IP: %s", g_ncm.config.ip_addr);
    ESP_LOGW(TAG, "Note: Full USB NCM requires tusb_config.h with CFG_TUD_NET enabled");
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

    // Start DHCP server if netif is ready
    if (g_ncm.netif) {
        esp_netif_dhcps_start(g_ncm.netif);
    }

    g_ncm.running = true;
    ESP_LOGI(TAG, "USB NCM started");

    return 0;
}

void usb_ncm_stop(void)
{
    if (!g_ncm.running) return;

    if (g_ncm.netif) {
        esp_netif_dhcps_stop(g_ncm.netif);
    }
    g_ncm.running = false;

    ESP_LOGI(TAG, "USB NCM stopped");
}

bool usb_ncm_is_connected(void)
{
    return tud_connected();
}

void usb_ncm_get_stats(usb_ncm_stats_t* stats)
{
    if (stats) {
        *stats = g_ncm.stats;
        stats->connected = tud_connected();
    }
}

const char* usb_ncm_get_ip_addr(void)
{
    return g_ncm.config.ip_addr;
}

// TinyUSB NCM network callbacks (required by tinyusb NCM class)
bool tud_network_recv_cb(const uint8_t *src, uint16_t size)
{
    if (g_ncm.netif && size > 0) {
        esp_netif_receive(g_ncm.netif, (void*)src, size, NULL);
        g_ncm.stats.packets_received++;
        g_ncm.stats.bytes_received += size;
    }
    return true;
}

uint16_t tud_network_xmit_cb(uint8_t *dst, void *ref, uint16_t arg)
{
    if (dst && ref && arg > 0) {
        memcpy(dst, ref, arg);
        return arg;
    }
    return 0;
}

void tud_network_init_cb(void)
{
    ESP_LOGI(TAG, "USB network initialized");
}

// TinyUSB mount callbacks
void tud_mount_cb(void)
{
    ESP_LOGI(TAG, "USB mounted");
    g_ncm.stats.connected = true;
    if (g_ncm.netif) {
        esp_netif_action_connected(g_ncm.netif, 0, 0, NULL);
    }
}

void tud_umount_cb(void)
{
    ESP_LOGI(TAG, "USB unmounted");
    g_ncm.stats.connected = false;
    if (g_ncm.netif) {
        esp_netif_action_disconnected(g_ncm.netif, 0, 0, NULL);
    }
}

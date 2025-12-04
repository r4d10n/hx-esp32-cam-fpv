/**
 * ESP32 FPV Ground Station - USB Network (CDC+NCM)
 *
 * Custom initialization to ensure CDC ACM is initialized before NCM.
 * Provides DHCP server on USB interface and CDC ACM serial for status output.
 */

#include "sdkconfig.h"

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET

#include "usb_network.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "usb_netif.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "tinyusb_net.h"

static const char *TAG = "usb_net";

static esp_netif_t *s_usb_netif = NULL;
static volatile bool s_connected = false;
static char s_ip_str[16] = CONFIG_USB_NETIF_DEFAULT_IP;
static char s_client_ip_str[16] = "none";

// CDC ACM callback for line state changes
static void cdc_line_state_callback(int itf, cdcacm_event_t *event)
{
    bool dtr = event->line_state_changed_data.dtr;
    bool rts = event->line_state_changed_data.rts;
    ESP_LOGI(TAG, "CDC line state: DTR=%d, RTS=%d", dtr, rts);
}

// NCM receive callback - forward to netif
extern esp_netif_t *usb_netif_p;  // From usb-netif component
static esp_err_t ncm_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    if (s_usb_netif) {
        void *buf_copy = malloc(len);
        if (!buf_copy) return ESP_ERR_NO_MEM;
        memcpy(buf_copy, buffer, len);
        return esp_netif_receive(s_usb_netif, buf_copy, len, NULL);
    }
    return ESP_OK;
}

esp_err_t usb_network_init(void)
{
    ESP_LOGI(TAG, "Initializing USB CDC+NCM composite");

    // Step 1: Install TinyUSB driver
    const tinyusb_config_t tusb_cfg = {
        .external_phy = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "TinyUSB driver installed");

    // Step 2: Initialize CDC ACM (must be before NCM to be properly enumerated)
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx = NULL,
        .callback_rx_wanted_char = NULL,
        .callback_line_state_changed = &cdc_line_state_callback,
        .callback_line_coding_changed = NULL,
    };
    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
    ESP_LOGI(TAG, "CDC ACM initialized");

    // Step 3: Initialize NCM network
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_ETH));
    tinyusb_net_config_t net_cfg = {
        .mac_addr = {mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]},
        .on_recv_callback = ncm_recv_callback,
    };
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));
    ESP_LOGI(TAG, "NCM network initialized");

    // Step 4: Create esp_netif for the USB network interface
    s_usb_netif = netif_create(NULL, NULL, NULL);
    if (s_usb_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create USB netif");
        return ESP_FAIL;
    }
    usb_netif_p = s_usb_netif;  // Set global for usb-netif component

    // Start the interface and set MAC
    esp_netif_action_start(s_usb_netif, 0, 0, 0);
    ESP_ERROR_CHECK(esp_netif_set_mac(s_usb_netif, mac));

    // Get the actual IP address from the interface
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_usb_netif, &ip_info) == ESP_OK) {
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&ip_info.ip));
    }

    s_connected = true;
    ESP_LOGI(TAG, "USB CDC+NCM ready: IP=%s", s_ip_str);

    return ESP_OK;
}

esp_err_t usb_network_start(void)
{
    ESP_LOGI(TAG, "USB network started, connect via USB to access http://%s/", s_ip_str);
    return ESP_OK;
}

void usb_network_stop(void)
{
    if (s_usb_netif) {
        esp_netif_action_stop(s_usb_netif, NULL, 0, NULL);
    }
    s_connected = false;
}

bool usb_network_is_connected(void)
{
    return s_connected;
}

const char *usb_network_get_ip(void)
{
    return s_ip_str;
}

const char *usb_network_get_client_ip(void)
{
    // Try to get DHCP client info from the netif
    if (s_usb_netif) {
        esp_netif_pair_mac_ip_t clients[4];  // Support up to 4 clients
        memset(clients, 0, sizeof(clients));

        // Get DHCP leases
        if (esp_netif_dhcps_get_clients_by_mac(s_usb_netif, 4, clients) == ESP_OK) {
            // Return first valid client IP
            for (int i = 0; i < 4; i++) {
                if (clients[i].ip.addr != 0) {
                    snprintf(s_client_ip_str, sizeof(s_client_ip_str), IPSTR, IP2STR(&clients[i].ip));
                    return s_client_ip_str;
                }
            }
        }
    }
    return "no client";
}

void usb_cdc_write(const char *data, size_t len)
{
    if (tud_cdc_connected()) {
        tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, (const uint8_t *)data, len);
        tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(100));
    }
}

void usb_cdc_printf(const char *fmt, ...)
{
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (len > 0) {
        usb_cdc_write(buf, len);
    }
}

#endif // CONFIG_FPV_GS_ENABLE_USB_NET

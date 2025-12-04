/**
 * ESP32 FPV Ground Station - USB Network (NCM/RNDIS)
 */

#include "sdkconfig.h"

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET

#include "usb_network.h"
#include <stdbool.h>
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "tinyusb.h"
#include "tinyusb_net.h"

static const char *TAG = "usb_net";

static esp_netif_t *s_usb_netif = NULL;
static volatile bool s_connected = false;
static char s_ip_str[16] = "0.0.0.0";

// USB device MAC address (locally administered)
static uint8_t s_mac_addr[6] = {0x02, 0x02, 0x84, 0x6A, 0x96, 0x01};

// Receive callback from TinyUSB - called when packet received from host
static void usb_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    (void)ctx;
    if (s_usb_netif && buffer && len > 0) {
        esp_netif_receive(s_usb_netif, buffer, len, NULL);
    }
}

// Transmit driver function for lwIP
static esp_err_t usb_netif_transmit(void *h, void *buffer, size_t len)
{
    (void)h;
    if (tinyusb_net_send_sync(buffer, len, NULL, pdMS_TO_TICKS(100)) != ESP_OK) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

// Free RX buffer callback
static void usb_netif_free_rx_buffer(void *h, void *buffer)
{
    (void)h;
    (void)buffer;
    // Buffer is managed by TinyUSB, no need to free
}

// Network interface driver configuration
static const esp_netif_driver_ifconfig_t s_driver_config = {
    .handle = (void *)1,  // Non-null handle
    .transmit = usb_netif_transmit,
    .driver_free_rx_buffer = usb_netif_free_rx_buffer,
};

// Called when USB device is initialized
static void usb_net_init_cb(void *ctx)
{
    (void)ctx;
    ESP_LOGI(TAG, "USB network device initialized");
    s_connected = true;

    if (s_usb_netif) {
        esp_netif_action_start(s_usb_netif, NULL, 0, NULL);
    }
}

esp_err_t usb_network_init(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initializing USB network interface");

    // Generate unique MAC from device base MAC
    uint8_t base_mac[6];
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
    memcpy(s_mac_addr, base_mac, 6);
    s_mac_addr[0] = 0x02;  // Set locally administered bit
    s_mac_addr[0] &= 0xFE; // Clear multicast bit

    // Initialize TinyUSB driver
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,  // Use default
        .string_descriptor = NULL,  // Use Kconfig strings
        .external_phy = false,
        .configuration_descriptor = NULL,
    };

    ret = tinyusb_driver_install(&tusb_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install TinyUSB driver: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize TinyUSB network class
    const tinyusb_net_config_t net_cfg = {
        .mac_addr = {s_mac_addr[0], s_mac_addr[1], s_mac_addr[2],
                     s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]},
        .on_recv_callback = usb_recv_callback,
        .on_init_callback = usb_net_init_cb,
    };

    ret = tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init TinyUSB network: %s", esp_err_to_name(ret));
        return ret;
    }

    // Create network interface
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
        ESP_LOGE(TAG, "Failed to create USB network interface");
        return ESP_FAIL;
    }

    // Set MAC address
    esp_netif_set_mac(s_usb_netif, s_mac_addr);

    // Configure static IP
    esp_netif_dhcps_stop(s_usb_netif);

    esp_netif_ip_info_t ip_info = {0};
    ip_info.ip.addr = ipaddr_addr(CONFIG_FPV_GS_USB_NET_IP);
    ip_info.netmask.addr = ipaddr_addr(CONFIG_FPV_GS_USB_NET_MASK);
    ip_info.gw.addr = ip_info.ip.addr;

    ret = esp_netif_set_ip_info(s_usb_netif, &ip_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set IP: %s", esp_err_to_name(ret));
        return ret;
    }

    snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&ip_info.ip));

    // Start DHCP server for connected host
    esp_netif_dhcps_start(s_usb_netif);

    // Attach driver
    esp_netif_attach(s_usb_netif, (void *)1);

    ESP_LOGI(TAG, "USB network ready: IP=%s, MAC=%02x:%02x:%02x:%02x:%02x:%02x",
             s_ip_str,
             s_mac_addr[0], s_mac_addr[1], s_mac_addr[2],
             s_mac_addr[3], s_mac_addr[4], s_mac_addr[5]);

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

#endif // CONFIG_FPV_GS_ENABLE_USB_NET

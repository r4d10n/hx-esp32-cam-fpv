/**
 * ESP32 FPV Ground Station - USB Network (NCM/RNDIS)
 *
 * Uses chegewara/usb-netif component for USB NCM + esp_netif integration.
 * Provides DHCP server on USB interface (default IP: 192.168.4.1).
 */

#include "sdkconfig.h"

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET

#include "usb_network.h"
#include <string.h>
#include "esp_log.h"
#include "esp_netif.h"
#include "usb_netif.h"

static const char *TAG = "usb_net";

static esp_netif_t *s_usb_netif = NULL;
static volatile bool s_connected = false;
static char s_ip_str[16] = "192.168.4.1";

esp_err_t usb_network_init(void)
{
    ESP_LOGI(TAG, "Initializing USB network interface");

    // Use usb-netif component for proper USB NCM + esp_netif + DHCP integration
    // This handles: TinyUSB driver, NCM device class, esp_netif, DHCP server
    s_usb_netif = usb_ip_init_default_config();

    if (s_usb_netif == NULL) {
        ESP_LOGE(TAG, "Failed to initialize USB network interface");
        return ESP_FAIL;
    }

    // Get the actual IP address from the interface
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_usb_netif, &ip_info) == ESP_OK) {
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&ip_info.ip));
    }

    s_connected = true;
    ESP_LOGI(TAG, "USB network ready: IP=%s", s_ip_str);

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

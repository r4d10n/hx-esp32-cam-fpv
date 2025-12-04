/**
 * ESP32 FPV Ground Station - USB Device Manager Implementation
 *
 * Manages USB device initialization and runtime mode switching
 * between NCM (network) and UVC (video) streaming.
 *
 * Build modes:
 * - USB_NET + USB_UVC: Full composite with CDC+NCM+UVC (requires esp_tinyusb)
 * - USB_NET only: CDC+NCM networking (requires esp_tinyusb)
 * - USB_UVC only: Standalone UVC webcam (uses usb_device_uvc only)
 */

#include "sdkconfig.h"

#if defined(CONFIG_FPV_GS_ENABLE_USB_NET) || defined(CONFIG_FPV_GS_ENABLE_USB_UVC)

#include "usb_device.h"
#include "usb_network.h"
#include "usb_uvc.h"
#include "fpv_gs.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "esp_log.h"
#include "esp_mac.h"

// USB NET mode requires esp_tinyusb for CDC+NCM composite
#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "esp_netif.h"
#include "usb_netif.h"
#include "tinyusb_net.h"
#define HAVE_CDC_ACM 1
#define HAVE_TUD_CONNECTED 1
#else
// UVC-only mode - usb_device_uvc handles TinyUSB internally
#define HAVE_CDC_ACM 0
#define HAVE_TUD_CONNECTED 0
#endif

static const char *TAG = "usb_dev";

// Current mode
static usb_mode_t s_current_mode = USB_MODE_NONE;
static bool s_initialized = false;
static bool s_started = false;

#if HAVE_CDC_ACM
// CDC state
static volatile bool s_cdc_connected = false;
#endif

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
// NCM state
static esp_netif_t *s_usb_netif = NULL;
static char s_ip_str[16] = "192.168.7.1";
static char s_client_ip_str[16] = "none";
extern esp_netif_t *usb_netif_p;  // From usb-netif component

// CDC ACM callback for line state changes
static void cdc_line_state_callback(int itf, cdcacm_event_t *event)
{
    bool dtr = event->line_state_changed_data.dtr;
    bool rts = event->line_state_changed_data.rts;
    s_cdc_connected = dtr;
    ESP_LOGI(TAG, "CDC line state: DTR=%d, RTS=%d", dtr, rts);
}

// NCM receive callback - forward to netif
static esp_err_t ncm_recv_callback(void *buffer, uint16_t len, void *ctx)
{
    if (s_usb_netif && s_current_mode == USB_MODE_NCM) {
        void *buf_copy = malloc(len);
        if (!buf_copy) return ESP_ERR_NO_MEM;
        memcpy(buf_copy, buffer, len);
        return esp_netif_receive(s_usb_netif, buf_copy, len, NULL);
    }
    return ESP_OK;
}
#endif

esp_err_t usb_device_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing USB Device Manager");

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
    // Full mode with esp_tinyusb: Install TinyUSB driver for CDC+NCM composite
    const tinyusb_config_t tusb_cfg = {
        .external_phy = false,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "TinyUSB driver installed");

    // Initialize CDC ACM (for status output)
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

    // Initialize NCM network
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_ETH));
    tinyusb_net_config_t net_cfg = {
        .mac_addr = {mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]},
        .on_recv_callback = ncm_recv_callback,
    };
    ESP_ERROR_CHECK(tinyusb_net_init(TINYUSB_USBDEV_0, &net_cfg));
    ESP_LOGI(TAG, "NCM network initialized");

    // Create esp_netif for the USB network interface
    s_usb_netif = netif_create(NULL, NULL, NULL);
    if (s_usb_netif == NULL) {
        ESP_LOGE(TAG, "Failed to create USB netif");
        return ESP_FAIL;
    }
    usb_netif_p = s_usb_netif;

    // Start the interface and set MAC
    esp_netif_action_start(s_usb_netif, 0, 0, 0);
    ESP_ERROR_CHECK(esp_netif_set_mac(s_usb_netif, mac));

    // Get the actual IP address from the interface
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(s_usb_netif, &ip_info) == ESP_OK) {
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&ip_info.ip));
    }
    ESP_LOGI(TAG, "NCM ready: IP=%s", s_ip_str);
#endif

#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
    // Initialize UVC - this handles its own TinyUSB setup in UVC-only mode
    ESP_ERROR_CHECK(usb_uvc_init());
    ESP_LOGI(TAG, "UVC initialized");
#endif

    s_initialized = true;

    // Determine default mode
#if defined(CONFIG_FPV_GS_ENABLE_USB_UVC) && defined(CONFIG_FPV_GS_ENABLE_USB_NET)
    #ifdef CONFIG_FPV_GS_UVC_DEFAULT_ACTIVE
        s_current_mode = USB_MODE_UVC;
    #else
        s_current_mode = USB_MODE_NCM;
    #endif
#elif defined(CONFIG_FPV_GS_ENABLE_USB_UVC)
    s_current_mode = USB_MODE_UVC;
#elif defined(CONFIG_FPV_GS_ENABLE_USB_NET)
    s_current_mode = USB_MODE_NCM;
#else
    s_current_mode = USB_MODE_NONE;
#endif

    ESP_LOGI(TAG, "USB Device Manager initialized, default mode: %s",
             usb_device_mode_str(s_current_mode));

    return ESP_OK;
}

esp_err_t usb_device_start(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_started) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting USB Device in %s mode", usb_device_mode_str(s_current_mode));

    // Start the appropriate streaming mode
    switch (s_current_mode) {
        case USB_MODE_NCM:
#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
            ESP_LOGI(TAG, "NCM mode active, connect via USB to access http://%s/", s_ip_str);
#endif
#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
            usb_uvc_suspend();
#endif
            break;

        case USB_MODE_UVC:
#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
            ESP_ERROR_CHECK(usb_uvc_start());
            ESP_LOGI(TAG, "UVC mode active, device appears as USB camera");
#endif
            break;

        default:
            ESP_LOGI(TAG, "No streaming mode active");
            break;
    }

    s_started = true;
    return ESP_OK;
}

usb_mode_t usb_device_get_mode(void)
{
    return s_current_mode;
}

esp_err_t usb_device_set_mode_ncm(void)
{
#ifndef CONFIG_FPV_GS_ENABLE_USB_NET
    ESP_LOGW(TAG, "NCM mode not enabled in build");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_current_mode == USB_MODE_NCM) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Switching to NCM mode");

#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
    // Suspend UVC streaming
    usb_uvc_suspend();
#endif

    s_current_mode = USB_MODE_NCM;
    ESP_LOGI(TAG, "NCM mode active, http://%s/", s_ip_str);

    return ESP_OK;
#endif
}

esp_err_t usb_device_set_mode_uvc(void)
{
#ifndef CONFIG_FPV_GS_ENABLE_USB_UVC
    ESP_LOGW(TAG, "UVC mode not enabled in build");
    return ESP_ERR_NOT_SUPPORTED;
#else
    if (s_current_mode == USB_MODE_UVC) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Switching to UVC mode");

    // Resume or start UVC streaming
    if (usb_uvc_get_state() == UVC_STATE_SUSPENDED) {
        usb_uvc_resume();
    } else {
        usb_uvc_start();
    }

    s_current_mode = USB_MODE_UVC;
    ESP_LOGI(TAG, "UVC mode active, device appears as USB camera");

    return ESP_OK;
#endif
}

usb_mode_t usb_device_toggle_mode(void)
{
#if defined(CONFIG_FPV_GS_ENABLE_USB_NET) && defined(CONFIG_FPV_GS_ENABLE_USB_UVC)
    if (s_current_mode == USB_MODE_NCM) {
        usb_device_set_mode_uvc();
    } else {
        usb_device_set_mode_ncm();
    }
#endif
    return s_current_mode;
}

bool usb_device_is_connected(void)
{
#if HAVE_TUD_CONNECTED
    return tud_connected();
#else
    // In UVC-only mode, check if UVC host is streaming
#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC
    return usb_uvc_is_streaming();
#else
    return false;
#endif
#endif
}

const char *usb_device_mode_str(usb_mode_t mode)
{
    switch (mode) {
        case USB_MODE_NONE: return "NONE";
        case USB_MODE_NCM:  return "NCM";
        case USB_MODE_UVC:  return "UVC";
        default:            return "UNKNOWN";
    }
}

// Public API functions that mirror usb_network.h for compatibility

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET
const char *usb_network_get_ip(void)
{
    return s_ip_str;
}

const char *usb_network_get_client_ip(void)
{
    if (s_usb_netif) {
        esp_netif_pair_mac_ip_t clients[4];
        memset(clients, 0, sizeof(clients));

        if (esp_netif_dhcps_get_clients_by_mac(s_usb_netif, 4, clients) == ESP_OK) {
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

bool usb_network_is_connected(void)
{
    return s_current_mode == USB_MODE_NCM && tud_connected();
}
#endif

// CDC write functions - only available when USB_NET is enabled (esp_tinyusb provides CDC)
#if HAVE_CDC_ACM
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
#else
// Stubs for UVC-only mode (no CDC support without esp_tinyusb)
void usb_cdc_write(const char *data, size_t len)
{
    (void)data;
    (void)len;
}

void usb_cdc_printf(const char *fmt, ...)
{
    (void)fmt;
}
#endif

#endif // CONFIG_FPV_GS_ENABLE_USB_NET || CONFIG_FPV_GS_ENABLE_USB_UVC

/**
 * ESP32 FPV Ground Station - USB Network (NCM/RNDIS)
 *
 * Provides USB networking capability using TinyUSB NCM class.
 * The ESP32-S3 appears as a USB network adapter on the host PC.
 */

#ifndef USB_NETWORK_H
#define USB_NETWORK_H

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef CONFIG_FPV_GS_ENABLE_USB_NET

/**
 * Initialize USB network interface
 * Sets up TinyUSB with NCM class and creates network interface
 *
 * @return ESP_OK on success
 */
esp_err_t usb_network_init(void);

/**
 * Start USB network interface
 * Begins USB enumeration and network operations
 *
 * @return ESP_OK on success
 */
esp_err_t usb_network_start(void);

/**
 * Stop USB network interface
 */
void usb_network_stop(void);

/**
 * Check if USB network is connected to host
 *
 * @return true if connected and enumerated
 */
bool usb_network_is_connected(void);

/**
 * Get USB network interface IP address string
 *
 * @return IP address string (e.g., "192.168.8.1")
 */
const char *usb_network_get_ip(void);

#else

// Stub functions when USB network is disabled
static inline esp_err_t usb_network_init(void) { return ESP_OK; }
static inline esp_err_t usb_network_start(void) { return ESP_OK; }
static inline void usb_network_stop(void) {}
static inline bool usb_network_is_connected(void) { return false; }
static inline const char *usb_network_get_ip(void) { return "0.0.0.0"; }

#endif // CONFIG_FPV_GS_ENABLE_USB_NET

#endif // USB_NETWORK_H

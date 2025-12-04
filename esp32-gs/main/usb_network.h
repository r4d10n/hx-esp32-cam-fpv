/**
 * ESP32 FPV Ground Station - USB Network (NCM/RNDIS)
 *
 * Provides USB networking capability using TinyUSB NCM class.
 * The ESP32-S3 appears as a USB network adapter on the host PC.
 */

#ifndef USB_NETWORK_H
#define USB_NETWORK_H

#include <stdbool.h>
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
 * @return IP address string (e.g., "192.168.7.1")
 */
const char *usb_network_get_ip(void);

/**
 * Get connected client's IP address (from DHCP)
 *
 * @return Client IP address string or "no client"
 */
const char *usb_network_get_client_ip(void);

/**
 * Write data to USB CDC ACM serial port
 *
 * @param data Data to write
 * @param len Length of data
 */
void usb_cdc_write(const char *data, size_t len);

/**
 * Printf-style output to USB CDC ACM serial port
 *
 * @param fmt Format string
 * @param ... Arguments
 */
void usb_cdc_printf(const char *fmt, ...);

#else

// Stub functions when USB network is disabled
static inline esp_err_t usb_network_init(void) { return ESP_OK; }
static inline esp_err_t usb_network_start(void) { return ESP_OK; }
static inline void usb_network_stop(void) {}
static inline bool usb_network_is_connected(void) { return false; }
static inline const char *usb_network_get_ip(void) { return "0.0.0.0"; }
static inline const char *usb_network_get_client_ip(void) { return "disabled"; }
static inline void usb_cdc_write(const char *data, size_t len) { (void)data; (void)len; }
static inline void usb_cdc_printf(const char *fmt, ...) { (void)fmt; }

#endif // CONFIG_FPV_GS_ENABLE_USB_NET

#endif // USB_NETWORK_H

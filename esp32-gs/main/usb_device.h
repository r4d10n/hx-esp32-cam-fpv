/**
 * ESP32 FPV Ground Station - USB Device Manager
 *
 * Manages the USB composite device (CDC + NCM + UVC) and
 * provides runtime switching between NCM and UVC modes.
 * Only one of NCM or UVC can be actively streaming at a time.
 */

#ifndef USB_DEVICE_H
#define USB_DEVICE_H

#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"

/**
 * USB streaming mode
 */
typedef enum {
    USB_MODE_NONE,      // No streaming active
    USB_MODE_NCM,       // NCM network streaming active
    USB_MODE_UVC,       // UVC video streaming active
} usb_mode_t;

/**
 * Initialize USB device subsystem
 * Sets up TinyUSB with composite device (CDC + NCM + UVC)
 *
 * @return ESP_OK on success
 */
esp_err_t usb_device_init(void);

/**
 * Start USB device
 * Begins USB enumeration with default mode
 *
 * @return ESP_OK on success
 */
esp_err_t usb_device_start(void);

/**
 * Get current USB streaming mode
 *
 * @return Current active mode
 */
usb_mode_t usb_device_get_mode(void);

/**
 * Switch to NCM mode (network streaming)
 * Suspends UVC if active
 *
 * @return ESP_OK on success
 */
esp_err_t usb_device_set_mode_ncm(void);

/**
 * Switch to UVC mode (video streaming)
 * Suspends NCM if active
 *
 * @return ESP_OK on success
 */
esp_err_t usb_device_set_mode_uvc(void);

/**
 * Toggle between NCM and UVC modes
 *
 * @return The new active mode
 */
usb_mode_t usb_device_toggle_mode(void);

/**
 * Check if USB device is connected to host
 *
 * @return true if connected
 */
bool usb_device_is_connected(void);

/**
 * Get USB device mode as string
 *
 * @param mode Mode to convert
 * @return Mode name string
 */
const char *usb_device_mode_str(usb_mode_t mode);

#endif // USB_DEVICE_H

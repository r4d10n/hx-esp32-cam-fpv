/**
 * ESP32 FPV Ground Station - USB UVC (USB Video Class)
 *
 * Provides USB webcam functionality using TinyUSB UVC class.
 * The ESP32-S3 appears as a USB camera to the host PC/Android.
 * MJPEG frames from the frame buffer are streamed over USB.
 */

#ifndef USB_UVC_H
#define USB_UVC_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "sdkconfig.h"

#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC

/**
 * UVC streaming state
 */
typedef enum {
    UVC_STATE_IDLE,         // UVC initialized but not streaming
    UVC_STATE_STREAMING,    // UVC actively streaming frames
    UVC_STATE_SUSPENDED,    // UVC suspended (NCM active)
} usb_uvc_state_t;

/**
 * UVC configuration
 */
typedef struct {
    uint16_t width;         // Frame width
    uint16_t height;        // Frame height
    uint8_t fps;            // Target frame rate
    bool bulk_mode;         // Use bulk transfer (false = isochronous)
} usb_uvc_config_t;

/**
 * UVC statistics
 */
typedef struct {
    uint32_t frames_sent;       // Total frames sent to USB
    uint32_t frames_dropped;    // Frames dropped (USB busy)
    uint32_t bytes_sent;        // Total bytes sent
    uint32_t last_frame_size;   // Size of last frame sent
    int64_t last_frame_time;    // Timestamp of last frame
} usb_uvc_stats_t;

/**
 * Initialize USB UVC subsystem
 * Must be called after TinyUSB driver is installed
 *
 * @return ESP_OK on success
 */
esp_err_t usb_uvc_init(void);

/**
 * Start UVC streaming
 * Begins sending frames from frame buffer to USB host
 *
 * @return ESP_OK on success
 */
esp_err_t usb_uvc_start(void);

/**
 * Stop UVC streaming
 * Stops sending frames but keeps UVC device active
 */
void usb_uvc_stop(void);

/**
 * Suspend UVC (when switching to NCM mode)
 * UVC remains enumerated but doesn't stream
 */
void usb_uvc_suspend(void);

/**
 * Resume UVC (when switching from NCM mode)
 */
void usb_uvc_resume(void);

/**
 * Get current UVC state
 *
 * @return Current UVC state
 */
usb_uvc_state_t usb_uvc_get_state(void);

/**
 * Check if UVC host is connected and streaming
 *
 * @return true if host has started streaming
 */
bool usb_uvc_is_streaming(void);

/**
 * Get UVC statistics
 *
 * @param stats Pointer to stats structure to fill
 */
void usb_uvc_get_stats(usb_uvc_stats_t *stats);

/**
 * Get current UVC configuration
 *
 * @return Pointer to current configuration
 */
const usb_uvc_config_t *usb_uvc_get_config(void);

#else

// Stub functions when UVC is disabled
static inline esp_err_t usb_uvc_init(void) { return ESP_OK; }
static inline esp_err_t usb_uvc_start(void) { return ESP_OK; }
static inline void usb_uvc_stop(void) {}
static inline void usb_uvc_suspend(void) {}
static inline void usb_uvc_resume(void) {}
static inline bool usb_uvc_is_streaming(void) { return false; }

#endif // CONFIG_FPV_GS_ENABLE_USB_UVC

#endif // USB_UVC_H

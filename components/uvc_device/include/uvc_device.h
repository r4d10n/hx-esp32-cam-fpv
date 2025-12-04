/*
 * UVC Device API for ESP32-S3
 *
 * Provides USB Video Class device functionality for streaming
 * MJPEG video from the ESP32 camera.
 */

#ifndef UVC_DEVICE_H_
#define UVC_DEVICE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Frame dimensions
typedef enum {
    UVC_FRAME_VGA = 0,      // 640x480
    UVC_FRAME_SVGA,         // 800x600
    UVC_FRAME_HD,           // 1280x720
    UVC_FRAME_COUNT
} uvc_frame_size_t;

// UVC device configuration
typedef struct {
    uvc_frame_size_t initial_frame_size;
    uint8_t initial_fps;
    uint8_t jpeg_quality;   // 0-63, lower = higher quality
} uvc_device_config_t;

// UVC device state
typedef struct {
    bool is_streaming;
    uvc_frame_size_t current_frame_size;
    uint8_t current_fps;
    uint32_t frame_count;
    uint32_t error_count;
} uvc_device_state_t;

/**
 * Initialize UVC device
 *
 * @param config Device configuration
 * @return 0 on success, negative on error
 */
int uvc_device_init(const uvc_device_config_t* config);

/**
 * Start UVC device task
 *
 * Creates the USB task and starts processing
 */
void uvc_device_start(void);

/**
 * Check if USB host is streaming
 *
 * @return true if host has started video streaming
 */
bool uvc_device_is_streaming(void);

/**
 * Send a JPEG frame to USB host
 *
 * @param data JPEG frame data
 * @param size Frame size in bytes
 * @return 0 on success, negative on error
 */
int uvc_device_send_frame(const uint8_t* data, size_t size);

/**
 * Get current device state
 *
 * @param state Output state structure
 */
void uvc_device_get_state(uvc_device_state_t* state);

/**
 * Get frame dimensions for a frame size enum
 *
 * @param size Frame size enum
 * @param width Output width
 * @param height Output height
 */
void uvc_get_frame_dimensions(uvc_frame_size_t size, int* width, int* height);

#ifdef __cplusplus
}
#endif

#endif /* UVC_DEVICE_H_ */

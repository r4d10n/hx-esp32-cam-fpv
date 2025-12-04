/**
 * ESP32 FPV Ground Station - USB UVC Implementation
 *
 * Implements USB Video Class device functionality for streaming
 * MJPEG frames from the FPV receiver to host applications.
 *
 * Uses the usb_device_uvc component which provides a callback-based
 * pull model where the host requests frames via fb_get_cb.
 */

#include "sdkconfig.h"

#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC

#include "usb_uvc.h"
#include "frame_buffer.h"
#include "fpv_gs.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Use usb_device_uvc component API
#include "usb_device_uvc.h"

static const char *TAG = "usb_uvc";

// State
static usb_uvc_state_t s_state = UVC_STATE_IDLE;
static usb_uvc_config_t s_config = {0};
static usb_uvc_stats_t s_stats = {0};
static SemaphoreHandle_t s_mutex = NULL;
static volatile bool s_host_streaming = false;

// UVC frame buffer for transfers
static uint8_t *s_uvc_buffer = NULL;
static size_t s_uvc_buffer_size = 0;

// Current frame being served to host
static uvc_fb_t s_current_fb = {0};
static volatile bool s_frame_valid = false;

/**
 * UVC start callback - called when host opens UVC device
 */
static esp_err_t uvc_on_start(uvc_format_t format, int width, int height, int rate, void *cb_ctx)
{
    (void)cb_ctx;

    ESP_LOGI(TAG, "Host started UVC: format=%d, %dx%d @ %d fps",
             format, width, height, rate);

    s_host_streaming = true;

    if (s_state == UVC_STATE_STREAMING || s_state == UVC_STATE_SUSPENDED) {
        // Update stats
        s_stats.frames_sent = 0;
        s_stats.frames_dropped = 0;
        s_stats.bytes_sent = 0;
    }

    return ESP_OK;
}

/**
 * UVC stop callback - called when host closes UVC device
 */
static void uvc_on_stop(void *cb_ctx)
{
    (void)cb_ctx;

    ESP_LOGI(TAG, "Host stopped UVC streaming");
    s_host_streaming = false;
    s_frame_valid = false;
}

/**
 * UVC frame get callback - called when host requests a new frame
 * Returns a frame buffer structure or NULL if no frame available
 */
static uvc_fb_t* uvc_on_fb_get(void *cb_ctx)
{
    (void)cb_ctx;

    // Check if we're supposed to be streaming
    if (s_state != UVC_STATE_STREAMING || !s_host_streaming) {
        return NULL;
    }

    // Check for new frame in frame buffer
    if (!frame_buffer_has_new_frame()) {
        return NULL;
    }

    frame_info_t *frame = frame_buffer_get_latest();
    if (frame == NULL || !frame->valid || frame->size == 0) {
        if (frame) frame_buffer_release(frame);
        return NULL;
    }

    // Check frame size
    if (frame->size > s_uvc_buffer_size) {
        ESP_LOGW(TAG, "Frame too large: %u > %u",
                 (unsigned)frame->size, (unsigned)s_uvc_buffer_size);
        s_stats.frames_dropped++;
        frame_buffer_release(frame);
        return NULL;
    }

    // Copy frame data to UVC buffer
    memcpy(s_uvc_buffer, frame->data, frame->size);

    // Setup the frame buffer structure
    s_current_fb.buf = s_uvc_buffer;
    s_current_fb.len = frame->size;
    s_current_fb.width = s_config.width;
    s_current_fb.height = s_config.height;
    s_current_fb.format = UVC_FORMAT_JPEG;
    gettimeofday(&s_current_fb.timestamp, NULL);

    // Release source frame
    frame_buffer_release(frame);

    s_frame_valid = true;

    return &s_current_fb;
}

/**
 * UVC frame return callback - called when frame is no longer needed
 */
static void uvc_on_fb_return(uvc_fb_t *fb, void *cb_ctx)
{
    (void)cb_ctx;

    if (fb == &s_current_fb && s_frame_valid) {
        // Update statistics
        s_stats.frames_sent++;
        s_stats.bytes_sent += fb->len;
        s_stats.last_frame_size = fb->len;
        s_stats.last_frame_time = esp_timer_get_time();

        s_frame_valid = false;
    }
}

esp_err_t usb_uvc_init(void)
{
    ESP_LOGI(TAG, "Initializing USB UVC");

    // Create mutex
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Set configuration from Kconfig
    s_config.width = CONFIG_FPV_GS_UVC_WIDTH;
    s_config.height = CONFIG_FPV_GS_UVC_HEIGHT;
    s_config.fps = CONFIG_FPV_GS_UVC_FPS;
#ifdef CONFIG_FPV_GS_UVC_BULK_MODE
    s_config.bulk_mode = true;
#else
    s_config.bulk_mode = false;
#endif

    // Allocate UVC transfer buffer
    // Use PSRAM if available, otherwise internal RAM
    s_uvc_buffer_size = CONFIG_FPV_GS_MAX_FRAME_SIZE;
    s_uvc_buffer = heap_caps_malloc(s_uvc_buffer_size,
                                     MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_uvc_buffer == NULL) {
        s_uvc_buffer = heap_caps_malloc(s_uvc_buffer_size,
                                         MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (s_uvc_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate UVC transfer buffer");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UVC config: %dx%d @ %d fps, %s mode",
             s_config.width, s_config.height, s_config.fps,
             s_config.bulk_mode ? "bulk" : "isochronous");

    // Configure UVC device with callbacks
    uvc_device_config_t uvc_config = {
        .uvc_buffer = s_uvc_buffer,
        .uvc_buffer_size = s_uvc_buffer_size,
        .start_cb = uvc_on_start,
        .fb_get_cb = uvc_on_fb_get,
        .fb_return_cb = uvc_on_fb_return,
        .stop_cb = uvc_on_stop,
        .cb_ctx = NULL,
    };

    esp_err_t ret = uvc_device_config(0, &uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UVC device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize the UVC device (makes it visible to host)
    ret = uvc_device_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init UVC device: %s", esp_err_to_name(ret));
        return ret;
    }

    s_state = UVC_STATE_IDLE;
    memset(&s_stats, 0, sizeof(s_stats));

    ESP_LOGI(TAG, "USB UVC initialized");
    return ESP_OK;
}

esp_err_t usb_uvc_start(void)
{
    if (s_state == UVC_STATE_STREAMING) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Starting UVC streaming");

    s_state = UVC_STATE_STREAMING;

    ESP_LOGI(TAG, "UVC streaming started - waiting for host connection");
    return ESP_OK;
}

void usb_uvc_stop(void)
{
    if (s_state == UVC_STATE_IDLE) {
        return;
    }

    ESP_LOGI(TAG, "Stopping UVC streaming");

    s_state = UVC_STATE_IDLE;
    s_frame_valid = false;

    ESP_LOGI(TAG, "UVC streaming stopped");
}

void usb_uvc_suspend(void)
{
    if (s_state != UVC_STATE_STREAMING) {
        return;
    }

    ESP_LOGI(TAG, "Suspending UVC (NCM active)");

    s_state = UVC_STATE_SUSPENDED;
    s_frame_valid = false;
}

void usb_uvc_resume(void)
{
    if (s_state != UVC_STATE_SUSPENDED) {
        return;
    }

    ESP_LOGI(TAG, "Resuming UVC streaming");

    s_state = UVC_STATE_STREAMING;
}

usb_uvc_state_t usb_uvc_get_state(void)
{
    return s_state;
}

bool usb_uvc_is_streaming(void)
{
    return s_state == UVC_STATE_STREAMING && s_host_streaming;
}

void usb_uvc_get_stats(usb_uvc_stats_t *stats)
{
    if (stats) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        memcpy(stats, &s_stats, sizeof(usb_uvc_stats_t));
        xSemaphoreGive(s_mutex);
    }
}

const usb_uvc_config_t *usb_uvc_get_config(void)
{
    return &s_config;
}

#endif // CONFIG_FPV_GS_ENABLE_USB_UVC

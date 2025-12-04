/**
 * ESP32 FPV Ground Station - USB UVC Implementation
 *
 * Implements USB Video Class device functionality for streaming
 * MJPEG frames from the FPV receiver to host applications.
 */

#include "sdkconfig.h"

#ifdef CONFIG_FPV_GS_ENABLE_USB_UVC

#include "usb_uvc.h"
#include "frame_buffer.h"
#include "fpv_gs.h"
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "tusb.h"
#include "usb_device_uvc.h"

static const char *TAG = "usb_uvc";

// UVC streaming task
#define UVC_TASK_STACK_SIZE     4096
#define UVC_TASK_PRIORITY       5

// Frame timing
#define UVC_FRAME_INTERVAL_US   (1000000 / CONFIG_FPV_GS_UVC_FPS)
#define UVC_MIN_FRAME_INTERVAL  16666   // ~60fps max

// State
static usb_uvc_state_t s_state = UVC_STATE_IDLE;
static usb_uvc_config_t s_config = {0};
static usb_uvc_stats_t s_stats = {0};
static TaskHandle_t s_uvc_task = NULL;
static SemaphoreHandle_t s_mutex = NULL;
static volatile bool s_streaming_active = false;
static volatile bool s_host_streaming = false;
static volatile bool s_task_running = false;

// Frame transfer state
static volatile bool s_frame_in_progress = false;
static uint8_t *s_frame_buffer = NULL;
static size_t s_frame_buffer_size = 0;

/**
 * UVC frame transfer complete callback
 * Called by TinyUSB when a frame transfer completes
 */
void tud_video_frame_xfer_complete_cb(uint_fast8_t ctl_idx, uint_fast8_t stm_idx)
{
    (void)ctl_idx;
    (void)stm_idx;
    s_frame_in_progress = false;
}

/**
 * UVC streaming control callback
 * Called when host starts/stops streaming
 */
static int uvc_on_streaming_ctrl(uint8_t ctl_idx, uint8_t stm_idx,
                                  const uvc_streaming_ctrl_t *ctrl,
                                  bool start)
{
    (void)ctl_idx;
    (void)stm_idx;
    (void)ctrl;

    s_host_streaming = start;

    if (start) {
        ESP_LOGI(TAG, "Host started UVC streaming");
    } else {
        ESP_LOGI(TAG, "Host stopped UVC streaming");
    }

    return 0;
}

/**
 * UVC streaming task
 * Monitors frame buffer and sends complete frames to USB host
 */
static void uvc_streaming_task(void *arg)
{
    (void)arg;

    int64_t last_frame_time = 0;
    int64_t frame_interval = UVC_FRAME_INTERVAL_US;

    ESP_LOGI(TAG, "UVC streaming task started");
    s_task_running = true;

    while (s_task_running) {
        // Check if we should be streaming
        if (s_state != UVC_STATE_STREAMING || !s_host_streaming) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // Check if previous frame transfer is complete
        if (s_frame_in_progress) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // Rate limiting - wait for frame interval
        int64_t now = esp_timer_get_time();
        int64_t elapsed = now - last_frame_time;
        if (elapsed < frame_interval) {
            int64_t wait_us = frame_interval - elapsed;
            if (wait_us > 1000) {
                vTaskDelay(pdMS_TO_TICKS(wait_us / 1000));
            }
            continue;
        }

        // Try to get a new frame
        if (!frame_buffer_has_new_frame()) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        frame_info_t *frame = frame_buffer_get_latest();
        if (frame == NULL || !frame->valid || frame->size == 0) {
            if (frame) frame_buffer_release(frame);
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        // Check if TinyUSB video is ready
        if (!tud_video_n_streaming(0, 0)) {
            frame_buffer_release(frame);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // Copy frame data (frame buffer may be reused)
        if (frame->size > s_frame_buffer_size) {
            ESP_LOGW(TAG, "Frame too large: %u > %u",
                     (unsigned)frame->size, (unsigned)s_frame_buffer_size);
            s_stats.frames_dropped++;
            frame_buffer_release(frame);
            continue;
        }

        memcpy(s_frame_buffer, frame->data, frame->size);
        size_t frame_size = frame->size;
        frame_buffer_release(frame);

        // Submit frame to UVC
        s_frame_in_progress = true;
        bool success = tud_video_n_frame_xfer(0, 0, s_frame_buffer, frame_size);

        if (success) {
            last_frame_time = esp_timer_get_time();
            s_stats.frames_sent++;
            s_stats.bytes_sent += frame_size;
            s_stats.last_frame_size = frame_size;
            s_stats.last_frame_time = last_frame_time;
        } else {
            s_frame_in_progress = false;
            s_stats.frames_dropped++;
            ESP_LOGD(TAG, "Frame transfer failed");
        }
    }

    ESP_LOGI(TAG, "UVC streaming task exiting");
    s_uvc_task = NULL;
    vTaskDelete(NULL);
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

    // Allocate frame buffer for UVC transfers
    // Use PSRAM if available, otherwise internal RAM
    s_frame_buffer_size = CONFIG_FPV_GS_MAX_FRAME_SIZE;
    s_frame_buffer = heap_caps_malloc(s_frame_buffer_size,
                                       MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (s_frame_buffer == NULL) {
        s_frame_buffer = heap_caps_malloc(s_frame_buffer_size,
                                           MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (s_frame_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate UVC frame buffer");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "UVC config: %dx%d @ %d fps, %s mode",
             s_config.width, s_config.height, s_config.fps,
             s_config.bulk_mode ? "bulk" : "isochronous");

    // Configure UVC device
    uvc_device_config_t uvc_config = {
        .uvc_buffer_size = s_frame_buffer_size,
        .uvc_buffer = NULL,  // Let driver allocate
        .start_cb = uvc_on_streaming_ctrl,
        .fb_get_cb = NULL,   // We push frames, not pull
        .fb_return_cb = NULL,
    };

    esp_err_t ret = uvc_device_config(0, &uvc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UVC device: %s", esp_err_to_name(ret));
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

    // Create streaming task if not already running
    if (s_uvc_task == NULL) {
        BaseType_t ret = xTaskCreate(
            uvc_streaming_task,
            "uvc_stream",
            UVC_TASK_STACK_SIZE,
            NULL,
            UVC_TASK_PRIORITY,
            &s_uvc_task
        );

        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create UVC streaming task");
            return ESP_ERR_NO_MEM;
        }
    }

    s_state = UVC_STATE_STREAMING;
    s_streaming_active = true;

    ESP_LOGI(TAG, "UVC streaming started");
    return ESP_OK;
}

void usb_uvc_stop(void)
{
    if (s_state == UVC_STATE_IDLE) {
        return;
    }

    ESP_LOGI(TAG, "Stopping UVC streaming");

    s_streaming_active = false;
    s_state = UVC_STATE_IDLE;

    // Wait for any in-progress transfer to complete
    int timeout = 100;
    while (s_frame_in_progress && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        timeout--;
    }

    ESP_LOGI(TAG, "UVC streaming stopped");
}

void usb_uvc_suspend(void)
{
    if (s_state != UVC_STATE_STREAMING) {
        return;
    }

    ESP_LOGI(TAG, "Suspending UVC (NCM active)");

    s_streaming_active = false;
    s_state = UVC_STATE_SUSPENDED;

    // Wait for any in-progress transfer
    int timeout = 100;
    while (s_frame_in_progress && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
        timeout--;
    }
}

void usb_uvc_resume(void)
{
    if (s_state != UVC_STATE_SUSPENDED) {
        return;
    }

    ESP_LOGI(TAG, "Resuming UVC streaming");

    s_state = UVC_STATE_STREAMING;
    s_streaming_active = true;
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

/*
 * UVC Device Implementation for ESP32-S3
 *
 * Implements USB Video Class streaming for MJPEG camera data.
 */

#include "uvc_device.h"
#include "tusb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

static const char* TAG = "UVC";

//--------------------------------------------------------------------
// Internal State
//--------------------------------------------------------------------

static struct {
    bool initialized;
    bool streaming;
    uvc_frame_size_t frame_size;
    uint8_t fps;
    uint32_t frame_count;
    uint32_t error_count;
    SemaphoreHandle_t mutex;
    TaskHandle_t usb_task;
} s_uvc = {0};

// Frame dimensions lookup
static const struct {
    int width;
    int height;
} s_frame_dims[UVC_FRAME_COUNT] = {
    { 640, 480 },   // VGA
    { 800, 600 },   // SVGA
    { 1280, 720 },  // HD
};

//--------------------------------------------------------------------
// USB Task
//--------------------------------------------------------------------

static void usb_device_task(void* param) {
    (void)param;
    ESP_LOGI(TAG, "USB device task started");

    while (1) {
        tud_task();
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

//--------------------------------------------------------------------
// TinyUSB Callbacks
//--------------------------------------------------------------------

void tud_mount_cb(void) {
    ESP_LOGI(TAG, "USB mounted");
}

void tud_umount_cb(void) {
    ESP_LOGI(TAG, "USB unmounted");
    s_uvc.streaming = false;
}

void tud_suspend_cb(bool remote_wakeup_en) {
    (void)remote_wakeup_en;
    ESP_LOGI(TAG, "USB suspended");
    s_uvc.streaming = false;
}

void tud_resume_cb(void) {
    ESP_LOGI(TAG, "USB resumed");
}

//--------------------------------------------------------------------
// Video Class Callbacks
//--------------------------------------------------------------------

#if CFG_TUD_VIDEO

// Called when streaming starts
void tud_video_frame_done_cb(uint8_t ctl_idx, uint8_t stm_idx) {
    (void)ctl_idx;
    (void)stm_idx;
    // Frame transmission complete
}

// Probe control request
bool tud_video_set_probe_cb(uint8_t rhport, uint8_t ctl_idx,
                            video_probe_and_commit_control_t const* set) {
    (void)rhport;
    (void)ctl_idx;

    if (set) {
        ESP_LOGI(TAG, "SET_PROBE: format=%d frame=%d interval=%lu",
                 set->bFormatIndex, set->bFrameIndex,
                 (unsigned long)set->dwFrameInterval);

        // Validate and store probe settings
        if (set->bFrameIndex > 0 && set->bFrameIndex <= UVC_FRAME_COUNT) {
            s_uvc.frame_size = (uvc_frame_size_t)(set->bFrameIndex - 1);
        }

        // Calculate FPS from interval (100ns units)
        if (set->dwFrameInterval > 0) {
            s_uvc.fps = (uint8_t)(10000000 / set->dwFrameInterval);
        }
    }

    return true;
}

// Commit control request
bool tud_video_commit_cb(uint8_t rhport, uint8_t ctl_idx,
                         video_probe_and_commit_control_t const* parameters) {
    (void)rhport;
    (void)ctl_idx;

    ESP_LOGI(TAG, "COMMIT: format=%d frame=%d",
             parameters->bFormatIndex, parameters->bFrameIndex);

    s_uvc.streaming = true;
    return true;
}

#endif // CFG_TUD_VIDEO

//--------------------------------------------------------------------
// Public API Implementation
//--------------------------------------------------------------------

void uvc_get_frame_dimensions(uvc_frame_size_t size, int* width, int* height) {
    if (size < UVC_FRAME_COUNT) {
        *width = s_frame_dims[size].width;
        *height = s_frame_dims[size].height;
    } else {
        *width = 640;
        *height = 480;
    }
}

int uvc_device_init(const uvc_device_config_t* config) {
    if (s_uvc.initialized) {
        return -1;
    }

    ESP_LOGI(TAG, "Initializing UVC device");

    s_uvc.mutex = xSemaphoreCreateMutex();
    if (!s_uvc.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return -1;
    }

    s_uvc.frame_size = config->initial_frame_size;
    s_uvc.fps = config->initial_fps;
    s_uvc.streaming = false;
    s_uvc.frame_count = 0;
    s_uvc.error_count = 0;

    // Initialize TinyUSB
    if (!tusb_init()) {
        ESP_LOGE(TAG, "TinyUSB init failed");
        return -1;
    }

    s_uvc.initialized = true;
    ESP_LOGI(TAG, "UVC device initialized");

    return 0;
}

void uvc_device_start(void) {
    if (!s_uvc.initialized) {
        return;
    }

    xTaskCreatePinnedToCore(
        usb_device_task,
        "usb_task",
        4096,
        NULL,
        5,
        &s_uvc.usb_task,
        1
    );

    ESP_LOGI(TAG, "UVC device started");
}

bool uvc_device_is_streaming(void) {
    return s_uvc.streaming;
}

int uvc_device_send_frame(const uint8_t* data, size_t size) {
    if (!s_uvc.initialized || !s_uvc.streaming) {
        return -1;
    }

    if (!data || size == 0) {
        return -1;
    }

#if CFG_TUD_VIDEO
    // Send frame via TinyUSB video API
    // The video class handles packetization
    if (tud_video_n_streaming(0, 0)) {
        static bool toggle = false;
        toggle = !toggle;

        // UVC payload header
        uint8_t header[2];
        header[0] = 2;  // Header length
        header[1] = toggle ? 0x01 : 0x00;  // Toggle frame ID

        // Send in chunks with UVC headers
        size_t offset = 0;
        const size_t max_payload = CFG_TUD_VIDEO_EP_BUFSIZE - 2;

        while (offset < size) {
            size_t chunk = size - offset;
            if (chunk > max_payload) {
                chunk = max_payload;
            }

            bool is_last = (offset + chunk >= size);
            if (is_last) {
                header[1] |= 0x02;  // EOF marker
            }

            // Create packet buffer
            uint8_t packet[CFG_TUD_VIDEO_EP_BUFSIZE];
            packet[0] = header[0];
            packet[1] = header[1];
            memcpy(packet + 2, data + offset, chunk);

            // Send packet
            if (!tud_video_n_frame_xfer(0, 0, packet, chunk + 2)) {
                s_uvc.error_count++;
                return -1;
            }

            offset += chunk;
            header[1] &= ~0x02;  // Clear EOF for next chunk
        }

        s_uvc.frame_count++;
        return 0;
    }
#endif

    return -1;
}

void uvc_device_get_state(uvc_device_state_t* state) {
    if (state) {
        xSemaphoreTake(s_uvc.mutex, portMAX_DELAY);
        state->is_streaming = s_uvc.streaming;
        state->current_frame_size = s_uvc.frame_size;
        state->current_fps = s_uvc.fps;
        state->frame_count = s_uvc.frame_count;
        state->error_count = s_uvc.error_count;
        xSemaphoreGive(s_uvc.mutex);
    }
}

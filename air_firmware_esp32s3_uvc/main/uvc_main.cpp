/*
 * uvc_main.cpp - ESP32-S3 USB UVC Camera Main Application
 *
 * This firmware streams the ESP32-S3 camera as a USB Video Class (UVC) device.
 * The camera's JPEG output is streamed as MJPEG over USB, making it compatible
 * with any standard camera application on the host system.
 *
 * Connect the ESP32-S3 to a computer via USB-C and it will appear as a
 * standard USB webcam in any camera application (Cheese, OBS, VLC, etc.)
 */

#include <cstring>
#include <cstdio>
#include <algorithm>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#include "uvc_device.h"

static const char *TAG = "UVC_CAM";

//--------------------------------------------------------------------
// Camera Configuration (XIAO ESP32S3 Sense)
//--------------------------------------------------------------------

#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21

//--------------------------------------------------------------------
// Configuration
//--------------------------------------------------------------------

#define INITIAL_FRAME_WIDTH     640
#define INITIAL_FRAME_HEIGHT    480
#define JPEG_QUALITY            12  // 0-63, lower = higher quality

//--------------------------------------------------------------------
// LED Status
//--------------------------------------------------------------------

static void init_led(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << LED_GPIO_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level((gpio_num_t)LED_GPIO_NUM, 0);
}

static void set_led(bool on) {
    gpio_set_level((gpio_num_t)LED_GPIO_NUM, on ? 1 : 0);
}

//--------------------------------------------------------------------
// Camera Initialization
//--------------------------------------------------------------------

static framesize_t get_framesize_from_resolution(int width, int height) {
    if (width == 640 && height == 480) return FRAMESIZE_VGA;
    if (width == 800 && height == 600) return FRAMESIZE_SVGA;
    if (width == 1280 && height == 720) return FRAMESIZE_HD;
    if (width == 1024 && height == 768) return FRAMESIZE_XGA;
    if (width == 1280 && height == 1024) return FRAMESIZE_SXGA;
    if (width == 1600 && height == 1200) return FRAMESIZE_UXGA;
    return FRAMESIZE_VGA;
}

static esp_err_t init_camera(int width, int height) {
    ESP_LOGI(TAG, "Initializing camera %dx%d", width, height);

    camera_config_t config;
    memset(&config, 0, sizeof(config));

    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;

    config.xclk_freq_hz = 20000000;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = get_framesize_from_resolution(width, height);
    config.jpeg_quality = JPEG_QUALITY;
    config.fb_count = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", err);
        return err;
    }

    // Configure sensor settings
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        // Flip/Mirror settings if needed for your mounting
        s->set_vflip(s, 0);
        s->set_hmirror(s, 0);

        // Enable auto adjustments
        s->set_whitebal(s, 1);
        s->set_awb_gain(s, 1);
        s->set_exposure_ctrl(s, 1);
        s->set_aec2(s, 1);
        s->set_gain_ctrl(s, 1);

        const char* sensor_name = "Unknown";
        if (s->id.MIDH == 0x26) sensor_name = "OV2640";
        else if (s->id.MIDH == 0x56) sensor_name = "OV5640";
        ESP_LOGI(TAG, "Camera sensor: %s", sensor_name);
    }

    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

//--------------------------------------------------------------------
// Camera Streaming Task
//--------------------------------------------------------------------

static void camera_stream_task(void* arg) {
    ESP_LOGI(TAG, "Camera streaming task started");

    bool led_state = false;
    uint32_t frames_sent = 0;
    int64_t last_fps_time = esp_timer_get_time();
    uint32_t fps_counter = 0;

    while (1) {
        // Check if USB host is requesting video
        if (uvc_device_is_streaming()) {
            // Capture frame from camera
            camera_fb_t* fb = esp_camera_fb_get();
            if (fb) {
                // Send frame to USB
                int ret = uvc_device_send_frame(fb->buf, fb->len);
                if (ret == 0) {
                    frames_sent++;
                    fps_counter++;

                    // Toggle LED to show activity
                    led_state = !led_state;
                    set_led(led_state);
                }

                esp_camera_fb_return(fb);
            }

            // Calculate and print FPS every 5 seconds
            int64_t now = esp_timer_get_time();
            if (now - last_fps_time >= 5000000) {
                float fps = fps_counter / 5.0f;
                ESP_LOGI(TAG, "Streaming: %.1f fps, %lu total frames",
                         fps, frames_sent);
                fps_counter = 0;
                last_fps_time = now;
            }
        } else {
            // Not streaming - turn off LED and wait
            set_led(false);
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        // Small delay to prevent tight loop
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

//--------------------------------------------------------------------
// Main Application Entry
//--------------------------------------------------------------------

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "ESP32-S3 USB UVC Camera");
    ESP_LOGI(TAG, "=================================");

    // Initialize NVS (required for some components)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize status LED
    init_led();
    set_led(true);

    // Print memory info
    ESP_LOGI(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
    ESP_LOGI(TAG, "Free PSRAM: %lu bytes",
             (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    // Initialize camera
    ret = init_camera(INITIAL_FRAME_WIDTH, INITIAL_FRAME_HEIGHT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed!");
        // Blink LED rapidly to indicate error
        while (1) {
            set_led(true);
            vTaskDelay(pdMS_TO_TICKS(100));
            set_led(false);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    // Initialize UVC device
    uvc_device_config_t uvc_config = {
        .initial_frame_size = UVC_FRAME_VGA,
        .initial_fps = 30,
        .jpeg_quality = JPEG_QUALITY,
    };

    if (uvc_device_init(&uvc_config) != 0) {
        ESP_LOGE(TAG, "UVC device initialization failed!");
        while (1) {
            set_led(true);
            vTaskDelay(pdMS_TO_TICKS(200));
            set_led(false);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }

    // Start UVC device (creates USB task)
    uvc_device_start();

    // Create camera streaming task
    xTaskCreatePinnedToCore(
        camera_stream_task,
        "cam_stream",
        4096,
        NULL,
        5,
        NULL,
        0  // Run on core 0 (USB runs on core 1)
    );

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "USB UVC Camera Ready!");
    ESP_LOGI(TAG, "Connect via USB to use as webcam");
    ESP_LOGI(TAG, "=================================");
    set_led(false);

    // Main loop - monitor and report status
    while (1) {
        uvc_device_state_t state;
        uvc_device_get_state(&state);

        static bool last_streaming = false;
        if (state.is_streaming != last_streaming) {
            last_streaming = state.is_streaming;
            ESP_LOGI(TAG, "Streaming: %s", state.is_streaming ? "STARTED" : "STOPPED");
        }

        // Periodic status report
        static uint32_t last_status = 0;
        uint32_t now = esp_timer_get_time() / 1000000;
        if (now - last_status >= 30) {
            last_status = now;
            ESP_LOGI(TAG, "Status: streaming=%d, frames=%lu, errors=%lu, heap=%lu",
                     state.is_streaming ? 1 : 0,
                     state.frame_count,
                     state.error_count,
                     (unsigned long)esp_get_free_heap_size());
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

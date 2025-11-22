/**
 * @file main.c
 * @brief ESP32-P4 High-Resolution FPV System - Main Application
 *
 * This application integrates:
 * - MIPI camera capture (IMX219/IMX477/OV5647)
 * - H.264 hardware encoding
 * - Inter-processor communication with ESP32-C5
 * - High-throughput video transmission over WiFi 6
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_timer.h"

#include "mipi_camera.h"
#include "h264_encoder.h"
#include "esp32_ipc.h"

static const char *TAG = "MAIN";

// Configuration
#define CAMERA_SENSOR        MIPI_CAMERA_SENSOR_IMX219
#define CAMERA_RESOLUTION    MIPI_CAMERA_RESOLUTION_FHD  // 1920x1080
#define CAMERA_FPS           60
#define H264_BITRATE         6000000  // 6 Mbps
#define H264_GOP_SIZE        60       // I-frame every 2 seconds @ 30fps

// Statistics
static volatile uint32_t frames_captured = 0;
static volatile uint32_t frames_encoded = 0;
static volatile uint32_t nalus_sent = 0;

/**
 * @brief Camera frame callback
 *
 * Called when a new frame is captured from the MIPI camera.
 */
static bool camera_frame_callback(
    const uint8_t *frame_data,
    const mipi_camera_frame_info_t *frame_info,
    void *user_data)
{
    frames_captured++;

    // Encode frame using H.264 hardware encoder
    esp_err_t ret = h264_encoder_encode_frame(
        frame_data,
        frame_info->data_size,
        frame_info->timestamp_us,
        false  // force_keyframe
    );

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to encode frame: %s", esp_err_to_name(ret));
    }

    return true;  // Continue capturing
}

/**
 * @brief H.264 NAL unit callback
 *
 * Called when a NAL unit is encoded and ready for transmission.
 */
static void nalu_callback(
    const uint8_t *nalu_data,
    const h264_nal_info_t *nalu_info,
    void *user_data)
{
    frames_encoded++;

    // Send NAL unit to ESP32-C5 via IPC
    esp_err_t ret = ipc_master_send_video(
        nalu_data,
        nalu_info->size,
        nalu_info->type,
        nalu_info->is_keyframe,
        nalu_info->frame_index,
        nalu_info->pts_us,
        nalu_info->dts_us
    );

    if (ret == ESP_OK) {
        nalus_sent++;
    } else {
        ESP_LOGE(TAG, "Failed to send NAL unit: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Initialize MIPI camera
 */
static esp_err_t init_camera(void)
{
    ESP_LOGI(TAG, "Initializing MIPI camera...");

    mipi_camera_config_t camera_config = {
        .sensor = CAMERA_SENSOR,
        .resolution = CAMERA_RESOLUTION,
        .fps = CAMERA_FPS,
        .format = MIPI_CAMERA_FORMAT_YUV422,

        // Image processing
        .hdr_enabled = true,
        .auto_exposure = true,
        .auto_white_balance = true,

        // MIPI CSI-2 configuration
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 456000000,  // 456 MHz

        // I2C configuration (adjust pins for your board)
        .i2c_sda_pin = 8,
        .i2c_scl_pin = 9,
        .i2c_addr = 0x10,  // IMX219 default address

        // MIPI pins (adjust for your board)
        .mipi_clk_p_pin = 10,
        .mipi_clk_n_pin = 11,
        .mipi_data0_p_pin = 12,
        .mipi_data0_n_pin = 13,
        .mipi_data1_p_pin = 14,
        .mipi_data1_n_pin = 15,
    };

    esp_err_t ret = mipi_camera_init(&camera_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register frame callback
    ret = mipi_camera_register_frame_callback(camera_frame_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register camera callback: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MIPI camera initialized successfully");
    return ESP_OK;
}

/**
 * @brief Initialize H.264 encoder
 */
static esp_err_t init_encoder(void)
{
    ESP_LOGI(TAG, "Initializing H.264 encoder...");

    h264_encoder_config_t encoder_config = {
        .width = 1920,
        .height = 1080,
        .fps = CAMERA_FPS,

        // Rate control
        .rc_mode = H264_RC_MODE_VBR,
        .bitrate_bps = H264_BITRATE,
        .max_bitrate_bps = H264_BITRATE * 2,

        // Quality
        .qp_min = 18,
        .qp_max = 35,
        .qp_initial = 25,

        // GOP settings
        .gop_size = H264_GOP_SIZE,
        .enable_b_frames = false,  // Disable for low latency

        // Profile and level
        .profile = H264_PROFILE_HIGH,
        .level = H264_LEVEL_4_1,

        // Advanced
        .enable_cabac = true,
        .enable_deblock = true,
        .deblock_alpha = 0,
        .deblock_beta = 0,

        // Performance
        .thread_count = 1,
    };

    esp_err_t ret = h264_encoder_init(&encoder_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Encoder init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register NAL unit callback
    ret = h264_encoder_register_nalu_callback(nalu_callback, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register NAL callback: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "H.264 encoder initialized successfully");
    return ESP_OK;
}

/**
 * @brief Initialize IPC with ESP32-C5
 */
static esp_err_t init_ipc(void)
{
    ESP_LOGI(TAG, "Initializing IPC...");

    ipc_master_config_t ipc_config = {
        // SPI pins (adjust for your board)
        .mosi_pin = 16,
        .miso_pin = 17,
        .clk_pin = 18,
        .cs_pin = 19,
        .handshake_pin = 20,

        // SPI configuration
        .spi_clock_hz = 50000000,  // 50 MHz
        .dma_buffer_size = 4096,
        .dma_channel = 1,

        .max_transfer_size = 32768,  // 32 KB max transfer
    };

    esp_err_t ret = ipc_master_init(&ipc_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "IPC init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Wait for ESP32-C5 to be ready
    ESP_LOGI(TAG, "Waiting for ESP32-C5...");
    int timeout = 100;  // 10 seconds
    while (!ipc_master_is_slave_ready() && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (timeout <= 0) {
        ESP_LOGE(TAG, "ESP32-C5 not ready!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "IPC initialized, ESP32-C5 ready");
    return ESP_OK;
}

/**
 * @brief Statistics task
 *
 * Periodically prints performance statistics.
 */
static void stats_task(void *pvParameters)
{
    uint64_t last_time = esp_timer_get_time();
    uint32_t last_captured = 0;
    uint32_t last_encoded = 0;
    uint32_t last_sent = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));  // Every 5 seconds

        uint64_t now = esp_timer_get_time();
        float elapsed = (now - last_time) / 1000000.0f;

        uint32_t captured = frames_captured;
        uint32_t encoded = frames_encoded;
        uint32_t sent = nalus_sent;

        float capture_fps = (captured - last_captured) / elapsed;
        float encode_fps = (encoded - last_encoded) / elapsed;
        float tx_rate = (sent - last_sent) / elapsed;

        ESP_LOGI(TAG, "=== Statistics ===");
        ESP_LOGI(TAG, "Capture: %.1f fps", capture_fps);
        ESP_LOGI(TAG, "Encode:  %.1f fps", encode_fps);
        ESP_LOGI(TAG, "TX:      %.1f NALUs/s", tx_rate);

        // Get encoder stats
        h264_encoder_stats_t enc_stats;
        if (h264_encoder_get_stats(&enc_stats) == ESP_OK) {
            ESP_LOGI(TAG, "Encoder: %.2f Mbps, QP avg: %d, encode time: %u us",
                     enc_stats.actual_bitrate_bps / 1000000.0f,
                     enc_stats.average_qp,
                     enc_stats.encode_time_avg_us);
        }

        // Get IPC stats
        ipc_stats_t ipc_stats;
        if (ipc_get_stats(&ipc_stats) == ESP_OK) {
            ESP_LOGI(TAG, "IPC: %.2f Mbps, %u packets, %u errors",
                     ipc_stats.throughput_mbps,
                     (uint32_t)ipc_stats.packets_sent,
                     ipc_stats.crc_errors);
        }

        ESP_LOGI(TAG, "=================");

        last_time = now;
        last_captured = captured;
        last_encoded = encoded;
        last_sent = sent;
    }
}

/**
 * @brief Main application entry point
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, " ESP32-P4 High-Resolution FPV");
    ESP_LOGI(TAG, "=================================");

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize IPC first (ESP32-C5 needs to be ready)
    ESP_ERROR_CHECK(init_ipc());

    // Initialize H.264 encoder
    ESP_ERROR_CHECK(init_encoder());

    // Initialize MIPI camera
    ESP_ERROR_CHECK(init_camera());

    // Start statistics task
    xTaskCreate(stats_task, "stats", 4096, NULL, 5, NULL);

    // Start camera capture
    ESP_LOGI(TAG, "Starting camera capture...");
    ESP_ERROR_CHECK(mipi_camera_start());

    ESP_LOGI(TAG, "System running!");

    // Main loop (could handle configuration updates, etc.)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

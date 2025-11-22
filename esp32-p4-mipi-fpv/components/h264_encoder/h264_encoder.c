/**
 * @file h264_encoder.c
 * @brief Hardware H.264 Encoder Implementation for ESP32-P4
 */

#include "h264_encoder.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "H264_ENC";

// Encoder context
typedef struct {
    h264_encoder_config_t config;
    bool initialized;
    bool encoding;

    // NAL callback
    h264_nalu_cb_t nalu_callback;
    void *nalu_callback_user_data;

    // Frame queue
    QueueHandle_t frame_queue;
    TaskHandle_t encode_task_handle;

    // Statistics
    h264_encoder_stats_t stats;
    uint64_t stats_start_time_us;

    // Encoder state
    uint32_t frame_count;
    uint32_t keyframe_count;
    bool force_keyframe;

    SemaphoreHandle_t mutex;
} h264_encoder_context_t;

static h264_encoder_context_t s_ctx = {0};

// Frame buffer structure
typedef struct {
    uint8_t *yuv_data;
    size_t yuv_size;
    uint64_t pts_us;
    bool force_keyframe;
} frame_buffer_t;

/**
 * @brief Encode a frame to H.264
 *
 * This function interfaces with ESP32-P4's hardware H.264 encoder.
 * In a real implementation, this would use the ESP-IDF H.264 HAL.
 */
static esp_err_t encode_frame_internal(const uint8_t *yuv_data, size_t yuv_size,
                                       uint64_t pts_us, bool is_keyframe)
{
    uint64_t encode_start = esp_timer_get_time();

    // TODO: Use actual ESP32-P4 H.264 hardware encoder API
    // For now, this is a simulation

    // Simulate encoding time (actual HW encoding is much faster)
    // Hardware encoder typically takes 5-15ms for 1080p
    vTaskDelay(pdMS_TO_TICKS(8));

    // Generate simulated NAL units
    // In real implementation, these come from the hardware encoder

    // SPS (Sequence Parameter Set) - sent with keyframes
    if (is_keyframe && s_ctx.nalu_callback) {
        uint8_t sps_data[] = {
            0x00, 0x00, 0x00, 0x01, 0x67,  // NAL header (SPS)
            0x42, 0xC0, 0x28,  // Profile, level, etc.
            // ... (full SPS would be here)
        };

        h264_nal_info_t sps_info = {
            .type = H264_NAL_TYPE_SPS,
            .is_keyframe = false,
            .frame_index = s_ctx.frame_count,
            .pts_us = pts_us,
            .dts_us = pts_us,
            .size = sizeof(sps_data),
        };

        s_ctx.nalu_callback(sps_data, &sps_info, s_ctx.nalu_callback_user_data);
    }

    // PPS (Picture Parameter Set) - sent with keyframes
    if (is_keyframe && s_ctx.nalu_callback) {
        uint8_t pps_data[] = {
            0x00, 0x00, 0x00, 0x01, 0x68,  // NAL header (PPS)
            0xCE, 0x38, 0x80,
        };

        h264_nal_info_t pps_info = {
            .type = H264_NAL_TYPE_PPS,
            .is_keyframe = false,
            .frame_index = s_ctx.frame_count,
            .pts_us = pts_us,
            .dts_us = pts_us,
            .size = sizeof(pps_data),
        };

        s_ctx.nalu_callback(pps_data, &pps_info, s_ctx.nalu_callback_user_data);
    }

    // IDR or P-frame
    if (s_ctx.nalu_callback) {
        // Simulate compressed frame size
        size_t compressed_size = yuv_size / 20;  // Typical 20:1 compression
        uint8_t *frame_data = malloc(compressed_size);

        if (frame_data) {
            // Fill with simulated compressed data
            memset(frame_data, 0xAA, compressed_size);

            // Add NAL header
            frame_data[0] = 0x00;
            frame_data[1] = 0x00;
            frame_data[2] = 0x00;
            frame_data[3] = 0x01;
            frame_data[4] = is_keyframe ? 0x65 : 0x41;  // IDR or P-slice

            h264_nal_info_t frame_info = {
                .type = is_keyframe ? H264_NAL_TYPE_IDR : H264_NAL_TYPE_SLICE,
                .is_keyframe = is_keyframe,
                .frame_index = s_ctx.frame_count,
                .pts_us = pts_us,
                .dts_us = pts_us,
                .size = compressed_size,
            };

            s_ctx.nalu_callback(frame_data, &frame_info, s_ctx.nalu_callback_user_data);

            free(frame_data);

            // Update statistics
            s_ctx.stats.total_bytes += compressed_size;
            if (is_keyframe) {
                s_ctx.stats.keyframes_encoded++;
            }
        }
    }

    uint64_t encode_end = esp_timer_get_time();
    uint32_t encode_time_us = encode_end - encode_start;

    // Update statistics
    s_ctx.stats.frames_encoded++;
    s_ctx.stats.encode_time_avg_us =
        (s_ctx.stats.encode_time_avg_us * (s_ctx.stats.frames_encoded - 1) + encode_time_us) /
        s_ctx.stats.frames_encoded;

    if (encode_time_us > s_ctx.stats.encode_time_max_us) {
        s_ctx.stats.encode_time_max_us = encode_time_us;
    }

    return ESP_OK;
}

/**
 * @brief Encoding task
 */
static void encode_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Encoder task started");

    frame_buffer_t *frame = NULL;

    while (s_ctx.encoding) {
        // Wait for frame
        if (xQueueReceive(s_ctx.frame_queue, &frame, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!frame) continue;

            // Determine if this should be a keyframe
            bool is_keyframe = frame->force_keyframe || s_ctx.force_keyframe ||
                             (s_ctx.frame_count % s_ctx.config.gop_size == 0);

            if (is_keyframe) {
                s_ctx.force_keyframe = false;
            }

            // Encode frame
            esp_err_t ret = encode_frame_internal(
                frame->yuv_data,
                frame->yuv_size,
                frame->pts_us,
                is_keyframe
            );

            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Frame encoding failed");
            }

            s_ctx.frame_count++;

            // Free frame buffer
            free(frame->yuv_data);
            free(frame);
            frame = NULL;
        }
    }

    ESP_LOGI(TAG, "Encoder task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Initialize H.264 encoder
 */
esp_err_t h264_encoder_init(const h264_encoder_config_t *config)
{
    if (s_ctx.initialized) {
        ESP_LOGE(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing H.264 encoder...");
    ESP_LOGI(TAG, "Resolution: %dx%d @ %d fps", config->width, config->height, config->fps);
    ESP_LOGI(TAG, "Bitrate: %u bps, Profile: %d, Level: %d",
             config->bitrate_bps, config->profile, config->level);

    memcpy(&s_ctx.config, config, sizeof(h264_encoder_config_t));

    // Create frame queue
    s_ctx.frame_queue = xQueueCreate(5, sizeof(frame_buffer_t*));
    if (!s_ctx.frame_queue) {
        ESP_LOGE(TAG, "Failed to create frame queue");
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    s_ctx.mutex = xSemaphoreCreateMutex();
    if (!s_ctx.mutex) {
        vQueueDelete(s_ctx.frame_queue);
        return ESP_ERR_NO_MEM;
    }

    // Initialize statistics
    memset(&s_ctx.stats, 0, sizeof(h264_encoder_stats_t));
    s_ctx.stats_start_time_us = esp_timer_get_time();

    s_ctx.frame_count = 0;
    s_ctx.keyframe_count = 0;
    s_ctx.force_keyframe = true;  // First frame is keyframe

    s_ctx.initialized = true;

    ESP_LOGI(TAG, "H.264 encoder initialized successfully");
    return ESP_OK;
}

/**
 * @brief Deinitialize encoder
 */
esp_err_t h264_encoder_deinit(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.encoding) {
        // Stop encoding first (task will be deleted)
        s_ctx.encoding = false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Clear frame queue
    if (s_ctx.frame_queue) {
        frame_buffer_t *frame;
        while (xQueueReceive(s_ctx.frame_queue, &frame, 0) == pdTRUE) {
            if (frame) {
                free(frame->yuv_data);
                free(frame);
            }
        }
        vQueueDelete(s_ctx.frame_queue);
        s_ctx.frame_queue = NULL;
    }

    if (s_ctx.mutex) {
        vSemaphoreDelete(s_ctx.mutex);
        s_ctx.mutex = NULL;
    }

    s_ctx.initialized = false;
    ESP_LOGI(TAG, "H.264 encoder deinitialized");

    return ESP_OK;
}

/**
 * @brief Register NAL unit callback
 */
esp_err_t h264_encoder_register_nalu_callback(
    h264_nalu_cb_t callback,
    void *user_data)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.nalu_callback = callback;
    s_ctx.nalu_callback_user_data = user_data;

    return ESP_OK;
}

/**
 * @brief Encode a YUV frame
 */
esp_err_t h264_encoder_encode_frame(
    const uint8_t *yuv_data,
    size_t yuv_size,
    uint64_t pts_us,
    bool force_keyframe)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!s_ctx.encoding) {
        // Start encoding task on first frame
        s_ctx.encoding = true;

        BaseType_t ret = xTaskCreate(
            encode_task,
            "h264_encode",
            16384,
            NULL,
            configMAX_PRIORITIES - 2,
            &s_ctx.encode_task_handle
        );

        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create encode task");
            s_ctx.encoding = false;
            return ESP_FAIL;
        }
    }

    // Create frame buffer
    frame_buffer_t *frame = malloc(sizeof(frame_buffer_t));
    if (!frame) {
        return ESP_ERR_NO_MEM;
    }

    frame->yuv_data = malloc(yuv_size);
    if (!frame->yuv_data) {
        free(frame);
        return ESP_ERR_NO_MEM;
    }

    memcpy(frame->yuv_data, yuv_data, yuv_size);
    frame->yuv_size = yuv_size;
    frame->pts_us = pts_us;
    frame->force_keyframe = force_keyframe;

    // Queue frame for encoding
    if (xQueueSend(s_ctx.frame_queue, &frame, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Frame queue full, dropping frame");
        free(frame->yuv_data);
        free(frame);
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Request keyframe
 */
esp_err_t h264_encoder_request_keyframe(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.force_keyframe = true;
    return ESP_OK;
}

/**
 * @brief Set bitrate
 */
esp_err_t h264_encoder_set_bitrate(uint32_t bitrate_bps)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config.bitrate_bps = bitrate_bps;
    ESP_LOGI(TAG, "Bitrate updated to %u bps", bitrate_bps);

    return ESP_OK;
}

/**
 * @brief Set QP range
 */
esp_err_t h264_encoder_set_qp_range(uint8_t qp_min, uint8_t qp_max)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (qp_min > 51 || qp_max > 51 || qp_min > qp_max) {
        return ESP_ERR_INVALID_ARG;
    }

    s_ctx.config.qp_min = qp_min;
    s_ctx.config.qp_max = qp_max;

    ESP_LOGI(TAG, "QP range updated to %d-%d", qp_min, qp_max);
    return ESP_OK;
}

/**
 * @brief Get encoder statistics
 */
esp_err_t h264_encoder_get_stats(h264_encoder_stats_t *stats)
{
    if (!s_ctx.initialized || !stats) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Calculate actual FPS
        uint64_t now = esp_timer_get_time();
        uint64_t elapsed_us = now - s_ctx.stats_start_time_us;

        if (elapsed_us > 0) {
            s_ctx.stats.actual_fps = (float)s_ctx.stats.frames_encoded * 1000000.0f / elapsed_us;
            s_ctx.stats.actual_bitrate_bps = (float)s_ctx.stats.total_bytes * 8 * 1000000.0f / elapsed_us;
        }

        // Estimate average QP (simplified)
        s_ctx.stats.average_qp = (s_ctx.config.qp_min + s_ctx.config.qp_max) / 2;

        memcpy(stats, &s_ctx.stats, sizeof(h264_encoder_stats_t));
        xSemaphoreGive(s_ctx.mutex);
    }

    return ESP_OK;
}

/**
 * @brief Reset statistics
 */
esp_err_t h264_encoder_reset_stats(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memset(&s_ctx.stats, 0, sizeof(h264_encoder_stats_t));
        s_ctx.stats_start_time_us = esp_timer_get_time();
        xSemaphoreGive(s_ctx.mutex);
    }

    return ESP_OK;
}

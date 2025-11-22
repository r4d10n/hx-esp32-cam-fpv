/**
 * @file mipi_camera.c
 * @brief MIPI CSI-2 Camera Driver Implementation for ESP32-P4
 */

#include "mipi_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#include "sensors/imx219.h"
#include "sensors/imx477.h"
#include "sensors/ov5647.h"

static const char *TAG = "MIPI_CAM";

// Forward declarations
typedef struct {
    esp_err_t (*init)(mipi_camera_config_t *config);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*set_exposure)(uint16_t exposure_us);
    esp_err_t (*set_gain)(uint8_t gain);
    esp_err_t (*set_hdr)(bool enable);
} sensor_ops_t;

typedef struct {
    mipi_camera_config_t config;
    bool initialized;
    bool running;

    sensor_ops_t *sensor_ops;

    // I2C handle
    i2c_port_t i2c_port;

    // Frame callback
    mipi_camera_frame_cb_t frame_callback;
    void *frame_callback_user_data;

    // Statistics
    uint32_t frame_index;
    uint32_t frames_captured;
    uint32_t frames_dropped;
    uint64_t last_frame_time_us;
    float actual_fps;

    // DMA buffers
    uint8_t *dma_buffers[2];
    size_t dma_buffer_size;
    volatile uint8_t current_buffer;

    // Synchronization
    SemaphoreHandle_t frame_mutex;
    TaskHandle_t capture_task_handle;
} mipi_camera_context_t;

static mipi_camera_context_t s_ctx = {0};

// Resolution table
static const mipi_camera_resolution_info_t resolution_table[][MIPI_CAMERA_RESOLUTION_4K + 1] = {
    // IMX219
    {
        {640, 480, 90},      // VGA
        {1280, 720, 60},     // HD
        {1920, 1080, 60},    // FHD
        {2048, 1536, 40},    // 2K
        {0, 0, 0},           // 4K not supported
    },
    // IMX477
    {
        {640, 480, 120},     // VGA
        {1280, 720, 120},    // HD
        {1920, 1080, 60},    // FHD
        {2028, 1520, 40},    // 2K
        {4056, 3040, 10},    // 4K
    },
    // OV5647
    {
        {640, 480, 90},      // VGA
        {1280, 720, 60},     // HD
        {1920, 1080, 30},    // FHD
        {2592, 1944, 15},    // 2K
        {0, 0, 0},           // 4K not supported
    },
};

/**
 * @brief I2C initialization
 */
static esp_err_t mipi_i2c_init(const mipi_camera_config_t *config)
{
    i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = config->i2c_sda_pin,
        .scl_io_num = config->i2c_scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 400000,  // 400 kHz
    };

    s_ctx.i2c_port = I2C_NUM_0;

    esp_err_t ret = i2c_param_config(s_ctx.i2c_port, &i2c_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C param config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_driver_install(s_ctx.i2c_port, I2C_MODE_MASTER, 0, 0, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "I2C initialized on port %d", s_ctx.i2c_port);
    return ESP_OK;
}

/**
 * @brief I2C write register
 */
esp_err_t mipi_i2c_write_reg(uint8_t addr, uint16_t reg, uint8_t value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xFF, true);
    i2c_master_write_byte(cmd, reg & 0xFF, true);
    i2c_master_write_byte(cmd, value, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(s_ctx.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

/**
 * @brief I2C read register
 */
esp_err_t mipi_i2c_read_reg(uint8_t addr, uint16_t reg, uint8_t *value)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, (reg >> 8) & 0xFF, true);
    i2c_master_write_byte(cmd, reg & 0xFF, true);

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, value, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(s_ctx.i2c_port, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    return ret;
}

/**
 * @brief MIPI CSI-2 initialization
 */
static esp_err_t mipi_csi_init(const mipi_camera_config_t *config)
{
    ESP_LOGI(TAG, "Initializing MIPI CSI-2 interface...");

    // TODO: Use ESP32-P4 CSI driver when available
    // For now, this is a placeholder that would use the actual hardware driver

    ESP_LOGI(TAG, "MIPI CSI-2: %d lanes @ %u Hz",
             config->mipi_lane_count,
             config->mipi_clk_freq_hz);

    return ESP_OK;
}

/**
 * @brief Capture task
 */
static void capture_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Capture task started");

    while (s_ctx.running) {
        // Simulate frame capture (actual implementation would use CSI DMA)
        uint64_t now = esp_timer_get_time();

        // Calculate FPS
        if (s_ctx.last_frame_time_us > 0) {
            uint64_t delta_us = now - s_ctx.last_frame_time_us;
            s_ctx.actual_fps = 1000000.0f / delta_us;
        }
        s_ctx.last_frame_time_us = now;

        if (s_ctx.frame_callback) {
            mipi_camera_frame_info_t frame_info = {
                .frame_index = s_ctx.frame_index++,
                .timestamp_us = now,
                .width = resolution_table[s_ctx.config.sensor][s_ctx.config.resolution].width,
                .height = resolution_table[s_ctx.config.sensor][s_ctx.config.resolution].height,
                .format = s_ctx.config.format,
                .data_size = s_ctx.dma_buffer_size,
                .exposure_time_us = s_ctx.config.exposure_time_us,
                .gain = s_ctx.config.gain,
            };

            // Call user callback
            if (xSemaphoreTake(s_ctx.frame_mutex, portMAX_DELAY) == pdTRUE) {
                bool continue_capture = s_ctx.frame_callback(
                    s_ctx.dma_buffers[s_ctx.current_buffer],
                    &frame_info,
                    s_ctx.frame_callback_user_data
                );

                s_ctx.frames_captured++;
                xSemaphoreGive(s_ctx.frame_mutex);

                if (!continue_capture) {
                    ESP_LOGW(TAG, "Capture stopped by callback");
                    break;
                }
            }

            // Switch buffer
            s_ctx.current_buffer = (s_ctx.current_buffer + 1) % 2;
        }

        // Frame rate control
        uint32_t frame_period_ms = 1000 / s_ctx.config.fps;
        vTaskDelay(pdMS_TO_TICKS(frame_period_ms));
    }

    ESP_LOGI(TAG, "Capture task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Initialize MIPI camera
 */
esp_err_t mipi_camera_init(const mipi_camera_config_t *config)
{
    if (s_ctx.initialized) {
        ESP_LOGE(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing MIPI camera...");
    ESP_LOGI(TAG, "Sensor: %d, Resolution: %d, FPS: %d",
             config->sensor, config->resolution, config->fps);

    memcpy(&s_ctx.config, config, sizeof(mipi_camera_config_t));

    // Initialize I2C
    esp_err_t ret = mipi_i2c_init(config);
    if (ret != ESP_OK) {
        return ret;
    }

    // Initialize MIPI CSI-2
    ret = mipi_csi_init(config);
    if (ret != ESP_OK) {
        return ret;
    }

    // Select sensor operations
    switch (config->sensor) {
        case MIPI_CAMERA_SENSOR_IMX219:
            s_ctx.sensor_ops = imx219_get_ops();
            break;
        case MIPI_CAMERA_SENSOR_IMX477:
            s_ctx.sensor_ops = imx477_get_ops();
            break;
        case MIPI_CAMERA_SENSOR_OV5647:
            s_ctx.sensor_ops = ov5647_get_ops();
            break;
        default:
            ESP_LOGE(TAG, "Unknown sensor type: %d", config->sensor);
            return ESP_ERR_NOT_SUPPORTED;
    }

    // Initialize sensor
    ret = s_ctx.sensor_ops->init(&s_ctx.config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Allocate DMA buffers
    const mipi_camera_resolution_info_t *res_info =
        &resolution_table[config->sensor][config->resolution];

    // Calculate buffer size based on format
    size_t bytes_per_pixel = 2;  // YUV422 = 2 bytes/pixel
    s_ctx.dma_buffer_size = res_info->width * res_info->height * bytes_per_pixel;

    for (int i = 0; i < 2; i++) {
        s_ctx.dma_buffers[i] = heap_caps_malloc(s_ctx.dma_buffer_size,
                                                 MALLOC_CAP_DMA);
        if (!s_ctx.dma_buffers[i]) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffer %d", i);
            return ESP_ERR_NO_MEM;
        }
        memset(s_ctx.dma_buffers[i], 0, s_ctx.dma_buffer_size);
    }

    // Create mutex
    s_ctx.frame_mutex = xSemaphoreCreateMutex();
    if (!s_ctx.frame_mutex) {
        return ESP_ERR_NO_MEM;
    }

    s_ctx.initialized = true;
    ESP_LOGI(TAG, "MIPI camera initialized successfully");
    ESP_LOGI(TAG, "Buffer size: %u bytes", s_ctx.dma_buffer_size);

    return ESP_OK;
}

/**
 * @brief Deinitialize camera
 */
esp_err_t mipi_camera_deinit(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.running) {
        mipi_camera_stop();
    }

    // Free DMA buffers
    for (int i = 0; i < 2; i++) {
        if (s_ctx.dma_buffers[i]) {
            heap_caps_free(s_ctx.dma_buffers[i]);
            s_ctx.dma_buffers[i] = NULL;
        }
    }

    // Delete mutex
    if (s_ctx.frame_mutex) {
        vSemaphoreDelete(s_ctx.frame_mutex);
        s_ctx.frame_mutex = NULL;
    }

    // Stop sensor
    if (s_ctx.sensor_ops && s_ctx.sensor_ops->stop) {
        s_ctx.sensor_ops->stop();
    }

    // Deinitialize I2C
    i2c_driver_delete(s_ctx.i2c_port);

    s_ctx.initialized = false;
    ESP_LOGI(TAG, "MIPI camera deinitialized");

    return ESP_OK;
}

/**
 * @brief Register frame callback
 */
esp_err_t mipi_camera_register_frame_callback(
    mipi_camera_frame_cb_t callback,
    void *user_data)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.frame_callback = callback;
    s_ctx.frame_callback_user_data = user_data;

    return ESP_OK;
}

/**
 * @brief Start capturing
 */
esp_err_t mipi_camera_start(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting camera capture...");

    // Start sensor
    esp_err_t ret = s_ctx.sensor_ops->start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Sensor start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    s_ctx.running = true;
    s_ctx.frame_index = 0;
    s_ctx.frames_captured = 0;
    s_ctx.frames_dropped = 0;

    // Create capture task
    BaseType_t task_ret = xTaskCreate(
        capture_task,
        "mipi_capture",
        8192,
        NULL,
        configMAX_PRIORITIES - 1,  // High priority
        &s_ctx.capture_task_handle
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create capture task");
        s_ctx.running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Camera capture started");
    return ESP_OK;
}

/**
 * @brief Stop capturing
 */
esp_err_t mipi_camera_stop(void)
{
    if (!s_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping camera capture...");

    s_ctx.running = false;

    // Wait for capture task to stop
    if (s_ctx.capture_task_handle) {
        // Task will delete itself
        vTaskDelay(pdMS_TO_TICKS(100));
        s_ctx.capture_task_handle = NULL;
    }

    // Stop sensor
    if (s_ctx.sensor_ops && s_ctx.sensor_ops->stop) {
        s_ctx.sensor_ops->stop();
    }

    ESP_LOGI(TAG, "Camera capture stopped");
    return ESP_OK;
}

/**
 * @brief Set exposure time
 */
esp_err_t mipi_camera_set_exposure(uint16_t exposure_us)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config.exposure_time_us = exposure_us;

    if (s_ctx.sensor_ops && s_ctx.sensor_ops->set_exposure) {
        return s_ctx.sensor_ops->set_exposure(exposure_us);
    }

    return ESP_OK;
}

/**
 * @brief Set gain
 */
esp_err_t mipi_camera_set_gain(uint8_t gain)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config.gain = gain;

    if (s_ctx.sensor_ops && s_ctx.sensor_ops->set_gain) {
        return s_ctx.sensor_ops->set_gain(gain);
    }

    return ESP_OK;
}

/**
 * @brief Set HDR mode
 */
esp_err_t mipi_camera_set_hdr(bool enable)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config.hdr_enabled = enable;

    if (s_ctx.sensor_ops && s_ctx.sensor_ops->set_hdr) {
        return s_ctx.sensor_ops->set_hdr(enable);
    }

    return ESP_ERR_NOT_SUPPORTED;
}

/**
 * @brief Get resolution info
 */
esp_err_t mipi_camera_get_resolution_info(
    mipi_camera_sensor_t sensor,
    mipi_camera_resolution_t resolution,
    mipi_camera_resolution_info_t *info)
{
    if (!info || sensor >= 3 || resolution > MIPI_CAMERA_RESOLUTION_4K) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(info, &resolution_table[sensor][resolution],
           sizeof(mipi_camera_resolution_info_t));

    return ESP_OK;
}

/**
 * @brief Get actual FPS
 */
float mipi_camera_get_actual_fps(void)
{
    return s_ctx.actual_fps;
}

/**
 * @brief Get dropped frames
 */
uint32_t mipi_camera_get_dropped_frames(void)
{
    return s_ctx.frames_dropped;
}

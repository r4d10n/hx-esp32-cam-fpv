#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/isp.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MIPI CSI-2 Camera Driver for ESP32-P4
 *
 * Supports high-resolution MIPI cameras:
 * - Sony IMX219 (8MP, 1920x1080 @ 60fps)
 * - Sony IMX477 (12.3MP, 1920x1080 @ 60fps)
 * - OmniVision OV5647 (5MP, 1920x1080 @ 30fps)
 */

/**
 * @brief Supported camera sensors
 */
typedef enum {
    MIPI_CAMERA_SENSOR_IMX219,      ///< Sony IMX219 8MP
    MIPI_CAMERA_SENSOR_IMX477,      ///< Sony IMX477 12.3MP HQ
    MIPI_CAMERA_SENSOR_OV5647,      ///< OmniVision OV5647 5MP
} mipi_camera_sensor_t;

/**
 * @brief Supported resolutions
 */
typedef enum {
    MIPI_CAMERA_RESOLUTION_VGA,           // 640x480
    MIPI_CAMERA_RESOLUTION_HD,            // 1280x720
    MIPI_CAMERA_RESOLUTION_FHD,           // 1920x1080
    MIPI_CAMERA_RESOLUTION_2K,            // 2048x1536
    MIPI_CAMERA_RESOLUTION_4K,            // 4056x3040 (IMX477 only)
} mipi_camera_resolution_t;

/**
 * @brief Pixel formats
 */
typedef enum {
    MIPI_CAMERA_FORMAT_RAW8,
    MIPI_CAMERA_FORMAT_RAW10,
    MIPI_CAMERA_FORMAT_RAW12,
    MIPI_CAMERA_FORMAT_YUV422,
    MIPI_CAMERA_FORMAT_RGB565,
    MIPI_CAMERA_FORMAT_RGB888,
} mipi_camera_format_t;

/**
 * @brief Resolution information
 */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t max_fps;
} mipi_camera_resolution_info_t;

/**
 * @brief Camera configuration
 */
typedef struct {
    mipi_camera_sensor_t sensor;          ///< Camera sensor type
    mipi_camera_resolution_t resolution;  ///< Video resolution
    uint8_t fps;                          ///< Frame rate (up to max_fps)
    mipi_camera_format_t format;          ///< Output pixel format

    // Image processing
    bool hdr_enabled;                     ///< HDR mode (if supported)
    bool auto_exposure;                   ///< Auto exposure control
    bool auto_white_balance;              ///< Auto white balance
    uint16_t exposure_time_us;            ///< Manual exposure time (if auto_exposure=false)
    uint8_t gain;                         ///< Manual gain 0-255 (if auto_exposure=false)

    // MIPI CSI-2 configuration
    uint8_t mipi_lane_count;              ///< Number of data lanes (2 or 4)
    uint32_t mipi_clk_freq_hz;            ///< MIPI clock frequency

    // I2C configuration for sensor control
    uint8_t i2c_sda_pin;
    uint8_t i2c_scl_pin;
    uint8_t i2c_addr;                     ///< Sensor I2C address

    // MIPI pins
    uint8_t mipi_clk_p_pin;
    uint8_t mipi_clk_n_pin;
    uint8_t mipi_data0_p_pin;
    uint8_t mipi_data0_n_pin;
    uint8_t mipi_data1_p_pin;
    uint8_t mipi_data1_n_pin;
    uint8_t mipi_data2_p_pin;
    uint8_t mipi_data2_n_pin;
    uint8_t mipi_data3_p_pin;
    uint8_t mipi_data3_n_pin;
} mipi_camera_config_t;

/**
 * @brief Frame metadata
 */
typedef struct {
    uint32_t frame_index;                 ///< Sequential frame number
    uint64_t timestamp_us;                ///< Capture timestamp (microseconds)
    uint32_t width;
    uint32_t height;
    mipi_camera_format_t format;
    size_t data_size;                     ///< Size of frame data in bytes
    uint16_t exposure_time_us;            ///< Actual exposure time used
    uint8_t gain;                         ///< Actual gain used
} mipi_camera_frame_info_t;

/**
 * @brief Frame callback function
 *
 * Called when a new frame is available.
 *
 * @param frame_data Pointer to frame data (valid only during callback)
 * @param frame_info Frame metadata
 * @param user_data User data passed during callback registration
 *
 * @return true to continue capturing, false to stop
 */
typedef bool (*mipi_camera_frame_cb_t)(
    const uint8_t *frame_data,
    const mipi_camera_frame_info_t *frame_info,
    void *user_data
);

/**
 * @brief Initialize MIPI camera
 *
 * @param config Camera configuration
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_init(const mipi_camera_config_t *config);

/**
 * @brief Deinitialize camera and free resources
 *
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_deinit(void);

/**
 * @brief Register frame callback
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_register_frame_callback(
    mipi_camera_frame_cb_t callback,
    void *user_data
);

/**
 * @brief Start capturing frames
 *
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_start(void);

/**
 * @brief Stop capturing frames
 *
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_stop(void);

/**
 * @brief Set exposure time (manual mode only)
 *
 * @param exposure_us Exposure time in microseconds
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_set_exposure(uint16_t exposure_us);

/**
 * @brief Set gain (manual mode only)
 *
 * @param gain Gain value 0-255
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_set_gain(uint8_t gain);

/**
 * @brief Enable/disable HDR mode
 *
 * @param enable true to enable HDR
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if sensor doesn't support HDR
 */
esp_err_t mipi_camera_set_hdr(bool enable);

/**
 * @brief Get resolution information
 *
 * @param sensor Sensor type
 * @param resolution Resolution enum
 * @param[out] info Resolution information
 * @return ESP_OK on success
 */
esp_err_t mipi_camera_get_resolution_info(
    mipi_camera_sensor_t sensor,
    mipi_camera_resolution_t resolution,
    mipi_camera_resolution_info_t *info
);

/**
 * @brief Get current FPS
 *
 * @return Actual FPS
 */
float mipi_camera_get_actual_fps(void);

/**
 * @brief Get dropped frame count
 *
 * @return Number of dropped frames since start
 */
uint32_t mipi_camera_get_dropped_frames(void);

#ifdef __cplusplus
}
#endif

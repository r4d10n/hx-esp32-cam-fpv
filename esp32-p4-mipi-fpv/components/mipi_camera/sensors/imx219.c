/**
 * @file imx219.c
 * @brief Sony IMX219 8MP MIPI Camera Sensor Driver
 */

#include "imx219.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "IMX219";

#define IMX219_I2C_ADDR     0x10

// Register definitions
#define IMX219_REG_MODE_SELECT      0x0100
#define IMX219_REG_SW_RESET         0x0103
#define IMX219_REG_ORIENTATION      0x0172
#define IMX219_REG_EXPOSURE_HI      0x015A
#define IMX219_REG_EXPOSURE_LO      0x015B
#define IMX219_REG_GAIN             0x0157

// Mode values
#define IMX219_MODE_STANDBY         0x00
#define IMX219_MODE_STREAMING       0x01

static mipi_camera_config_t s_config;

/**
 * @brief Write sensor register
 */
static esp_err_t imx219_write_reg(uint16_t reg, uint8_t value)
{
    return mipi_i2c_write_reg(IMX219_I2C_ADDR, reg, value);
}

/**
 * @brief Read sensor register
 */
static esp_err_t imx219_read_reg(uint16_t reg, uint8_t *value)
{
    return mipi_i2c_read_reg(IMX219_I2C_ADDR, reg, value);
}

/**
 * @brief Initialize IMX219 sensor
 */
static esp_err_t imx219_init(mipi_camera_config_t *config)
{
    ESP_LOGI(TAG, "Initializing IMX219 sensor...");

    memcpy(&s_config, config, sizeof(mipi_camera_config_t));

    // Software reset
    esp_err_t ret = imx219_write_reg(IMX219_REG_SW_RESET, 0x01);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Software reset failed");
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(10));  // Wait for reset

    // Read chip ID (optional verification)
    uint8_t chip_id_hi, chip_id_lo;
    imx219_read_reg(0x0000, &chip_id_hi);
    imx219_read_reg(0x0001, &chip_id_lo);
    uint16_t chip_id = (chip_id_hi << 8) | chip_id_lo;
    ESP_LOGI(TAG, "Chip ID: 0x%04X (expected 0x0219)", chip_id);

    // Configure resolution based on config
    // TODO: Load appropriate register table for selected resolution

    // Set initial exposure and gain
    if (!config->auto_exposure) {
        imx219_write_reg(IMX219_REG_EXPOSURE_HI, (config->exposure_time_us >> 8) & 0xFF);
        imx219_write_reg(IMX219_REG_EXPOSURE_LO, config->exposure_time_us & 0xFF);
    }

    if (config->gain > 0) {
        imx219_write_reg(IMX219_REG_GAIN, config->gain);
    }

    // Orientation
    uint8_t orient = 0x00;
    if (config->hmirror) orient |= 0x01;
    if (config->vflip) orient |= 0x02;
    imx219_write_reg(IMX219_REG_ORIENTATION, orient);

    ESP_LOGI(TAG, "IMX219 initialized successfully");
    return ESP_OK;
}

/**
 * @brief Start streaming
 */
static esp_err_t imx219_start(void)
{
    ESP_LOGI(TAG, "Starting streaming...");
    return imx219_write_reg(IMX219_REG_MODE_SELECT, IMX219_MODE_STREAMING);
}

/**
 * @brief Stop streaming
 */
static esp_err_t imx219_stop(void)
{
    ESP_LOGI(TAG, "Stopping streaming...");
    return imx219_write_reg(IMX219_REG_MODE_SELECT, IMX219_MODE_STANDBY);
}

/**
 * @brief Set exposure time
 */
static esp_err_t imx219_set_exposure(uint16_t exposure_us)
{
    // Convert microseconds to register value (sensor-specific)
    uint16_t exposure_val = exposure_us;  // Simplified

    esp_err_t ret = imx219_write_reg(IMX219_REG_EXPOSURE_HI, (exposure_val >> 8) & 0xFF);
    if (ret == ESP_OK) {
        ret = imx219_write_reg(IMX219_REG_EXPOSURE_LO, exposure_val & 0xFF);
    }

    return ret;
}

/**
 * @brief Set gain
 */
static esp_err_t imx219_set_gain(uint8_t gain)
{
    return imx219_write_reg(IMX219_REG_GAIN, gain);
}

/**
 * @brief Set HDR mode
 */
static esp_err_t imx219_set_hdr(bool enable)
{
    // IMX219 supports HDR mode
    ESP_LOGW(TAG, "HDR mode %s", enable ? "enabled" : "disabled");
    // TODO: Implement HDR configuration
    return ESP_OK;
}

/**
 * @brief Get sensor operations
 */
static sensor_ops_t imx219_ops = {
    .init = imx219_init,
    .start = imx219_start,
    .stop = imx219_stop,
    .set_exposure = imx219_set_exposure,
    .set_gain = imx219_set_gain,
    .set_hdr = imx219_set_hdr,
};

sensor_ops_t* imx219_get_ops(void)
{
    return &imx219_ops;
}

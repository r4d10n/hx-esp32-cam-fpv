/**
 * @file imx477.c
 * @brief Sony IMX477 12.3MP HQ MIPI Camera Sensor Driver
 */

#include "imx477.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "IMX477";

#define IMX477_I2C_ADDR     0x1A

// Register definitions
#define IMX477_REG_MODE_SELECT      0x0100
#define IMX477_REG_SW_RESET         0x0103

static mipi_camera_config_t s_config;

static esp_err_t imx477_write_reg(uint16_t reg, uint8_t value)
{
    return mipi_i2c_write_reg(IMX477_I2C_ADDR, reg, value);
}

static esp_err_t imx477_init(mipi_camera_config_t *config)
{
    ESP_LOGI(TAG, "Initializing IMX477 sensor...");
    memcpy(&s_config, config, sizeof(mipi_camera_config_t));

    // Software reset
    imx477_write_reg(IMX477_REG_SW_RESET, 0x01);
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "IMX477 initialized successfully");
    return ESP_OK;
}

static esp_err_t imx477_start(void)
{
    return imx477_write_reg(IMX477_REG_MODE_SELECT, 0x01);
}

static esp_err_t imx477_stop(void)
{
    return imx477_write_reg(IMX477_REG_MODE_SELECT, 0x00);
}

static esp_err_t imx477_set_exposure(uint16_t exposure_us)
{
    return ESP_OK;  // TODO: Implement
}

static esp_err_t imx477_set_gain(uint8_t gain)
{
    return ESP_OK;  // TODO: Implement
}

static esp_err_t imx477_set_hdr(bool enable)
{
    return ESP_OK;  // IMX477 supports HDR
}

static sensor_ops_t imx477_ops = {
    .init = imx477_init,
    .start = imx477_start,
    .stop = imx477_stop,
    .set_exposure = imx477_set_exposure,
    .set_gain = imx477_set_gain,
    .set_hdr = imx477_set_hdr,
};

sensor_ops_t* imx477_get_ops(void)
{
    return &imx477_ops;
}

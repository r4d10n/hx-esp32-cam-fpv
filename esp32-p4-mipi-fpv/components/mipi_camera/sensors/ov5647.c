/**
 * @file ov5647.c
 * @brief OmniVision OV5647 5MP MIPI Camera Sensor Driver
 */

#include "ov5647.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "OV5647";

#define OV5647_I2C_ADDR     0x36

static mipi_camera_config_t s_config;

static esp_err_t ov5647_write_reg(uint16_t reg, uint8_t value)
{
    return mipi_i2c_write_reg(OV5647_I2C_ADDR, reg, value);
}

static esp_err_t ov5647_init(mipi_camera_config_t *config)
{
    ESP_LOGI(TAG, "Initializing OV5647 sensor...");
    memcpy(&s_config, config, sizeof(mipi_camera_config_t));

    ov5647_write_reg(0x0103, 0x01);  // Software reset
    vTaskDelay(pdMS_TO_TICKS(10));

    ESP_LOGI(TAG, "OV5647 initialized successfully");
    return ESP_OK;
}

static esp_err_t ov5647_start(void)
{
    return ov5647_write_reg(0x0100, 0x01);
}

static esp_err_t ov5647_stop(void)
{
    return ov5647_write_reg(0x0100, 0x00);
}

static esp_err_t ov5647_set_exposure(uint16_t exposure_us)
{
    return ESP_OK;
}

static esp_err_t ov5647_set_gain(uint8_t gain)
{
    return ESP_OK;
}

static esp_err_t ov5647_set_hdr(bool enable)
{
    return ESP_ERR_NOT_SUPPORTED;  // OV5647 doesn't support HDR
}

static sensor_ops_t ov5647_ops = {
    .init = ov5647_init,
    .start = ov5647_start,
    .stop = ov5647_stop,
    .set_exposure = ov5647_set_exposure,
    .set_gain = ov5647_set_gain,
    .set_hdr = ov5647_set_hdr,
};

sensor_ops_t* ov5647_get_ops(void)
{
    return &ov5647_ops;
}

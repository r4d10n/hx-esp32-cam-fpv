#pragma once

#include "../include/mipi_camera.h"

typedef struct {
    esp_err_t (*init)(mipi_camera_config_t *config);
    esp_err_t (*start)(void);
    esp_err_t (*stop)(void);
    esp_err_t (*set_exposure)(uint16_t exposure_us);
    esp_err_t (*set_gain)(uint8_t gain);
    esp_err_t (*set_hdr)(bool enable);
} sensor_ops_t;

sensor_ops_t* imx219_get_ops(void);

// I2C functions from mipi_camera.c
extern esp_err_t mipi_i2c_write_reg(uint8_t addr, uint16_t reg, uint8_t value);
extern esp_err_t mipi_i2c_read_reg(uint8_t addr, uint16_t reg, uint8_t *value);

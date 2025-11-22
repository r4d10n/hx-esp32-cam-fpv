/**
 * @file fec_encoder_wifi.h
 * @brief FEC Encoder for WiFi (placeholder)
 *
 * This would link to the actual FEC implementation from common/fec
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct fec_encoder_s fec_encoder_t;

fec_encoder_t* fec_encoder_create(uint8_t k, uint8_t n, size_t mtu);
void fec_encoder_destroy(fec_encoder_t *enc);
esp_err_t fec_encoder_encode(fec_encoder_t *enc, const uint8_t *data, size_t size);

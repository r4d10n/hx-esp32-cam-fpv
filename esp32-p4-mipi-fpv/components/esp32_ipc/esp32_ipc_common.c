/**
 * @file esp32_ipc_common.c
 * @brief Common IPC functions (CRC, statistics)
 */

#include "esp32_ipc.h"
#include <string.h>

// Statistics
static ipc_stats_t s_stats = {0};

/**
 * @brief CRC16-CCITT implementation
 */
uint16_t ipc_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Get IPC statistics
 */
esp_err_t ipc_get_stats(ipc_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(stats, &s_stats, sizeof(ipc_stats_t));
    return ESP_OK;
}

/**
 * @brief Reset statistics
 */
esp_err_t ipc_reset_stats(void)
{
    memset(&s_stats, 0, sizeof(ipc_stats_t));
    return ESP_OK;
}

/**
 * @brief Update statistics (internal)
 */
void ipc_update_stats_tx(size_t bytes)
{
    s_stats.packets_sent++;
    s_stats.bytes_sent += bytes;
}

void ipc_update_stats_rx(size_t bytes)
{
    s_stats.packets_received++;
    s_stats.bytes_received += bytes;
}

void ipc_update_stats_error(uint32_t error_type)
{
    switch (error_type) {
        case 0: s_stats.crc_errors++; break;
        case 1: s_stats.timeout_errors++; break;
        case 2: s_stats.overflow_errors++; break;
    }
}

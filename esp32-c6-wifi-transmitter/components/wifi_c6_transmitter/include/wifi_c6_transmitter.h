/**
 * @file wifi_c6_transmitter.h
 * @brief WiFi 6 (802.11ax) Transmitter for ESP32-C6
 *
 * This component handles WiFi 6 packet transmission on the ESP32-C6.
 * Features:
 * - WiFi 6 (802.11ax) support
 * - Packet injection in monitor mode
 * - 2.4GHz and 5GHz band support
 * - MCS0-MCS11 modulation
 * - Forward Error Correction (FEC) integration
 * - TX queue management
 * - Real-time statistics
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi band selection
 */
typedef enum {
    WIFI_C6_BAND_2_4GHZ = 0,  /**< 2.4 GHz band */
    WIFI_C6_BAND_5GHZ = 1      /**< 5 GHz band */
} wifi_c6_band_t;

/**
 * @brief WiFi 6 MCS (Modulation and Coding Scheme)
 */
typedef enum {
    WIFI_C6_MCS_0 = 0,   /**< BPSK 1/2 */
    WIFI_C6_MCS_1 = 1,   /**< QPSK 1/2 */
    WIFI_C6_MCS_2 = 2,   /**< QPSK 3/4 */
    WIFI_C6_MCS_3 = 3,   /**< 16-QAM 1/2 */
    WIFI_C6_MCS_4 = 4,   /**< 16-QAM 3/4 */
    WIFI_C6_MCS_5 = 5,   /**< 64-QAM 2/3 */
    WIFI_C6_MCS_6 = 6,   /**< 64-QAM 3/4 */
    WIFI_C6_MCS_7 = 7,   /**< 64-QAM 5/6 */
    WIFI_C6_MCS_8 = 8,   /**< 256-QAM 3/4 */
    WIFI_C6_MCS_9 = 9,   /**< 256-QAM 5/6 */
    WIFI_C6_MCS_10 = 10, /**< 1024-QAM 3/4 */
    WIFI_C6_MCS_11 = 11  /**< 1024-QAM 5/6 */
} wifi_c6_mcs_t;

/**
 * @brief WiFi transmitter configuration
 */
typedef struct {
    wifi_c6_band_t band;        /**< WiFi band (2.4GHz or 5GHz) */
    uint8_t channel;            /**< WiFi channel number */
    wifi_c6_mcs_t mcs;          /**< Modulation and coding scheme */
    uint8_t tx_power_dbm;       /**< TX power in dBm (5-20) */
    bool enable_fec;            /**< Enable forward error correction */
    uint8_t fec_k;              /**< FEC: data blocks (default 6) */
    uint8_t fec_n;              /**< FEC: total blocks (default 12) */
    size_t tx_queue_size;       /**< TX queue size in packets */
} wifi_c6_tx_config_t;

/**
 * @brief WiFi transmitter statistics
 */
typedef struct {
    uint64_t packets_sent;         /**< Total packets transmitted */
    uint64_t packets_failed;       /**< Failed transmissions */
    uint64_t bytes_sent;           /**< Total bytes transmitted */
    float throughput_mbps;         /**< Current throughput in Mbps */
    uint32_t queue_usage_percent;  /**< TX queue usage percentage */
    uint32_t fec_blocks_sent;      /**< FEC redundancy blocks sent */
} wifi_c6_tx_stats_t;

/**
 * @brief Packet priority for TX queue
 */
typedef enum {
    WIFI_C6_PRIORITY_LOW = 0,
    WIFI_C6_PRIORITY_NORMAL = 1,
    WIFI_C6_PRIORITY_HIGH = 2
} wifi_c6_priority_t;

/**
 * @brief Initialize WiFi 6 transmitter
 *
 * @param config Transmitter configuration
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_init(const wifi_c6_tx_config_t *config);

/**
 * @brief Start WiFi transmission
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_start(void);

/**
 * @brief Stop WiFi transmission
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_stop(void);

/**
 * @brief Send video packet
 *
 * @param nalu_data H.264 NAL unit data
 * @param nalu_size Size of NAL unit
 * @param nal_type NAL unit type
 * @param is_keyframe True if keyframe
 * @param frame_index Frame sequence number
 * @param pts_us Presentation timestamp (microseconds)
 * @param dts_us Decode timestamp (microseconds)
 * @param priority Packet priority
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_send_video(
    const uint8_t *nalu_data, size_t nalu_size,
    uint8_t nal_type, bool is_keyframe,
    uint32_t frame_index, uint64_t pts_us, uint64_t dts_us,
    wifi_c6_priority_t priority);

/**
 * @brief Send telemetry packet
 *
 * @param telemetry_data Telemetry data
 * @param telemetry_size Size of telemetry data
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_send_telemetry(
    const uint8_t *telemetry_data, size_t telemetry_size);

/**
 * @brief Get transmitter statistics
 *
 * @param stats Pointer to statistics structure
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_get_stats(wifi_c6_tx_stats_t *stats);

/**
 * @brief Reset transmitter statistics
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_reset_stats(void);

/**
 * @brief Set WiFi channel
 *
 * @param channel Channel number
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_set_channel(uint8_t channel);

/**
 * @brief Set MCS (Modulation and Coding Scheme)
 *
 * @param mcs MCS value
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_set_mcs(wifi_c6_mcs_t mcs);

/**
 * @brief Set TX power
 *
 * @param tx_power_dbm TX power in dBm (5-20)
 * @return ESP_OK on success
 */
esp_err_t wifi_c6_tx_set_power(uint8_t tx_power_dbm);

#ifdef __cplusplus
}
#endif

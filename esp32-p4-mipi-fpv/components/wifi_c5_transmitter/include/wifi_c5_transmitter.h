#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi 6 (802.11ax) Transmitter for ESP32-C5
 *
 * High-throughput video transmission using WiFi packet injection.
 * Supports both 2.4GHz and 5GHz bands.
 */

/**
 * @brief WiFi frequency bands
 */
typedef enum {
    WIFI_BAND_2_4GHZ,                   ///< 2.4GHz band (ch 1-14)
    WIFI_BAND_5GHZ,                     ///< 5GHz band (ch 36-165)
} wifi_band_t;

/**
 * @brief WiFi 6 MCS (Modulation and Coding Scheme) indices
 */
typedef enum {
    WIFI_MCS_0,     // BPSK 1/2       - ~8.6 Mbps @ 20MHz
    WIFI_MCS_1,     // QPSK 1/2       - ~17.2 Mbps
    WIFI_MCS_2,     // QPSK 3/4       - ~25.8 Mbps
    WIFI_MCS_3,     // 16-QAM 1/2     - ~34.4 Mbps
    WIFI_MCS_4,     // 16-QAM 3/4     - ~51.6 Mbps
    WIFI_MCS_5,     // 64-QAM 2/3     - ~68.8 Mbps
    WIFI_MCS_6,     // 64-QAM 3/4     - ~77.4 Mbps
    WIFI_MCS_7,     // 64-QAM 5/6     - ~86 Mbps
    WIFI_MCS_8,     // 256-QAM 3/4    - ~103.2 Mbps (WiFi 6)
    WIFI_MCS_9,     // 256-QAM 5/6    - ~114.7 Mbps
    WIFI_MCS_10,    // 1024-QAM 3/4   - ~129 Mbps (WiFi 6)
    WIFI_MCS_11,    // 1024-QAM 5/6   - ~143.4 Mbps
} wifi_mcs_index_t;

/**
 * @brief WiFi transmitter configuration
 */
typedef struct {
    wifi_band_t band;                   ///< Frequency band
    uint8_t channel;                    ///< WiFi channel
    wifi_mcs_index_t mcs;               ///< Modulation scheme
    int8_t tx_power_dbm;                ///< TX power (5-20 dBm)

    // FEC settings
    bool enable_fec;                    ///< Enable FEC encoding
    uint8_t fec_k;                      ///< FEC data blocks (default: 6)
    uint8_t fec_n;                      ///< FEC total blocks (default: 12)

    // Packet settings
    uint16_t mtu;                       ///< Maximum packet size (default: 1500)
    uint8_t retry_count;                ///< TX retry count (0-15)

    // Device IDs (for protocol compatibility)
    uint16_t air_device_id;             ///< Air unit device ID
    uint16_t gs_device_id;              ///< Ground station device ID (0 = broadcast)

    // Performance
    size_t tx_queue_size;               ///< TX queue size (packets)
    uint8_t tx_task_priority;           ///< TX task priority
    uint8_t tx_task_core;               ///< TX task CPU core (0 or 1)
} wifi_tx_config_t;

/**
 * @brief Initialize WiFi transmitter
 *
 * @param config Transmitter configuration
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_init(const wifi_tx_config_t *config);

/**
 * @brief Deinitialize WiFi transmitter
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_deinit(void);

/**
 * @brief Start WiFi transmission
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_start(void);

/**
 * @brief Stop WiFi transmission
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_stop(void);

/**
 * @brief Send video NAL unit
 *
 * @param nalu_data NAL unit data
 * @param nalu_size NAL unit size
 * @param nal_type NAL type
 * @param is_keyframe True if keyframe
 * @param frame_index Frame number
 * @param pts_us Presentation timestamp
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_send_video(
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint8_t nal_type,
    bool is_keyframe,
    uint32_t frame_index,
    uint64_t pts_us
);

/**
 * @brief Send telemetry data
 *
 * @param data Telemetry data
 * @param size Data size
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_send_telemetry(
    const uint8_t *data,
    size_t size
);

/**
 * @brief Send OSD data
 *
 * @param osd_buffer OSD buffer data
 * @param size Buffer size
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_send_osd(
    const uint8_t *osd_buffer,
    size_t size
);

/**
 * @brief Set WiFi channel
 *
 * @param channel Channel number
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_set_channel(uint8_t channel);

/**
 * @brief Set TX power
 *
 * @param power_dbm TX power in dBm (5-20)
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_set_power(int8_t power_dbm);

/**
 * @brief Set MCS rate
 *
 * @param mcs MCS index
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_set_mcs(wifi_mcs_index_t mcs);

/**
 * @brief WiFi transmitter statistics
 */
typedef struct {
    uint64_t packets_sent;              ///< Total packets sent
    uint64_t packets_dropped;           ///< Packets dropped (queue full)
    uint64_t bytes_sent;                ///< Total bytes sent
    uint32_t video_packets_sent;        ///< Video packets sent
    uint32_t telemetry_packets_sent;    ///< Telemetry packets sent

    float throughput_mbps;              ///< Current throughput (Mbps)
    uint8_t queue_usage_percent;        ///< TX queue usage (0-100%)

    int8_t rssi_dbm;                    ///< RSSI from GS (if available)
    uint8_t packet_loss_percent;        ///< Estimated packet loss (0-100%)

    uint32_t fec_blocks_sent;           ///< FEC blocks sent
    uint32_t wifi_tx_errors;            ///< WiFi TX errors
} wifi_tx_stats_t;

/**
 * @brief Get transmitter statistics
 *
 * @param[out] stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_get_stats(wifi_tx_stats_t *stats);

/**
 * @brief Reset statistics
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_reset_stats(void);

/**
 * @brief Get channel information
 *
 * @param channel Channel number
 * @param[out] freq_mhz Frequency in MHz
 * @param[out] max_power_dbm Maximum allowed power
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_get_channel_info(
    uint8_t channel,
    uint16_t *freq_mhz,
    int8_t *max_power_dbm
);

/**
 * @brief Scan for cleanest channel
 *
 * @param band Frequency band
 * @param[out] best_channel Best channel found
 * @param[out] noise_floor Noise floor in dBm
 * @param timeout_ms Scan timeout
 * @return ESP_OK on success
 */
esp_err_t wifi_tx_scan_best_channel(
    wifi_band_t band,
    uint8_t *best_channel,
    int8_t *noise_floor,
    uint32_t timeout_ms
);

#ifdef __cplusplus
}
#endif

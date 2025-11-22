/**
 * @file wifi_receiver.h
 * @brief WiFi Monitor Mode Receiver for ESP32-S3
 *
 * This component handles WiFi packet reception in monitor mode.
 * Features:
 * - WiFi monitor mode configuration
 * - Raw packet capture and filtering
 * - 2.4GHz and 5GHz band support
 * - RSSI and noise floor monitoring
 * - Packet validation and CRC checking
 * - Multi-channel hopping support
 * - Real-time reception statistics
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief WiFi band selection
 */
typedef enum {
    WIFI_RX_BAND_2_4GHZ = 0,  /**< 2.4 GHz band */
    WIFI_RX_BAND_5GHZ = 1      /**< 5 GHz band */
} wifi_rx_band_t;

/**
 * @brief WiFi packet type
 */
typedef enum {
    WIFI_RX_PKT_MGMT = 0,     /**< Management frame */
    WIFI_RX_PKT_CTRL = 1,     /**< Control frame */
    WIFI_RX_PKT_DATA = 2,     /**< Data frame */
    WIFI_RX_PKT_UNKNOWN = 3   /**< Unknown frame type */
} wifi_rx_pkt_type_t;

/**
 * @brief Received packet information
 */
typedef struct {
    uint8_t *payload;              /**< Packet payload data */
    size_t payload_len;            /**< Payload length in bytes */
    wifi_rx_pkt_type_t pkt_type;   /**< Packet type */
    int8_t rssi;                   /**< RSSI in dBm */
    uint8_t channel;               /**< Reception channel */
    uint8_t rate;                  /**< Data rate */
    uint64_t timestamp_us;         /**< Reception timestamp (microseconds) */
    uint32_t sequence_num;         /**< Packet sequence number */
    bool crc_valid;                /**< CRC validation result */
    uint8_t src_mac[6];            /**< Source MAC address */
    uint8_t dst_mac[6];            /**< Destination MAC address */
} wifi_rx_packet_info_t;

/**
 * @brief WiFi receiver callback function type
 *
 * Called when a packet is received that matches the configured filter.
 * Must not block or perform lengthy operations.
 *
 * @param pkt_info Packet information structure
 * @param user_ctx User context pointer
 */
typedef void (*wifi_rx_callback_t)(const wifi_rx_packet_info_t *pkt_info, void *user_ctx);

/**
 * @brief WiFi receiver configuration
 */
typedef struct {
    wifi_rx_band_t band;           /**< WiFi band (2.4GHz or 5GHz) */
    uint8_t channel;               /**< WiFi channel number */
    uint8_t filter_mac[6];         /**< MAC address filter (all zeros = no filter) */
    bool enable_promiscuous;       /**< Enable promiscuous mode */
    bool filter_crc_errors;        /**< Filter out packets with CRC errors */
    wifi_rx_callback_t callback;   /**< Packet reception callback */
    void *callback_ctx;            /**< User context for callback */
    size_t rx_buffer_size;         /**< RX buffer size in packets */
} wifi_rx_config_t;

/**
 * @brief WiFi receiver statistics
 */
typedef struct {
    uint64_t packets_received;     /**< Total packets received */
    uint64_t packets_valid;        /**< Valid packets (CRC OK) */
    uint64_t packets_crc_error;    /**< Packets with CRC errors */
    uint64_t packets_dropped;      /**< Packets dropped due to buffer full */
    uint64_t bytes_received;       /**< Total bytes received */
    float throughput_mbps;         /**< Current throughput in Mbps */
    int8_t avg_rssi_dbm;           /**< Average RSSI in dBm */
    int8_t min_rssi_dbm;           /**< Minimum RSSI in dBm */
    int8_t max_rssi_dbm;           /**< Maximum RSSI in dBm */
    int8_t noise_floor_dbm;        /**< Noise floor in dBm */
    uint32_t buffer_usage_percent; /**< RX buffer usage percentage */
} wifi_rx_stats_t;

/**
 * @brief Initialize WiFi receiver
 *
 * Initializes the WiFi subsystem and configures it for monitor mode.
 *
 * @param config Receiver configuration
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if config is invalid
 *         ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t wifi_rx_init(const wifi_rx_config_t *config);

/**
 * @brief Deinitialize WiFi receiver
 *
 * Stops reception and frees all resources.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_deinit(void);

/**
 * @brief Start WiFi reception
 *
 * Begins receiving packets on the configured channel.
 *
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if not initialized
 */
esp_err_t wifi_rx_start(void);

/**
 * @brief Stop WiFi reception
 *
 * Stops packet reception without deinitializing.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_stop(void);

/**
 * @brief Set WiFi channel
 *
 * Changes the reception channel. Can be called while receiving.
 *
 * @param channel Channel number (1-13 for 2.4GHz, 36-165 for 5GHz)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if channel is invalid
 */
esp_err_t wifi_rx_set_channel(uint8_t channel);

/**
 * @brief Get current WiFi channel
 *
 * @param channel Pointer to store current channel
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_get_channel(uint8_t *channel);

/**
 * @brief Set MAC address filter
 *
 * Sets a MAC address filter. Only packets matching this MAC
 * (source or destination) will be passed to the callback.
 * Set to all zeros to disable filtering.
 *
 * @param mac MAC address (6 bytes)
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_set_mac_filter(const uint8_t mac[6]);

/**
 * @brief Enable/disable promiscuous mode
 *
 * @param enable True to enable, false to disable
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_set_promiscuous(bool enable);

/**
 * @brief Get receiver statistics
 *
 * @param stats Pointer to statistics structure
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_get_stats(wifi_rx_stats_t *stats);

/**
 * @brief Reset receiver statistics
 *
 * Resets all statistics counters to zero.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_reset_stats(void);

/**
 * @brief Get current RSSI
 *
 * Gets the RSSI of the most recently received packet.
 *
 * @param rssi Pointer to store RSSI value (dBm)
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_get_rssi(int8_t *rssi);

/**
 * @brief Get noise floor
 *
 * Gets the current noise floor measurement.
 *
 * @param noise_floor Pointer to store noise floor value (dBm)
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_get_noise_floor(int8_t *noise_floor);

/**
 * @brief Enable channel hopping
 *
 * Enables automatic channel hopping across specified channels.
 *
 * @param channels Array of channel numbers
 * @param num_channels Number of channels in array
 * @param dwell_time_ms Time to spend on each channel (ms)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if parameters are invalid
 */
esp_err_t wifi_rx_enable_channel_hopping(const uint8_t *channels, size_t num_channels, uint32_t dwell_time_ms);

/**
 * @brief Disable channel hopping
 *
 * Stops channel hopping and remains on current channel.
 *
 * @return ESP_OK on success
 */
esp_err_t wifi_rx_disable_channel_hopping(void);

#ifdef __cplusplus
}
#endif

/**
 * @file fec_decoder.h
 * @brief Forward Error Correction Decoder for ESP32-S3
 *
 * This component implements Reed-Solomon FEC decoding optimized for ESP32-S3.
 * It supports variable K/N ratios and provides real-time decoding capability
 * for WiFi-based video streaming applications.
 *
 * Features:
 * - Reed-Solomon (RS) decoding compatible with transmitter FEC
 * - Support for variable K/N ratios (6/12, 8/16, etc.)
 * - Block assembly and error correction
 * - Performance optimization for ESP32-S3
 * - Statistics tracking (corrected blocks, uncorrectable errors)
 * - Efficient memory management
 *
 * @copyright Copyright (c) 2024
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Maximum supported coding K value (number of data packets)
 */
#define FEC_DECODER_MAX_K 16

/**
 * @brief Maximum supported coding N value (total packets including FEC)
 */
#define FEC_DECODER_MAX_N 32

/**
 * @brief Default MTU size for packets
 */
#define FEC_DECODER_DEFAULT_MTU 1400

/**
 * @brief FEC packet header structure (must match transmitter format)
 */
typedef struct __attribute__((packed)) {
    uint8_t packet_version;     ///< Packet version (must be 2)
    uint8_t packet_signature;   ///< Packet signature (must be 56)
    uint16_t from_device_id;    ///< Source device ID
    uint16_t to_device_id;      ///< Destination device ID
    uint16_t size;              ///< Payload size in bytes
    uint32_t block_index : 24;  ///< Block sequence number
    uint32_t packet_index : 8;  ///< Packet index within block
} fec_packet_header_t;

/**
 * @brief FEC decoder statistics
 */
typedef struct {
    uint32_t blocks_received;        ///< Total blocks received
    uint32_t blocks_decoded;         ///< Blocks successfully decoded with FEC
    uint32_t blocks_complete;        ///< Blocks received complete (no FEC needed)
    uint32_t blocks_uncorrectable;   ///< Blocks with uncorrectable errors
    uint32_t packets_received;       ///< Total packets received
    uint32_t packets_corrected;      ///< Packets recovered via FEC
    uint32_t packets_duplicate;      ///< Duplicate packets received
    uint32_t packets_old;            ///< Old/late packets discarded
    uint32_t blocks_abandoned;       ///< Blocks abandoned due to newer block
    uint64_t total_bytes_decoded;    ///< Total bytes successfully decoded
    uint32_t current_block_index;    ///< Current block being processed
} fec_decoder_stats_t;

/**
 * @brief FEC decoder configuration
 */
typedef struct {
    uint8_t coding_k;           ///< Number of data packets per block
    uint8_t coding_n;           ///< Total packets per block (data + FEC)
    uint16_t mtu;               ///< Maximum transmission unit (packet payload size)
    uint8_t max_blocks_pending; ///< Maximum number of blocks to buffer (default: 4)
    bool enable_stats;          ///< Enable statistics tracking (default: true)
} fec_decoder_config_t;

/**
 * @brief Opaque FEC decoder handle
 */
typedef struct fec_decoder_s* fec_decoder_handle_t;

/**
 * @brief Callback function for decoded data
 *
 * Called when a complete packet is decoded and ready for processing.
 * This callback is executed from the decoder task context.
 *
 * @param data Pointer to decoded data
 * @param size Size of decoded data in bytes
 * @param user_ctx User context passed during initialization
 */
typedef void (*fec_decoder_data_cb_t)(const void *data, size_t size, void *user_ctx);

/**
 * @brief Initialize FEC decoder with default configuration
 *
 * @param config Decoder configuration
 * @return Configuration structure with default values
 */
fec_decoder_config_t fec_decoder_get_default_config(void);

/**
 * @brief Create and initialize FEC decoder
 *
 * @param config Decoder configuration
 * @param data_callback Callback function for decoded data
 * @param user_ctx User context passed to callback
 * @param[out] handle Pointer to store decoder handle
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t fec_decoder_create(
    const fec_decoder_config_t *config,
    fec_decoder_data_cb_t data_callback,
    void *user_ctx,
    fec_decoder_handle_t *handle
);

/**
 * @brief Destroy FEC decoder and free resources
 *
 * @param handle Decoder handle
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_destroy(fec_decoder_handle_t handle);

/**
 * @brief Process incoming encoded packet
 *
 * This function should be called for each received packet.
 * The decoder will assemble blocks and perform FEC decoding as needed.
 *
 * @param handle Decoder handle
 * @param data Packet data (including FEC header)
 * @param size Packet size in bytes
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t fec_decoder_process_packet(
    fec_decoder_handle_t handle,
    const void *data,
    size_t size
);

/**
 * @brief Get decoder statistics
 *
 * @param handle Decoder handle
 * @param[out] stats Pointer to store statistics
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_get_stats(
    fec_decoder_handle_t handle,
    fec_decoder_stats_t *stats
);

/**
 * @brief Reset decoder statistics
 *
 * @param handle Decoder handle
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_reset_stats(fec_decoder_handle_t handle);

/**
 * @brief Update decoder configuration (K/N ratio)
 *
 * This can be used to dynamically adjust FEC parameters.
 * Current blocks will be flushed.
 *
 * @param handle Decoder handle
 * @param coding_k New K value
 * @param coding_n New N value
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_update_coding(
    fec_decoder_handle_t handle,
    uint8_t coding_k,
    uint8_t coding_n
);

/**
 * @brief Flush pending blocks and reset decoder state
 *
 * @param handle Decoder handle
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_flush(fec_decoder_handle_t handle);

/**
 * @brief Get current block index being processed
 *
 * @param handle Decoder handle
 * @param[out] block_index Pointer to store block index
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_get_current_block(
    fec_decoder_handle_t handle,
    uint32_t *block_index
);

#ifdef __cplusplus
}
#endif

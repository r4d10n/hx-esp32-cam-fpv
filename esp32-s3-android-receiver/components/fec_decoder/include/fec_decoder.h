/**
 * @file fec_decoder.h
 * @brief Forward Error Correction (FEC) Decoder for ESP32-S3
 *
 * This component implements Reed-Solomon FEC decoding for video streams.
 * Features:
 * - Reed-Solomon (n,k) erasure coding
 * - Packet loss recovery
 * - Automatic block reconstruction
 * - Statistics tracking
 * - Configurable redundancy levels
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
 * @brief FEC decoder handle
 */
typedef struct fec_decoder_s fec_decoder_t;

/**
 * @brief FEC block status
 */
typedef enum {
    FEC_BLOCK_INCOMPLETE = 0,  /**< Still waiting for packets */
    FEC_BLOCK_COMPLETE = 1,    /**< All data packets received */
    FEC_BLOCK_RECOVERED = 2,   /**< Recovered using redundancy */
    FEC_BLOCK_FAILED = 3       /**< Cannot recover, too many losses */
} fec_block_status_t;

/**
 * @brief FEC decoder configuration
 */
typedef struct {
    uint8_t k;                     /**< Number of data blocks */
    uint8_t n;                     /**< Total blocks (data + redundancy) */
    size_t max_block_size;         /**< Maximum block size in bytes */
    size_t max_pending_blocks;     /**< Maximum concurrent FEC blocks */
    uint32_t block_timeout_ms;     /**< Block reconstruction timeout */
} fec_decoder_config_t;

/**
 * @brief FEC decoder statistics
 */
typedef struct {
    uint64_t blocks_received;      /**< Total FEC blocks processed */
    uint64_t blocks_complete;      /**< Blocks completed without FEC */
    uint64_t blocks_recovered;     /**< Blocks recovered via FEC */
    uint64_t blocks_failed;        /**< Blocks that couldn't be recovered */
    uint64_t packets_received;     /**< Total packets received */
    uint64_t packets_lost;         /**< Total packets lost */
    float packet_loss_rate;        /**< Packet loss rate (0.0-1.0) */
    float recovery_success_rate;   /**< FEC recovery success rate */
    uint32_t avg_recovery_time_us; /**< Average recovery time (microseconds) */
} fec_decoder_stats_t;

/**
 * @brief FEC decoded block callback
 *
 * Called when a FEC block is successfully decoded (either complete or recovered).
 *
 * @param block_id FEC block identifier
 * @param data Decoded data
 * @param data_len Length of decoded data
 * @param status Block completion status
 * @param user_ctx User context pointer
 */
typedef void (*fec_decoded_callback_t)(
    uint32_t block_id,
    const uint8_t *data,
    size_t data_len,
    fec_block_status_t status,
    void *user_ctx
);

/**
 * @brief Create FEC decoder instance
 *
 * @param config Decoder configuration
 * @param callback Decoded block callback
 * @param callback_ctx User context for callback
 * @return Decoder handle or NULL on failure
 */
fec_decoder_t* fec_decoder_create(
    const fec_decoder_config_t *config,
    fec_decoded_callback_t callback,
    void *callback_ctx
);

/**
 * @brief Destroy FEC decoder instance
 *
 * @param decoder Decoder handle
 */
void fec_decoder_destroy(fec_decoder_t *decoder);

/**
 * @brief Process received FEC packet
 *
 * Adds a packet to the appropriate FEC block and attempts reconstruction
 * if enough packets have been received.
 *
 * @param decoder Decoder handle
 * @param block_id FEC block identifier
 * @param packet_index Packet index within block (0 to n-1)
 * @param packet_data Packet data
 * @param packet_len Packet length
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if parameters are invalid
 *         ESP_ERR_NO_MEM if out of memory
 */
esp_err_t fec_decoder_process_packet(
    fec_decoder_t *decoder,
    uint32_t block_id,
    uint8_t packet_index,
    const uint8_t *packet_data,
    size_t packet_len
);

/**
 * @brief Force reconstruction of a FEC block
 *
 * Attempts to reconstruct a block even if not all packets received.
 * Useful when block timeout occurs.
 *
 * @param decoder Decoder handle
 * @param block_id FEC block identifier
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if block_id is invalid
 *         ESP_FAIL if reconstruction failed
 */
esp_err_t fec_decoder_flush_block(fec_decoder_t *decoder, uint32_t block_id);

/**
 * @brief Get decoder statistics
 *
 * @param decoder Decoder handle
 * @param stats Pointer to statistics structure
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_get_stats(fec_decoder_t *decoder, fec_decoder_stats_t *stats);

/**
 * @brief Reset decoder statistics
 *
 * @param decoder Decoder handle
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_reset_stats(fec_decoder_t *decoder);

/**
 * @brief Get pending blocks count
 *
 * Returns the number of FEC blocks currently being reconstructed.
 *
 * @param decoder Decoder handle
 * @param count Pointer to store count
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_get_pending_blocks(fec_decoder_t *decoder, size_t *count);

/**
 * @brief Purge old blocks
 *
 * Removes FEC blocks that have exceeded the timeout without completion.
 *
 * @param decoder Decoder handle
 * @return Number of blocks purged
 */
size_t fec_decoder_purge_old_blocks(fec_decoder_t *decoder);

#ifdef __cplusplus
}
#endif

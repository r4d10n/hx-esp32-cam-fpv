/**
 * ESP32 FPV Ground Station - FEC Decoder
 *
 * Forward Error Correction decoder for recovering lost packets.
 * Ported from zfec library (Reed-Solomon over GF(2^8)).
 */

#ifndef FEC_DECODER_H
#define FEC_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// FEC configuration
#define FEC_MAX_K           16      // Max data packets per block
#define FEC_MAX_N           32      // Max total packets per block
#define FEC_MAX_PACKET_SIZE 1500    // Max packet payload size

// FEC block state
typedef enum {
    FEC_BLOCK_EMPTY = 0,
    FEC_BLOCK_PARTIAL,
    FEC_BLOCK_READY,        // Has K+ packets, ready to decode
    FEC_BLOCK_COMPLETE,     // All K data packets present (no FEC needed)
    FEC_BLOCK_DECODED,      // Successfully decoded
    FEC_BLOCK_FAILED,       // Could not decode (not enough packets)
} fec_block_state_t;

// Single packet in a block
typedef struct {
    uint8_t *data;
    size_t size;
    uint8_t packet_index;   // 0..K-1 = data, K..N-1 = FEC
    bool present;
    bool recovered;         // Was recovered by FEC
} fec_packet_t;

// FEC block (collection of packets for one block_index)
typedef struct {
    uint32_t block_index;
    fec_block_state_t state;
    int64_t first_packet_time;  // For timeout
    uint8_t data_count;         // Number of data packets (index < K)
    uint8_t fec_count;          // Number of FEC packets (index >= K)
    uint8_t total_count;        // Total packets received
    size_t max_packet_size;     // Largest packet in this block
    fec_packet_t packets[FEC_MAX_N];
} fec_block_t;

// FEC decoder statistics
typedef struct {
    uint32_t blocks_received;
    uint32_t blocks_complete;       // All data packets present
    uint32_t blocks_recovered;      // Required FEC to recover
    uint32_t blocks_failed;         // Could not recover
    uint32_t packets_recovered;     // Individual packets recovered by FEC
    uint32_t packets_dropped;       // Packets from expired blocks
} fec_stats_t;

// Callback for decoded packets
typedef void (*fec_packet_callback_t)(uint32_t block_index, uint8_t packet_index,
                                       const uint8_t *data, size_t size, bool recovered);

/**
 * Initialize FEC decoder
 * @param k Number of data packets per block
 * @param n Total packets per block (k + parity)
 * @param block_count Number of blocks to buffer
 * @param timeout_ms Block timeout in milliseconds
 */
esp_err_t fec_decoder_init(uint8_t k, uint8_t n, uint8_t block_count, uint32_t timeout_ms);

/**
 * Deinitialize FEC decoder
 */
void fec_decoder_deinit(void);

/**
 * Set callback for decoded packets
 */
void fec_decoder_set_callback(fec_packet_callback_t callback);

/**
 * Add a received packet to the decoder
 * @param block_index FEC block index
 * @param packet_index Packet index within block (0..N-1)
 * @param data Packet payload
 * @param size Payload size
 * @return ESP_OK on success
 */
esp_err_t fec_decoder_add_packet(uint32_t block_index, uint8_t packet_index,
                                  const uint8_t *data, size_t size);

/**
 * Process pending blocks (call periodically)
 * Decodes ready blocks and expires timed-out blocks
 */
void fec_decoder_process(void);

/**
 * Get decoder statistics
 */
void fec_decoder_get_stats(fec_stats_t *stats);

/**
 * Reset decoder state
 */
void fec_decoder_reset(void);

#endif // FEC_DECODER_H

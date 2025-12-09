/**
 * @file wfb_rx.h
 * @brief WFB-NG Receiver for ESP32-S3
 *
 * Receives and decodes WFB-NG packets from WiFi promiscuous mode,
 * performs FEC recovery, and outputs decoded video/telemetry data.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <functional>
#include "wfb_protocol.h"
#include "wfb_crypto.h"

// ============================================================================
// Configuration
// ============================================================================

#define WFB_RX_MAX_BLOCKS           16      // Max concurrent FEC blocks
#define WFB_RX_PACKET_POOL_SIZE     192     // Pre-allocated packet buffers
#define WFB_RX_MAX_PACKET_SIZE      1600    // Max single packet size
#define WFB_RX_BLOCK_TIMEOUT_MS     100     // Expire incomplete blocks

// ============================================================================
// Types
// ============================================================================

/**
 * @brief Statistics for WFB-NG receiver
 */
struct WfbRxStats {
    // Packet counters
    uint32_t packets_received;
    uint32_t packets_decrypted;
    uint32_t packets_decrypt_failed;
    uint32_t packets_fec_recovered;
    uint32_t session_packets;

    // Block counters
    uint32_t blocks_complete;
    uint32_t blocks_recovered;
    uint32_t blocks_failed;

    // Output counters
    uint32_t frames_output;
    uint64_t bytes_output;

    // Signal info (from last packet)
    int8_t last_rssi;
    uint8_t last_mcs;
    uint16_t last_freq;

    void reset() {
        packets_received = 0;
        packets_decrypted = 0;
        packets_decrypt_failed = 0;
        packets_fec_recovered = 0;
        session_packets = 0;
        blocks_complete = 0;
        blocks_recovered = 0;
        blocks_failed = 0;
        frames_output = 0;
        bytes_output = 0;
        last_rssi = 0;
        last_mcs = 0;
        last_freq = 0;
    }

    void print() const;
};

/**
 * @brief FEC block state for WFB-NG
 */
struct WfbFecBlock {
    uint64_t block_index;           // Block number
    bool     is_active;
    bool     is_decoded;
    uint32_t first_packet_time_ms;

    // Fragment tracking
    uint8_t  fec_k;                 // Expected data fragments
    uint8_t  fec_n;                 // Expected total fragments
    uint8_t  fragments_received;
    uint32_t fragment_mask_lo;      // Bits 0-31
    uint32_t fragment_mask_hi;      // Bits 32-63

    // Fragment data
    uint8_t* fragment_data[64];     // Pointers to fragment buffers
    uint16_t fragment_size[64];     // Size of each fragment

    void reset() {
        block_index = 0;
        is_active = false;
        is_decoded = false;
        first_packet_time_ms = 0;
        fec_k = 0;
        fec_n = 0;
        fragments_received = 0;
        fragment_mask_lo = 0;
        fragment_mask_hi = 0;
        for (int i = 0; i < 64; i++) {
            fragment_data[i] = nullptr;
            fragment_size[i] = 0;
        }
    }

    bool has_fragment(uint8_t idx) const {
        if (idx < 32) return (fragment_mask_lo & (1u << idx)) != 0;
        else return (fragment_mask_hi & (1u << (idx - 32))) != 0;
    }

    void set_fragment(uint8_t idx) {
        if (idx < 32) fragment_mask_lo |= (1u << idx);
        else fragment_mask_hi |= (1u << (idx - 32));
    }

    bool can_decode() const {
        return fragments_received >= fec_k;
    }

    bool is_complete() const {
        // All K primary fragments received
        uint32_t primary_mask = (1u << fec_k) - 1;
        return (fragment_mask_lo & primary_mask) == primary_mask;
    }
};

/**
 * @brief Callback for decoded data output
 *
 * @param data Pointer to decoded data
 * @param len Length of decoded data
 * @param flags Packet flags from wfb_packet_hdr_t
 */
using WfbDataCallback = std::function<void(const uint8_t* data, size_t len, uint8_t flags)>;

/**
 * @brief Callback for session updates
 *
 * @param fec_k New FEC K parameter
 * @param fec_n New FEC N parameter
 * @param epoch Session epoch
 */
using WfbSessionCallback = std::function<void(uint8_t fec_k, uint8_t fec_n, uint64_t epoch)>;

// ============================================================================
// WFB-NG Receiver Class
// ============================================================================

class WfbReceiver {
public:
    WfbReceiver();
    ~WfbReceiver();

    /**
     * @brief Initialize the receiver
     *
     * @param use_psram Allocate buffers from PSRAM
     * @return true on success
     */
    bool init(bool use_psram = true);

    /**
     * @brief Deinitialize and free resources
     */
    void deinit();

    /**
     * @brief Load ground station key for decryption
     *
     * @param key_data 64-byte gs.key file contents
     * @return true on success
     */
    bool load_key(const uint8_t* key_data);

    /**
     * @brief Set transmitter public key (for session key decryption)
     *
     * @param tx_public_key 32-byte public key
     */
    void set_tx_public_key(const uint8_t* tx_public_key);

    /**
     * @brief Process a received WiFi packet
     *
     * Call this from WiFi promiscuous mode callback.
     *
     * @param data Full IEEE 802.11 frame (including header)
     * @param len Frame length
     * @param rssi Signal strength (dBm)
     * @param channel WiFi channel
     * @return true if packet was processed
     */
    bool process_packet(const uint8_t* data, size_t len, int8_t rssi = 0, uint8_t channel = 0);

    /**
     * @brief Periodic processing (expire old blocks, etc.)
     *
     * Call this regularly (e.g., every 10ms)
     */
    void process();

    /**
     * @brief Set callback for decoded data
     */
    void set_data_callback(WfbDataCallback callback);

    /**
     * @brief Set callback for session updates
     */
    void set_session_callback(WfbSessionCallback callback);

    /**
     * @brief Get current statistics
     */
    const WfbRxStats& get_stats() const { return m_stats; }

    /**
     * @brief Reset statistics
     */
    void reset_stats() { m_stats.reset(); }

    /**
     * @brief Check if session is valid (have session key)
     */
    bool has_session() const { return m_crypto.session_valid; }

    /**
     * @brief Get current FEC parameters
     */
    void get_fec_params(uint8_t& k, uint8_t& n) const {
        k = m_crypto.fec_k;
        n = m_crypto.fec_n;
    }

private:
    // IEEE 802.11 header parsing
    bool parse_ieee_header(const uint8_t* data, size_t len,
                           const uint8_t** payload, size_t* payload_len,
                           uint8_t* src_mac);

    // Packet processing
    bool process_session_packet(const uint8_t* payload, size_t len);
    bool process_data_packet(const uint8_t* payload, size_t len);

    // FEC block management
    WfbFecBlock* find_or_create_block(uint64_t block_index);
    void release_block(WfbFecBlock* block);
    bool decode_block(WfbFecBlock* block);
    void output_block(WfbFecBlock* block);
    size_t expire_blocks(uint32_t current_time_ms);

    // Packet buffer pool
    uint8_t* alloc_buffer();
    void free_buffer(uint8_t* buf);

private:
    bool m_initialized;
    bool m_use_psram;

    // Crypto context
    wfb_crypto_ctx_t m_crypto;
    uint8_t m_tx_public_key[WFB_CRYPTO_BOX_PUBLICKEYBYTES];
    bool m_tx_key_set;

    // FEC blocks
    WfbFecBlock m_blocks[WFB_RX_MAX_BLOCKS];
    size_t m_active_blocks;
    uint64_t m_next_expected_block;

    // Packet buffer pool
    uint8_t* m_packet_pool[WFB_RX_PACKET_POOL_SIZE];
    size_t m_pool_free_list[WFB_RX_PACKET_POOL_SIZE];
    size_t m_pool_free_count;

    // Decode buffer
    uint8_t* m_decode_buffer;
    size_t m_decode_buffer_size;

    // FEC context (zfec)
    void* m_fec_ctx;

    // Callbacks
    WfbDataCallback m_data_callback;
    WfbSessionCallback m_session_callback;

    // Statistics
    WfbRxStats m_stats;

    // Timing
    uint32_t m_last_process_time_ms;
};

#pragma once

#include <cstdint>
#include <cstddef>
#include "decoder_stats.h"
#include "block_manager.h"
#include "frame_assembler.h"

/**
 * FEC Decoder for ESP32-S3 Ground Station
 *
 * Decodes FEC-encoded WiFi packets from the air unit.
 * Handles packet reception, FEC decoding, and frame assembly.
 *
 * Usage:
 *   FecDecoder decoder;
 *   decoder.init(6, 12, 1464);  // k=6, n=12, mtu=1464
 *   decoder.set_frame_callback(my_frame_handler);
 *
 *   // In WiFi RX callback or packet processing loop:
 *   decoder.process_packet(packet_data, packet_size);
 *
 *   // Periodically:
 *   decoder.process();  // Process ready blocks
 */

// Forward declaration for FEC context
struct fec_t;

/**
 * Decoder configuration
 */
struct FecDecoderConfig {
    uint8_t coding_k = 6;           // Number of primary packets per block
    uint8_t coding_n = 12;          // Total packets per block
    uint16_t mtu = 1464;            // Maximum packet payload size
    bool use_psram = true;          // Use PSRAM for buffers
    uint16_t device_id = 0;         // Filter by device ID (0 = accept all)
    uint32_t block_timeout_ms = 100; // Timeout for incomplete blocks
};

/**
 * Main FEC Decoder class
 */
class FecDecoder {
public:
    FecDecoder();
    ~FecDecoder();

    /**
     * Initialize the decoder
     *
     * @param config Decoder configuration
     * @return true if initialization succeeded
     */
    bool init(const FecDecoderConfig& config);

    /**
     * Initialize with default configuration
     *
     * @param coding_k Primary packets per block
     * @param coding_n Total packets per block
     * @param mtu Maximum packet payload size
     * @return true if initialization succeeded
     */
    bool init(uint8_t coding_k = 6, uint8_t coding_n = 12, uint16_t mtu = 1464);

    /**
     * Deinitialize and free resources
     */
    void deinit();

    /**
     * Check if decoder is initialized
     */
    bool is_initialized() const { return m_initialized; }

    /**
     * Process an incoming packet
     *
     * This is the main entry point for WiFi RX data.
     * The packet should include the Packet_Header but NOT the 802.11 header.
     *
     * @param data Packet data including Packet_Header
     * @param size Total packet size
     * @return true if packet was accepted
     */
    bool process_packet(const uint8_t* data, size_t size);

    /**
     * Process an incoming packet from raw 802.11 frame
     *
     * Strips the 802.11 header and processes the payload.
     *
     * @param frame Raw 802.11 frame data
     * @param frame_size Total frame size
     * @param ieee_header_size Size of 802.11 header (typically 24)
     * @return true if packet was accepted
     */
    bool process_raw_frame(const uint8_t* frame, size_t frame_size,
                          size_t ieee_header_size = 24);

    /**
     * Process ready blocks
     *
     * Should be called periodically to decode ready blocks
     * and expire timed-out blocks.
     */
    void process();

    /**
     * Set callback for decoded frames
     */
    void set_frame_callback(FrameDecodedCallback callback);

    /**
     * Set callback for telemetry/OSD data
     */
    void set_data_callback(DataCallback callback);

    /**
     * Set packet filter device ID
     *
     * @param device_id Device ID to filter (0 = accept all)
     */
    void set_device_filter(uint16_t device_id);

    /**
     * Get current statistics
     */
    const FecDecoderStats& get_stats() const { return m_stats; }

    /**
     * Reset statistics
     */
    void reset_stats() { m_stats.reset(); }

    /**
     * Get configuration
     */
    const FecDecoderConfig& get_config() const { return m_config; }

    /**
     * Print statistics summary
     */
    void print_stats() const;

private:
    // Decode a complete block
    bool decode_block(FecBlock* block);

    // Dispatch decoded packets to frame assembler
    void dispatch_block(FecBlock* block);

    // Validate packet header
    bool validate_packet_header(const uint8_t* data, size_t size);

    bool m_initialized = false;
    FecDecoderConfig m_config;
    FecDecoderStats m_stats;

    // FEC context
    fec_t* m_fec = nullptr;

    // Sub-components
    BlockManager m_block_manager;
    FrameAssembler m_frame_assembler;

    // Decode buffers (in PSRAM)
    uint8_t** m_fec_src_ptrs = nullptr;
    uint8_t** m_fec_dst_ptrs = nullptr;
    uint8_t* m_decode_buffer = nullptr;

    // Time tracking
    uint32_t m_last_process_time_ms = 0;
};

/**
 * Global decoder instance (optional)
 */
extern FecDecoder g_fec_decoder;

#pragma once

#include <cstdint>

/**
 * FEC Decoder Statistics
 *
 * Tracks all decoding metrics for monitoring and debugging.
 * Target statistics for colorbars test capture:
 *   Packets processed: 11406
 *   Primary packets: 8542
 *   FEC packets: 2864
 *   FEC blocks complete: 3233
 *   FEC blocks recovered: 664
 *   FEC blocks failed: 23
 *   Frames decoded: 481
 *   Frames incomplete: 78
 */
struct FecDecoderStats {
    // Packet-level statistics
    uint32_t packets_processed = 0;     // Total packets received
    uint32_t primary_packets = 0;       // Primary data packets (index < k)
    uint32_t fec_packets = 0;           // FEC parity packets (index >= k)
    uint32_t duplicate_packets = 0;     // Duplicate packets dropped
    uint32_t invalid_packets = 0;       // Packets with invalid headers
    uint32_t filtered_packets = 0;      // Packets filtered by device ID

    // Block-level statistics
    uint32_t blocks_complete = 0;       // Blocks with all k primary packets
    uint32_t blocks_recovered = 0;      // Blocks recovered via FEC
    uint32_t blocks_failed = 0;         // Blocks that couldn't be recovered
    uint32_t blocks_skipped = 0;        // Old blocks skipped due to new arrivals

    // Frame-level statistics
    uint32_t frames_decoded = 0;        // Complete frames decoded
    uint32_t frames_incomplete = 0;     // Frames with missing parts

    // Data throughput
    uint64_t bytes_received = 0;        // Total bytes received
    uint64_t bytes_decoded = 0;         // Bytes successfully decoded

    // Signal quality
    int8_t rssi_dbm = 0;                // Latest RSSI
    int8_t noise_floor_dbm = 0;         // Latest noise floor

    // Timing
    uint32_t last_packet_time_ms = 0;   // Time of last packet
    uint32_t decode_time_us = 0;        // Average FEC decode time

    void reset() {
        packets_processed = 0;
        primary_packets = 0;
        fec_packets = 0;
        duplicate_packets = 0;
        invalid_packets = 0;
        filtered_packets = 0;
        blocks_complete = 0;
        blocks_recovered = 0;
        blocks_failed = 0;
        blocks_skipped = 0;
        frames_decoded = 0;
        frames_incomplete = 0;
        bytes_received = 0;
        bytes_decoded = 0;
        rssi_dbm = 0;
        noise_floor_dbm = 0;
        last_packet_time_ms = 0;
        decode_time_us = 0;
    }

    void print() const;
};

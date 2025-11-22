/**
 * @file test_fixtures.c
 * @brief Implementation of test fixtures and mock data
 */

#include "test_fixtures.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

// =============================================================================
// Test Configuration Data
// =============================================================================

/**
 * @brief Predefined test configurations
 */
const test_config_t test_configs[] = {
    // 720p configurations
    {1280, 720, 30, 2000000, 5000, 2.0f, 10.0f},   // 720p@30fps, 2Mbps
    {1280, 720, 60, 4000000, 8000, 2.0f, 10.0f},   // 720p@60fps, 4Mbps

    // 1080p configurations
    {1920, 1080, 30, 6000000, 15000, 2.0f, 10.0f}, // 1080p@30fps, 6Mbps
    {1920, 1080, 60, 12000000, 30000, 2.0f, 10.0f}, // 1080p@60fps, 12Mbps

    // 1440p configurations
    {2560, 1440, 30, 10000000, 25000, 2.0f, 10.0f}, // 1440p@30fps, 10Mbps
};

const int num_test_configs = sizeof(test_configs) / sizeof(test_configs[0]);

// =============================================================================
// Performance Baseline Data
// =============================================================================

/**
 * @brief Predefined performance baselines
 */
const performance_baseline_t performance_baselines[] = {
    // Latency tests
    {"Encoder Latency", "1080p@30fps", 50.0f, 0.0f, 0.0f, 0.0f, 20},
    {"Encoder Latency", "1080p@60fps", 100.0f, 0.0f, 0.0f, 0.0f, 20},
    {"IPC Latency", "1080p@30fps", 10.0f, 0.0f, 0.0f, 0.0f, 20},
    {"WiFi Latency", "1080p@30fps", 50.0f, 0.0f, 0.0f, 0.0f, 30},
    {"E2E Latency", "1080p@30fps", 150.0f, 0.0f, 0.0f, 0.0f, 30},

    // Throughput tests
    {"Throughput", "720p@30fps", 0.0f, 2.0f, 0.0f, 0.0f, 10},
    {"Throughput", "1080p@30fps", 0.0f, 6.0f, 0.0f, 0.0f, 10},
    {"Throughput", "1080p@60fps", 0.0f, 12.0f, 0.0f, 0.0f, 10},

    // CPU usage tests
    {"CPU Usage", "Encoder 1080p@60fps", 0.0f, 0.0f, 40.0f, 0.0f, 20},
    {"CPU Usage", "IPC Master 1080p@60fps", 0.0f, 0.0f, 10.0f, 0.0f, 20},
    {"CPU Usage", "WiFi TX 1080p@60fps", 0.0f, 0.0f, 20.0f, 0.0f, 20},

    // Memory tests
    {"Memory", "Encoder", 0.0f, 0.0f, 0.0f, 0.6f, 20},
    {"Memory", "IPC Buffers", 0.0f, 0.0f, 0.0f, 0.15f, 20},
    {"Memory", "WiFi TX", 0.0f, 0.0f, 0.0f, 0.3f, 20},
};

const int num_performance_baselines =
    sizeof(performance_baselines) / sizeof(performance_baselines[0]);

// =============================================================================
// Mock Frame Data Generation
// =============================================================================

/**
 * @brief Generate YUV422 test frame with pattern
 */
uint8_t *generate_test_frame_yuv422(
    uint16_t width,
    uint16_t height,
    uint32_t frame_number,
    uint8_t pattern_type)
{
    size_t size = width * height * 2;  // YUV422: 2 bytes per pixel
    uint8_t *data = (uint8_t *)malloc(size);

    if (!data) {
        return NULL;
    }

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x += 2) {
            size_t pos = (y * width + x) * 2;

            uint8_t y0, u, y1, v;

            switch (pattern_type) {
            case 0:  // Gradient pattern
                y0 = (x + frame_number) & 0xFF;
                y1 = (x + 1 + frame_number) & 0xFF;
                u = 128;
                v = 128;
                break;

            case 1:  // Checkerboard pattern
                y0 = ((x ^ y) & 1) ? 255 : 0;
                y1 = (((x + 1) ^ y) & 1) ? 255 : 0;
                u = 128;
                v = 128;
                break;

            case 2:  // Noise pattern (pseudo-random)
                y0 = (uint8_t)((frame_number * 73 + x * 97 + y * 109) & 0xFF);
                y1 = (uint8_t)((frame_number * 71 + (x + 1) * 101 + y * 107) & 0xFF);
                u = (uint8_t)((frame_number * 61 + x) & 0xFF);
                v = (uint8_t)((frame_number * 67 + y) & 0xFF);
                break;

            default:  // Default: gray
                y0 = y1 = 128;
                u = v = 128;
                break;
            }

            // YUV422 format: Y0 U Y1 V
            data[pos] = y0;
            data[pos + 1] = u;
            data[pos + 2] = y1;
            data[pos + 3] = v;
        }
    }

    return data;
}

/**
 * @brief Generate YUV420 test frame
 */
uint8_t *generate_test_frame_yuv420(
    uint16_t width,
    uint16_t height,
    uint32_t frame_number,
    uint8_t pattern_type)
{
    // YUV420: Y plane + U/V planes (1/4 size each)
    size_t size = width * height + (width * height) / 2;
    uint8_t *data = (uint8_t *)malloc(size);

    if (!data) {
        return NULL;
    }

    // Generate Y plane
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint8_t value;

            switch (pattern_type) {
            case 0:  // Gradient
                value = (x + frame_number) & 0xFF;
                break;
            case 1:  // Checkerboard
                value = ((x ^ y) & 1) ? 255 : 0;
                break;
            case 2:  // Noise
                value = (uint8_t)((frame_number * 73 + x * 97 + y * 109) & 0xFF);
                break;
            default:
                value = 128;
                break;
            }

            data[y * width + x] = value;
        }
    }

    // Generate U/V planes (downsampled 2x2)
    uint8_t *u_plane = &data[width * height];
    uint8_t *v_plane = &data[width * height + (width * height) / 4];

    for (uint32_t y = 0; y < height / 2; y++) {
        for (uint32_t x = 0; x < width / 2; x++) {
            u_plane[y * (width / 2) + x] = 128;
            v_plane[y * (width / 2) + x] = 128;
        }
    }

    return data;
}

// =============================================================================
// Mock H.264 NAL Unit Generation
// =============================================================================

/**
 * @brief Generate SPS NAL unit
 */
uint8_t *generate_mock_sps_nalu(uint16_t width, uint16_t height, uint8_t fps)
{
    // Simple SPS structure (not a complete valid H.264 SPS)
    // In production, this would use proper H.264 bitstream syntax
    uint8_t *data = (uint8_t *)malloc(64);
    if (!data) return NULL;

    // NAL header
    data[0] = 0x67;  // NAL type 7 (SPS)

    // Simplified SPS data with resolution encoded
    data[1] = (width >> 8) & 0xFF;
    data[2] = width & 0xFF;
    data[3] = (height >> 8) & 0xFF;
    data[4] = height & 0xFF;
    data[5] = fps;

    // Fill rest with zero
    memset(&data[6], 0, 58);

    return data;
}

/**
 * @brief Generate PPS NAL unit
 */
uint8_t *generate_mock_pps_nalu(void)
{
    uint8_t *data = (uint8_t *)malloc(32);
    if (!data) return NULL;

    // NAL header
    data[0] = 0x68;  // NAL type 8 (PPS)

    // Fill rest with zeros
    memset(&data[1], 0, 31);

    return data;
}

/**
 * @brief Generate I-frame NAL unit
 */
uint8_t *generate_mock_iframe_nalu(
    uint16_t width,
    uint16_t height,
    uint32_t frame_index,
    uint64_t pts_us)
{
    size_t estimated_size = estimate_nalu_size(width, height, 30, 6000000);
    uint8_t *data = (uint8_t *)malloc(estimated_size);
    if (!data) return NULL;

    // NAL header (IDR slice)
    data[0] = 0x65;  // NAL type 5 (IDR slice)

    // Encode frame metadata
    memcpy(&data[1], &frame_index, sizeof(frame_index));
    memcpy(&data[5], &pts_us, sizeof(pts_us));

    // Fill with pseudo-random data
    for (size_t i = 13; i < estimated_size; i++) {
        data[i] = (uint8_t)((frame_index * 73 + i * 97) & 0xFF);
    }

    return data;
}

/**
 * @brief Generate P-frame NAL unit
 */
uint8_t *generate_mock_pframe_nalu(
    uint16_t width,
    uint16_t height,
    uint32_t frame_index,
    uint64_t pts_us)
{
    // P-frames typically smaller than I-frames
    size_t estimated_size = estimate_nalu_size(width, height, 30, 6000000) / 3;
    uint8_t *data = (uint8_t *)malloc(estimated_size);
    if (!data) return NULL;

    // NAL header (non-IDR slice)
    data[0] = 0x61;  // NAL type 1 (P slice)

    // Encode frame metadata
    memcpy(&data[1], &frame_index, sizeof(frame_index));
    memcpy(&data[5], &pts_us, sizeof(pts_us));

    // Fill with pseudo-random data
    for (size_t i = 13; i < estimated_size; i++) {
        data[i] = (uint8_t)((frame_index * 79 + i * 103) & 0xFF);
    }

    return data;
}

// =============================================================================
// Mock IPC Packet Generation
// =============================================================================

/**
 * @brief Generate IPC video packet
 */
mock_ipc_video_packet_t *generate_mock_ipc_video_packet(
    uint16_t sequence,
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint32_t frame_index,
    uint64_t pts_us)
{
    mock_ipc_video_packet_t *packet =
        (mock_ipc_video_packet_t *)malloc(sizeof(*packet) + nalu_size);

    if (!packet) return NULL;

    packet->sequence = sequence;
    packet->nalu_type = nalu_data[0] & 0x1F;  // Extract NAL type
    packet->is_keyframe = (nalu_data[0] & 0x1F) == 5;  // Type 5 = IDR
    packet->frame_index = frame_index;
    packet->pts_us = pts_us;
    packet->payload = (uint8_t *)(packet + 1);
    packet->payload_size = nalu_size;

    // Copy NAL data
    memcpy(packet->payload, nalu_data, nalu_size);

    // Calculate CRC
    packet->crc16 = calculate_crc16(
        (const uint8_t *)packet,
        sizeof(*packet) + nalu_size,
        0xFFFF);

    return packet;
}

/**
 * @brief Generate IPC config packet
 */
mock_ipc_config_packet_t *generate_mock_ipc_config_packet(
    uint8_t channel,
    int8_t tx_power,
    uint8_t fec_k,
    uint8_t fec_n,
    uint32_t bitrate_bps)
{
    mock_ipc_config_packet_t *packet =
        (mock_ipc_config_packet_t *)malloc(sizeof(*packet));

    if (!packet) return NULL;

    packet->wifi_channel = channel;
    packet->wifi_tx_power_dbm = tx_power;
    packet->fec_k = fec_k;
    packet->fec_n = fec_n;
    packet->bitrate_bps = bitrate_bps;
    memset(packet->reserved, 0, sizeof(packet->reserved));

    return packet;
}

// =============================================================================
// Mock WiFi Packet Generation
// =============================================================================

/**
 * @brief Generate WiFi RTP packet
 */
mock_wifi_rtp_packet_t *generate_mock_wifi_rtp_packet(
    uint32_t ssrc,
    uint16_t sequence,
    uint32_t timestamp,
    const uint8_t *payload_data,
    size_t payload_size,
    bool marker)
{
    mock_wifi_rtp_packet_t *packet =
        (mock_wifi_rtp_packet_t *)malloc(sizeof(*packet) + payload_size);

    if (!packet) return NULL;

    packet->ssrc = ssrc;
    packet->sequence = sequence;
    packet->timestamp = timestamp;
    packet->payload = (uint8_t *)(packet + 1);
    packet->payload_size = payload_size;
    packet->marker = marker;

    // Copy payload
    if (payload_data) {
        memcpy(packet->payload, payload_data, payload_size);
    }

    return packet;
}

// =============================================================================
// Mock FEC Data Generation
// =============================================================================

/**
 * @brief Generate FEC block with parity packets
 */
mock_fec_block_t *generate_mock_fec_block(
    uint8_t k,
    uint8_t n,
    size_t packet_size)
{
    if (k >= n) return NULL;

    mock_fec_block_t *block = (mock_fec_block_t *)malloc(sizeof(*block));
    if (!block) return NULL;

    block->k = k;
    block->n = n;

    // Allocate packet pointers and sizes
    block->data_packets = (uint8_t **)malloc(k * sizeof(uint8_t *));
    block->parity_packets = (uint8_t **)malloc((n - k) * sizeof(uint8_t *));
    block->packet_sizes = (size_t *)malloc(n * sizeof(size_t));

    if (!block->data_packets || !block->parity_packets || !block->packet_sizes) {
        free_mock_fec_block(block);
        return NULL;
    }

    // Generate data packets with random content
    for (int i = 0; i < k; i++) {
        block->data_packets[i] = (uint8_t *)malloc(packet_size);
        if (!block->data_packets[i]) {
            free_mock_fec_block(block);
            return NULL;
        }

        block->packet_sizes[i] = packet_size;

        // Fill with pseudo-random data
        for (size_t j = 0; j < packet_size; j++) {
            block->data_packets[i][j] = (uint8_t)((i * 73 + j * 97) & 0xFF);
        }
    }

    // Generate parity packets (simple XOR for demo)
    for (int i = 0; i < (n - k); i++) {
        block->parity_packets[i] = (uint8_t *)malloc(packet_size);
        if (!block->parity_packets[i]) {
            free_mock_fec_block(block);
            return NULL;
        }

        block->packet_sizes[k + i] = packet_size;

        // Simple XOR of all data packets (not real FEC, just for structure)
        memset(block->parity_packets[i], 0, packet_size);
        for (int j = 0; j < k; j++) {
            for (size_t l = 0; l < packet_size; l++) {
                block->parity_packets[i][l] ^= block->data_packets[j][l];
            }
        }
    }

    return block;
}

/**
 * @brief Free FEC block
 */
void free_mock_fec_block(mock_fec_block_t *block)
{
    if (!block) return;

    if (block->data_packets) {
        for (int i = 0; i < block->k; i++) {
            if (block->data_packets[i]) free(block->data_packets[i]);
        }
        free(block->data_packets);
    }

    if (block->parity_packets) {
        for (int i = 0; i < (block->n - block->k); i++) {
            if (block->parity_packets[i]) free(block->parity_packets[i]);
        }
        free(block->parity_packets);
    }

    if (block->packet_sizes) free(block->packet_sizes);

    free(block);
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Allocate test frame buffer
 */
uint8_t *allocate_test_frame_buffer(uint16_t width, uint16_t height)
{
    // Assume YUV422: 2 bytes per pixel
    size_t size = width * height * 2;
    return (uint8_t *)malloc(size);
}

/**
 * @brief Estimate NAL unit size
 */
size_t estimate_nalu_size(
    uint16_t width,
    uint16_t height,
    uint8_t fps,
    uint32_t bitrate_bps)
{
    // Estimate: bitrate / fps / 8 (bits to bytes)
    // With some overhead for NAL headers
    uint32_t pixels = (uint32_t)width * height;
    uint32_t bytes_per_frame = bitrate_bps / (fps * 8);

    // NAL units are typically 1-5 per frame, with varying sizes
    // Average to 2000 bytes minimum
    return (bytes_per_frame > 2000) ? bytes_per_frame : 2000;
}

/**
 * @brief Calculate CRC16
 */
uint16_t calculate_crc16(const uint8_t *data, size_t size, uint16_t initial_crc)
{
    uint16_t crc = initial_crc;

    for (size_t i = 0; i < size; i++) {
        crc ^= (uint16_t)data[i] << 8;

        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = crc << 1;
            }
            crc &= 0xFFFF;
        }
    }

    return crc;
}

/**
 * @brief Simulate packet loss
 */
bool simulate_packet_loss(float loss_rate)
{
    if (loss_rate <= 0.0f) return false;
    if (loss_rate >= 1.0f) return true;

    // Use pseudo-random based on time
    uint32_t random = (uint32_t)((uint64_t)time(NULL) * 73);
    return (random % 100) < (uint32_t)(loss_rate * 100.0f);
}

/**
 * @brief Simulate latency
 */
uint32_t simulate_latency(uint32_t min_latency_ms, uint32_t max_latency_ms)
{
    if (min_latency_ms >= max_latency_ms) {
        return min_latency_ms * 1000;
    }

    uint32_t range = max_latency_ms - min_latency_ms;
    uint32_t random = (uint32_t)((uint64_t)time(NULL) * 97) % range;

    return (min_latency_ms + random) * 1000;  // Convert to microseconds
}

/**
 * @brief Simulate WiFi RSSI
 */
int8_t simulate_wifi_rssi(float distance_m, int8_t noise_dbm)
{
    // Simple path loss model: RSSI = -40 dBm @ 1m + 20*log10(distance)
    // Typical range: -30 dBm (very close) to -90 dBm (far)

    float path_loss = -40.0f - (20.0f * logf(distance_m > 0.1f ? distance_m : 0.1f));
    int8_t rssi = (int8_t)path_loss;

    // Add noise
    rssi += noise_dbm;

    // Clamp to reasonable range
    if (rssi > -30) rssi = -30;
    if (rssi < -90) rssi = -90;

    return rssi;
}

/**
 * @brief Simulate data corruption
 */
uint32_t simulate_data_corruption(uint8_t *data, size_t size, float error_rate)
{
    if (!data || error_rate <= 0.0f || error_rate >= 1.0f) {
        return 0;
    }

    uint32_t bits_corrupted = 0;

    for (size_t i = 0; i < size; i++) {
        for (int bit = 0; bit < 8; bit++) {
            uint32_t random = (uint32_t)((uint64_t)time(NULL) * 101 + i * 103 + bit);

            if ((random % 100) < (uint32_t)(error_rate * 100.0f)) {
                data[i] ^= (1 << bit);
                bits_corrupted++;
            }
        }
    }

    return bits_corrupted;
}

/**
 * @file test_fixtures.h
 * @brief Test fixtures and mock data for video pipeline integration tests
 *
 * Provides pre-generated test data and fixtures for testing various components
 * of the video pipeline without requiring actual hardware.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Mock Camera Data
// =============================================================================

/**
 * @brief Mock MIPI camera frame data
 *
 * Provides synthetic YUV422 frame data for testing encoder input
 */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t *data;
    size_t size;
    uint64_t timestamp_us;
    uint32_t frame_number;
} mock_camera_frame_t;

/**
 * @brief Generate synthetic YUV422 frame
 *
 * Creates a test pattern frame with optional animation
 *
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @param frame_number Frame sequence number (for animation)
 * @param pattern_type Pattern type (0=gradient, 1=checkerboard, 2=noise)
 * @return Allocated frame data (must be freed by caller)
 */
uint8_t *generate_test_frame_yuv422(
    uint16_t width,
    uint16_t height,
    uint32_t frame_number,
    uint8_t pattern_type);

/**
 * @brief Generate synthetic YUV420 frame
 *
 * Creates a test pattern frame for alternative format testing
 *
 * @param width Frame width in pixels
 * @param height Frame height in pixels
 * @param frame_number Frame sequence number
 * @param pattern_type Pattern type
 * @return Allocated frame data (must be freed by caller)
 */
uint8_t *generate_test_frame_yuv420(
    uint16_t width,
    uint16_t height,
    uint32_t frame_number,
    uint8_t pattern_type);

// =============================================================================
// Mock H.264 NAL Units
// =============================================================================

/**
 * @brief Mock H.264 NAL unit data
 */
typedef struct {
    uint8_t type;           ///< H.264 NAL type
    bool is_keyframe;       ///< True if I-frame
    uint32_t frame_index;   ///< Frame number
    uint8_t *data;          ///< NAL unit data
    size_t size;            ///< Data size
    uint64_t pts_us;        ///< Presentation timestamp
    uint64_t dts_us;        ///< Decode timestamp
} mock_nalu_t;

/**
 * @brief Generate mock H.264 SPS NAL unit
 *
 * Creates a valid H.264 sequence parameter set for testing
 *
 * @param width Video width
 * @param height Video height
 * @param fps Frame rate
 * @return Allocated NAL unit data
 */
uint8_t *generate_mock_sps_nalu(uint16_t width, uint16_t height, uint8_t fps);

/**
 * @brief Generate mock H.264 PPS NAL unit
 *
 * Creates a valid H.264 picture parameter set
 *
 * @return Allocated NAL unit data
 */
uint8_t *generate_mock_pps_nalu(void);

/**
 * @brief Generate mock H.264 I-frame NAL unit
 *
 * Creates synthetic I-frame data
 *
 * @param width Video width
 * @param height Video height
 * @param frame_index Frame number
 * @param pts_us Presentation timestamp
 * @return Allocated NAL unit data
 */
uint8_t *generate_mock_iframe_nalu(
    uint16_t width,
    uint16_t height,
    uint32_t frame_index,
    uint64_t pts_us);

/**
 * @brief Generate mock H.264 P-frame NAL unit
 *
 * Creates synthetic P-frame data
 *
 * @param width Video width
 * @param height Video height
 * @param frame_index Frame number
 * @param pts_us Presentation timestamp
 * @return Allocated NAL unit data
 */
uint8_t *generate_mock_pframe_nalu(
    uint16_t width,
    uint16_t height,
    uint32_t frame_index,
    uint64_t pts_us);

// =============================================================================
// Mock IPC Packets
// =============================================================================

/**
 * @brief Mock IPC video packet
 */
typedef struct {
    uint16_t sequence;           ///< Sequence number
    uint8_t nalu_type;           ///< NAL type
    bool is_keyframe;            ///< Keyframe flag
    uint32_t frame_index;        ///< Frame index
    uint64_t pts_us;             ///< Timestamp
    uint8_t *payload;            ///< Video data
    size_t payload_size;         ///< Payload size
    uint16_t crc16;              ///< CRC16 checksum
} mock_ipc_video_packet_t;

/**
 * @brief Mock IPC configuration packet
 */
typedef struct {
    uint8_t wifi_channel;        ///< WiFi channel (1-13)
    int8_t wifi_tx_power_dbm;    ///< TX power in dBm
    uint8_t fec_k;               ///< FEC data packets
    uint8_t fec_n;               ///< FEC total packets
    uint32_t bitrate_bps;        ///< Target bitrate
    uint8_t reserved[16];        ///< Reserved for future use
} mock_ipc_config_packet_t;

/**
 * @brief Generate mock IPC video packet
 *
 * @param sequence Sequence number
 * @param nalu_data NAL unit data
 * @param nalu_size NAL unit size
 * @param frame_index Frame index
 * @param pts_us Timestamp
 * @return Allocated packet structure
 */
mock_ipc_video_packet_t *generate_mock_ipc_video_packet(
    uint16_t sequence,
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint32_t frame_index,
    uint64_t pts_us);

/**
 * @brief Generate mock IPC config packet
 *
 * @param channel WiFi channel
 * @param tx_power TX power in dBm
 * @param fec_k FEC data packets
 * @param fec_n FEC total packets
 * @param bitrate_bps Target bitrate
 * @return Allocated packet structure
 */
mock_ipc_config_packet_t *generate_mock_ipc_config_packet(
    uint8_t channel,
    int8_t tx_power,
    uint8_t fec_k,
    uint8_t fec_n,
    uint32_t bitrate_bps);

// =============================================================================
// Mock WiFi Packets
// =============================================================================

/**
 * @brief Mock WiFi RTP packet
 */
typedef struct {
    uint32_t ssrc;               ///< Synchronization source
    uint16_t sequence;           ///< RTP sequence number
    uint32_t timestamp;          ///< RTP timestamp
    uint8_t *payload;            ///< Payload data
    size_t payload_size;         ///< Payload size
    bool marker;                 ///< RTP marker bit
} mock_wifi_rtp_packet_t;

/**
 * @brief Generate mock WiFi RTP packet
 *
 * @param ssrc SSRC
 * @param sequence RTP sequence number
 * @param timestamp RTP timestamp
 * @param payload_data Payload data
 * @param payload_size Payload size
 * @param marker Marker bit
 * @return Allocated packet structure
 */
mock_wifi_rtp_packet_t *generate_mock_wifi_rtp_packet(
    uint32_t ssrc,
    uint16_t sequence,
    uint32_t timestamp,
    const uint8_t *payload_data,
    size_t payload_size,
    bool marker);

// =============================================================================
// Mock FEC Data
// =============================================================================

/**
 * @brief Mock FEC block
 */
typedef struct {
    uint8_t k;                   ///< Data packets
    uint8_t n;                   ///< Total packets
    uint8_t **data_packets;      ///< Data packet pointers
    size_t *packet_sizes;        ///< Packet sizes
    uint8_t **parity_packets;    ///< Parity packet pointers
} mock_fec_block_t;

/**
 * @brief Generate mock FEC block
 *
 * Creates a block of data packets with FEC parity packets
 *
 * @param k Number of data packets
 * @param n Total number of packets (k + parity)
 * @param packet_size Size of each packet
 * @return Allocated FEC block structure
 */
mock_fec_block_t *generate_mock_fec_block(
    uint8_t k,
    uint8_t n,
    size_t packet_size);

/**
 * @brief Free mock FEC block
 *
 * @param block FEC block to free
 */
void free_mock_fec_block(mock_fec_block_t *block);

// =============================================================================
// Reference Data and Baselines
// =============================================================================

/**
 * @brief Test configuration parameters
 */
typedef struct {
    uint16_t width;              ///< Resolution width
    uint16_t height;             ///< Resolution height
    uint8_t fps;                 ///< Frame rate
    uint32_t bitrate_bps;        ///< Target bitrate
    uint32_t expected_frame_size; ///< Expected encoded frame size
    float expected_fps_tolerance; ///< ±% tolerance for FPS
    float expected_bitrate_tolerance; ///< ±% tolerance for bitrate
} test_config_t;

/**
 * @brief Predefined test configurations
 */
extern const test_config_t test_configs[];
extern const int num_test_configs;

/**
 * @brief Expected performance baseline
 */
typedef struct {
    const char *test_name;
    const char *scenario;
    float expected_latency_ms;
    float expected_throughput_mbps;
    float expected_cpu_percent;
    float expected_memory_mb;
    int tolerance_percent;
} performance_baseline_t;

/**
 * @brief Predefined performance baselines
 */
extern const performance_baseline_t performance_baselines[];
extern const int num_performance_baselines;

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Allocate and initialize test frame buffer
 *
 * @param width Frame width
 * @param height Frame height
 * @return Allocated buffer (caller must free)
 */
uint8_t *allocate_test_frame_buffer(uint16_t width, uint16_t height);

/**
 * @brief Calculate expected NAL unit size
 *
 * Rough estimation of NAL unit size based on resolution/bitrate
 *
 * @param width Frame width
 * @param height Frame height
 * @param fps Frame rate
 * @param bitrate_bps Target bitrate
 * @return Expected average NAL unit size
 */
size_t estimate_nalu_size(
    uint16_t width,
    uint16_t height,
    uint8_t fps,
    uint32_t bitrate_bps);

/**
 * @brief Calculate CRC16 checksum
 *
 * CRC16-CCITT (0xFFFF initial value)
 *
 * @param data Data buffer
 * @param size Data size
 * @param initial_crc Initial CRC value
 * @return Calculated CRC16
 */
uint16_t calculate_crc16(const uint8_t *data, size_t size, uint16_t initial_crc);

/**
 * @brief Inject simulated packet loss
 *
 * Randomly drops packets based on loss rate
 *
 * @param loss_rate Loss rate (0.0 = no loss, 1.0 = 100% loss)
 * @return true if packet should be dropped
 */
bool simulate_packet_loss(float loss_rate);

/**
 * @brief Inject simulated latency
 *
 * Adds random delay to simulate network/processing latency
 *
 * @param min_latency_ms Minimum latency
 * @param max_latency_ms Maximum latency
 * @return Latency to apply in microseconds
 */
uint32_t simulate_latency(uint32_t min_latency_ms, uint32_t max_latency_ms);

/**
 * @brief Simulate WiFi RSSI value
 *
 * Generates realistic RSSI values with optional degradation
 *
 * @param distance_m Distance in meters (for signal prediction)
 * @param noise_dbm Random noise to add
 * @return RSSI value in dBm
 */
int8_t simulate_wifi_rssi(float distance_m, int8_t noise_dbm);

/**
 * @brief Corrupt data with errors
 *
 * Simulates transmission errors
 *
 * @param data Data buffer
 * @param size Data size
 * @param error_rate Error rate (0.0 = no errors)
 * @return Number of bits corrupted
 */
uint32_t simulate_data_corruption(uint8_t *data, size_t size, float error_rate);

#ifdef __cplusplus
}
#endif

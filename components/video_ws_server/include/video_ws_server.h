/**
 * @file video_ws_server.h
 * @brief WebSocket server for streaming video to browser clients
 *
 * This component provides a WebSocket server that streams raw H.264/H.265
 * NAL units to connected browser clients. The browser performs video
 * decoding using the WebCodecs API, offloading decode from ESP32-S3.
 *
 * Protocol:
 * - Binary WebSocket messages contain raw video data
 * - First 4 bytes: message type and metadata
 * - Remaining bytes: payload (NAL units, etc.)
 *
 * Message Types:
 * - 0x01: Video frame data (H.264/H.265 NAL unit)
 * - 0x02: Codec configuration (SPS/PPS for H.264, VPS/SPS/PPS for H.265)
 * - 0x03: Statistics update
 * - 0x04: Stream start
 * - 0x05: Stream stop
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// WebSocket message types
#define VIDEO_WS_MSG_FRAME          0x01
#define VIDEO_WS_MSG_CODEC_CONFIG   0x02
#define VIDEO_WS_MSG_STATS          0x03
#define VIDEO_WS_MSG_STREAM_START   0x04
#define VIDEO_WS_MSG_STREAM_STOP    0x05

// Video codec types
#define VIDEO_CODEC_H264            0x01
#define VIDEO_CODEC_H265            0x02
#define VIDEO_CODEC_MJPEG           0x03

// H.264 NAL unit types (5 bits)
#define H264_NAL_SLICE              1
#define H264_NAL_DPA                2
#define H264_NAL_DPB                3
#define H264_NAL_DPC                4
#define H264_NAL_IDR                5
#define H264_NAL_SEI                6
#define H264_NAL_SPS                7
#define H264_NAL_PPS                8
#define H264_NAL_AUD                9

// H.265 NAL unit types (6 bits)
#define H265_NAL_TRAIL_N            0
#define H265_NAL_TRAIL_R            1
#define H265_NAL_IDR_W_RADL         19
#define H265_NAL_IDR_N_LP           20
#define H265_NAL_CRA_NUT            21
#define H265_NAL_VPS                32
#define H265_NAL_SPS                33
#define H265_NAL_PPS                34
#define H265_NAL_AUD                35
#define H265_NAL_SEI_PREFIX         39
#define H265_NAL_SEI_SUFFIX         40

/**
 * @brief WebSocket video frame header
 *
 * Sent at the start of each VIDEO_WS_MSG_FRAME message
 */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // VIDEO_WS_MSG_*
    uint8_t  codec;         // VIDEO_CODEC_*
    uint8_t  flags;         // Bit 0: keyframe, Bit 1: config present
    uint8_t  reserved;
    uint32_t timestamp;     // Presentation timestamp (90kHz)
    uint32_t frame_index;   // Frame sequence number
    uint16_t data_len;      // Length of following data
} video_ws_frame_hdr_t;

#define VIDEO_WS_FLAG_KEYFRAME      0x01
#define VIDEO_WS_FLAG_CONFIG        0x02

/**
 * @brief Codec configuration header
 *
 * Sent with VIDEO_WS_MSG_CODEC_CONFIG messages
 */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // VIDEO_WS_MSG_CODEC_CONFIG
    uint8_t  codec;         // VIDEO_CODEC_*
    uint16_t width;         // Video width in pixels
    uint16_t height;        // Video height in pixels
    uint8_t  fps;           // Frames per second
    uint8_t  profile;       // Codec profile
    uint16_t sps_len;       // SPS length (0 if not present)
    uint16_t pps_len;       // PPS length (0 if not present)
    uint16_t vps_len;       // VPS length (H.265 only, 0 for H.264)
    // Followed by: VPS data (if vps_len > 0), SPS data, PPS data
} video_ws_codec_config_t;

/**
 * @brief Stream statistics
 */
typedef struct __attribute__((packed)) {
    uint8_t  msg_type;      // VIDEO_WS_MSG_STATS
    uint8_t  reserved[3];
    uint32_t frames_received;
    uint32_t frames_decoded;
    uint32_t fec_recovered;
    uint32_t packets_lost;
    uint16_t bitrate_kbps;  // Current bitrate in kbps
    int8_t   rssi;          // WiFi RSSI
    uint8_t  snr;           // Signal to noise ratio
} video_ws_stats_t;

/**
 * @brief Server configuration
 */
typedef struct {
    uint16_t port;              // WebSocket server port (default: 8080)
    uint16_t max_clients;       // Maximum concurrent clients (default: 4)
    uint32_t send_buffer_size;  // Per-client send buffer (default: 64KB)
    bool     enable_cors;       // Enable CORS headers (default: true)
} video_ws_server_config_t;

/**
 * @brief Server statistics
 */
typedef struct {
    uint32_t clients_connected;
    uint32_t total_bytes_sent;
    uint32_t frames_sent;
    uint32_t frames_dropped;    // Dropped due to slow clients
    uint32_t send_errors;
} video_ws_server_stats_t;

/**
 * @brief Client connection callback
 *
 * @param client_id Unique client identifier
 * @param connected true if client connected, false if disconnected
 */
typedef void (*video_ws_client_callback_t)(int client_id, bool connected);

/**
 * @brief Initialize the video WebSocket server
 *
 * @param config Server configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int video_ws_server_init(const video_ws_server_config_t* config);

/**
 * @brief Start the WebSocket server
 *
 * @return 0 on success, negative error code on failure
 */
int video_ws_server_start(void);

/**
 * @brief Stop the WebSocket server
 */
void video_ws_server_stop(void);

/**
 * @brief Send video frame to all connected clients
 *
 * @param data Raw NAL unit data
 * @param len Data length
 * @param codec Codec type (VIDEO_CODEC_*)
 * @param timestamp Presentation timestamp
 * @param is_keyframe True if this is a keyframe (IDR for H.264)
 * @return Number of clients data was sent to, negative on error
 */
int video_ws_server_send_frame(const uint8_t* data, size_t len,
                               uint8_t codec, uint32_t timestamp,
                               bool is_keyframe);

/**
 * @brief Send codec configuration to all connected clients
 *
 * @param codec Codec type
 * @param width Video width
 * @param height Video height
 * @param fps Frames per second
 * @param sps SPS data (required for H.264/H.265)
 * @param sps_len SPS length
 * @param pps PPS data (required for H.264/H.265)
 * @param pps_len PPS length
 * @param vps VPS data (H.265 only, NULL for H.264)
 * @param vps_len VPS length
 * @return Number of clients sent to, negative on error
 */
int video_ws_server_send_config(uint8_t codec, uint16_t width, uint16_t height,
                                uint8_t fps,
                                const uint8_t* sps, uint16_t sps_len,
                                const uint8_t* pps, uint16_t pps_len,
                                const uint8_t* vps, uint16_t vps_len);

/**
 * @brief Send statistics update to all connected clients
 *
 * @param stats Statistics structure
 * @return Number of clients sent to, negative on error
 */
int video_ws_server_send_stats(const video_ws_stats_t* stats);

/**
 * @brief Get server statistics
 *
 * @param stats Output statistics structure
 */
void video_ws_server_get_stats(video_ws_server_stats_t* stats);

/**
 * @brief Set client connection callback
 *
 * @param callback Callback function
 */
void video_ws_server_set_client_callback(video_ws_client_callback_t callback);

/**
 * @brief Get number of connected clients
 *
 * @return Number of connected clients
 */
int video_ws_server_get_client_count(void);

/**
 * @brief Check if a NAL unit is a keyframe
 *
 * @param data NAL unit data (starting with NAL header)
 * @param len Data length
 * @param codec Codec type
 * @return true if keyframe, false otherwise
 */
bool video_ws_is_keyframe(const uint8_t* data, size_t len, uint8_t codec);

/**
 * @brief Check if a NAL unit is a parameter set (SPS/PPS/VPS)
 *
 * @param data NAL unit data
 * @param len Data length
 * @param codec Codec type
 * @return true if parameter set, false otherwise
 */
bool video_ws_is_parameter_set(const uint8_t* data, size_t len, uint8_t codec);

#ifdef __cplusplus
}
#endif

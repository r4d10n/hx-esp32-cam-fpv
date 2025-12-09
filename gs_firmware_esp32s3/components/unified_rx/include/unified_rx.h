/**
 * @file unified_rx.h
 * @brief Unified receiver for WFB-NG and ESP32-FPV protocols
 *
 * This component provides a single receiver that can detect and decode
 * both WFB-NG and ESP32-FPV protocol packets, supporting:
 * - WFB-NG: ChaCha20-Poly1305 encryption, session keys, H.264/H.265
 * - ESP32-FPV: Custom protocol with MJPEG, FEC 6/12
 *
 * Both protocols use Reed-Solomon FEC and WiFi monitor mode.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Detected protocol type
 */
typedef enum {
    PROTOCOL_UNKNOWN = 0,
    PROTOCOL_ESP32_FPV,     // Custom ESP32-FPV protocol (MJPEG)
    PROTOCOL_WFB_NG,        // WFB-NG (H.264/H.265)
    PROTOCOL_AUTO           // Auto-detect
} rx_protocol_t;

/**
 * @brief Video codec type
 */
typedef enum {
    CODEC_UNKNOWN = 0,
    CODEC_MJPEG,            // ESP32-FPV native
    CODEC_H264,             // WFB-NG / RTP
    CODEC_H265              // WFB-NG / RTP
} rx_codec_t;

/**
 * @brief Frame type
 */
typedef enum {
    FRAME_TYPE_VIDEO = 0,
    FRAME_TYPE_TELEMETRY,
    FRAME_TYPE_OSD,
    FRAME_TYPE_CONFIG
} rx_frame_type_t;

/**
 * @brief Video frame metadata
 */
typedef struct {
    rx_codec_t codec;
    uint32_t frame_index;
    uint32_t timestamp;     // 90kHz or frame time
    uint16_t width;
    uint16_t height;
    bool is_keyframe;
    uint8_t quality;        // MJPEG quality (0-63)
} rx_video_metadata_t;

/**
 * @brief Receiver statistics
 */
typedef struct {
    // Packets
    uint32_t packets_received;
    uint32_t packets_valid;
    uint32_t packets_fec_recovered;
    uint32_t packets_lost;
    uint32_t packets_dropped;

    // Frames
    uint32_t frames_complete;
    uint32_t frames_incomplete;
    uint32_t keyframes;

    // Protocol
    rx_protocol_t active_protocol;
    rx_codec_t active_codec;

    // Signal
    int8_t rssi;
    int8_t noise_floor;
    uint8_t snr;

    // Performance
    uint32_t bitrate_kbps;
    float fps;
} rx_stats_t;

/**
 * @brief Callback for decoded video frames
 *
 * @param data Frame data (MJPEG or NAL unit)
 * @param len Data length
 * @param metadata Frame metadata
 * @param user_data User context
 */
typedef void (*rx_video_callback_t)(const uint8_t* data, size_t len,
                                     const rx_video_metadata_t* metadata,
                                     void* user_data);

/**
 * @brief Callback for telemetry data
 *
 * @param data Telemetry data (MAVLink/MSP)
 * @param len Data length
 * @param user_data User context
 */
typedef void (*rx_telemetry_callback_t)(const uint8_t* data, size_t len,
                                         void* user_data);

/**
 * @brief Callback for OSD data
 *
 * @param data OSD buffer
 * @param len Data length
 * @param user_data User context
 */
typedef void (*rx_osd_callback_t)(const uint8_t* data, size_t len,
                                   void* user_data);

/**
 * @brief Receiver configuration
 */
typedef struct {
    rx_protocol_t protocol;     // PROTOCOL_AUTO for auto-detect
    uint8_t wifi_channel;       // WiFi channel (1-14, 36-165)

    // ESP32-FPV settings
    uint16_t device_id;         // Device ID filter (0 = accept all)

    // WFB-NG settings
    const char* gs_key_path;    // Path to gs.key (NULL = no encryption)
    uint8_t wfb_channel_id;     // WFB-NG channel ID

    // FEC settings
    uint8_t fec_k;              // Primary packets (default: 6)
    uint8_t fec_n;              // Total packets (default: 12)
    uint16_t mtu;               // MTU (default: 1464)

    // Buffer settings
    uint32_t frame_buffer_size; // Frame buffer size (default: 256KB)
    uint16_t max_concurrent_blocks; // Max FEC blocks (default: 16)
} rx_config_t;

/**
 * @brief Initialize the unified receiver
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int unified_rx_init(const rx_config_t* config);

/**
 * @brief Start receiving
 *
 * @return 0 on success, negative error code on failure
 */
int unified_rx_start(void);

/**
 * @brief Stop receiving
 */
void unified_rx_stop(void);

/**
 * @brief Process a raw WiFi packet from monitor mode
 *
 * @param data Raw 802.11 packet
 * @param len Packet length
 * @param rssi RSSI value
 * @param noise Noise floor
 */
void unified_rx_process_packet(const uint8_t* data, size_t len,
                                int8_t rssi, int8_t noise);

/**
 * @brief Set video frame callback
 *
 * @param callback Callback function
 * @param user_data User context
 */
void unified_rx_set_video_callback(rx_video_callback_t callback, void* user_data);

/**
 * @brief Set telemetry callback
 *
 * @param callback Callback function
 * @param user_data User context
 */
void unified_rx_set_telemetry_callback(rx_telemetry_callback_t callback, void* user_data);

/**
 * @brief Set OSD callback
 *
 * @param callback Callback function
 * @param user_data User context
 */
void unified_rx_set_osd_callback(rx_osd_callback_t callback, void* user_data);

/**
 * @brief Get receiver statistics
 *
 * @param stats Output statistics
 */
void unified_rx_get_stats(rx_stats_t* stats);

/**
 * @brief Set WiFi channel
 *
 * @param channel WiFi channel number
 * @return 0 on success, negative error code on failure
 */
int unified_rx_set_channel(uint8_t channel);

/**
 * @brief Force protocol (disable auto-detect)
 *
 * @param protocol Protocol to use
 */
void unified_rx_set_protocol(rx_protocol_t protocol);

/**
 * @brief Get current protocol
 *
 * @return Active protocol
 */
rx_protocol_t unified_rx_get_protocol(void);

#ifdef __cplusplus
}
#endif

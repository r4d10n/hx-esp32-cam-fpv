/**
 * @file frame_buffer.h
 * @brief Multi-codec frame buffer with proper jitter handling
 *
 * Provides frame buffering for MJPEG, H.264, and H.265 video streams
 * with support for:
 * - GOP-aligned playback for H.264/H.265
 * - Frame reordering (B-frame support)
 * - Jitter buffer with adaptive latency
 * - Reference frame tracking
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Codec types
 */
typedef enum {
    FB_CODEC_MJPEG = 0,
    FB_CODEC_H264,
    FB_CODEC_H265
} fb_codec_t;

/**
 * @brief Frame flags
 */
typedef enum {
    FB_FLAG_KEYFRAME    = 0x01,
    FB_FLAG_REFERENCE   = 0x02,
    FB_FLAG_END_OF_GOP  = 0x04,
    FB_FLAG_CONFIG      = 0x08   // Contains SPS/PPS/VPS
} fb_frame_flags_t;

/**
 * @brief Frame metadata
 */
typedef struct {
    fb_codec_t codec;
    uint32_t frame_index;
    uint32_t timestamp;         // Presentation timestamp
    uint32_t dts;               // Decode timestamp (for B-frames)
    uint16_t width;
    uint16_t height;
    uint8_t flags;              // fb_frame_flags_t
    uint8_t nal_type;           // NAL unit type (H.264/H.265)
} fb_frame_info_t;

/**
 * @brief Buffer configuration
 */
typedef struct {
    size_t max_frame_size;      // Max size per frame (default: 256KB)
    uint16_t max_frames;        // Max frames in buffer (default: 30)
    uint16_t target_latency_ms; // Target latency in ms (default: 100)
    uint16_t max_latency_ms;    // Max latency before drop (default: 500)
    bool reorder_frames;        // Enable B-frame reordering (default: true)
    bool use_psram;             // Use PSRAM if available (default: true)
} fb_config_t;

/**
 * @brief Buffer statistics
 */
typedef struct {
    uint32_t frames_received;
    uint32_t frames_output;
    uint32_t frames_dropped;
    uint32_t frames_buffered;
    uint32_t keyframes;
    uint32_t gops_complete;
    uint16_t current_latency_ms;
    uint16_t buffer_fullness_pct;
    float fps_in;
    float fps_out;
} fb_stats_t;

/**
 * @brief Frame output callback
 *
 * @param data Frame data
 * @param len Data length
 * @param info Frame metadata
 * @param user_data User context
 */
typedef void (*fb_output_callback_t)(const uint8_t* data, size_t len,
                                      const fb_frame_info_t* info,
                                      void* user_data);

/**
 * @brief Initialize frame buffer
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int frame_buffer_init(const fb_config_t* config);

/**
 * @brief Deinitialize frame buffer
 */
void frame_buffer_deinit(void);

/**
 * @brief Add frame to buffer
 *
 * @param data Frame data (NAL unit for H.264/H.265, complete JPEG for MJPEG)
 * @param len Data length
 * @param info Frame metadata
 * @return 0 on success, negative if buffer full
 */
int frame_buffer_push(const uint8_t* data, size_t len,
                       const fb_frame_info_t* info);

/**
 * @brief Set output callback
 *
 * Callback is invoked when frames are ready for output
 *
 * @param callback Output callback
 * @param user_data User context
 */
void frame_buffer_set_output_callback(fb_output_callback_t callback,
                                       void* user_data);

/**
 * @brief Process buffer and output ready frames
 *
 * Call this periodically to output buffered frames
 * at the correct presentation time.
 */
void frame_buffer_process(void);

/**
 * @brief Flush buffer
 *
 * Discards all buffered frames
 */
void frame_buffer_flush(void);

/**
 * @brief Get buffer statistics
 *
 * @param stats Output statistics
 */
void frame_buffer_get_stats(fb_stats_t* stats);

/**
 * @brief Set target latency
 *
 * @param latency_ms Target latency in milliseconds
 */
void frame_buffer_set_latency(uint16_t latency_ms);

/**
 * @brief Check if waiting for keyframe
 *
 * @return true if buffer is waiting for a keyframe to start playback
 */
bool frame_buffer_waiting_for_keyframe(void);

/**
 * @brief Store codec configuration (SPS/PPS/VPS)
 *
 * @param codec Codec type
 * @param sps SPS data (NULL if not present)
 * @param sps_len SPS length
 * @param pps PPS data (NULL if not present)
 * @param pps_len PPS length
 * @param vps VPS data (H.265 only, NULL otherwise)
 * @param vps_len VPS length
 */
void frame_buffer_set_codec_config(fb_codec_t codec,
                                    const uint8_t* sps, size_t sps_len,
                                    const uint8_t* pps, size_t pps_len,
                                    const uint8_t* vps, size_t vps_len);

/**
 * @brief Get codec configuration
 *
 * @param codec Output codec type
 * @param sps Output SPS data pointer
 * @param sps_len Output SPS length
 * @param pps Output PPS data pointer
 * @param pps_len Output PPS length
 * @param vps Output VPS data pointer (H.265 only)
 * @param vps_len Output VPS length
 * @return true if config is available
 */
bool frame_buffer_get_codec_config(fb_codec_t* codec,
                                    const uint8_t** sps, size_t* sps_len,
                                    const uint8_t** pps, size_t* pps_len,
                                    const uint8_t** vps, size_t* vps_len);

#ifdef __cplusplus
}
#endif

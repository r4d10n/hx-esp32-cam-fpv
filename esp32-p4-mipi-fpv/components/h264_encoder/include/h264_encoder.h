#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Hardware H.264 Encoder for ESP32-P4
 *
 * Utilizes ESP32-P4's built-in H.264 encoding engine for efficient
 * video compression with minimal CPU overhead.
 */

/**
 * @brief H.264 profile levels
 */
typedef enum {
    H264_PROFILE_BASELINE,              ///< Baseline profile (lowest complexity)
    H264_PROFILE_MAIN,                  ///< Main profile
    H264_PROFILE_HIGH,                  ///< High profile (best compression)
} h264_profile_t;

/**
 * @brief H.264 level constraints
 */
typedef enum {
    H264_LEVEL_3_0,                     ///< Level 3.0 (720p @ 30fps)
    H264_LEVEL_3_1,                     ///< Level 3.1 (720p @ 60fps)
    H264_LEVEL_4_0,                     ///< Level 4.0 (1080p @ 30fps)
    H264_LEVEL_4_1,                     ///< Level 4.1 (1080p @ 60fps)
} h264_level_t;

/**
 * @brief Rate control modes
 */
typedef enum {
    H264_RC_MODE_CBR,                   ///< Constant bitrate
    H264_RC_MODE_VBR,                   ///< Variable bitrate
    H264_RC_MODE_CQP,                   ///< Constant quantization parameter
} h264_rate_control_mode_t;

/**
 * @brief NAL unit types
 */
typedef enum {
    H264_NAL_TYPE_SLICE = 1,            ///< Non-IDR slice
    H264_NAL_TYPE_DPA = 2,
    H264_NAL_TYPE_DPB = 3,
    H264_NAL_TYPE_DPC = 4,
    H264_NAL_TYPE_IDR = 5,              ///< IDR (keyframe) slice
    H264_NAL_TYPE_SEI = 6,              ///< Supplemental enhancement information
    H264_NAL_TYPE_SPS = 7,              ///< Sequence parameter set
    H264_NAL_TYPE_PPS = 8,              ///< Picture parameter set
    H264_NAL_TYPE_AUD = 9,              ///< Access unit delimiter
    H264_NAL_TYPE_FILLER = 12,
} h264_nal_type_t;

/**
 * @brief Encoder configuration
 */
typedef struct {
    uint16_t width;                     ///< Frame width (must be multiple of 16)
    uint16_t height;                    ///< Frame height (must be multiple of 16)
    uint8_t fps;                        ///< Target frame rate

    // Rate control
    h264_rate_control_mode_t rc_mode;
    uint32_t bitrate_bps;               ///< Target bitrate (for CBR/VBR)
    uint32_t max_bitrate_bps;           ///< Max bitrate (for VBR)

    // Quality
    uint8_t qp_min;                     ///< Minimum QP (0-51, lower = better quality)
    uint8_t qp_max;                     ///< Maximum QP (0-51, higher = more compression)
    uint8_t qp_initial;                 ///< Initial QP

    // GOP (Group of Pictures) settings
    uint8_t gop_size;                   ///< I-frame interval (frames)
    bool enable_b_frames;               ///< Enable B-frames
    uint8_t b_frames_count;             ///< Number of B-frames between I/P frames

    // Profile and level
    h264_profile_t profile;
    h264_level_t level;

    // Advanced settings
    bool enable_cabac;                  ///< Enable CABAC entropy coding
    bool enable_deblock;                ///< Enable deblocking filter
    int8_t deblock_alpha;               ///< Deblock alpha offset (-6 to 6)
    int8_t deblock_beta;                ///< Deblock beta offset (-6 to 6)

    // Performance
    uint8_t thread_count;               ///< Encoding threads (1-4)
} h264_encoder_config_t;

/**
 * @brief NAL unit information
 */
typedef struct {
    h264_nal_type_t type;               ///< NAL unit type
    bool is_keyframe;                   ///< True if this is an I-frame
    uint32_t frame_index;               ///< Frame number
    uint64_t pts_us;                    ///< Presentation timestamp (microseconds)
    uint64_t dts_us;                    ///< Decode timestamp (microseconds)
    size_t size;                        ///< NAL unit size in bytes
} h264_nal_info_t;

/**
 * @brief NAL unit callback function
 *
 * Called when a NAL unit is encoded and ready for transmission.
 *
 * @param nalu_data Pointer to NAL unit data (valid only during callback)
 * @param nalu_info NAL unit metadata
 * @param user_data User data passed during callback registration
 */
typedef void (*h264_nalu_cb_t)(
    const uint8_t *nalu_data,
    const h264_nal_info_t *nalu_info,
    void *user_data
);

/**
 * @brief Initialize H.264 encoder
 *
 * @param config Encoder configuration
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_init(const h264_encoder_config_t *config);

/**
 * @brief Deinitialize encoder and free resources
 *
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_deinit(void);

/**
 * @brief Register NAL unit callback
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_register_nalu_callback(
    h264_nalu_cb_t callback,
    void *user_data
);

/**
 * @brief Encode a YUV frame
 *
 * @param yuv_data YUV422 or YUV420 frame data
 * @param yuv_size Size of YUV data
 * @param pts_us Presentation timestamp in microseconds
 * @param force_keyframe Force this frame to be a keyframe
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_encode_frame(
    const uint8_t *yuv_data,
    size_t yuv_size,
    uint64_t pts_us,
    bool force_keyframe
);

/**
 * @brief Request keyframe on next encode
 *
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_request_keyframe(void);

/**
 * @brief Update bitrate dynamically
 *
 * @param bitrate_bps New target bitrate
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_set_bitrate(uint32_t bitrate_bps);

/**
 * @brief Update QP range dynamically
 *
 * @param qp_min Minimum QP
 * @param qp_max Maximum QP
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_set_qp_range(uint8_t qp_min, uint8_t qp_max);

/**
 * @brief Get encoder statistics
 */
typedef struct {
    uint32_t frames_encoded;            ///< Total frames encoded
    uint32_t keyframes_encoded;         ///< Total keyframes encoded
    uint64_t total_bytes;               ///< Total encoded bytes
    float actual_fps;                   ///< Actual encoding FPS
    float actual_bitrate_bps;           ///< Actual bitrate
    uint8_t average_qp;                 ///< Average QP used
    uint32_t encode_time_avg_us;        ///< Average encode time per frame
    uint32_t encode_time_max_us;        ///< Maximum encode time
} h264_encoder_stats_t;

/**
 * @brief Get encoder statistics
 *
 * @param[out] stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_get_stats(h264_encoder_stats_t *stats);

/**
 * @brief Reset encoder statistics
 *
 * @return ESP_OK on success
 */
esp_err_t h264_encoder_reset_stats(void);

#ifdef __cplusplus
}
#endif

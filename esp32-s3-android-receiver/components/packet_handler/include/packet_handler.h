/**
 * @file packet_handler.h
 * @brief Packet Handler for Frame Assembly and Buffering
 *
 * This component manages packet reassembly, frame buffering, and ordering.
 * Features:
 * - WiFi packet parsing and validation
 * - Frame assembly from fragmented packets
 * - Jitter buffer management
 * - Frame reordering based on sequence numbers
 * - H.264 NAL unit extraction
 * - Timestamp management
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Packet type identifier
 */
typedef enum {
    PKT_TYPE_VIDEO = 0,        /**< Video data packet */
    PKT_TYPE_TELEMETRY = 1,    /**< Telemetry packet */
    PKT_TYPE_CONTROL = 2,      /**< Control packet */
    PKT_TYPE_SYNC = 3          /**< Synchronization packet */
} packet_type_t;

/**
 * @brief Video frame type
 */
typedef enum {
    FRAME_TYPE_I = 0,          /**< I-frame (keyframe) */
    FRAME_TYPE_P = 1,          /**< P-frame */
    FRAME_TYPE_B = 2,          /**< B-frame */
    FRAME_TYPE_UNKNOWN = 3     /**< Unknown frame type */
} frame_type_t;

/**
 * @brief Packet header structure
 */
typedef struct {
    packet_type_t type;        /**< Packet type */
    uint32_t sequence_num;     /**< Global sequence number */
    uint32_t frame_id;         /**< Frame identifier */
    uint16_t fragment_index;   /**< Fragment index within frame */
    uint16_t total_fragments;  /**< Total fragments in frame */
    uint64_t timestamp_us;     /**< Timestamp in microseconds */
    uint16_t payload_size;     /**< Payload size in bytes */
    uint8_t flags;             /**< Packet flags */
} packet_header_t;

/**
 * @brief Assembled frame information
 */
typedef struct {
    uint32_t frame_id;         /**< Frame identifier */
    frame_type_t frame_type;   /**< Frame type (I, P, B) */
    uint8_t *data;             /**< Frame data */
    size_t data_len;           /**< Frame data length */
    uint64_t pts_us;           /**< Presentation timestamp (microseconds) */
    uint64_t dts_us;           /**< Decode timestamp (microseconds) */
    uint64_t receive_time_us;  /**< Reception timestamp */
    uint8_t nal_type;          /**< H.264 NAL unit type */
    bool is_complete;          /**< True if all fragments received */
} assembled_frame_t;

/**
 * @brief Packet handler configuration
 */
typedef struct {
    size_t jitter_buffer_size; /**< Jitter buffer size in frames */
    uint32_t frame_timeout_ms; /**< Frame assembly timeout */
    size_t max_frame_size;     /**< Maximum frame size in bytes */
    bool enable_reordering;    /**< Enable packet reordering */
    bool enable_validation;    /**< Enable packet validation */
} packet_handler_config_t;

/**
 * @brief Packet handler statistics
 */
typedef struct {
    uint64_t packets_processed;    /**< Total packets processed */
    uint64_t packets_invalid;      /**< Invalid packets */
    uint64_t frames_assembled;     /**< Successfully assembled frames */
    uint64_t frames_incomplete;    /**< Incomplete frames (timeout) */
    uint64_t frames_dropped;       /**< Frames dropped */
    uint64_t packets_reordered;    /**< Packets that needed reordering */
    uint64_t bytes_processed;      /**< Total bytes processed */
    uint32_t jitter_buffer_usage;  /**< Jitter buffer usage percentage */
    uint32_t avg_fragments_per_frame; /**< Average fragments per frame */
    uint32_t avg_assembly_time_us; /**< Average frame assembly time */
} packet_handler_stats_t;

/**
 * @brief Frame ready callback
 *
 * Called when a complete frame has been assembled and is ready for processing.
 *
 * @param frame Assembled frame information
 * @param user_ctx User context pointer
 */
typedef void (*frame_ready_callback_t)(const assembled_frame_t *frame, void *user_ctx);

/**
 * @brief Packet handler handle
 */
typedef struct packet_handler_s packet_handler_t;

/**
 * @brief Create packet handler instance
 *
 * @param config Handler configuration
 * @param callback Frame ready callback
 * @param callback_ctx User context for callback
 * @return Handler handle or NULL on failure
 */
packet_handler_t* packet_handler_create(
    const packet_handler_config_t *config,
    frame_ready_callback_t callback,
    void *callback_ctx
);

/**
 * @brief Destroy packet handler instance
 *
 * @param handler Handler handle
 */
void packet_handler_destroy(packet_handler_t *handler);

/**
 * @brief Process received packet
 *
 * Parses packet header, validates, and adds to appropriate frame buffer.
 *
 * @param handler Handler handle
 * @param packet_data Raw packet data
 * @param packet_len Packet length
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if parameters are invalid
 *         ESP_ERR_INVALID_STATE if packet is invalid
 */
esp_err_t packet_handler_process(
    packet_handler_t *handler,
    const uint8_t *packet_data,
    size_t packet_len
);

/**
 * @brief Parse packet header
 *
 * Extracts header information from raw packet data.
 *
 * @param packet_data Raw packet data
 * @param packet_len Packet length
 * @param header Pointer to store parsed header
 * @return ESP_OK on success
 */
esp_err_t packet_handler_parse_header(
    const uint8_t *packet_data,
    size_t packet_len,
    packet_header_t *header
);

/**
 * @brief Flush pending frames
 *
 * Forces assembly of all pending frames, even if incomplete.
 *
 * @param handler Handler handle
 * @return Number of frames flushed
 */
size_t packet_handler_flush(packet_handler_t *handler);

/**
 * @brief Get handler statistics
 *
 * @param handler Handler handle
 * @param stats Pointer to statistics structure
 * @return ESP_OK on success
 */
esp_err_t packet_handler_get_stats(packet_handler_t *handler, packet_handler_stats_t *stats);

/**
 * @brief Reset handler statistics
 *
 * @param handler Handler handle
 * @return ESP_OK on success
 */
esp_err_t packet_handler_reset_stats(packet_handler_t *handler);

/**
 * @brief Get jitter buffer status
 *
 * @param handler Handler handle
 * @param pending_frames Pointer to store number of pending frames
 * @param buffer_usage Pointer to store buffer usage percentage
 * @return ESP_OK on success
 */
esp_err_t packet_handler_get_buffer_status(
    packet_handler_t *handler,
    size_t *pending_frames,
    uint32_t *buffer_usage
);

/**
 * @brief Set frame timeout
 *
 * Updates the timeout for frame assembly.
 *
 * @param handler Handler handle
 * @param timeout_ms Timeout in milliseconds
 * @return ESP_OK on success
 */
esp_err_t packet_handler_set_frame_timeout(packet_handler_t *handler, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

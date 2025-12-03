/**
 * ESP32 FPV Ground Station - Frame Buffer
 *
 * Ring buffer for assembled JPEG frames with support for
 * concurrent reading (WebSocket) and writing (packet RX).
 */

#ifndef FRAME_BUFFER_H
#define FRAME_BUFFER_H

#include "fpv_gs.h"

// Frame info structure
typedef struct {
    uint8_t *data;
    size_t size;
    uint32_t frame_index;
    int64_t timestamp;
    bool valid;
} frame_info_t;

/**
 * Initialize frame buffer subsystem
 * @return ESP_OK on success
 */
esp_err_t frame_buffer_init(void);

/**
 * Start a new frame
 * @param frame_index Frame index from video packet
 */
void frame_buffer_start_frame(uint32_t frame_index);

/**
 * Add a part to the current frame
 * @param part_index Part index (0-based)
 * @param data JPEG data for this part
 * @param len Data length
 * @return true if added successfully
 */
bool frame_buffer_add_part(uint8_t part_index, const uint8_t *data, size_t len);

/**
 * Commit current frame (mark as complete)
 * @return true if frame is valid and committed
 */
bool frame_buffer_commit_frame(void);

/**
 * Discard current incomplete frame
 */
void frame_buffer_discard_current(void);

/**
 * Get the latest complete frame for reading
 * @return Pointer to frame info, or NULL if none available
 */
frame_info_t *frame_buffer_get_latest(void);

/**
 * Release a frame after reading
 * @param frame Frame to release
 */
void frame_buffer_release(frame_info_t *frame);

/**
 * Check if a new frame is available since last get
 * @return true if new frame available
 */
bool frame_buffer_has_new_frame(void);

/**
 * Get frame buffer statistics
 * @param frames_buffered Output: number of buffered frames
 * @param memory_used Output: bytes used
 */
void frame_buffer_get_stats(uint8_t *frames_buffered, size_t *memory_used);

#endif // FRAME_BUFFER_H

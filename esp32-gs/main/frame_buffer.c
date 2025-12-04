/**
 * ESP32 FPV Ground Station - Frame Buffer
 */

#include "frame_buffer.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "frame_buf";

#define FRAME_BUFFER_COUNT CONFIG_FPV_GS_FRAME_BUFFER_COUNT
#define MAX_FRAME_SIZE_CONFIG CONFIG_FPV_GS_MAX_FRAME_SIZE
#define MIN_FRAME_SIZE 32768      // 32KB minimum
#define MAX_FRAME_SIZE_CAP 131072 // 128KB maximum per buffer
#define RESERVED_RAM 40960        // 40KB reserve for WiFi/TCP stack
#define MAX_PARTS 128  // Maximum parts per frame
#define FRAME_TIMEOUT_US (100 * 1000)  // 100ms timeout for incomplete frames

// Actual allocated buffer size (determined at runtime)
static size_t s_buffer_size = 0;

// Frame buffer entry
typedef struct {
    uint8_t *data;              // JPEG data buffer
    size_t size;                // Current data size
    size_t capacity;            // Buffer capacity
    uint32_t frame_index;       // Frame index
    int64_t start_timestamp;    // When frame assembly started
    int64_t timestamp;          // Completion timestamp
    uint8_t parts_mask[16];     // Bitmask for received parts (128 bits)
    uint8_t last_part_index;    // Index of last part (when known)
    bool in_progress;           // Currently being written
    bool complete;              // Frame is complete
    bool being_read;            // Currently being read by WebSocket
} frame_buffer_entry_t;

// Ring buffer state
static frame_buffer_entry_t s_frames[FRAME_BUFFER_COUNT];
static volatile uint8_t s_write_idx = 0;
static volatile uint8_t s_latest_complete_idx = 0xFF;
static volatile uint32_t s_last_served_frame = 0;
static SemaphoreHandle_t s_mutex = NULL;

// Part assembly buffer for current frame
static struct {
    uint8_t *data;
    size_t offsets[MAX_PARTS];  // Offset of each part in data buffer
    size_t sizes[MAX_PARTS];    // Size of each part
    uint8_t count;
    uint8_t last_part;
    bool has_last;
} s_assembly;

esp_err_t frame_buffer_init(void)
{
    // Create mutex
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Check available memory
    size_t free_psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t free_internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Free memory: PSRAM=%u, Internal=%u",
             (unsigned)free_psram, (unsigned)free_internal);

    bool using_psram = false;

    // Calculate optimal buffer size based on available RAM
    // Need: FRAME_BUFFER_COUNT buffers + 1 assembly buffer
    size_t total_buffers = FRAME_BUFFER_COUNT + 1;
    size_t available_for_buffers;

    if (free_psram >= total_buffers * MIN_FRAME_SIZE) {
        // Use PSRAM - can use larger buffers
        available_for_buffers = free_psram;
        using_psram = true;
    } else {
        // Use internal RAM - leave reserve for WiFi/TCP
        available_for_buffers = (free_internal > RESERVED_RAM) ?
                                (free_internal - RESERVED_RAM) : 0;
    }

    // Calculate per-buffer size
    s_buffer_size = available_for_buffers / total_buffers;

    // Clamp to reasonable bounds
    if (s_buffer_size < MIN_FRAME_SIZE) {
        s_buffer_size = MIN_FRAME_SIZE;
        ESP_LOGW(TAG, "Low memory, using minimum buffer size %u", (unsigned)s_buffer_size);
    }
    if (s_buffer_size > MAX_FRAME_SIZE_CAP) {
        s_buffer_size = MAX_FRAME_SIZE_CAP;
    }

    // Align to 4KB for efficient allocation
    s_buffer_size = (s_buffer_size / 4096) * 4096;
    if (s_buffer_size < MIN_FRAME_SIZE) {
        s_buffer_size = MIN_FRAME_SIZE;
    }

    ESP_LOGI(TAG, "Calculated buffer size: %u bytes (%u KB)",
             (unsigned)s_buffer_size, (unsigned)(s_buffer_size / 1024));

    // Allocate frame buffers
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (using_psram) {
            s_frames[i].data = heap_caps_malloc(s_buffer_size,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        }

        if (s_frames[i].data == NULL) {
            // Fall back to internal RAM
            s_frames[i].data = heap_caps_malloc(s_buffer_size,
                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
            using_psram = false;
        }

        if (s_frames[i].data == NULL) {
            ESP_LOGE(TAG, "Failed to allocate frame buffer %d", i);
            // Free previously allocated buffers
            for (int j = 0; j < i; j++) {
                free(s_frames[j].data);
                s_frames[j].data = NULL;
            }
            return ESP_ERR_NO_MEM;
        }

        s_frames[i].capacity = s_buffer_size;
        s_frames[i].size = 0;
        s_frames[i].frame_index = 0;
        s_frames[i].in_progress = false;
        s_frames[i].complete = false;
        s_frames[i].being_read = false;
        memset(s_frames[i].parts_mask, 0, sizeof(s_frames[i].parts_mask));
    }

    // Allocate assembly buffer (same size as frame buffers)
    if (using_psram) {
        s_assembly.data = heap_caps_malloc(s_buffer_size,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_assembly.data == NULL) {
        s_assembly.data = heap_caps_malloc(s_buffer_size,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (s_assembly.data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate assembly buffer");
        return ESP_ERR_NO_MEM;
    }

    // Log memory status after allocation
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Frame buffer initialized: %d buffers x %u KB (%s RAM)",
             FRAME_BUFFER_COUNT, (unsigned)(s_buffer_size / 1024),
             using_psram ? "PSRAM" : "internal");
    ESP_LOGI(TAG, "Total frame memory: %u KB, free heap: %u KB",
             (unsigned)(s_buffer_size * total_buffers / 1024),
             (unsigned)(free_heap / 1024));

    return ESP_OK;
}

void frame_buffer_start_frame(uint32_t frame_index)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // Find a free buffer slot (not being read)
    uint8_t idx = s_write_idx;
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (!s_frames[idx].being_read) {
            break;
        }
        idx = (idx + 1) % FRAME_BUFFER_COUNT;
    }

    // Reset the selected buffer
    frame_buffer_entry_t *f = &s_frames[idx];
    f->size = 0;
    f->frame_index = frame_index;
    f->start_timestamp = esp_timer_get_time();
    f->in_progress = true;
    f->complete = false;
    f->last_part_index = 0xFF;
    memset(f->parts_mask, 0, sizeof(f->parts_mask));

    // Reset assembly state
    s_assembly.count = 0;
    s_assembly.has_last = false;
    memset(s_assembly.offsets, 0, sizeof(s_assembly.offsets));
    memset(s_assembly.sizes, 0, sizeof(s_assembly.sizes));

    s_write_idx = idx;

    xSemaphoreGive(s_mutex);
}

bool frame_buffer_add_part(uint8_t part_index, const uint8_t *data, size_t len)
{
    if (part_index >= MAX_PARTS || data == NULL || len == 0) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    frame_buffer_entry_t *f = &s_frames[s_write_idx];

    if (!f->in_progress) {
        xSemaphoreGive(s_mutex);
        return false;
    }

    // Check for frame timeout - discard stale frames
    int64_t now = esp_timer_get_time();
    if (now - f->start_timestamp > FRAME_TIMEOUT_US) {
        // Frame took too long, discard it
        f->in_progress = false;
        f->complete = false;
        g_stats.frames_incomplete++;
        xSemaphoreGive(s_mutex);
        return false;
    }

    // Check if already received this part
    uint8_t byte_idx = part_index / 8;
    uint8_t bit_idx = part_index % 8;
    if (f->parts_mask[byte_idx] & (1 << bit_idx)) {
        // Already have this part
        xSemaphoreGive(s_mutex);
        return true;
    }

    // Store part data in assembly buffer
    size_t offset = 0;
    for (int i = 0; i < part_index; i++) {
        offset += s_assembly.sizes[i];
    }

    // Check if we have room
    size_t total_after = offset + len;
    for (int i = part_index + 1; i < MAX_PARTS; i++) {
        total_after += s_assembly.sizes[i];
    }

    if (total_after > s_buffer_size) {
        ESP_LOGW(TAG, "Frame too large (%u > %u), dropping part",
                 (unsigned)total_after, (unsigned)s_buffer_size);
        xSemaphoreGive(s_mutex);
        return false;
    }

    // Store part info
    s_assembly.offsets[part_index] = offset;
    s_assembly.sizes[part_index] = len;

    // Copy data (we'll reassemble in order when committing)
    // For now, store temporarily
    memcpy(s_assembly.data + offset, data, len);

    // Mark part as received
    f->parts_mask[byte_idx] |= (1 << bit_idx);
    s_assembly.count++;

    xSemaphoreGive(s_mutex);
    return true;
}

bool frame_buffer_commit_frame(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    frame_buffer_entry_t *f = &s_frames[s_write_idx];

    if (!f->in_progress) {
        xSemaphoreGive(s_mutex);
        return false;
    }

    // Assemble frame from parts in order
    bool valid = true;

    // Find highest part index we have
    int max_part = -1;
    for (int i = MAX_PARTS - 1; i >= 0; i--) {
        uint8_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        if (f->parts_mask[byte_idx] & (1 << bit_idx)) {
            max_part = i;
            break;
        }
    }

    if (max_part < 0) {
        valid = false;
    } else {
        // Check we have all parts 0 to max_part
        for (int i = 0; i <= max_part && valid; i++) {
            uint8_t byte_idx = i / 8;
            uint8_t bit_idx = i % 8;
            if (!(f->parts_mask[byte_idx] & (1 << bit_idx))) {
                valid = false;
                ESP_LOGD(TAG, "Missing part %d for frame %lu", i, (unsigned long)f->frame_index);
            }
        }
    }

    if (valid) {
        // Copy assembled data to frame buffer
        size_t offset = 0;
        for (int i = 0; i <= max_part; i++) {
            if (s_assembly.sizes[i] > 0) {
                memcpy(f->data + offset,
                       s_assembly.data + s_assembly.offsets[i],
                       s_assembly.sizes[i]);
                offset += s_assembly.sizes[i];
            }
        }
        f->size = offset;

        // Validate JPEG header
        if (f->size >= 2 && f->data[0] == 0xFF && f->data[1] == 0xD8) {
            f->complete = true;
            f->timestamp = esp_timer_get_time();
            f->in_progress = false;
            s_latest_complete_idx = s_write_idx;

            // Move to next buffer
            s_write_idx = (s_write_idx + 1) % FRAME_BUFFER_COUNT;

            ESP_LOGD(TAG, "Frame %lu complete, %u bytes",
                     (unsigned long)f->frame_index, f->size);
        } else {
            valid = false;
            ESP_LOGD(TAG, "Invalid JPEG header for frame %lu", (unsigned long)f->frame_index);
        }
    }

    if (!valid) {
        f->in_progress = false;
        f->complete = false;
    }

    xSemaphoreGive(s_mutex);
    return valid;
}

void frame_buffer_discard_current(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    frame_buffer_entry_t *f = &s_frames[s_write_idx];
    f->in_progress = false;
    f->complete = false;
    f->size = 0;

    xSemaphoreGive(s_mutex);
}

frame_info_t *frame_buffer_get_latest(void)
{
    static frame_info_t info;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    if (s_latest_complete_idx >= FRAME_BUFFER_COUNT) {
        xSemaphoreGive(s_mutex);
        return NULL;
    }

    frame_buffer_entry_t *f = &s_frames[s_latest_complete_idx];

    if (!f->complete || f->being_read) {
        xSemaphoreGive(s_mutex);
        return NULL;
    }

    // Check if this is a new frame
    if (f->frame_index <= s_last_served_frame) {
        xSemaphoreGive(s_mutex);
        return NULL;
    }

    f->being_read = true;
    s_last_served_frame = f->frame_index;

    info.data = f->data;
    info.size = f->size;
    info.frame_index = f->frame_index;
    info.timestamp = f->timestamp;
    info.valid = true;

    xSemaphoreGive(s_mutex);
    return &info;
}

void frame_buffer_release(frame_info_t *frame)
{
    if (frame == NULL) return;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // Find and release the frame
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (s_frames[i].data == frame->data) {
            s_frames[i].being_read = false;
            break;
        }
    }

    xSemaphoreGive(s_mutex);
}

bool frame_buffer_has_new_frame(void)
{
    if (s_latest_complete_idx >= FRAME_BUFFER_COUNT) {
        return false;
    }

    frame_buffer_entry_t *f = &s_frames[s_latest_complete_idx];
    return f->complete && !f->being_read && f->frame_index > s_last_served_frame;
}

void frame_buffer_get_stats(uint8_t *frames_buffered, size_t *memory_used)
{
    uint8_t count = 0;
    size_t mem = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (s_frames[i].complete) {
            count++;
            mem += s_frames[i].size;
        }
    }

    xSemaphoreGive(s_mutex);

    if (frames_buffered) *frames_buffered = count;
    if (memory_used) *memory_used = mem;
}

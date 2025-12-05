/**
 * ESP32 FPV Ground Station - Frame Buffer
 */

#include "frame_buffer.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char *TAG = "frame_buf";

#define FRAME_BUFFER_COUNT CONFIG_FPV_GS_FRAME_BUFFER_COUNT
#define MAX_FRAME_SIZE CONFIG_FPV_GS_MAX_FRAME_SIZE
#define MAX_PARTS 128  // Maximum parts per frame

// Watchdog timeout for being_read flag (1 second)
#define BEING_READ_TIMEOUT_US (1 * 1000000)

// Frame buffer entry
typedef struct {
    uint8_t *data;              // JPEG data buffer
    size_t size;                // Current data size
    size_t capacity;            // Buffer capacity
    uint32_t frame_index;       // Frame index
    int64_t timestamp;          // Completion timestamp
    uint8_t parts_mask[16];     // Bitmask for received parts (128 bits)
    uint8_t last_part_index;    // Index of last part (when known)
    bool in_progress;           // Currently being written
    bool complete;              // Frame is complete
    bool being_read;            // Currently being read by consumer
    int64_t being_read_since;   // Timestamp when being_read was set (for watchdog)
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

// Periodic logging state
static int64_t s_last_log_time = 0;
static uint32_t s_last_logged_frame = 0;
static uint32_t s_frames_since_log = 0;
static size_t s_bytes_since_log = 0;
#define LOG_INTERVAL_US (5 * 1000000)  // 5 seconds

// Debug counters for has_new_frame results
static uint32_t s_has_new_checks = 0;
static uint32_t s_has_new_not_complete = 0;
static uint32_t s_has_new_being_read = 0;
static uint32_t s_has_new_already_served = 0;
static uint32_t s_has_new_success = 0;

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

    // Allocate frame buffers
    // Try PSRAM first, fall back to internal RAM
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (free_psram >= MAX_FRAME_SIZE) {
            s_frames[i].data = heap_caps_malloc(MAX_FRAME_SIZE,
                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (s_frames[i].data != NULL) {
                using_psram = true;
            }
        }

        if (s_frames[i].data == NULL) {
            // Fall back to internal RAM
            s_frames[i].data = heap_caps_malloc(MAX_FRAME_SIZE,
                MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
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

        s_frames[i].capacity = MAX_FRAME_SIZE;
        s_frames[i].size = 0;
        s_frames[i].frame_index = 0;
        s_frames[i].in_progress = false;
        s_frames[i].complete = false;
        s_frames[i].being_read = false;
        memset(s_frames[i].parts_mask, 0, sizeof(s_frames[i].parts_mask));
    }

    // Allocate assembly buffer
    if (free_psram >= MAX_FRAME_SIZE) {
        s_assembly.data = heap_caps_malloc(MAX_FRAME_SIZE,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (s_assembly.data == NULL) {
        s_assembly.data = heap_caps_malloc(MAX_FRAME_SIZE,
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    if (s_assembly.data == NULL) {
        ESP_LOGE(TAG, "Failed to allocate assembly buffer");
        return ESP_ERR_NO_MEM;
    }

    // Log memory status after allocation
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Frame buffer initialized: %d buffers x %d bytes (%s RAM)",
             FRAME_BUFFER_COUNT, MAX_FRAME_SIZE,
             using_psram ? "PSRAM" : "internal");
    ESP_LOGI(TAG, "Free internal heap after allocation: %u bytes", (unsigned)free_heap);

    return ESP_OK;
}

void frame_buffer_start_frame(uint32_t frame_index)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int64_t now = esp_timer_get_time();

    // Watchdog: clear stale being_read flags that have been set too long
    // This prevents deadlock if a consumer crashes or forgets to release
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (s_frames[i].being_read &&
            (now - s_frames[i].being_read_since) > BEING_READ_TIMEOUT_US) {
            ESP_LOGW(TAG, "Watchdog: clearing stale being_read on buffer %d (stuck for %lld ms)",
                     i, (long long)(now - s_frames[i].being_read_since) / 1000);
            s_frames[i].being_read = false;
        }
    }

    // Find a free buffer slot:
    // - Not currently being read by a consumer
    // - Not the latest complete frame (protect it for UVC which polls slowly)
    uint8_t idx = s_write_idx;
    bool found = false;
    for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
        if (!s_frames[idx].being_read && idx != s_latest_complete_idx) {
            found = true;
            break;
        }
        idx = (idx + 1) % FRAME_BUFFER_COUNT;
    }

    // If no free buffer, we must overwrite the latest complete frame
    // This happens when all buffers are in use and input rate > consumer rate
    if (!found) {
        idx = s_write_idx;
        for (int i = 0; i < FRAME_BUFFER_COUNT; i++) {
            if (!s_frames[idx].being_read) {
                break;
            }
            idx = (idx + 1) % FRAME_BUFFER_COUNT;
        }
        ESP_LOGD(TAG, "No free buffer, overwriting latest complete frame");
    }

    // Reset the selected buffer
    frame_buffer_entry_t *f = &s_frames[idx];
    f->size = 0;
    f->frame_index = frame_index;
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

    if (total_after > MAX_FRAME_SIZE) {
        ESP_LOGW(TAG, "Frame too large, dropping part");
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

            // Update periodic logging stats
            s_frames_since_log++;
            s_bytes_since_log += f->size;
            s_last_logged_frame = f->frame_index;

            // Log every 5 seconds with actual FPS
            int64_t now = esp_timer_get_time();
            if (now - s_last_log_time >= LOG_INTERVAL_US) {
                int64_t elapsed_us = now - s_last_log_time;
                float actual_fps = (elapsed_us > 0) ?
                    (s_frames_since_log * 1000000.0f / elapsed_us) : 0;
                float avg_size_kb = (s_frames_since_log > 0) ?
                    (s_bytes_since_log / s_frames_since_log / 1024.0f) : 0;
                ESP_LOGI(TAG, "Frame %lu: %.1f fps, %u frames, avg %.1f KB/frame",
                         (unsigned long)f->frame_index, actual_fps,
                         (unsigned)s_frames_since_log, avg_size_kb);
                s_last_log_time = now;
                s_frames_since_log = 0;
                s_bytes_since_log = 0;
            }
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
    f->being_read_since = esp_timer_get_time();
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
    s_has_new_checks++;

    // Lock-free implementation: use volatile reads
    // On ESP32, aligned 32-bit reads are atomic
    // Worst case of race condition is a false negative, which just means poll again

    // Read volatile state without mutex
    uint8_t idx = s_latest_complete_idx;
    if (idx >= FRAME_BUFFER_COUNT) {
        return false;
    }

    // Read frame state - these reads may be slightly stale but that's OK
    // We're just checking "is there likely a new frame?"
    frame_buffer_entry_t *f = &s_frames[idx];
    bool complete = f->complete;
    bool being_read = f->being_read;
    uint32_t frame_index = f->frame_index;
    uint32_t last_served = s_last_served_frame;

    bool has_new = false;
    if (!complete) {
        s_has_new_not_complete++;
    } else if (being_read) {
        s_has_new_being_read++;
    } else if (frame_index <= last_served) {
        s_has_new_already_served++;
    } else {
        has_new = true;
        s_has_new_success++;
    }

    // Periodic debug logging (no mutex needed for stats - just informational)
    static int64_t last_debug_log = 0;
    int64_t now = esp_timer_get_time();
    if (now - last_debug_log >= LOG_INTERVAL_US) {
        ESP_LOGI(TAG, "has_new_frame: checks=%lu not_complete=%lu being_read=%lu already_served=%lu success=%lu",
                 (unsigned long)s_has_new_checks,
                 (unsigned long)s_has_new_not_complete, (unsigned long)s_has_new_being_read,
                 (unsigned long)s_has_new_already_served, (unsigned long)s_has_new_success);
        last_debug_log = now;
        s_has_new_checks = 0;
        s_has_new_not_complete = 0;
        s_has_new_being_read = 0;
        s_has_new_already_served = 0;
        s_has_new_success = 0;
    }

    return has_new;
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

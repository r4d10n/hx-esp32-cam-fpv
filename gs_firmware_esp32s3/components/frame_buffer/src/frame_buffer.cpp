/**
 * @file frame_buffer.cpp
 * @brief Multi-codec frame buffer implementation
 */

#include "frame_buffer.h"

#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#else
#include <stdio.h>
#include <time.h>
#define ESP_LOGI(tag, ...) printf("[%s] ", tag); printf(__VA_ARGS__); printf("\n")
#define ESP_LOGW(tag, ...) printf("[%s] WARN: ", tag); printf(__VA_ARGS__); printf("\n")
#define ESP_LOGE(tag, ...) printf("[%s] ERROR: ", tag); printf(__VA_ARGS__); printf("\n")
#endif

static const char* TAG = "frame_buffer";

// Default configuration
#define DEFAULT_MAX_FRAME_SIZE      (256 * 1024)
#define DEFAULT_MAX_FRAMES          30
#define DEFAULT_TARGET_LATENCY_MS   100
#define DEFAULT_MAX_LATENCY_MS      500

// Frame slot in buffer
typedef struct {
    uint8_t* data;
    size_t len;
    size_t capacity;
    fb_frame_info_t info;
    uint32_t receive_time;
    bool valid;
} frame_slot_t;

// Codec configuration storage
typedef struct {
    uint8_t* sps;
    size_t sps_len;
    uint8_t* pps;
    size_t pps_len;
    uint8_t* vps;
    size_t vps_len;
    fb_codec_t codec;
    bool valid;
} codec_config_t;

// Buffer state
static struct {
    fb_config_t config;
    fb_stats_t stats;

    frame_slot_t* frames;
    uint16_t frame_count;
    uint16_t head;          // Write position
    uint16_t tail;          // Read position

    codec_config_t codec_config;

    fb_output_callback_t output_cb;
    void* output_cb_user;

    uint32_t last_output_time;
    uint32_t last_keyframe_time;
    bool waiting_for_keyframe;

    // FPS calculation
    uint32_t frames_in_count;
    uint32_t frames_out_count;
    uint32_t last_fps_time;

#ifdef ESP_PLATFORM
    SemaphoreHandle_t mutex;
#endif

    bool initialized;
} g_fb = {0};

// Helper to get current time in ms
static uint32_t get_time_ms(void)
{
#ifdef ESP_PLATFORM
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

// Allocate memory (prefer PSRAM)
static void* fb_alloc(size_t size)
{
#ifdef ESP_PLATFORM
    if (g_fb.config.use_psram) {
        void* ptr = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (ptr) return ptr;
    }
#endif
    return malloc(size);
}

// Free memory
static void fb_free(void* ptr)
{
    free(ptr);
}

int frame_buffer_init(const fb_config_t* config)
{
    if (g_fb.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return -1;
    }

    // Apply configuration
    if (config) {
        g_fb.config = *config;
    } else {
        g_fb.config.max_frame_size = DEFAULT_MAX_FRAME_SIZE;
        g_fb.config.max_frames = DEFAULT_MAX_FRAMES;
        g_fb.config.target_latency_ms = DEFAULT_TARGET_LATENCY_MS;
        g_fb.config.max_latency_ms = DEFAULT_MAX_LATENCY_MS;
        g_fb.config.reorder_frames = true;
        g_fb.config.use_psram = true;
    }

    // Allocate frame slots
    g_fb.frames = (frame_slot_t*)fb_alloc(sizeof(frame_slot_t) * g_fb.config.max_frames);
    if (!g_fb.frames) {
        ESP_LOGE(TAG, "Failed to allocate frame slots");
        return -1;
    }
    memset(g_fb.frames, 0, sizeof(frame_slot_t) * g_fb.config.max_frames);

    // Allocate frame data buffers
    for (int i = 0; i < g_fb.config.max_frames; i++) {
        g_fb.frames[i].data = (uint8_t*)fb_alloc(g_fb.config.max_frame_size);
        if (!g_fb.frames[i].data) {
            ESP_LOGE(TAG, "Failed to allocate frame buffer %d", i);
            // Clean up
            for (int j = 0; j < i; j++) {
                fb_free(g_fb.frames[j].data);
            }
            fb_free(g_fb.frames);
            return -1;
        }
        g_fb.frames[i].capacity = g_fb.config.max_frame_size;
    }

#ifdef ESP_PLATFORM
    g_fb.mutex = xSemaphoreCreateMutex();
#endif

    g_fb.waiting_for_keyframe = true;
    g_fb.last_fps_time = get_time_ms();

    memset(&g_fb.stats, 0, sizeof(g_fb.stats));
    memset(&g_fb.codec_config, 0, sizeof(g_fb.codec_config));

    g_fb.initialized = true;

    ESP_LOGI(TAG, "Frame buffer initialized (%d frames, %zu bytes each)",
             g_fb.config.max_frames, g_fb.config.max_frame_size);

    return 0;
}

void frame_buffer_deinit(void)
{
    if (!g_fb.initialized) return;

#ifdef ESP_PLATFORM
    xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif

    // Free frame buffers
    for (int i = 0; i < g_fb.config.max_frames; i++) {
        if (g_fb.frames[i].data) {
            fb_free(g_fb.frames[i].data);
        }
    }
    fb_free(g_fb.frames);

    // Free codec config
    if (g_fb.codec_config.sps) fb_free(g_fb.codec_config.sps);
    if (g_fb.codec_config.pps) fb_free(g_fb.codec_config.pps);
    if (g_fb.codec_config.vps) fb_free(g_fb.codec_config.vps);

#ifdef ESP_PLATFORM
    xSemaphoreGive(g_fb.mutex);
    vSemaphoreDelete(g_fb.mutex);
#endif

    g_fb.initialized = false;
    ESP_LOGI(TAG, "Frame buffer deinitialized");
}

int frame_buffer_push(const uint8_t* data, size_t len,
                       const fb_frame_info_t* info)
{
    if (!g_fb.initialized || !data || len == 0 || !info) {
        return -1;
    }

    if (len > g_fb.config.max_frame_size) {
        ESP_LOGW(TAG, "Frame too large: %zu > %zu", len, g_fb.config.max_frame_size);
        return -1;
    }

#ifdef ESP_PLATFORM
    xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif

    g_fb.stats.frames_received++;
    g_fb.frames_in_count++;

    uint32_t now = get_time_ms();

    // Check if this is a keyframe
    bool is_keyframe = (info->flags & FB_FLAG_KEYFRAME) != 0;

    if (is_keyframe) {
        g_fb.stats.keyframes++;
        g_fb.last_keyframe_time = now;
        g_fb.waiting_for_keyframe = false;
    }

    // If waiting for keyframe, drop non-keyframes
    if (g_fb.waiting_for_keyframe && !is_keyframe) {
        g_fb.stats.frames_dropped++;
#ifdef ESP_PLATFORM
        xSemaphoreGive(g_fb.mutex);
#endif
        return 0;
    }

    // Find slot to use
    frame_slot_t* slot = &g_fb.frames[g_fb.head];

    // Check if slot is in use (buffer full)
    if (slot->valid) {
        // Drop oldest frame
        g_fb.stats.frames_dropped++;
        slot->valid = false;
    }

    // Copy frame data
    memcpy(slot->data, data, len);
    slot->len = len;
    slot->info = *info;
    slot->receive_time = now;
    slot->valid = true;

    // Advance head
    g_fb.head = (g_fb.head + 1) % g_fb.config.max_frames;
    g_fb.frame_count++;

    // Update buffer fullness
    g_fb.stats.frames_buffered = g_fb.frame_count;
    g_fb.stats.buffer_fullness_pct = (g_fb.frame_count * 100) / g_fb.config.max_frames;

#ifdef ESP_PLATFORM
    xSemaphoreGive(g_fb.mutex);
#endif

    return 0;
}

void frame_buffer_set_output_callback(fb_output_callback_t callback,
                                       void* user_data)
{
    g_fb.output_cb = callback;
    g_fb.output_cb_user = user_data;
}

void frame_buffer_process(void)
{
    if (!g_fb.initialized || !g_fb.output_cb) return;

#ifdef ESP_PLATFORM
    xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif

    uint32_t now = get_time_ms();

    // Update FPS stats
    uint32_t elapsed = now - g_fb.last_fps_time;
    if (elapsed >= 1000) {
        g_fb.stats.fps_in = (float)g_fb.frames_in_count * 1000.0f / elapsed;
        g_fb.stats.fps_out = (float)g_fb.frames_out_count * 1000.0f / elapsed;
        g_fb.frames_in_count = 0;
        g_fb.frames_out_count = 0;
        g_fb.last_fps_time = now;
    }

    // Check if we have frames to output
    while (g_fb.frame_count > 0) {
        frame_slot_t* slot = &g_fb.frames[g_fb.tail];

        if (!slot->valid) {
            // Skip invalid slot
            g_fb.tail = (g_fb.tail + 1) % g_fb.config.max_frames;
            g_fb.frame_count--;
            continue;
        }

        // Calculate frame age
        uint32_t age = now - slot->receive_time;

        // For MJPEG, output immediately with minimal latency
        // For H.264/H.265, apply target latency for jitter buffer
        uint32_t target_latency = (slot->info.codec == FB_CODEC_MJPEG) ?
                                   20 : g_fb.config.target_latency_ms;

        // Check if frame is ready to output
        if (age >= target_latency) {
            g_fb.stats.current_latency_ms = age;

            // Output frame
#ifdef ESP_PLATFORM
            xSemaphoreGive(g_fb.mutex);
#endif
            g_fb.output_cb(slot->data, slot->len, &slot->info, g_fb.output_cb_user);
#ifdef ESP_PLATFORM
            xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif

            g_fb.stats.frames_output++;
            g_fb.frames_out_count++;
            g_fb.last_output_time = now;

            // Mark slot as free
            slot->valid = false;
            g_fb.tail = (g_fb.tail + 1) % g_fb.config.max_frames;
            g_fb.frame_count--;
        } else if (age > g_fb.config.max_latency_ms) {
            // Frame too old, drop it
            ESP_LOGW(TAG, "Dropping old frame: age=%" PRIu32 "ms", age);
            g_fb.stats.frames_dropped++;
            slot->valid = false;
            g_fb.tail = (g_fb.tail + 1) % g_fb.config.max_frames;
            g_fb.frame_count--;
        } else {
            // Frame not ready yet
            break;
        }
    }

    g_fb.stats.frames_buffered = g_fb.frame_count;

#ifdef ESP_PLATFORM
    xSemaphoreGive(g_fb.mutex);
#endif
}

void frame_buffer_flush(void)
{
    if (!g_fb.initialized) return;

#ifdef ESP_PLATFORM
    xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif

    for (int i = 0; i < g_fb.config.max_frames; i++) {
        g_fb.frames[i].valid = false;
    }

    g_fb.head = 0;
    g_fb.tail = 0;
    g_fb.frame_count = 0;
    g_fb.waiting_for_keyframe = true;

    ESP_LOGI(TAG, "Buffer flushed");

#ifdef ESP_PLATFORM
    xSemaphoreGive(g_fb.mutex);
#endif
}

void frame_buffer_get_stats(fb_stats_t* stats)
{
    if (stats) {
#ifdef ESP_PLATFORM
        xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif
        *stats = g_fb.stats;
#ifdef ESP_PLATFORM
        xSemaphoreGive(g_fb.mutex);
#endif
    }
}

void frame_buffer_set_latency(uint16_t latency_ms)
{
    g_fb.config.target_latency_ms = latency_ms;
    ESP_LOGI(TAG, "Set target latency to %dms", latency_ms);
}

bool frame_buffer_waiting_for_keyframe(void)
{
    return g_fb.waiting_for_keyframe;
}

void frame_buffer_set_codec_config(fb_codec_t codec,
                                    const uint8_t* sps, size_t sps_len,
                                    const uint8_t* pps, size_t pps_len,
                                    const uint8_t* vps, size_t vps_len)
{
#ifdef ESP_PLATFORM
    xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif

    // Free existing
    if (g_fb.codec_config.sps) fb_free(g_fb.codec_config.sps);
    if (g_fb.codec_config.pps) fb_free(g_fb.codec_config.pps);
    if (g_fb.codec_config.vps) fb_free(g_fb.codec_config.vps);

    g_fb.codec_config.codec = codec;

    // Store SPS
    if (sps && sps_len > 0) {
        g_fb.codec_config.sps = (uint8_t*)fb_alloc(sps_len);
        if (g_fb.codec_config.sps) {
            memcpy(g_fb.codec_config.sps, sps, sps_len);
            g_fb.codec_config.sps_len = sps_len;
        }
    } else {
        g_fb.codec_config.sps = NULL;
        g_fb.codec_config.sps_len = 0;
    }

    // Store PPS
    if (pps && pps_len > 0) {
        g_fb.codec_config.pps = (uint8_t*)fb_alloc(pps_len);
        if (g_fb.codec_config.pps) {
            memcpy(g_fb.codec_config.pps, pps, pps_len);
            g_fb.codec_config.pps_len = pps_len;
        }
    } else {
        g_fb.codec_config.pps = NULL;
        g_fb.codec_config.pps_len = 0;
    }

    // Store VPS (H.265 only)
    if (vps && vps_len > 0) {
        g_fb.codec_config.vps = (uint8_t*)fb_alloc(vps_len);
        if (g_fb.codec_config.vps) {
            memcpy(g_fb.codec_config.vps, vps, vps_len);
            g_fb.codec_config.vps_len = vps_len;
        }
    } else {
        g_fb.codec_config.vps = NULL;
        g_fb.codec_config.vps_len = 0;
    }

    g_fb.codec_config.valid = true;

    ESP_LOGI(TAG, "Stored codec config: codec=%d, SPS=%zu, PPS=%zu, VPS=%zu",
             codec, sps_len, pps_len, vps_len);

#ifdef ESP_PLATFORM
    xSemaphoreGive(g_fb.mutex);
#endif
}

bool frame_buffer_get_codec_config(fb_codec_t* codec,
                                    const uint8_t** sps, size_t* sps_len,
                                    const uint8_t** pps, size_t* pps_len,
                                    const uint8_t** vps, size_t* vps_len)
{
    if (!g_fb.codec_config.valid) return false;

#ifdef ESP_PLATFORM
    xSemaphoreTake(g_fb.mutex, portMAX_DELAY);
#endif

    if (codec) *codec = g_fb.codec_config.codec;
    if (sps) *sps = g_fb.codec_config.sps;
    if (sps_len) *sps_len = g_fb.codec_config.sps_len;
    if (pps) *pps = g_fb.codec_config.pps;
    if (pps_len) *pps_len = g_fb.codec_config.pps_len;
    if (vps) *vps = g_fb.codec_config.vps;
    if (vps_len) *vps_len = g_fb.codec_config.vps_len;

#ifdef ESP_PLATFORM
    xSemaphoreGive(g_fb.mutex);
#endif

    return true;
}

#include "frame_assembler.h"
#include <cstring>
#include <algorithm>
#include <cinttypes>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_log.h"
static const char* TAG = "frame_asm";
#define LOG_E(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
#else
#include <cstdio>
#define LOG_E(fmt, ...) fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) fprintf(stderr, "[W] " fmt "\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...) printf("[I] " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) // printf("[D] " fmt "\n", ##__VA_ARGS__)
#endif

// Minimum packet size for video header
static constexpr size_t MIN_VIDEO_PACKET_SIZE = sizeof(VideoPacketHeader);

FrameAssembler::FrameAssembler() {
    m_current_frame.reset();
}

FrameAssembler::~FrameAssembler() {
    deinit();
}

bool FrameAssembler::init(bool use_psram) {
    if (m_initialized) {
        deinit();
    }

    m_use_psram = use_psram;

    // Allocate frame buffer
    LOG_I("Allocating frame buffer: %d KB", MAX_FRAME_SIZE / 1024);

#ifdef ESP_PLATFORM
    if (use_psram) {
        m_frame_buffer = (uint8_t*)heap_caps_malloc(MAX_FRAME_SIZE, MALLOC_CAP_SPIRAM);
    } else {
        m_frame_buffer = (uint8_t*)heap_caps_malloc(MAX_FRAME_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    }
#else
    m_frame_buffer = new uint8_t[MAX_FRAME_SIZE];
#endif

    if (!m_frame_buffer) {
        LOG_E("Failed to allocate frame buffer");
        return false;
    }

    m_frame_buffer_size = MAX_FRAME_SIZE;
    memset(m_part_offsets, 0, sizeof(m_part_offsets));
    m_current_frame.reset();

    m_frames_decoded = 0;
    m_frames_incomplete = 0;

    m_initialized = true;
    LOG_I("FrameAssembler initialized");
    return true;
}

void FrameAssembler::deinit() {
    if (!m_initialized) return;

    if (m_frame_buffer) {
#ifdef ESP_PLATFORM
        heap_caps_free(m_frame_buffer);
#else
        delete[] m_frame_buffer;
#endif
        m_frame_buffer = nullptr;
    }
    m_frame_buffer_size = 0;
    m_current_frame.reset();

    m_initialized = false;
    LOG_I("FrameAssembler deinitialized");
}

void FrameAssembler::process_packet(const uint8_t* data, size_t size) {
    if (!m_initialized || !data || size == 0) return;

    // Check minimum size for header
    if (size < sizeof(uint8_t)) return;

    // Get packet type from first byte (Air2Ground_Header.type)
    PacketType type = (PacketType)data[0];

    switch (type) {
        case PacketType::Video:
            process_video_packet(data, size);
            break;

        case PacketType::Telemetry:
        case PacketType::OSD:
        case PacketType::Config:
            // Pass through to data callback
            if (m_data_callback) {
                m_data_callback(data, size, type);
            }
            break;

        default:
            LOG_D("Unknown packet type: %d", (int)type);
            break;
    }
}

void FrameAssembler::process_video_packet(const uint8_t* data, size_t size) {
    if (size < MIN_VIDEO_PACKET_SIZE) {
        LOG_W("Video packet too small: %zu < %zu", size, MIN_VIDEO_PACKET_SIZE);
        return;
    }

    const VideoPacketHeader* header = (const VideoPacketHeader*)data;

    uint32_t frame_index = header->frame_index;
    uint8_t part_index = header->part_index;
    bool is_last = header->last_part;
    uint8_t resolution = header->resolution;

    // Calculate payload size and offset
    size_t payload_size = size - sizeof(VideoPacketHeader);
    const uint8_t* payload = data + sizeof(VideoPacketHeader);

    LOG_D("Video packet: frame=%" PRIu32 " part=%d last=%d size=%zu",
          frame_index, part_index, is_last, payload_size);

    // Check if this is a new frame
    if (frame_index != m_current_frame.frame_index) {
        // Flush incomplete frame if we had one
        if (m_current_frame.parts_received > 0 && !m_current_frame.complete) {
            LOG_D("Incomplete frame %" PRIu32 " (%d parts)", m_current_frame.frame_index,
                  m_current_frame.parts_received);
            m_frames_incomplete++;

            // Still dispatch partial frame if we have data and callback
            // (decoder might be able to use partial JPEG)
            if (m_frame_callback && m_current_frame.current_size > 0) {
                // Don't dispatch partial frames - they're useless for JPEG
                // m_frame_callback(m_frame_buffer, m_current_frame.current_size,
                //                  m_current_frame.frame_index, m_current_frame.resolution);
            }
        }

        // Start new frame
        m_current_frame.reset();
        m_current_frame.frame_index = frame_index;
        m_current_frame.resolution = resolution;
        memset(m_part_offsets, 0, sizeof(m_part_offsets));

        LOG_D("Starting new frame %" PRIu32, frame_index);
    }

    // Check if we already have this part
    if (m_current_frame.has_part(part_index)) {
        LOG_D("Duplicate part %d for frame %" PRIu32, part_index, frame_index);
        return;
    }

    // Mark last part info
    if (is_last) {
        m_current_frame.expected_parts = part_index + 1;
    }

    // Store part data
    // For now, we assume parts arrive mostly in order and just append
    // A more robust implementation would reassemble out-of-order parts
    if (part_index < MAX_PARTS_PER_FRAME) {
        // Calculate offset based on part index (assume fixed part size for simplicity)
        // Better approach: track actual offsets per part
        size_t offset = 0;

        // If parts arrive in order, just append
        if (part_index == m_current_frame.parts_received) {
            offset = m_current_frame.current_size;
        } else {
            // Out of order - need to track per-part offsets
            // For simplicity, assume average part size and estimate
            // In production, we'd want proper tracking
            LOG_W("Out-of-order part: got %d, have %d parts",
                  part_index, m_current_frame.parts_received);
            // Skip this part for now - better to have gap than corrupted data
            return;
        }

        // Check buffer bounds
        if (offset + payload_size > m_frame_buffer_size) {
            LOG_E("Frame buffer overflow: offset=%zu + size=%zu > %zu",
                  offset, payload_size, m_frame_buffer_size);
            return;
        }

        // Copy payload to frame buffer
        memcpy(m_frame_buffer + offset, payload, payload_size);
        m_part_offsets[part_index] = offset;

        m_current_frame.set_part(part_index);
        m_current_frame.parts_received++;
        m_current_frame.current_size = offset + payload_size;

        LOG_D("Added part %d, frame size now %zu bytes",
              part_index, m_current_frame.current_size);
    }

    // Check if frame is complete
    if (m_current_frame.expected_parts > 0 &&
        m_current_frame.parts_received >= m_current_frame.expected_parts) {
        m_current_frame.complete = true;
        dispatch_frame();
    }
}

void FrameAssembler::dispatch_frame() {
    if (!m_current_frame.complete) return;

    LOG_D("Dispatching complete frame %" PRIu32 " (%zu bytes, %d parts)",
          m_current_frame.frame_index,
          m_current_frame.current_size,
          m_current_frame.parts_received);

    m_frames_decoded++;

    if (m_frame_callback) {
        m_frame_callback(m_frame_buffer, m_current_frame.current_size,
                        m_current_frame.frame_index, m_current_frame.resolution);
    }

    // Reset for next frame
    m_current_frame.reset();
}

bool FrameAssembler::flush() {
    if (m_current_frame.parts_received > 0 && !m_current_frame.complete) {
        LOG_D("Flushing incomplete frame %" PRIu32 " (%d parts)",
              m_current_frame.frame_index, m_current_frame.parts_received);
        m_frames_incomplete++;
        m_current_frame.reset();
        return true;
    }
    return false;
}

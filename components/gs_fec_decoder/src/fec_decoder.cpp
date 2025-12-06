#include "fec_decoder.h"
#include "fec.h"
#include <cstring>
#include <algorithm>
#include <cinttypes>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char* TAG = "fec_dec";
#define LOG_E(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
static uint32_t get_time_ms() { return (uint32_t)(esp_timer_get_time() / 1000); }
#else
#include <cstdio>
#include <chrono>
#define LOG_E(fmt, ...) fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) fprintf(stderr, "[W] " fmt "\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...) printf("[I] " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) // printf("[D] " fmt "\n", ##__VA_ARGS__)
static uint32_t get_time_ms() {
    using namespace std::chrono;
    return (uint32_t)duration_cast<milliseconds>(
        steady_clock::now().time_since_epoch()).count();
}
#endif

// Global decoder instance
FecDecoder g_fec_decoder;

// Block numbers for FEC encoding/decoding
static constexpr unsigned BLOCK_NUMS[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
};

FecDecoder::FecDecoder() {
}

FecDecoder::~FecDecoder() {
    deinit();
}

bool FecDecoder::init(const FecDecoderConfig& config) {
    if (m_initialized) {
        deinit();
    }

    m_config = config;

    // Initialize FEC library if needed
    init_fec();

    // Create FEC context
    m_fec = fec_new(config.coding_k, config.coding_n);
    if (!m_fec) {
        LOG_E("Failed to create FEC context");
        return false;
    }

    // Initialize block manager
    if (!m_block_manager.init(config.coding_k, config.coding_n, config.mtu, config.use_psram)) {
        LOG_E("Failed to init block manager");
        fec_free(m_fec);
        m_fec = nullptr;
        return false;
    }

    // Initialize frame assembler
    if (!m_frame_assembler.init(config.use_psram)) {
        LOG_E("Failed to init frame assembler");
        m_block_manager.deinit();
        fec_free(m_fec);
        m_fec = nullptr;
        return false;
    }

    // Allocate FEC pointer arrays
    size_t ptr_array_size = sizeof(uint8_t*) * config.coding_k;

#ifdef ESP_PLATFORM
    if (config.use_psram) {
        m_fec_src_ptrs = (uint8_t**)heap_caps_malloc(ptr_array_size, MALLOC_CAP_SPIRAM);
        m_fec_dst_ptrs = (uint8_t**)heap_caps_malloc(ptr_array_size, MALLOC_CAP_SPIRAM);
        m_decode_buffer = (uint8_t*)heap_caps_malloc(config.mtu * config.coding_k, MALLOC_CAP_SPIRAM);
    } else {
        m_fec_src_ptrs = (uint8_t**)malloc(ptr_array_size);
        m_fec_dst_ptrs = (uint8_t**)malloc(ptr_array_size);
        m_decode_buffer = (uint8_t*)malloc(config.mtu * config.coding_k);
    }
#else
    m_fec_src_ptrs = new uint8_t*[config.coding_k];
    m_fec_dst_ptrs = new uint8_t*[config.coding_k];
    m_decode_buffer = new uint8_t[config.mtu * config.coding_k];
#endif

    if (!m_fec_src_ptrs || !m_fec_dst_ptrs || !m_decode_buffer) {
        LOG_E("Failed to allocate FEC buffers");
        deinit();
        return false;
    }

    m_stats.reset();
    m_last_process_time_ms = get_time_ms();

    m_initialized = true;
    LOG_I("FecDecoder initialized: k=%d, n=%d, mtu=%d",
          config.coding_k, config.coding_n, config.mtu);

    return true;
}

bool FecDecoder::init(uint8_t coding_k, uint8_t coding_n, uint16_t mtu) {
    FecDecoderConfig config;
    config.coding_k = coding_k;
    config.coding_n = coding_n;
    config.mtu = mtu;
    return init(config);
}

void FecDecoder::deinit() {
    if (!m_initialized) return;

    m_frame_assembler.deinit();
    m_block_manager.deinit();

    if (m_fec) {
        fec_free(m_fec);
        m_fec = nullptr;
    }

#ifdef ESP_PLATFORM
    if (m_fec_src_ptrs) heap_caps_free(m_fec_src_ptrs);
    if (m_fec_dst_ptrs) heap_caps_free(m_fec_dst_ptrs);
    if (m_decode_buffer) heap_caps_free(m_decode_buffer);
#else
    delete[] m_fec_src_ptrs;
    delete[] m_fec_dst_ptrs;
    delete[] m_decode_buffer;
#endif

    m_fec_src_ptrs = nullptr;
    m_fec_dst_ptrs = nullptr;
    m_decode_buffer = nullptr;

    m_initialized = false;
    LOG_I("FecDecoder deinitialized");
}

bool FecDecoder::validate_packet_header(const uint8_t* data, size_t size) {
    if (size < PACKET_OVERHEAD) {
        return false;
    }

    const Packet_Header* header = (const Packet_Header*)data;

    // Check signature
    if (header->packet_version != PACKET_VERSION ||
        header->packet_signature != PACKET_SIGNATURE) {
        return false;
    }

    // Check device ID filter if set
    if (m_config.device_id != 0 &&
        header->fromDeviceId != m_config.device_id) {
        m_stats.filtered_packets++;
        return false;
    }

    return true;
}

bool FecDecoder::process_packet(const uint8_t* data, size_t size) {
    if (!m_initialized || !data || size == 0) {
        return false;
    }

    m_stats.packets_processed++;
    m_stats.bytes_received += size;
    m_stats.last_packet_time_ms = get_time_ms();

    // Validate packet header
    if (!validate_packet_header(data, size)) {
        m_stats.invalid_packets++;
        return false;
    }

    const Packet_Header* header = (const Packet_Header*)data;
    uint32_t block_index = header->block_index;
    uint8_t packet_index = header->packet_index;

    LOG_D("Packet: block=%" PRIu32 ", pkt=%d, size=%zu",
          block_index, packet_index, size);

    // Track primary vs FEC packets
    if (packet_index < m_config.coding_k) {
        m_stats.primary_packets++;
    } else {
        m_stats.fec_packets++;
    }

    // Extract payload (skip FEC header)
    const uint8_t* payload = data + PACKET_OVERHEAD;
    size_t payload_size = size - PACKET_OVERHEAD;

    // Add packet to block manager
    FecBlock* block = m_block_manager.add_packet(block_index, packet_index,
                                                  payload, payload_size);
    if (!block) {
        m_stats.duplicate_packets++;
        return false;
    }

    // Check if block can be decoded immediately
    if (block->can_decode(m_config.coding_k) && !block->is_processed) {
        decode_block(block);
    }

    return true;
}

bool FecDecoder::process_raw_frame(const uint8_t* frame, size_t frame_size,
                                   size_t ieee_header_size) {
    if (frame_size <= ieee_header_size) {
        return false;
    }

    const uint8_t* payload = frame + ieee_header_size;
    size_t payload_size = frame_size - ieee_header_size;

    return process_packet(payload, payload_size);
}

bool FecDecoder::decode_block(FecBlock* block) {
    if (!block || !m_fec) return false;

    uint32_t start_time = get_time_ms();
    (void)start_time; // Used only for timing

#ifdef ESP_PLATFORM
    uint64_t start_us = esp_timer_get_time();
#endif

    bool success = false;

    if (block->is_complete(m_config.coding_k)) {
        // All primary packets present - no FEC decoding needed
        LOG_D("Block %" PRIu32 " complete, no FEC needed", block->index);
        m_stats.blocks_complete++;
        success = true;
    } else {
        // Need FEC recovery
        LOG_D("Block %" PRIu32 " needs FEC: %d primary + %d fec packets",
              block->index, block->primary_count, block->fec_count);

        // Build source pointer array (mix of primary and FEC packets)
        unsigned indices[MAX_CODING_K];
        size_t primary_idx = 0;
        size_t fec_idx = 0;
        size_t dst_idx = 0;

        // Determine which packets we have and need
        for (size_t i = 0; i < m_config.coding_k; i++) {
            if (block->primary_mask & (1u << i)) {
                // Have this primary packet
                m_fec_src_ptrs[i] = block->primary_packets[i];
                indices[i] = i;
                primary_idx++;
            } else {
                // Missing primary - use FEC packet
                while (fec_idx < (m_config.coding_n - m_config.coding_k) &&
                       !(block->fec_mask & (1u << fec_idx))) {
                    fec_idx++;
                }

                if (fec_idx < (m_config.coding_n - m_config.coding_k)) {
                    m_fec_src_ptrs[i] = block->fec_packets[fec_idx];
                    indices[i] = m_config.coding_k + fec_idx;
                    fec_idx++;
                } else {
                    // Not enough packets
                    LOG_W("Block %" PRIu32 ": not enough packets for FEC", block->index);
                    m_stats.blocks_failed++;
                    block->is_processed = true;
                    return false;
                }

                // Set up destination for recovered packet
                m_fec_dst_ptrs[dst_idx] = m_decode_buffer + (dst_idx * m_config.mtu);
                dst_idx++;
            }
        }

        // Perform FEC decode
        fec_decode(m_fec, (const gf*const*)m_fec_src_ptrs,
                   (gf*const*)m_fec_dst_ptrs, indices, m_config.mtu);

        // Copy recovered packets back to block
        dst_idx = 0;
        for (size_t i = 0; i < m_config.coding_k; i++) {
            if (!(block->primary_mask & (1u << i))) {
                // This was a missing packet - now recovered
                block->primary_packets[i] = m_fec_dst_ptrs[dst_idx];
                block->primary_sizes[i] = m_config.mtu;
                // Note: Don't update mask/count since we're using decode buffer
                dst_idx++;
            }
        }

        m_stats.blocks_recovered++;
        success = true;
    }

    if (success) {
        dispatch_block(block);
    }

    block->is_processed = true;

#ifdef ESP_PLATFORM
    uint64_t end_us = esp_timer_get_time();
    m_stats.decode_time_us = (uint32_t)(end_us - start_us);
#endif

    // Release the block
    m_block_manager.release_block(block);

    return success;
}

void FecDecoder::dispatch_block(FecBlock* block) {
    // Dispatch packets to frame assembler in order
    for (size_t i = 0; i < m_config.coding_k; i++) {
        uint8_t* packet_data = block->primary_packets[i];
        size_t packet_size = block->primary_sizes[i];

        if (packet_data && packet_size > 0) {
            m_stats.bytes_decoded += packet_size;
            m_frame_assembler.process_packet(packet_data, packet_size);
        }
    }
}

void FecDecoder::process() {
    if (!m_initialized) return;

    uint32_t current_time = get_time_ms();

    // Process any ready blocks
    FecBlock* ready_blocks[MAX_BLOCKS_IN_FLIGHT];
    size_t count = m_block_manager.get_ready_blocks(ready_blocks, MAX_BLOCKS_IN_FLIGHT);

    for (size_t i = 0; i < count; i++) {
        if (!ready_blocks[i]->is_processed) {
            decode_block(ready_blocks[i]);
        }
    }

    // Expire old blocks
    size_t expired = m_block_manager.expire_blocks(current_time, m_config.block_timeout_ms);
    m_stats.blocks_failed += expired;

    m_last_process_time_ms = current_time;
}

void FecDecoder::set_frame_callback(FrameDecodedCallback callback) {
    m_frame_assembler.set_frame_callback(callback);
}

void FecDecoder::set_data_callback(DataCallback callback) {
    m_frame_assembler.set_data_callback(callback);
}

void FecDecoder::set_device_filter(uint16_t device_id) {
    m_config.device_id = device_id;
}

void FecDecoder::print_stats() const {
    m_stats.print();
}

// Statistics print implementation
void FecDecoderStats::print() const {
    printf("\n=== FEC Decoder Statistics ===\n");
    printf("Packets processed: %" PRIu32 "\n", packets_processed);
    printf("  Primary packets: %" PRIu32 "\n", primary_packets);
    printf("  FEC packets:     %" PRIu32 "\n", fec_packets);
    printf("  Duplicate:       %" PRIu32 "\n", duplicate_packets);
    printf("  Invalid:         %" PRIu32 "\n", invalid_packets);
    printf("  Filtered:        %" PRIu32 "\n", filtered_packets);
    printf("FEC blocks complete:  %" PRIu32 "\n", blocks_complete);
    printf("FEC blocks recovered: %" PRIu32 "\n", blocks_recovered);
    printf("FEC blocks failed:    %" PRIu32 "\n", blocks_failed);
    printf("FEC blocks skipped:   %" PRIu32 "\n", blocks_skipped);
    printf("Frames decoded:    %" PRIu32 "\n", frames_decoded);
    printf("Frames incomplete: %" PRIu32 "\n", frames_incomplete);
    printf("Bytes received:    %" PRIu64 "\n", bytes_received);
    printf("Bytes decoded:     %" PRIu64 "\n", bytes_decoded);
    if (decode_time_us > 0) {
        printf("Last decode time:  %" PRIu32 " us\n", decode_time_us);
    }
    printf("==============================\n\n");
}

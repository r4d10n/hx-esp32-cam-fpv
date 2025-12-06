#include "block_manager.h"
#include <cstring>
#include <algorithm>
#include <cinttypes>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char* TAG = "block_mgr";
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

BlockManager::BlockManager() {
    // Initialize block storage
    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        m_blocks[i].reset();
    }
}

BlockManager::~BlockManager() {
    deinit();
}

bool BlockManager::init(uint8_t coding_k, uint8_t coding_n, uint16_t mtu, bool use_psram) {
    if (m_initialized) {
        deinit();
    }

    if (coding_k == 0 || coding_k > MAX_CODING_K) {
        LOG_E("Invalid coding_k: %d (max %d)", coding_k, MAX_CODING_K);
        return false;
    }
    if (coding_n <= coding_k || coding_n > MAX_CODING_N) {
        LOG_E("Invalid coding_n: %d (must be > k and <= %d)", coding_n, MAX_CODING_N);
        return false;
    }
    if (mtu == 0 || mtu > 2000) {
        LOG_E("Invalid MTU: %d", mtu);
        return false;
    }

    m_coding_k = coding_k;
    m_coding_n = coding_n;
    m_mtu = mtu;
    m_use_psram = use_psram;

    // Allocate packet buffer pool
    size_t packet_size = mtu;
    LOG_I("Allocating packet pool: %d buffers x %d bytes = %d KB",
          PACKET_POOL_SIZE, packet_size, (PACKET_POOL_SIZE * packet_size) / 1024);

    for (size_t i = 0; i < PACKET_POOL_SIZE; i++) {
#ifdef ESP_PLATFORM
        if (use_psram) {
            m_packet_pool[i] = (uint8_t*)heap_caps_malloc(packet_size, MALLOC_CAP_SPIRAM);
        } else {
            m_packet_pool[i] = (uint8_t*)heap_caps_malloc(packet_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        }
#else
        m_packet_pool[i] = new uint8_t[packet_size];
#endif
        if (!m_packet_pool[i]) {
            LOG_E("Failed to allocate packet buffer %d", i);
            deinit();
            return false;
        }
        m_pool_free_list[i] = i;
    }
    m_pool_free_count = PACKET_POOL_SIZE;

    // Reset all blocks
    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        m_blocks[i].reset();
    }
    m_active_count = 0;
    m_next_expected_block = 0;

    m_initialized = true;
    LOG_I("BlockManager initialized: k=%d, n=%d, mtu=%d, psram=%d",
          coding_k, coding_n, mtu, use_psram);
    return true;
}

void BlockManager::deinit() {
    if (!m_initialized) return;

    // Free packet pool
    for (size_t i = 0; i < PACKET_POOL_SIZE; i++) {
        if (m_packet_pool[i]) {
#ifdef ESP_PLATFORM
            heap_caps_free(m_packet_pool[i]);
#else
            delete[] m_packet_pool[i];
#endif
            m_packet_pool[i] = nullptr;
        }
    }
    m_pool_free_count = 0;

    // Reset blocks
    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        m_blocks[i].reset();
    }
    m_active_count = 0;

    m_initialized = false;
    LOG_I("BlockManager deinitialized");
}

uint8_t* BlockManager::alloc_packet_buffer() {
    if (m_pool_free_count == 0) {
        LOG_W("Packet pool exhausted");
        return nullptr;
    }
    m_pool_free_count--;
    size_t idx = m_pool_free_list[m_pool_free_count];
    return m_packet_pool[idx];
}

void BlockManager::free_packet_buffer(uint8_t* buffer) {
    if (!buffer) return;

    // Find buffer index
    for (size_t i = 0; i < PACKET_POOL_SIZE; i++) {
        if (m_packet_pool[i] == buffer) {
            m_pool_free_list[m_pool_free_count++] = i;
            return;
        }
    }
    LOG_W("Attempted to free unknown buffer");
}

FecBlock* BlockManager::find_or_create_block(uint32_t block_index) {
    // First, look for existing block
    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        if (m_blocks[i].is_active && m_blocks[i].index == block_index) {
            return &m_blocks[i];
        }
    }

    // Find a free slot
    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        if (!m_blocks[i].is_active) {
            m_blocks[i].reset();
            m_blocks[i].index = block_index;
            m_blocks[i].is_active = true;
            m_blocks[i].first_packet_time_ms = get_time_ms();
            m_active_count++;
            LOG_D("Created new block %" PRIu32 " in slot %d", block_index, (int)i);
            return &m_blocks[i];
        }
    }

    // No free slots - find oldest block and replace it
    uint32_t oldest_time = UINT32_MAX;
    size_t oldest_idx = 0;
    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        if (m_blocks[i].is_active && m_blocks[i].first_packet_time_ms < oldest_time) {
            oldest_time = m_blocks[i].first_packet_time_ms;
            oldest_idx = i;
        }
    }

    LOG_W("Block pool full, evicting block %" PRIu32 " for %" PRIu32,
          m_blocks[oldest_idx].index, block_index);
    release_block(&m_blocks[oldest_idx]);

    m_blocks[oldest_idx].reset();
    m_blocks[oldest_idx].index = block_index;
    m_blocks[oldest_idx].is_active = true;
    m_blocks[oldest_idx].first_packet_time_ms = get_time_ms();
    m_active_count++;

    return &m_blocks[oldest_idx];
}

FecBlock* BlockManager::add_packet(uint32_t block_index, uint8_t packet_index,
                                   const uint8_t* data, uint16_t size) {
    if (!m_initialized) return nullptr;

    // Check if this is an old block we've already moved past
    if (block_index < m_next_expected_block) {
        // Allow some tolerance for out-of-order packets
        if (m_next_expected_block - block_index > MAX_BLOCKS_IN_FLIGHT * 2) {
            // Very old block, likely a restart - reset tracking
            LOG_W("Very old block %" PRIu32 " (expected %" PRIu32 ") - resetting",
                  block_index, m_next_expected_block);
            m_next_expected_block = block_index;
        } else {
            LOG_D("Old block %" PRIu32 " (expected %" PRIu32 ") - dropping", block_index, m_next_expected_block);
            return nullptr;
        }
    }

    // Find or create block
    FecBlock* block = find_or_create_block(block_index);
    if (!block) {
        return nullptr;
    }

    // Check if packet already received
    if (packet_index < m_coding_k) {
        // Primary packet
        if (block->primary_mask & (1u << packet_index)) {
            LOG_D("Duplicate primary packet %d in block %" PRIu32, packet_index, block_index);
            return nullptr; // Duplicate
        }

        // Allocate buffer and copy data
        uint8_t* buffer = alloc_packet_buffer();
        if (!buffer) {
            return nullptr;
        }
        memcpy(buffer, data, std::min((size_t)size, (size_t)m_mtu));

        block->primary_packets[packet_index] = buffer;
        block->primary_sizes[packet_index] = size;
        block->primary_mask |= (1u << packet_index);
        block->primary_count++;
        LOG_D("Added primary packet %d to block %" PRIu32 " (%d/%d)",
              packet_index, block_index, block->primary_count, m_coding_k);
    } else {
        // FEC packet
        uint8_t fec_idx = packet_index - m_coding_k;
        if (fec_idx >= MAX_FEC_PACKETS) {
            LOG_W("FEC index out of range: %d", fec_idx);
            return nullptr;
        }

        if (block->fec_mask & (1u << fec_idx)) {
            LOG_D("Duplicate FEC packet %d in block %" PRIu32, packet_index, block_index);
            return nullptr; // Duplicate
        }

        // Allocate buffer and copy data
        uint8_t* buffer = alloc_packet_buffer();
        if (!buffer) {
            return nullptr;
        }
        memcpy(buffer, data, std::min((size_t)size, (size_t)m_mtu));

        block->fec_packets[fec_idx] = buffer;
        block->fec_sizes[fec_idx] = size;
        block->fec_mask |= (1u << fec_idx);
        block->fec_count++;
        LOG_D("Added FEC packet %d to block %" PRIu32 " (%d FEC packets)",
              packet_index, block_index, block->fec_count);
    }

    return block;
}

FecBlock* BlockManager::get_block(uint32_t block_index) {
    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        if (m_blocks[i].is_active && m_blocks[i].index == block_index) {
            return &m_blocks[i];
        }
    }
    return nullptr;
}

void BlockManager::release_block(FecBlock* block) {
    if (!block || !block->is_active) return;

    // Free all packet buffers
    for (int i = 0; i < MAX_CODING_K; i++) {
        if (block->primary_packets[i]) {
            free_packet_buffer(block->primary_packets[i]);
            block->primary_packets[i] = nullptr;
        }
    }
    for (int i = 0; i < MAX_FEC_PACKETS; i++) {
        if (block->fec_packets[i]) {
            free_packet_buffer(block->fec_packets[i]);
            block->fec_packets[i] = nullptr;
        }
    }

    // Update expected block if this was the next one
    if (block->index >= m_next_expected_block) {
        m_next_expected_block = block->index + 1;
    }

    block->reset();
    if (m_active_count > 0) m_active_count--;
}

size_t BlockManager::get_ready_blocks(FecBlock** out_blocks, size_t max_blocks) {
    size_t count = 0;

    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT && count < max_blocks; i++) {
        FecBlock* block = &m_blocks[i];
        if (block->is_active && !block->is_processed && block->can_decode(m_coding_k)) {
            out_blocks[count++] = block;
        }
    }

    // Sort by block index for ordered processing
    std::sort(out_blocks, out_blocks + count,
              [](FecBlock* a, FecBlock* b) { return a->index < b->index; });

    return count;
}

size_t BlockManager::expire_blocks(uint32_t current_time_ms, uint32_t timeout_ms) {
    size_t expired = 0;

    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        FecBlock* block = &m_blocks[i];
        if (block->is_active && !block->is_processed) {
            uint32_t age = current_time_ms - block->first_packet_time_ms;
            if (age > timeout_ms) {
                LOG_D("Expiring block %" PRIu32 " (age %" PRIu32 " ms, %d/%d packets)",
                      block->index, age, block->total_packets(), m_coding_k);
                release_block(block);
                expired++;
            }
        }
    }

    return expired;
}

size_t BlockManager::skip_to_block(uint32_t new_block_index) {
    size_t skipped = 0;

    for (size_t i = 0; i < MAX_BLOCKS_IN_FLIGHT; i++) {
        FecBlock* block = &m_blocks[i];
        if (block->is_active && block->index < new_block_index) {
            LOG_D("Skipping block %" PRIu32 " (new index %" PRIu32 ")", block->index, new_block_index);
            release_block(block);
            skipped++;
        }
    }

    if (new_block_index > m_next_expected_block) {
        m_next_expected_block = new_block_index;
    }

    return skipped;
}

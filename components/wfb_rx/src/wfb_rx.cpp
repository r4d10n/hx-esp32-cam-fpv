/**
 * @file wfb_rx.cpp
 * @brief WFB-NG Receiver Implementation
 */

#include "wfb_rx.h"
#include "fec.h"
#include <cstring>
#include <algorithm>
#include <cinttypes>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
static const char* TAG = "wfb_rx";
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

// IEEE 802.11 header size
#define IEEE80211_HEADER_SIZE   24

// ============================================================================
// Statistics
// ============================================================================

void WfbRxStats::print() const {
    printf("\n=== WFB-NG Receiver Statistics ===\n");
    printf("Packets received:    %" PRIu32 "\n", packets_received);
    printf("  Decrypted:         %" PRIu32 "\n", packets_decrypted);
    printf("  Decrypt failed:    %" PRIu32 "\n", packets_decrypt_failed);
    printf("  FEC recovered:     %" PRIu32 "\n", packets_fec_recovered);
    printf("  Session packets:   %" PRIu32 "\n", session_packets);
    printf("Blocks complete:     %" PRIu32 "\n", blocks_complete);
    printf("Blocks recovered:    %" PRIu32 "\n", blocks_recovered);
    printf("Blocks failed:       %" PRIu32 "\n", blocks_failed);
    printf("Frames output:       %" PRIu32 "\n", frames_output);
    printf("Bytes output:        %" PRIu64 "\n", bytes_output);
    printf("Last RSSI:           %d dBm\n", last_rssi);
    printf("Last MCS:            %u\n", last_mcs);
    printf("==================================\n\n");
}

// ============================================================================
// Constructor / Destructor
// ============================================================================

WfbReceiver::WfbReceiver()
    : m_initialized(false)
    , m_use_psram(true)
    , m_tx_key_set(false)
    , m_active_blocks(0)
    , m_next_expected_block(0)
    , m_pool_free_count(0)
    , m_decode_buffer(nullptr)
    , m_decode_buffer_size(0)
    , m_fec_ctx(nullptr)
    , m_last_process_time_ms(0) {
    wfb_crypto_init(&m_crypto);
    memset(m_tx_public_key, 0, sizeof(m_tx_public_key));
    memset(m_packet_pool, 0, sizeof(m_packet_pool));
}

WfbReceiver::~WfbReceiver() {
    deinit();
}

// ============================================================================
// Initialization
// ============================================================================

bool WfbReceiver::init(bool use_psram) {
    if (m_initialized) {
        deinit();
    }

    m_use_psram = use_psram;

    // Initialize FEC library
    init_fec();

    // Allocate packet buffer pool
    LOG_I("Allocating packet pool: %d buffers x %d bytes",
          WFB_RX_PACKET_POOL_SIZE, WFB_RX_MAX_PACKET_SIZE);

    for (size_t i = 0; i < WFB_RX_PACKET_POOL_SIZE; i++) {
#ifdef ESP_PLATFORM
        if (use_psram) {
            m_packet_pool[i] = (uint8_t*)heap_caps_malloc(
                WFB_RX_MAX_PACKET_SIZE, MALLOC_CAP_SPIRAM);
        } else {
            m_packet_pool[i] = (uint8_t*)heap_caps_malloc(
                WFB_RX_MAX_PACKET_SIZE, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        }
#else
        m_packet_pool[i] = new uint8_t[WFB_RX_MAX_PACKET_SIZE];
#endif
        if (!m_packet_pool[i]) {
            LOG_E("Failed to allocate packet buffer %zu", i);
            deinit();
            return false;
        }
        m_pool_free_list[i] = i;
    }
    m_pool_free_count = WFB_RX_PACKET_POOL_SIZE;

    // Allocate decode buffer (for FEC recovery)
    m_decode_buffer_size = WFB_RX_MAX_PACKET_SIZE * 32;  // Max fragments
#ifdef ESP_PLATFORM
    if (use_psram) {
        m_decode_buffer = (uint8_t*)heap_caps_malloc(
            m_decode_buffer_size, MALLOC_CAP_SPIRAM);
    } else {
        m_decode_buffer = (uint8_t*)heap_caps_malloc(
            m_decode_buffer_size, MALLOC_CAP_8BIT);
    }
#else
    m_decode_buffer = new uint8_t[m_decode_buffer_size];
#endif
    if (!m_decode_buffer) {
        LOG_E("Failed to allocate decode buffer");
        deinit();
        return false;
    }

    // Reset blocks
    for (size_t i = 0; i < WFB_RX_MAX_BLOCKS; i++) {
        m_blocks[i].reset();
    }

    m_stats.reset();
    m_last_process_time_ms = get_time_ms();
    m_initialized = true;

    LOG_I("WFB-NG receiver initialized");
    return true;
}

void WfbReceiver::deinit() {
    if (!m_initialized) return;

    // Free FEC context
    if (m_fec_ctx) {
        fec_free((fec_t*)m_fec_ctx);
        m_fec_ctx = nullptr;
    }

    // Free packet pool
    for (size_t i = 0; i < WFB_RX_PACKET_POOL_SIZE; i++) {
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

    // Free decode buffer
    if (m_decode_buffer) {
#ifdef ESP_PLATFORM
        heap_caps_free(m_decode_buffer);
#else
        delete[] m_decode_buffer;
#endif
        m_decode_buffer = nullptr;
    }

    // Reset blocks
    for (size_t i = 0; i < WFB_RX_MAX_BLOCKS; i++) {
        m_blocks[i].reset();
    }

    m_initialized = false;
    LOG_I("WFB-NG receiver deinitialized");
}

// ============================================================================
// Key Management
// ============================================================================

bool WfbReceiver::load_key(const uint8_t* key_data) {
    if (!key_data) return false;
    return wfb_crypto_load_gs_key(&m_crypto, key_data);
}

void WfbReceiver::set_tx_public_key(const uint8_t* tx_public_key) {
    if (!tx_public_key) return;
    memcpy(m_tx_public_key, tx_public_key, WFB_CRYPTO_BOX_PUBLICKEYBYTES);
    m_tx_key_set = true;
}

// ============================================================================
// Buffer Pool
// ============================================================================

uint8_t* WfbReceiver::alloc_buffer() {
    if (m_pool_free_count == 0) {
        LOG_W("Packet pool exhausted");
        return nullptr;
    }
    m_pool_free_count--;
    size_t idx = m_pool_free_list[m_pool_free_count];
    return m_packet_pool[idx];
}

void WfbReceiver::free_buffer(uint8_t* buf) {
    if (!buf) return;

    for (size_t i = 0; i < WFB_RX_PACKET_POOL_SIZE; i++) {
        if (m_packet_pool[i] == buf) {
            m_pool_free_list[m_pool_free_count++] = i;
            return;
        }
    }
    LOG_W("Attempted to free unknown buffer");
}

// ============================================================================
// Callbacks
// ============================================================================

void WfbReceiver::set_data_callback(WfbDataCallback callback) {
    m_data_callback = callback;
}

void WfbReceiver::set_session_callback(WfbSessionCallback callback) {
    m_session_callback = callback;
}

// ============================================================================
// IEEE 802.11 Header Parsing
// ============================================================================

bool WfbReceiver::parse_ieee_header(const uint8_t* data, size_t len,
                                     const uint8_t** payload, size_t* payload_len,
                                     uint8_t* src_mac) {
    if (len < IEEE80211_HEADER_SIZE) {
        return false;
    }

    // Frame control
    uint16_t frame_ctrl = data[0] | (data[1] << 8);

    // Check if it's a data frame (type = 0x08)
    uint8_t frame_type = (frame_ctrl >> 2) & 0x03;
    uint8_t frame_subtype = (frame_ctrl >> 4) & 0x0F;

    if (frame_type != 0x02) {  // 0x02 = Data frame
        return false;
    }

    // Extract source MAC (Address 2, bytes 10-15)
    if (src_mac) {
        memcpy(src_mac, data + 10, 6);
    }

    // Check for WFB-NG magic in source MAC
    if (!wfb_is_wfb_mac(data + 10)) {
        return false;
    }

    *payload = data + IEEE80211_HEADER_SIZE;
    *payload_len = len - IEEE80211_HEADER_SIZE;

    return true;
}

// ============================================================================
// Packet Processing
// ============================================================================

bool WfbReceiver::process_packet(const uint8_t* data, size_t len,
                                  int8_t rssi, uint8_t channel) {
    if (!m_initialized || !data || len == 0) {
        return false;
    }

    m_stats.packets_received++;
    m_stats.last_rssi = rssi;

    // Parse IEEE 802.11 header
    const uint8_t* payload;
    size_t payload_len;
    uint8_t src_mac[6];

    if (!parse_ieee_header(data, len, &payload, &payload_len, src_mac)) {
        return false;
    }

    // Need at least packet type byte
    if (payload_len < 1) {
        return false;
    }

    uint8_t packet_type = payload[0];

    if (packet_type == WFB_PACKET_SESSION) {
        return process_session_packet(payload, payload_len);
    } else if (packet_type == WFB_PACKET_DATA) {
        return process_data_packet(payload, payload_len);
    }

    return false;
}

bool WfbReceiver::process_session_packet(const uint8_t* payload, size_t len) {
    m_stats.session_packets++;

    if (!m_tx_key_set) {
        LOG_D("Session packet received but TX public key not set");
        return false;
    }

    bool result = wfb_crypto_process_session(&m_crypto, payload, len, m_tx_public_key);

    if (result && m_session_callback) {
        m_session_callback(m_crypto.fec_k, m_crypto.fec_n, m_crypto.session_epoch);
    }

    // Recreate FEC context with new parameters if needed
    if (result && m_fec_ctx) {
        fec_free((fec_t*)m_fec_ctx);
        m_fec_ctx = fec_new(m_crypto.fec_k, m_crypto.fec_n);
    }

    return result;
}

bool WfbReceiver::process_data_packet(const uint8_t* payload, size_t len) {
    if (len < sizeof(wfb_block_hdr_t)) {
        return false;
    }

    const wfb_block_hdr_t* hdr = (const wfb_block_hdr_t*)payload;
    uint64_t data_nonce = hdr->data_nonce;
    uint64_t block_index = wfb_get_block_index(data_nonce);
    uint8_t fragment_index = wfb_get_fragment_index(data_nonce);

    const uint8_t* encrypted = payload + sizeof(wfb_block_hdr_t);
    size_t encrypted_len = len - sizeof(wfb_block_hdr_t);

    // Decrypt packet
    uint8_t* decrypt_buf = alloc_buffer();
    if (!decrypt_buf) {
        return false;
    }

    size_t decrypted_len;
    bool decrypt_ok = wfb_crypto_decrypt(&m_crypto, encrypted, encrypted_len,
                                          data_nonce, decrypt_buf, &decrypted_len);

    if (!decrypt_ok) {
        free_buffer(decrypt_buf);
        m_stats.packets_decrypt_failed++;
        return false;
    }

    m_stats.packets_decrypted++;

    // Parse decrypted packet header
    if (decrypted_len < sizeof(wfb_packet_hdr_t)) {
        free_buffer(decrypt_buf);
        return false;
    }

    const wfb_packet_hdr_t* pkt_hdr = (const wfb_packet_hdr_t*)decrypt_buf;
    uint16_t packet_size = wfb_get_packet_size(pkt_hdr);
    uint8_t flags = pkt_hdr->flags;

    // Find or create block
    WfbFecBlock* block = find_or_create_block(block_index);
    if (!block) {
        free_buffer(decrypt_buf);
        return false;
    }

    // Check if fragment already received
    if (block->has_fragment(fragment_index)) {
        free_buffer(decrypt_buf);
        return false;  // Duplicate
    }

    // Store fragment
    block->fragment_data[fragment_index] = decrypt_buf;
    block->fragment_size[fragment_index] = (uint16_t)decrypted_len;
    block->set_fragment(fragment_index);
    block->fragments_received++;

    LOG_D("Block %" PRIu64 " frag %u: %zu bytes (%u/%u)",
          block_index, fragment_index, decrypted_len,
          block->fragments_received, block->fec_k);

    // Try to decode block if we have enough fragments
    if (!block->is_decoded && block->can_decode()) {
        decode_block(block);
    }

    return true;
}

// ============================================================================
// FEC Block Management
// ============================================================================

WfbFecBlock* WfbReceiver::find_or_create_block(uint64_t block_index) {
    // Look for existing block
    for (size_t i = 0; i < WFB_RX_MAX_BLOCKS; i++) {
        if (m_blocks[i].is_active && m_blocks[i].block_index == block_index) {
            return &m_blocks[i];
        }
    }

    // Find free slot
    for (size_t i = 0; i < WFB_RX_MAX_BLOCKS; i++) {
        if (!m_blocks[i].is_active) {
            m_blocks[i].reset();
            m_blocks[i].block_index = block_index;
            m_blocks[i].is_active = true;
            m_blocks[i].first_packet_time_ms = get_time_ms();
            m_blocks[i].fec_k = m_crypto.fec_k;
            m_blocks[i].fec_n = m_crypto.fec_n;
            m_active_blocks++;
            LOG_D("Created block %" PRIu64, block_index);
            return &m_blocks[i];
        }
    }

    // No free slots - evict oldest
    uint32_t oldest_time = UINT32_MAX;
    size_t oldest_idx = 0;
    for (size_t i = 0; i < WFB_RX_MAX_BLOCKS; i++) {
        if (m_blocks[i].is_active && m_blocks[i].first_packet_time_ms < oldest_time) {
            oldest_time = m_blocks[i].first_packet_time_ms;
            oldest_idx = i;
        }
    }

    LOG_W("Block pool full, evicting block %" PRIu64, m_blocks[oldest_idx].block_index);
    release_block(&m_blocks[oldest_idx]);

    m_blocks[oldest_idx].reset();
    m_blocks[oldest_idx].block_index = block_index;
    m_blocks[oldest_idx].is_active = true;
    m_blocks[oldest_idx].first_packet_time_ms = get_time_ms();
    m_blocks[oldest_idx].fec_k = m_crypto.fec_k;
    m_blocks[oldest_idx].fec_n = m_crypto.fec_n;
    m_active_blocks++;

    return &m_blocks[oldest_idx];
}

void WfbReceiver::release_block(WfbFecBlock* block) {
    if (!block || !block->is_active) return;

    // Free all fragment buffers
    for (int i = 0; i < 64; i++) {
        if (block->fragment_data[i]) {
            free_buffer(block->fragment_data[i]);
            block->fragment_data[i] = nullptr;
        }
    }

    block->reset();
    if (m_active_blocks > 0) m_active_blocks--;
}

bool WfbReceiver::decode_block(WfbFecBlock* block) {
    if (!block || block->is_decoded) return false;

    // Create FEC context if needed
    if (!m_fec_ctx) {
        m_fec_ctx = fec_new(block->fec_k, block->fec_n);
        if (!m_fec_ctx) {
            LOG_E("Failed to create FEC context");
            m_stats.blocks_failed++;
            block->is_decoded = true;
            return false;
        }
    }

    if (block->is_complete()) {
        // All primary fragments present - no FEC recovery needed
        LOG_D("Block %" PRIu64 " complete, no FEC needed", block->block_index);
        m_stats.blocks_complete++;
    } else {
        // Need FEC recovery
        LOG_D("Block %" PRIu64 " needs FEC: %u fragments",
              block->block_index, block->fragments_received);

        // Build input arrays for FEC decode
        const uint8_t* src_ptrs[32];
        uint8_t* dst_ptrs[32];
        unsigned indices[32];

        size_t src_idx = 0;
        size_t dst_idx = 0;
        size_t fec_idx = block->fec_k;  // Start of FEC fragments

        for (size_t i = 0; i < block->fec_k; i++) {
            if (block->has_fragment(i)) {
                // Have this primary fragment
                src_ptrs[i] = block->fragment_data[i] + sizeof(wfb_packet_hdr_t);
                indices[i] = i;
            } else {
                // Missing - use FEC fragment
                while (fec_idx < block->fec_n && !block->has_fragment(fec_idx)) {
                    fec_idx++;
                }

                if (fec_idx >= block->fec_n) {
                    LOG_W("Not enough fragments for FEC");
                    m_stats.blocks_failed++;
                    block->is_decoded = true;
                    return false;
                }

                src_ptrs[i] = block->fragment_data[fec_idx] + sizeof(wfb_packet_hdr_t);
                indices[i] = fec_idx;
                dst_ptrs[dst_idx] = m_decode_buffer + (dst_idx * WFB_RX_MAX_PACKET_SIZE);
                dst_idx++;
                fec_idx++;
            }
        }

        // Get fragment size (assume all same size)
        size_t frag_size = 0;
        for (size_t i = 0; i < block->fec_n; i++) {
            if (block->fragment_data[i]) {
                frag_size = block->fragment_size[i] - sizeof(wfb_packet_hdr_t);
                break;
            }
        }

        if (frag_size == 0) {
            m_stats.blocks_failed++;
            block->is_decoded = true;
            return false;
        }

        // Perform FEC decode
        fec_decode((fec_t*)m_fec_ctx,
                   (const gf*const*)src_ptrs,
                   (gf*const*)dst_ptrs,
                   indices,
                   frag_size);

        m_stats.blocks_recovered++;
        m_stats.packets_fec_recovered += dst_idx;
    }

    block->is_decoded = true;
    output_block(block);

    return true;
}

void WfbReceiver::output_block(WfbFecBlock* block) {
    if (!block || !m_data_callback) return;

    // Output primary fragments in order
    for (size_t i = 0; i < block->fec_k; i++) {
        const uint8_t* frag_data = nullptr;
        size_t frag_len = 0;

        if (block->fragment_data[i]) {
            // Have original fragment
            const wfb_packet_hdr_t* hdr = (const wfb_packet_hdr_t*)block->fragment_data[i];
            frag_data = block->fragment_data[i] + sizeof(wfb_packet_hdr_t);
            frag_len = wfb_get_packet_size(hdr);
        }
        // Note: Recovered fragments would be in m_decode_buffer
        // This needs more complex tracking - simplified for now

        if (frag_data && frag_len > 0 && !(frag_data[-sizeof(wfb_packet_hdr_t)] & WFB_PACKET_FEC_ONLY)) {
            m_data_callback(frag_data, frag_len, 0);
            m_stats.frames_output++;
            m_stats.bytes_output += frag_len;
        }
    }

    release_block(block);
}

size_t WfbReceiver::expire_blocks(uint32_t current_time_ms) {
    size_t expired = 0;

    for (size_t i = 0; i < WFB_RX_MAX_BLOCKS; i++) {
        WfbFecBlock* block = &m_blocks[i];
        if (block->is_active && !block->is_decoded) {
            uint32_t age = current_time_ms - block->first_packet_time_ms;
            if (age > WFB_RX_BLOCK_TIMEOUT_MS) {
                LOG_D("Expiring block %" PRIu64 " (%u frags)",
                      block->block_index, block->fragments_received);
                m_stats.blocks_failed++;
                release_block(block);
                expired++;
            }
        }
    }

    return expired;
}

void WfbReceiver::process() {
    if (!m_initialized) return;

    uint32_t current_time = get_time_ms();
    expire_blocks(current_time);
    m_last_process_time_ms = current_time;
}

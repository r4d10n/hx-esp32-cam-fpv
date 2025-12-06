#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Block Manager for FEC Decoding
 *
 * Manages FEC blocks as they're being assembled from incoming packets.
 * Each block contains coding_k primary packets and up to (coding_n - coding_k) FEC packets.
 * A block can be decoded when at least coding_k packets are received (any combination).
 */

// Maximum supported FEC parameters
static constexpr uint8_t MAX_CODING_K = 16;
static constexpr uint8_t MAX_CODING_N = 32;
static constexpr uint8_t MAX_FEC_PACKETS = MAX_CODING_N - MAX_CODING_K;

// Maximum number of blocks to track simultaneously
static constexpr size_t MAX_BLOCKS_IN_FLIGHT = 8;

/**
 * Represents a single FEC block being assembled
 */
struct FecBlock {
    uint32_t index = 0;                 // Block index from packet header

    // Primary packet storage (indices 0 to k-1)
    uint8_t* primary_packets[MAX_CODING_K] = {nullptr};
    uint16_t primary_sizes[MAX_CODING_K] = {0};
    uint16_t primary_mask = 0;          // Bitmask of received primary packets
    uint8_t primary_count = 0;          // Count of received primary packets

    // FEC packet storage (indices k to n-1)
    uint8_t* fec_packets[MAX_FEC_PACKETS] = {nullptr};
    uint16_t fec_sizes[MAX_FEC_PACKETS] = {0};
    uint16_t fec_mask = 0;              // Bitmask of received FEC packets
    uint8_t fec_count = 0;              // Count of received FEC packets

    // Processing state
    bool is_active = false;             // Block is being assembled
    bool is_processed = false;          // Block has been decoded/dispatched
    uint32_t first_packet_time_ms = 0;  // Time first packet arrived

    void reset() {
        index = 0;
        for (int i = 0; i < MAX_CODING_K; i++) {
            primary_packets[i] = nullptr;
            primary_sizes[i] = 0;
        }
        for (int i = 0; i < MAX_FEC_PACKETS; i++) {
            fec_packets[i] = nullptr;
            fec_sizes[i] = 0;
        }
        primary_mask = 0;
        primary_count = 0;
        fec_mask = 0;
        fec_count = 0;
        is_active = false;
        is_processed = false;
        first_packet_time_ms = 0;
    }

    // Check if we have enough packets to decode
    bool can_decode(uint8_t coding_k) const {
        return (primary_count + fec_count) >= coding_k;
    }

    // Check if all primary packets are present (no FEC needed)
    bool is_complete(uint8_t coding_k) const {
        return primary_count >= coding_k;
    }

    // Get total packet count
    uint8_t total_packets() const {
        return primary_count + fec_count;
    }
};

/**
 * Block Manager class
 *
 * Manages a pool of FEC blocks and handles block lifecycle.
 */
class BlockManager {
public:
    BlockManager();
    ~BlockManager();

    /**
     * Initialize the block manager with FEC parameters
     *
     * @param coding_k Number of primary packets per block
     * @param coding_n Total packets per block (primary + FEC)
     * @param mtu Maximum packet payload size
     * @param use_psram Whether to allocate packet buffers in PSRAM
     * @return true if initialization succeeded
     */
    bool init(uint8_t coding_k, uint8_t coding_n, uint16_t mtu, bool use_psram = true);

    /**
     * Deinitialize and free all resources
     */
    void deinit();

    /**
     * Add a packet to the appropriate block
     *
     * @param block_index Block index from packet header
     * @param packet_index Packet index within block
     * @param data Packet payload data (will be copied)
     * @param size Size of packet payload
     * @return Pointer to the block, or nullptr if packet was dropped
     */
    FecBlock* add_packet(uint32_t block_index, uint8_t packet_index,
                         const uint8_t* data, uint16_t size);

    /**
     * Get a block by index (if it exists)
     *
     * @param block_index Block index to find
     * @return Pointer to block, or nullptr if not found
     */
    FecBlock* get_block(uint32_t block_index);

    /**
     * Release a block after processing
     *
     * @param block Block to release
     */
    void release_block(FecBlock* block);

    /**
     * Get blocks that are ready for decoding
     *
     * @param out_blocks Array to store block pointers
     * @param max_blocks Maximum blocks to return
     * @return Number of blocks ready
     */
    size_t get_ready_blocks(FecBlock** out_blocks, size_t max_blocks);

    /**
     * Expire old blocks that haven't completed
     *
     * @param current_time_ms Current time in milliseconds
     * @param timeout_ms Block timeout in milliseconds
     * @return Number of blocks expired
     */
    size_t expire_blocks(uint32_t current_time_ms, uint32_t timeout_ms);

    /**
     * Skip to a new block index (discard older blocks)
     *
     * @param new_block_index New minimum block index
     * @return Number of blocks skipped
     */
    size_t skip_to_block(uint32_t new_block_index);

    // Accessors
    uint8_t get_coding_k() const { return m_coding_k; }
    uint8_t get_coding_n() const { return m_coding_n; }
    uint16_t get_mtu() const { return m_mtu; }
    uint32_t get_next_expected_block() const { return m_next_expected_block; }
    size_t get_active_block_count() const { return m_active_count; }

private:
    // Allocate a packet buffer
    uint8_t* alloc_packet_buffer();
    void free_packet_buffer(uint8_t* buffer);

    // Find or create a block slot
    FecBlock* find_or_create_block(uint32_t block_index);

    uint8_t m_coding_k = 0;
    uint8_t m_coding_n = 0;
    uint16_t m_mtu = 0;
    bool m_use_psram = true;
    bool m_initialized = false;

    uint32_t m_next_expected_block = 0;
    size_t m_active_count = 0;

    // Block storage
    FecBlock m_blocks[MAX_BLOCKS_IN_FLIGHT];

    // Packet buffer pool
    static constexpr size_t PACKET_POOL_SIZE = MAX_BLOCKS_IN_FLIGHT * MAX_CODING_N;
    uint8_t* m_packet_pool[PACKET_POOL_SIZE] = {nullptr};
    size_t m_pool_free_count = 0;
    size_t m_pool_free_list[PACKET_POOL_SIZE] = {0};
};

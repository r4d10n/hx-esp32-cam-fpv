/**
 * FEC Decoder Test Application
 *
 * Tests the FEC decoder with captured WiFi packets from a PCAP file.
 *
 * Usage:
 *   fec_test <pcap_file> [options]
 *
 * Options:
 *   -k <num>    FEC coding_k parameter (default: 6)
 *   -n <num>    FEC coding_n parameter (default: 12)
 *   -m <num>    MTU size (default: 1464)
 *   -v          Verbose output
 *   -h          Show help
 *
 * Expected output for colorbars capture:
 *   Packets processed: 11406
 *   Primary packets: 8542
 *   FEC packets: 2864
 *   FEC blocks complete: 3233
 *   FEC blocks recovered: 664
 *   FEC blocks failed: 23
 *   Frames decoded: 481
 *   Frames incomplete: 78
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Include FEC decoder
// Note: On ESP32, these would be in components/gs_fec_decoder/include
// For desktop testing, we include them directly

// FEC library headers from common component
// Note: fec.h/fec.cpp is C++ despite having C-style interface
#include "../../components/common/fec.h"

// Local test headers
#include "pcap_parser.h"

// Simplified versions of the decoder structures for desktop testing
#include "../../components/gs_fec_decoder/include/decoder_stats.h"

// Define structures needed for standalone desktop testing
#pragma pack(push, 1)
struct Packet_Header_Test {
    uint8_t packet_version;
    uint8_t packet_signature;
    uint16_t fromDeviceId;
    uint16_t toDeviceId;
    uint16_t size;
    uint32_t block_index : 24;
    uint32_t packet_index : 8;
};
#pragma pack(pop)

#define PACKET_VERSION_TEST 2
#define PACKET_SIGNATURE_TEST 56
#define PACKET_OVERHEAD_TEST 12

// Test configuration
struct TestConfig {
    std::string pcap_file;
    uint8_t coding_k = 6;
    uint8_t coding_n = 12;
    uint16_t mtu = 1464;
    bool verbose = false;
};

// Simplified FEC decoder for standalone testing
class TestFecDecoder {
private:
    static constexpr int MAX_BLOCKS = 16;
    static constexpr int MAX_PACKETS_PER_BLOCK = 32;
    static constexpr int MAX_FEC = 16;

    struct Block {
        uint32_t index = 0;
        bool active = false;
        bool processed = false;
        uint16_t primary_mask = 0;
        uint16_t fec_mask = 0;
        uint8_t primary_count = 0;
        uint8_t fec_count = 0;
        uint8_t* packets[MAX_PACKETS_PER_BLOCK] = {nullptr};

        void reset() {
            index = 0;
            active = false;
            processed = false;
            primary_mask = 0;
            fec_mask = 0;
            primary_count = 0;
            fec_count = 0;
        }
    };

public:
    TestFecDecoder() {
        for (int i = 0; i < MAX_BLOCKS; i++) {
            m_blocks[i] = Block();
        }
    }

    ~TestFecDecoder() {
        deinit();
    }

    bool init(uint8_t coding_k, uint8_t coding_n, uint16_t mtu) {
        m_coding_k = coding_k;
        m_coding_n = coding_n;
        m_mtu = mtu;

        init_fec();
        m_fec = fec_new(coding_k, coding_n);
        if (!m_fec) return false;

        // Allocate packet buffers
        for (int i = 0; i < MAX_BLOCKS; i++) {
            for (int j = 0; j < MAX_PACKETS_PER_BLOCK; j++) {
                m_blocks[i].packets[j] = new uint8_t[mtu];
            }
        }

        // Allocate decode buffers
        m_decode_buffer = new uint8_t[mtu * coding_k];
        m_fec_src_ptrs = new uint8_t*[coding_k];
        m_fec_dst_ptrs = new uint8_t*[coding_k];

        m_initialized = true;
        return true;
    }

    void deinit() {
        if (m_fec) {
            fec_free(m_fec);
            m_fec = nullptr;
        }

        for (int i = 0; i < MAX_BLOCKS; i++) {
            for (int j = 0; j < MAX_PACKETS_PER_BLOCK; j++) {
                delete[] m_blocks[i].packets[j];
                m_blocks[i].packets[j] = nullptr;
            }
        }

        delete[] m_decode_buffer;
        delete[] m_fec_src_ptrs;
        delete[] m_fec_dst_ptrs;
        m_decode_buffer = nullptr;
        m_fec_src_ptrs = nullptr;
        m_fec_dst_ptrs = nullptr;

        m_initialized = false;
    }

    bool process_packet(const uint8_t* data, size_t size) {
        if (!m_initialized || size < PACKET_OVERHEAD_TEST) return false;

        const Packet_Header_Test* header = (const Packet_Header_Test*)data;

        // Validate header
        if (header->packet_version != PACKET_VERSION_TEST ||
            header->packet_signature != PACKET_SIGNATURE_TEST) {
            m_stats.invalid_packets++;
            return false;
        }

        m_stats.packets_processed++;
        m_stats.bytes_received += size;

        uint32_t block_index = header->block_index;
        uint8_t packet_index = header->packet_index;

        // Track packet types
        if (packet_index < m_coding_k) {
            m_stats.primary_packets++;
        } else {
            m_stats.fec_packets++;
        }

        // Find or create block
        Block* block = find_block(block_index);
        if (!block) {
            // All slots full with newer blocks - skip
            if (block_index < m_oldest_block_index) {
                return false;
            }
            // Expire oldest and use that slot
            block = expire_oldest_block();
            if (!block) return false;
            block->index = block_index;
            block->active = true;
        }

        // Check for duplicate
        if (packet_index < m_coding_k) {
            if (block->primary_mask & (1u << packet_index)) {
                m_stats.duplicate_packets++;
                return false;
            }
            // Store primary packet
            size_t payload_size = size - PACKET_OVERHEAD_TEST;
            memcpy(block->packets[packet_index], data + PACKET_OVERHEAD_TEST,
                   std::min(payload_size, (size_t)m_mtu));
            block->primary_mask |= (1u << packet_index);
            block->primary_count++;
        } else {
            uint8_t fec_idx = packet_index - m_coding_k;
            if (fec_idx >= MAX_FEC) {
                return false;
            }
            if (block->fec_mask & (1u << fec_idx)) {
                m_stats.duplicate_packets++;
                return false;
            }
            // Store FEC packet
            size_t payload_size = size - PACKET_OVERHEAD_TEST;
            memcpy(block->packets[m_coding_k + fec_idx], data + PACKET_OVERHEAD_TEST,
                   std::min(payload_size, (size_t)m_mtu));
            block->fec_mask |= (1u << fec_idx);
            block->fec_count++;
        }

        // Check if block can be decoded
        if (block->primary_count + block->fec_count >= m_coding_k && !block->processed) {
            decode_block(block);
        }

        return true;
    }

    void process_remaining() {
        // Try to decode any remaining blocks
        for (int i = 0; i < MAX_BLOCKS; i++) {
            Block* block = &m_blocks[i];
            if (block->active && !block->processed) {
                if (block->primary_count + block->fec_count >= m_coding_k) {
                    decode_block(block);
                } else {
                    // Incomplete block
                    m_stats.blocks_failed++;
                }
            }
        }
    }

    const FecDecoderStats& get_stats() const { return m_stats; }

    void print_stats() const {
        printf("\n=== Final Statistics ===\n");
        printf("Packets processed: %u\n", m_stats.packets_processed);
        printf("  Primary packets: %u\n", m_stats.primary_packets);
        printf("  FEC packets:     %u\n", m_stats.fec_packets);
        printf("FEC blocks complete:  %u\n", m_stats.blocks_complete);
        printf("FEC blocks recovered: %u\n", m_stats.blocks_recovered);
        printf("FEC blocks failed:    %u\n", m_stats.blocks_failed);
        printf("Frames decoded:    %u\n", m_stats.frames_decoded);
        printf("Frames incomplete: %u\n", m_stats.frames_incomplete);
        printf("========================\n\n");
    }

    Block* find_block(uint32_t index) {
        // Look for existing block
        for (int i = 0; i < MAX_BLOCKS; i++) {
            if (m_blocks[i].active && m_blocks[i].index == index) {
                return &m_blocks[i];
            }
        }
        // Look for free slot
        for (int i = 0; i < MAX_BLOCKS; i++) {
            if (!m_blocks[i].active) {
                m_blocks[i].reset();
                m_blocks[i].index = index;
                m_blocks[i].active = true;
                return &m_blocks[i];
            }
        }
        return nullptr;
    }

    Block* expire_oldest_block() {
        uint32_t oldest_idx = UINT32_MAX;
        Block* oldest = nullptr;

        for (int i = 0; i < MAX_BLOCKS; i++) {
            if (m_blocks[i].active && m_blocks[i].index < oldest_idx) {
                oldest_idx = m_blocks[i].index;
                oldest = &m_blocks[i];
            }
        }

        if (oldest) {
            if (!oldest->processed) {
                m_stats.blocks_failed++;
            }
            oldest->reset();
            m_oldest_block_index = oldest_idx + 1;
        }

        return oldest;
    }

    void decode_block(Block* block) {
        if (block->primary_count >= m_coding_k) {
            // All primary packets present - no FEC needed
            m_stats.blocks_complete++;
            dispatch_block(block);
        } else {
            // Need FEC decode
            unsigned indices[32];
            size_t primary_idx = 0;
            size_t fec_idx = 0;
            size_t dst_idx = 0;

            // Build source pointers
            for (size_t i = 0; i < m_coding_k; i++) {
                if (block->primary_mask & (1u << i)) {
                    m_fec_src_ptrs[i] = block->packets[i];
                    indices[i] = i;
                } else {
                    // Use FEC packet
                    while (fec_idx < MAX_FEC && !(block->fec_mask & (1u << fec_idx))) {
                        fec_idx++;
                    }
                    if (fec_idx < MAX_FEC) {
                        m_fec_src_ptrs[i] = block->packets[m_coding_k + fec_idx];
                        indices[i] = m_coding_k + fec_idx;
                        fec_idx++;
                    }
                    m_fec_dst_ptrs[dst_idx] = m_decode_buffer + (dst_idx * m_mtu);
                    dst_idx++;
                }
            }

            // Decode
            fec_decode(m_fec, (const gf*const*)m_fec_src_ptrs,
                      (gf*const*)m_fec_dst_ptrs, indices, m_mtu);

            // Copy recovered packets back
            dst_idx = 0;
            for (size_t i = 0; i < m_coding_k; i++) {
                if (!(block->primary_mask & (1u << i))) {
                    memcpy(block->packets[i], m_fec_dst_ptrs[dst_idx], m_mtu);
                    dst_idx++;
                }
            }

            m_stats.blocks_recovered++;
            dispatch_block(block);
        }

        block->processed = true;
    }

    void dispatch_block(Block* block) {
        // Process decoded packets for frame assembly
        for (size_t i = 0; i < m_coding_k; i++) {
            uint8_t* pkt = block->packets[i];
            if (!pkt) continue;

            // Check if this is a video packet (type == 0)
            if (pkt[0] == 0) {
                // Extract frame info
                // VideoPacketHeader: type(1) + size(4) + pong(1) + version(1) + crc(1)
                //                   + airDeviceId(2) + gsDeviceId(2) + resolution(1)
                //                   + part_info(1) + frame_index(4)

                if (m_mtu >= 18) {
                    uint8_t part_info = pkt[15];
                    uint8_t part_index = part_info & 0x7F;
                    bool is_last = (part_info & 0x80) != 0;
                    uint32_t frame_index;
                    memcpy(&frame_index, pkt + 14, 4);

                    // Simple frame tracking
                    if (frame_index != m_current_frame) {
                        if (m_current_frame_parts > 0 && !m_current_frame_complete) {
                            m_stats.frames_incomplete++;
                        }
                        m_current_frame = frame_index;
                        m_current_frame_parts = 0;
                        m_current_frame_complete = false;
                    }

                    m_current_frame_parts++;

                    if (is_last) {
                        m_stats.frames_decoded++;
                        m_current_frame_complete = true;
                    }
                }
            }

            m_stats.bytes_decoded += m_mtu;
        }
    }

    bool m_initialized = false;
    uint8_t m_coding_k = 6;
    uint8_t m_coding_n = 12;
    uint16_t m_mtu = 1464;

    fec_t* m_fec = nullptr;
    Block m_blocks[MAX_BLOCKS];

    uint8_t* m_decode_buffer = nullptr;
    uint8_t** m_fec_src_ptrs = nullptr;
    uint8_t** m_fec_dst_ptrs = nullptr;

    uint32_t m_oldest_block_index = 0;

    // Frame tracking
    uint32_t m_current_frame = 0;
    uint8_t m_current_frame_parts = 0;
    bool m_current_frame_complete = false;

    FecDecoderStats m_stats;
};

void print_usage(const char* program) {
    printf("FEC Decoder Test\n");
    printf("Usage: %s <pcap_file> [options]\n", program);
    printf("\nOptions:\n");
    printf("  -k <num>    FEC coding_k parameter (default: 6)\n");
    printf("  -n <num>    FEC coding_n parameter (default: 12)\n");
    printf("  -m <num>    MTU size (default: 1464)\n");
    printf("  -v          Verbose output\n");
    printf("  -h          Show this help\n");
    printf("\nExpected statistics for colorbars capture:\n");
    printf("  Packets processed: 11406\n");
    printf("  Primary packets: 8542\n");
    printf("  FEC packets: 2864\n");
    printf("  FEC blocks complete: 3233\n");
    printf("  FEC blocks recovered: 664\n");
    printf("  FEC blocks failed: 23\n");
    printf("  Frames decoded: 481\n");
    printf("  Frames incomplete: 78\n");
}

bool parse_args(int argc, char* argv[], TestConfig& config) {
    if (argc < 2) {
        print_usage(argv[0]);
        return false;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return false;
        } else if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            config.coding_k = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            config.coding_n = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config.mtu = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (argv[i][0] != '-') {
            config.pcap_file = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return false;
        }
    }

    if (config.pcap_file.empty()) {
        fprintf(stderr, "Error: No PCAP file specified\n");
        print_usage(argv[0]);
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    TestConfig config;

    if (!parse_args(argc, argv, config)) {
        return 1;
    }

    printf("FEC Decoder Test\n");
    printf("================\n");
    printf("PCAP file: %s\n", config.pcap_file.c_str());
    printf("FEC parameters: k=%d, n=%d, mtu=%d\n",
           config.coding_k, config.coding_n, config.mtu);

    // Open PCAP file
    PcapParser parser;
    if (!parser.open(config.pcap_file)) {
        fprintf(stderr, "Failed to open PCAP file\n");
        return 1;
    }

    // Filter by air unit MAC address
    uint8_t air_mac[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};
    parser.set_mac_filter(air_mac);

    // Initialize decoder
    TestFecDecoder decoder;
    if (!decoder.init(config.coding_k, config.coding_n, config.mtu)) {
        fprintf(stderr, "Failed to initialize decoder\n");
        return 1;
    }

    printf("\nProcessing packets...\n");

    // Process all packets
    size_t packet_count = 0;
    ParsedPacket packet;

    while (parser.read_packet(packet)) {
        if (packet.payload && packet.payload_size > 0) {
            decoder.process_packet(packet.payload, packet.payload_size);

            if (config.verbose && (packet_count % 1000 == 0)) {
                printf("  Processed %zu packets...\n", packet_count);
            }
        }
        packet_count++;
    }

    // Process any remaining incomplete blocks
    decoder.process_remaining();

    printf("Total packets in capture: %zu\n", packet_count);

    // Print final statistics
    decoder.print_stats();

    // Compare with expected values
    const FecDecoderStats& stats = decoder.get_stats();
    printf("Comparison with expected values:\n");
    printf("  Packets processed: %u (expected: 11406) %s\n",
           stats.packets_processed,
           stats.packets_processed == 11406 ? "OK" : "DIFF");
    printf("  Primary packets: %u (expected: 8542) %s\n",
           stats.primary_packets,
           stats.primary_packets == 8542 ? "OK" : "DIFF");
    printf("  FEC packets: %u (expected: 2864) %s\n",
           stats.fec_packets,
           stats.fec_packets == 2864 ? "OK" : "DIFF");

    return 0;
}

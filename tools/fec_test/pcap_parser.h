#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>
#include <functional>

/**
 * PCAP/PCAPNG Parser for FEC Decoder Testing
 *
 * Parses WiFi captures in PCAP and PCAPNG format and extracts
 * the raw 802.11 frame payloads for FEC decoding.
 */

// PCAP file magic numbers
static constexpr uint32_t PCAP_MAGIC_NATIVE = 0xa1b2c3d4;      // Native byte order
static constexpr uint32_t PCAP_MAGIC_SWAPPED = 0xd4c3b2a1;    // Swapped byte order
static constexpr uint32_t PCAP_MAGIC_NSEC_NATIVE = 0xa1b23c4d; // Nanosecond resolution
static constexpr uint32_t PCAP_MAGIC_NSEC_SWAPPED = 0x4d3cb2a1;

// PCAPNG section header magic
static constexpr uint32_t PCAPNG_MAGIC = 0x0a0d0d0a;
static constexpr uint32_t PCAPNG_BYTE_ORDER_MAGIC = 0x1a2b3c4d;

// Data Link Types
static constexpr uint32_t DLT_IEEE802_11 = 105;          // Raw 802.11
static constexpr uint32_t DLT_IEEE802_11_RADIO = 127;    // 802.11 + radiotap
static constexpr uint32_t DLT_PRISM_HEADER = 119;        // Prism header + 802.11

/**
 * PCAP global header structure
 */
#pragma pack(push, 1)
struct PcapGlobalHeader {
    uint32_t magic_number;
    uint16_t version_major;
    uint16_t version_minor;
    int32_t thiszone;
    uint32_t sigfigs;
    uint32_t snaplen;
    uint32_t network;          // DLT_* link type
};

/**
 * PCAP packet header structure
 */
struct PcapPacketHeader {
    uint32_t ts_sec;           // Timestamp seconds
    uint32_t ts_usec;          // Timestamp microseconds (or nanoseconds)
    uint32_t incl_len;         // Number of bytes captured
    uint32_t orig_len;         // Original packet length
};

/**
 * Radiotap header (variable length)
 */
struct RadiotapHeader {
    uint8_t it_version;        // Version (always 0)
    uint8_t it_pad;
    uint16_t it_len;           // Total header length
    uint32_t it_present;       // Bitmap of present fields
};
#pragma pack(pop)

/**
 * Parsed packet info
 */
struct ParsedPacket {
    uint64_t timestamp_us;     // Packet timestamp in microseconds
    int8_t rssi_dbm;           // Signal strength (if available)
    uint8_t channel;           // WiFi channel (if available)
    const uint8_t* payload;    // Pointer to 802.11 payload (after headers)
    size_t payload_size;       // Size of payload
    const uint8_t* raw_frame;  // Pointer to full 802.11 frame
    size_t raw_frame_size;     // Size of full frame
};

/**
 * Callback type for processed packets
 */
using PacketCallback = std::function<void(const ParsedPacket& packet)>;

/**
 * PCAP Parser class
 */
class PcapParser {
public:
    PcapParser();
    ~PcapParser();

    /**
     * Open a PCAP or PCAPNG file
     *
     * @param filename Path to the capture file
     * @return true if file opened successfully
     */
    bool open(const std::string& filename);

    /**
     * Open from memory buffer
     *
     * @param data Pointer to PCAP data
     * @param size Size of data
     * @return true if data is valid PCAP
     */
    bool open_buffer(const uint8_t* data, size_t size);

    /**
     * Close the file
     */
    void close();

    /**
     * Check if file is open
     */
    bool is_open() const { return m_file != nullptr || m_buffer != nullptr; }

    /**
     * Read and process next packet
     *
     * @param packet Output packet info
     * @return true if packet was read, false if EOF or error
     */
    bool read_packet(ParsedPacket& packet);

    /**
     * Process all packets with callback
     *
     * @param callback Function to call for each packet
     * @return Number of packets processed
     */
    size_t process_all(PacketCallback callback);

    /**
     * Reset to beginning of file
     */
    bool reset();

    // Accessors
    uint32_t get_link_type() const { return m_link_type; }
    bool is_byte_swapped() const { return m_swapped; }
    bool is_pcapng() const { return m_is_pcapng; }
    size_t get_packet_count() const { return m_packet_count; }

    /**
     * Filter packets by MAC address
     *
     * @param mac 6-byte MAC address to filter (nullptr to disable)
     */
    void set_mac_filter(const uint8_t* mac);

    /**
     * Set 802.11 header size (for calculating payload offset)
     *
     * @param size Header size in bytes (typically 24)
     */
    void set_ieee_header_size(size_t size) { m_ieee_header_size = size; }

private:
    bool read_pcap_header();
    bool read_pcapng_header();
    bool read_packet_internal(ParsedPacket& packet);
    size_t parse_radiotap_header(const uint8_t* data, size_t size,
                                  int8_t* out_rssi, uint8_t* out_channel);

    // Byte swapping helpers
    uint16_t swap16(uint16_t val) const;
    uint32_t swap32(uint32_t val) const;

    FILE* m_file = nullptr;
    const uint8_t* m_buffer = nullptr;
    size_t m_buffer_size = 0;
    size_t m_buffer_pos = 0;

    bool m_swapped = false;
    bool m_is_pcapng = false;
    bool m_is_nsec = false;
    uint32_t m_link_type = 0;

    size_t m_ieee_header_size = 24;
    size_t m_packet_count = 0;

    uint8_t m_mac_filter[6] = {0};
    bool m_use_mac_filter = false;

    // Packet buffer for reading
    std::vector<uint8_t> m_packet_buffer;
};

/**
 * Helper function to check if a file exists and get its size
 */
bool get_file_info(const std::string& filename, size_t* out_size = nullptr);

/**
 * Helper function to load entire file into memory
 */
std::vector<uint8_t> load_file(const std::string& filename);

#include "pcap_parser.h"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
#include <io.h>
#define stat _stat
#else
#include <sys/stat.h>
#endif

// Source MAC address we're looking for (from air unit)
// 0x11, 0x22, 0x33, 0x44, 0x55, 0x66
static constexpr uint8_t AIR_UNIT_MAC[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66};

// Position of source MAC in 802.11 data frame header
static constexpr size_t IEEE80211_SRC_MAC_OFFSET = 10;

PcapParser::PcapParser() {
    m_packet_buffer.reserve(65536); // Reserve space for large packets
}

PcapParser::~PcapParser() {
    close();
}

uint16_t PcapParser::swap16(uint16_t val) const {
    if (!m_swapped) return val;
    return ((val & 0xFF) << 8) | ((val >> 8) & 0xFF);
}

uint32_t PcapParser::swap32(uint32_t val) const {
    if (!m_swapped) return val;
    return ((val & 0xFF) << 24) |
           ((val & 0xFF00) << 8) |
           ((val >> 8) & 0xFF00) |
           ((val >> 24) & 0xFF);
}

bool PcapParser::open(const std::string& filename) {
    close();

    m_file = fopen(filename.c_str(), "rb");
    if (!m_file) {
        fprintf(stderr, "Failed to open file: %s\n", filename.c_str());
        return false;
    }

    // Read magic number to determine format
    uint32_t magic = 0;
    if (fread(&magic, sizeof(magic), 1, m_file) != 1) {
        fprintf(stderr, "Failed to read magic number\n");
        close();
        return false;
    }

    // Rewind to beginning
    fseek(m_file, 0, SEEK_SET);

    // Determine format
    if (magic == PCAP_MAGIC_NATIVE || magic == PCAP_MAGIC_NSEC_NATIVE) {
        m_swapped = false;
        m_is_nsec = (magic == PCAP_MAGIC_NSEC_NATIVE);
        return read_pcap_header();
    } else if (magic == PCAP_MAGIC_SWAPPED || magic == PCAP_MAGIC_NSEC_SWAPPED) {
        m_swapped = true;
        m_is_nsec = (magic == PCAP_MAGIC_NSEC_SWAPPED);
        return read_pcap_header();
    } else if (magic == PCAPNG_MAGIC) {
        m_is_pcapng = true;
        return read_pcapng_header();
    } else {
        fprintf(stderr, "Unknown file format (magic: 0x%08x)\n", magic);
        close();
        return false;
    }
}

bool PcapParser::open_buffer(const uint8_t* data, size_t size) {
    close();

    if (!data || size < sizeof(PcapGlobalHeader)) {
        return false;
    }

    m_buffer = data;
    m_buffer_size = size;
    m_buffer_pos = 0;

    // Read magic number
    uint32_t magic = *(const uint32_t*)data;

    if (magic == PCAP_MAGIC_NATIVE || magic == PCAP_MAGIC_NSEC_NATIVE) {
        m_swapped = false;
        m_is_nsec = (magic == PCAP_MAGIC_NSEC_NATIVE);
        return read_pcap_header();
    } else if (magic == PCAP_MAGIC_SWAPPED || magic == PCAP_MAGIC_NSEC_SWAPPED) {
        m_swapped = true;
        m_is_nsec = (magic == PCAP_MAGIC_NSEC_SWAPPED);
        return read_pcap_header();
    } else if (magic == PCAPNG_MAGIC) {
        m_is_pcapng = true;
        return read_pcapng_header();
    }

    return false;
}

void PcapParser::close() {
    if (m_file) {
        fclose(m_file);
        m_file = nullptr;
    }
    m_buffer = nullptr;
    m_buffer_size = 0;
    m_buffer_pos = 0;
    m_packet_count = 0;
    m_is_pcapng = false;
    m_swapped = false;
    m_is_nsec = false;
}

bool PcapParser::read_pcap_header() {
    PcapGlobalHeader header;

    if (m_file) {
        if (fread(&header, sizeof(header), 1, m_file) != 1) {
            return false;
        }
    } else if (m_buffer) {
        if (m_buffer_pos + sizeof(header) > m_buffer_size) {
            return false;
        }
        memcpy(&header, m_buffer + m_buffer_pos, sizeof(header));
        m_buffer_pos += sizeof(header);
    } else {
        return false;
    }

    m_link_type = swap32(header.network);

    printf("PCAP file: version %d.%d, link type %d, snaplen %d\n",
           swap16(header.version_major), swap16(header.version_minor),
           m_link_type, swap32(header.snaplen));

    return true;
}

bool PcapParser::read_pcapng_header() {
    // PCAPNG is more complex - for now, we'll handle the basic case
    // A full implementation would parse all block types

    fprintf(stderr, "PCAPNG support is limited - converting to basic parsing\n");

    // Skip section header block
    if (m_file) {
        // Read block length
        uint32_t block_type, block_len;
        if (fread(&block_type, 4, 1, m_file) != 1) return false;
        if (fread(&block_len, 4, 1, m_file) != 1) return false;

        // Skip to end of block
        fseek(m_file, block_len - 12, SEEK_CUR);

        // Read interface description block
        if (fread(&block_type, 4, 1, m_file) != 1) return false;
        if (fread(&block_len, 4, 1, m_file) != 1) return false;

        // Link type is at offset 0 in IDB
        uint16_t link_type;
        if (fread(&link_type, 2, 1, m_file) != 1) return false;
        m_link_type = link_type;

        // Skip rest of IDB
        fseek(m_file, block_len - 14, SEEK_CUR);

        printf("PCAPNG file: link type %d\n", m_link_type);
    }

    return true;
}

size_t PcapParser::parse_radiotap_header(const uint8_t* data, size_t size,
                                          int8_t* out_rssi, uint8_t* out_channel) {
    if (size < sizeof(RadiotapHeader)) {
        return 0;
    }

    const RadiotapHeader* rt = (const RadiotapHeader*)data;
    size_t rt_len = rt->it_len;

    if (rt_len > size) {
        return 0;
    }

    // Parse radiotap fields if present
    // This is a simplified parser - full implementation would handle all field types
    *out_rssi = -50;  // Default value
    *out_channel = 0;

    // Check for common fields
    uint32_t present = rt->it_present;

    // Field offsets depend on which fields are present
    // For now, just return the header length
    size_t offset = sizeof(RadiotapHeader);

    // Handle extended present bitmaps
    while (present & (1u << 31)) {
        if (offset + 4 > rt_len) break;
        present = *(const uint32_t*)(data + offset);
        offset += 4;
    }

    // Try to find RSSI (DBM_ANTSIGNAL = bit 5)
    // This is simplified - proper parsing requires tracking alignment
    for (size_t i = offset; i < rt_len - 1; i++) {
        // Look for reasonable RSSI value in expected range
        int8_t val = (int8_t)data[i];
        if (val >= -100 && val <= -10) {
            *out_rssi = val;
            break;
        }
    }

    return rt_len;
}

bool PcapParser::read_packet(ParsedPacket& packet) {
    return read_packet_internal(packet);
}

bool PcapParser::read_packet_internal(ParsedPacket& packet) {
    PcapPacketHeader pkt_hdr;

    // Read packet header
    if (m_is_pcapng) {
        // PCAPNG enhanced packet block
        uint32_t block_type, block_len;

        while (true) {
            if (m_file) {
                if (fread(&block_type, 4, 1, m_file) != 1) return false;
                if (fread(&block_len, 4, 1, m_file) != 1) return false;
            } else {
                if (m_buffer_pos + 8 > m_buffer_size) return false;
                block_type = *(const uint32_t*)(m_buffer + m_buffer_pos);
                block_len = *(const uint32_t*)(m_buffer + m_buffer_pos + 4);
                m_buffer_pos += 8;
            }

            // Enhanced Packet Block = 0x00000006
            if (block_type == 0x00000006) {
                // Skip interface ID and timestamp (high)
                uint32_t skip_data[3];
                if (m_file) {
                    fread(skip_data, 4, 3, m_file);
                } else {
                    m_buffer_pos += 12;
                }

                // Read captured length
                uint32_t cap_len, orig_len;
                if (m_file) {
                    fread(&cap_len, 4, 1, m_file);
                    fread(&orig_len, 4, 1, m_file);
                } else {
                    cap_len = *(const uint32_t*)(m_buffer + m_buffer_pos);
                    orig_len = *(const uint32_t*)(m_buffer + m_buffer_pos + 4);
                    m_buffer_pos += 8;
                }

                pkt_hdr.incl_len = cap_len;
                pkt_hdr.orig_len = orig_len;
                pkt_hdr.ts_sec = 0;
                pkt_hdr.ts_usec = 0;
                break;
            } else {
                // Skip other block types
                if (m_file) {
                    fseek(m_file, block_len - 12, SEEK_CUR);
                } else {
                    m_buffer_pos += block_len - 12;
                }
            }
        }
    } else {
        // Regular PCAP
        if (m_file) {
            if (fread(&pkt_hdr, sizeof(pkt_hdr), 1, m_file) != 1) {
                return false;
            }
        } else {
            if (m_buffer_pos + sizeof(pkt_hdr) > m_buffer_size) {
                return false;
            }
            memcpy(&pkt_hdr, m_buffer + m_buffer_pos, sizeof(pkt_hdr));
            m_buffer_pos += sizeof(pkt_hdr);
        }
    }

    // Swap if needed
    uint32_t incl_len = swap32(pkt_hdr.incl_len);
    uint32_t ts_sec = swap32(pkt_hdr.ts_sec);
    uint32_t ts_usec = swap32(pkt_hdr.ts_usec);

    if (incl_len > 65535) {
        fprintf(stderr, "Invalid packet length: %u\n", incl_len);
        return false;
    }

    // Resize buffer if needed
    if (m_packet_buffer.size() < incl_len) {
        m_packet_buffer.resize(incl_len);
    }

    // Read packet data
    if (m_file) {
        if (fread(m_packet_buffer.data(), 1, incl_len, m_file) != incl_len) {
            return false;
        }
    } else {
        if (m_buffer_pos + incl_len > m_buffer_size) {
            return false;
        }
        memcpy(m_packet_buffer.data(), m_buffer + m_buffer_pos, incl_len);
        m_buffer_pos += incl_len;
    }

    // Handle PCAPNG padding
    if (m_is_pcapng) {
        // Packets are padded to 4-byte boundary
        size_t padding = (4 - (incl_len & 3)) & 3;
        uint32_t trailing_len;
        if (m_file) {
            fseek(m_file, padding, SEEK_CUR);
            fread(&trailing_len, 4, 1, m_file);
        } else {
            m_buffer_pos += padding + 4;
        }
    }

    // Parse based on link type
    size_t header_offset = 0;
    int8_t rssi = -50;
    uint8_t channel = 0;

    if (m_link_type == DLT_IEEE802_11_RADIO) {
        // Parse radiotap header
        header_offset = parse_radiotap_header(m_packet_buffer.data(), incl_len, &rssi, &channel);
    } else if (m_link_type == DLT_PRISM_HEADER) {
        // Prism header is typically 144 bytes
        header_offset = 144;
    }

    // Check for valid 802.11 frame
    if (header_offset + m_ieee_header_size > incl_len) {
        return read_packet_internal(packet); // Skip invalid packet, read next
    }

    const uint8_t* frame_start = m_packet_buffer.data() + header_offset;
    size_t frame_len = incl_len - header_offset;

    // MAC filtering
    if (m_use_mac_filter) {
        if (frame_len >= IEEE80211_SRC_MAC_OFFSET + 6) {
            if (memcmp(frame_start + IEEE80211_SRC_MAC_OFFSET, m_mac_filter, 6) != 0) {
                return read_packet_internal(packet); // Skip, read next
            }
        }
    }

    // Fill in packet info
    packet.timestamp_us = (uint64_t)ts_sec * 1000000ULL + ts_usec;
    packet.rssi_dbm = rssi;
    packet.channel = channel;
    packet.raw_frame = frame_start;
    packet.raw_frame_size = frame_len;

    // Calculate payload offset (skip 802.11 header)
    if (frame_len > m_ieee_header_size) {
        packet.payload = frame_start + m_ieee_header_size;
        packet.payload_size = frame_len - m_ieee_header_size;
    } else {
        packet.payload = nullptr;
        packet.payload_size = 0;
    }

    m_packet_count++;
    return true;
}

size_t PcapParser::process_all(PacketCallback callback) {
    ParsedPacket packet;
    size_t count = 0;

    while (read_packet(packet)) {
        callback(packet);
        count++;
    }

    return count;
}

bool PcapParser::reset() {
    if (m_file) {
        fseek(m_file, 0, SEEK_SET);
        m_packet_count = 0;

        if (m_is_pcapng) {
            return read_pcapng_header();
        } else {
            return read_pcap_header();
        }
    } else if (m_buffer) {
        m_buffer_pos = 0;
        m_packet_count = 0;

        if (m_is_pcapng) {
            return read_pcapng_header();
        } else {
            return read_pcap_header();
        }
    }

    return false;
}

void PcapParser::set_mac_filter(const uint8_t* mac) {
    if (mac) {
        memcpy(m_mac_filter, mac, 6);
        m_use_mac_filter = true;
    } else {
        m_use_mac_filter = false;
    }
}

// Helper functions
bool get_file_info(const std::string& filename, size_t* out_size) {
    struct stat st;
    if (stat(filename.c_str(), &st) != 0) {
        return false;
    }
    if (out_size) {
        *out_size = st.st_size;
    }
    return true;
}

std::vector<uint8_t> load_file(const std::string& filename) {
    std::vector<uint8_t> result;

    FILE* f = fopen(filename.c_str(), "rb");
    if (!f) return result;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size > 0) {
        result.resize(size);
        fread(result.data(), 1, size, f);
    }

    fclose(f);
    return result;
}

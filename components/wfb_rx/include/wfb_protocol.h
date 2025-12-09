/**
 * @file wfb_protocol.h
 * @brief WFB-NG (WiFiBroadcast Next Generation) Protocol Definitions
 *
 * This file contains packet structures and constants for WFB-NG protocol
 * compatibility. Based on https://github.com/svpcom/wfb-ng
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Protocol Constants
// ============================================================================

#define WFB_PACKET_DATA         0x01
#define WFB_PACKET_SESSION      0x02

#define WFB_FEC_VDM_RS          0x01    // Reed-Solomon FEC

#define WFB_WIFI_MTU            4045    // Max injected packet size
#define WFB_MAX_PAYLOAD         3976    // Max payload after headers

// Crypto constants (libsodium compatible)
#define WFB_CRYPTO_BOX_NONCEBYTES       24
#define WFB_CRYPTO_BOX_PUBLICKEYBYTES   32
#define WFB_CRYPTO_BOX_SECRETKEYBYTES   32
#define WFB_CRYPTO_AEAD_KEYBYTES        32
#define WFB_CRYPTO_AEAD_NPUBBYTES       8
#define WFB_CRYPTO_AEAD_ABYTES          16  // Auth tag size

#define WFB_SESSION_KEY_ANNOUNCE_MSEC   1000

#define WFB_MAX_RX_INTERFACES   8
#define WFB_RX_ANT_MAX          4

// Default FEC parameters
#define WFB_DEFAULT_FEC_K       8
#define WFB_DEFAULT_FEC_N       12

// ============================================================================
// Packet Structures
// ============================================================================

/**
 * @brief WFB-NG Block/Data packet header (9 bytes)
 *
 * This header is present on all data packets after the IEEE 802.11 header.
 * The data_nonce encodes block_index and fragment_index:
 *   data_nonce = (block_index << 8) | fragment_index
 */
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;       // WFB_PACKET_DATA or WFB_PACKET_SESSION
    uint64_t data_nonce;        // Block/fragment encoding, also used as AEAD nonce
} wfb_block_hdr_t;

/**
 * @brief WFB-NG Session key packet header (33 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  packet_type;                               // WFB_PACKET_SESSION
    uint8_t  session_nonce[WFB_CRYPTO_BOX_NONCEBYTES]; // 24 bytes
} wfb_session_hdr_t;

/**
 * @brief WFB-NG Session data (encrypted, inside session packet)
 *
 * This is encrypted with the ground station's public key.
 */
typedef struct __attribute__((packed)) {
    uint64_t epoch;                                 // Session epoch for replay protection
    uint32_t channel_id;                            // Link/channel identifier
    uint8_t  fec_type;                              // WFB_FEC_VDM_RS
    uint8_t  fec_k;                                 // Data fragments per block
    uint8_t  fec_n;                                 // Total fragments per block
    uint8_t  session_key[WFB_CRYPTO_AEAD_KEYBYTES]; // 32 bytes - ChaCha20 key
    // Optional TLV data follows
} wfb_session_data_t;

/**
 * @brief WFB-NG Packet header (inside encrypted data, 3 bytes)
 *
 * Present after decryption of data packets.
 */
typedef struct __attribute__((packed)) {
    uint8_t  flags;             // Bit 0: FEC_ONLY packet flag
    uint16_t packet_size;       // Payload size (big-endian)
} wfb_packet_hdr_t;

#define WFB_PACKET_FEC_ONLY     0x01

/**
 * @brief RX antenna/signal info per packet
 */
typedef struct __attribute__((packed)) {
    uint8_t  wlan_idx;                          // WiFi interface index
    uint8_t  antenna[WFB_RX_ANT_MAX];           // Antenna indices
    int8_t   rssi[WFB_RX_ANT_MAX];              // RSSI per antenna
    int8_t   noise[WFB_RX_ANT_MAX];             // Noise floor per antenna
    uint16_t freq;                              // Frequency in MHz
    uint8_t  mcs_index;                         // MCS rate index
    uint8_t  bandwidth;                         // Bandwidth (20/40 MHz)
} wfb_rx_info_t;

// ============================================================================
// Inline Helper Functions
// ============================================================================

/**
 * @brief Extract block index from data_nonce
 */
static inline uint64_t wfb_get_block_index(uint64_t data_nonce) {
    return data_nonce >> 8;
}

/**
 * @brief Extract fragment index from data_nonce
 */
static inline uint8_t wfb_get_fragment_index(uint64_t data_nonce) {
    return (uint8_t)(data_nonce & 0xFF);
}

/**
 * @brief Create data_nonce from block and fragment indices
 */
static inline uint64_t wfb_make_nonce(uint64_t block_index, uint8_t fragment_index) {
    return (block_index << 8) | fragment_index;
}

/**
 * @brief Convert 16-bit big-endian to host byte order
 */
static inline uint16_t wfb_be16_to_host(uint16_t be_val) {
    return ((be_val & 0xFF) << 8) | ((be_val >> 8) & 0xFF);
}

/**
 * @brief Get packet size from wfb_packet_hdr_t (handles big-endian)
 */
static inline uint16_t wfb_get_packet_size(const wfb_packet_hdr_t* hdr) {
    return wfb_be16_to_host(hdr->packet_size);
}

// ============================================================================
// Channel ID / MAC Address Encoding
// ============================================================================

/**
 * @brief WFB-NG MAC address format: W:B:X:X:X:X
 *
 * The channel_id is encoded in the MAC address as:
 *   channel_id = (link_id << 8) | radio_port
 *
 * link_id: identifies the link (0-255)
 * radio_port: identifies the stream (0-255)
 */

/**
 * @brief Extract channel_id from 802.11 source MAC address
 */
static inline uint32_t wfb_get_channel_id(const uint8_t* mac) {
    // MAC format: 0x57 0x42 [link_id_hi] [link_id_lo] [radio_port_hi] [radio_port_lo]
    // But simplified: last 4 bytes encode channel_id
    return ((uint32_t)mac[2] << 24) | ((uint32_t)mac[3] << 16) |
           ((uint32_t)mac[4] << 8) | mac[5];
}

/**
 * @brief Check if MAC address matches WFB-NG magic bytes
 */
static inline int wfb_is_wfb_mac(const uint8_t* mac) {
    return (mac[0] == 0x57 && mac[1] == 0x42); // "WB"
}

#ifdef __cplusplus
}
#endif

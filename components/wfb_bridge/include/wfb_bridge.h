/**
 * @file wfb_bridge.h
 * @brief Bridge between WFB-NG receiver and WebSocket video server
 *
 * This component:
 * 1. Receives decoded video data from WFB-NG receiver
 * 2. Parses RTP/H.264/H.265 streams to extract NAL units
 * 3. Sends NAL units to browser clients via WebSocket
 *
 * Supports:
 * - H.264 (AVC) from WFB-NG video streams
 * - H.265 (HEVC) from WFB-NG video streams
 * - Automatic codec detection and configuration
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Bridge configuration
 */
typedef struct {
    const char* gs_key_path;    // Path to ground station key file (gs.key)
    uint16_t ws_port;           // WebSocket server port (default: 8080)
    uint8_t  channel_id;        // WFB-NG channel to listen on (default: 0)
    bool     auto_detect_codec; // Auto-detect codec from stream (default: true)
} wfb_bridge_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint32_t packets_received;
    uint32_t packets_decrypted;
    uint32_t fec_recovered;
    uint32_t frames_extracted;
    uint32_t keyframes;
    uint32_t clients_connected;
    int8_t   rssi;
    uint8_t  snr;
} wfb_bridge_stats_t;

/**
 * @brief Initialize the WFB bridge
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int wfb_bridge_init(const wfb_bridge_config_t* config);

/**
 * @brief Start the bridge (starts WFB receiver and WebSocket server)
 *
 * @return 0 on success, negative error code on failure
 */
int wfb_bridge_start(void);

/**
 * @brief Stop the bridge
 */
void wfb_bridge_stop(void);

/**
 * @brief Process a raw WiFi packet
 *
 * Called by the WiFi driver when a packet is received in monitor mode.
 *
 * @param data Raw packet data (including IEEE 802.11 header)
 * @param len Packet length
 * @param rssi RSSI value
 * @param noise Noise floor
 */
void wfb_bridge_process_packet(const uint8_t* data, size_t len,
                               int8_t rssi, int8_t noise);

/**
 * @brief Get bridge statistics
 *
 * @param stats Output statistics structure
 */
void wfb_bridge_get_stats(wfb_bridge_stats_t* stats);

/**
 * @brief Set WiFi channel
 *
 * @param channel WiFi channel number (1-14 for 2.4GHz, 36-165 for 5GHz)
 * @return 0 on success, negative error code on failure
 */
int wfb_bridge_set_channel(uint8_t channel);

#ifdef __cplusplus
}
#endif

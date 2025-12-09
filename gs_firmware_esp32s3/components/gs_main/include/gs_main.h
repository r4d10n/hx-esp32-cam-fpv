/**
 * @file gs_main.h
 * @brief ESP32-S3 Ground Station main component
 *
 * Integrates all ground station functionality:
 * - USB NCM network connectivity
 * - HTTP server with OTA and configuration
 * - WebSocket video streaming
 * - Unified receiver (WFB-NG + ESP32-FPV)
 * - Frame buffering and output
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Ground station configuration
 */
typedef struct {
    // Network
    uint16_t http_port;         // HTTP server port (default: 80)
    uint16_t ws_port;           // WebSocket port (default: 80, shared with HTTP)

    // WiFi
    uint8_t wifi_channel;       // Monitor mode channel (default: 7)

    // Receiver
    uint16_t device_id;         // ESP32-FPV device ID filter (0 = all)
    const char* gs_key_path;    // WFB-NG key path

    // FEC
    uint8_t fec_k;              // FEC K parameter (default: 6)
    uint8_t fec_n;              // FEC N parameter (default: 12)

    // Buffer
    uint16_t target_latency_ms; // Target latency (default: 100)
} gs_config_t;

/**
 * @brief Ground station statistics
 */
typedef struct {
    // Receiver
    uint32_t packets_received;
    uint32_t packets_valid;
    uint32_t packets_lost;
    uint32_t fec_recovered;

    // Frames
    uint32_t frames_received;
    uint32_t frames_output;
    uint32_t frames_dropped;
    uint32_t keyframes;

    // Performance
    float fps;
    uint32_t bitrate_kbps;
    uint16_t latency_ms;

    // Signal
    int8_t rssi;
    uint8_t snr;

    // Network
    uint32_t ws_clients;
    bool usb_connected;

    // Protocol
    uint8_t active_protocol;    // 0=unknown, 1=ESP32-FPV, 2=WFB-NG
    uint8_t active_codec;       // 0=unknown, 1=MJPEG, 2=H264, 3=H265
} gs_stats_t;

/**
 * @brief Initialize ground station
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int gs_init(const gs_config_t* config);

/**
 * @brief Start ground station
 *
 * Starts all services: USB NCM, HTTP server, WiFi receiver
 *
 * @return 0 on success, negative error code on failure
 */
int gs_start(void);

/**
 * @brief Stop ground station
 */
void gs_stop(void);

/**
 * @brief Get ground station statistics
 *
 * @param stats Output statistics
 */
void gs_get_stats(gs_stats_t* stats);

/**
 * @brief Set WiFi channel
 *
 * @param channel Channel number (1-14 for 2.4GHz)
 * @return 0 on success, negative error code on failure
 */
int gs_set_channel(uint8_t channel);

/**
 * @brief Get current WiFi channel
 *
 * @return Current channel number
 */
uint8_t gs_get_channel(void);

/**
 * @brief Set target latency
 *
 * @param latency_ms Latency in milliseconds
 */
void gs_set_latency(uint16_t latency_ms);

/**
 * @brief WiFi promiscuous mode callback (for use with esp_wifi)
 *
 * @param buf Packet buffer
 * @param type Packet type
 */
void gs_wifi_rx_callback(void* buf, int type);

#ifdef __cplusplus
}
#endif

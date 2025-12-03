/**
 * ESP32 FPV Ground Station - Packet RX Handler
 *
 * Processes incoming packets from promiscuous mode callback,
 * filters FPV packets, and extracts video frames.
 */

#ifndef PACKET_RX_H
#define PACKET_RX_H

#include "fpv_gs.h"

/**
 * Initialize packet RX subsystem
 * @return ESP_OK on success
 */
esp_err_t packet_rx_init(void);

/**
 * Handle incoming packet from promiscuous callback
 * Called from WiFi task context - must be fast!
 *
 * @param data Packet data (includes 802.11 header)
 * @param len Packet length
 * @param rssi Signal strength in dBm
 */
void packet_rx_handle(const uint8_t *data, size_t len, int8_t rssi);

/**
 * Start packet processing task
 * @return ESP_OK on success
 */
esp_err_t packet_rx_start(void);

/**
 * Stop packet processing
 */
void packet_rx_stop(void);

/**
 * Get current RSSI (averaged)
 * @return RSSI in dBm
 */
int8_t packet_rx_get_rssi(void);

#endif // PACKET_RX_H

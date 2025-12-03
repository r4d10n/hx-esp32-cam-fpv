/**
 * ESP32 FPV Ground Station - WiFi Manager
 *
 * Handles SoftAP and promiscuous mode for simultaneous
 * packet capture and web client serving.
 */

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "fpv_gs.h"
#include "esp_wifi.h"

/**
 * Initialize WiFi subsystem with SoftAP and promiscuous mode
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_init(void);

/**
 * Start WiFi (SoftAP + promiscuous)
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_start(void);

/**
 * Stop WiFi
 * @return ESP_OK on success
 */
esp_err_t wifi_manager_stop(void);

/**
 * Set WiFi channel (on the fly)
 * @param channel WiFi channel (1-14)
 * @return ESP_OK on success
 */
esp_err_t wifi_set_channel(uint8_t channel);

/**
 * Get current WiFi channel
 * @return Current channel number
 */
uint8_t wifi_get_channel(void);

/**
 * Enable/disable promiscuous mode
 * @param enable true to enable
 * @return ESP_OK on success
 */
esp_err_t wifi_set_promiscuous(bool enable);

/**
 * Get connected station count
 * @return Number of connected stations
 */
uint8_t wifi_get_station_count(void);

#endif // WIFI_MANAGER_H

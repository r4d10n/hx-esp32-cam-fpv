/**
 * ESP32 FPV Ground Station - Configuration Manager
 *
 * Handles persistent configuration storage in NVS.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "fpv_gs.h"

/**
 * Initialize configuration manager and load config from NVS
 * @return ESP_OK on success
 */
esp_err_t config_manager_init(void);

/**
 * Load configuration from NVS into global config
 * @return ESP_OK on success
 */
esp_err_t config_load(void);

/**
 * Save current configuration to NVS
 * @return ESP_OK on success
 */
esp_err_t config_save(void);

/**
 * Reset configuration to defaults
 * @return ESP_OK on success
 */
esp_err_t config_reset_defaults(void);

/**
 * Set WiFi channel (applies immediately)
 * @param channel WiFi channel (1-14)
 * @return ESP_OK on success
 */
esp_err_t config_set_channel(uint8_t channel);

/**
 * Set AP credentials (requires restart to apply)
 * @param ssid AP SSID
 * @param password AP password
 * @return ESP_OK on success
 */
esp_err_t config_set_ap_credentials(const char *ssid, const char *password);

/**
 * Get configuration as JSON string
 * @param buf Output buffer
 * @param buf_len Buffer length
 * @return Number of bytes written, or -1 on error
 */
int config_to_json(char *buf, size_t buf_len);

/**
 * Parse configuration from JSON string
 * @param json JSON string
 * @return ESP_OK on success
 */
esp_err_t config_from_json(const char *json);

#endif // CONFIG_MANAGER_H

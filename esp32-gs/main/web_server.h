/**
 * ESP32 FPV Ground Station - Web Server
 *
 * HTTP server for static files and WebSocket for MJPEG streaming.
 */

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "fpv_gs.h"

/**
 * Initialize web server
 * @return ESP_OK on success
 */
esp_err_t web_server_init(void);

/**
 * Start web server
 * @return ESP_OK on success
 */
esp_err_t web_server_start(void);

/**
 * Stop web server
 * @return ESP_OK on success
 */
esp_err_t web_server_stop(void);

/**
 * Get number of connected WebSocket clients
 * @return Client count
 */
uint8_t web_server_get_client_count(void);

#endif // WEB_SERVER_H

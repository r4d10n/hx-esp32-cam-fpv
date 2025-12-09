/**
 * @file usb_ncm.h
 * @brief USB NCM (Network Control Model) device for ESP32-S3
 *
 * Provides network connectivity over USB, allowing a host (phone/PC)
 * to connect to the ESP32-S3 ground station and access the web interface.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB NCM configuration
 */
typedef struct {
    const char* ip_addr;        // Device IP address (default: "192.168.7.1")
    const char* netmask;        // Network mask (default: "255.255.255.0")
    const char* gw_addr;        // Gateway (default: "192.168.7.1")
    const char* hostname;       // mDNS hostname (default: "esp32-fpv-gs")
} usb_ncm_config_t;

/**
 * @brief USB NCM statistics
 */
typedef struct {
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t bytes_sent;
    uint32_t bytes_received;
    bool connected;
} usb_ncm_stats_t;

/**
 * @brief Initialize USB NCM network device
 *
 * @param config Configuration (NULL for defaults)
 * @return 0 on success, negative error code on failure
 */
int usb_ncm_init(const usb_ncm_config_t* config);

/**
 * @brief Start USB NCM device
 *
 * @return 0 on success, negative error code on failure
 */
int usb_ncm_start(void);

/**
 * @brief Stop USB NCM device
 */
void usb_ncm_stop(void);

/**
 * @brief Check if USB host is connected
 *
 * @return true if connected, false otherwise
 */
bool usb_ncm_is_connected(void);

/**
 * @brief Get USB NCM statistics
 *
 * @param stats Output statistics structure
 */
void usb_ncm_get_stats(usb_ncm_stats_t* stats);

/**
 * @brief Get the device IP address string
 *
 * @return IP address string
 */
const char* usb_ncm_get_ip_addr(void);

#ifdef __cplusplus
}
#endif

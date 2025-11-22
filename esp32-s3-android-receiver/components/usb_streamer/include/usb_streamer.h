/**
 * @file usb_streamer.h
 * @brief USB Streamer for Android Connectivity (ESP32-S3)
 *
 * This component handles USB communication with Android devices.
 * Features:
 * - USB CDC (Communication Device Class) support
 * - USB Bulk transfer for high throughput
 * - Multiple stream support (video, telemetry, control)
 * - Flow control and backpressure handling
 * - Statistics and monitoring
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief USB stream type
 */
typedef enum {
    USB_STREAM_VIDEO = 0,      /**< Video data stream */
    USB_STREAM_TELEMETRY = 1,  /**< Telemetry data stream */
    USB_STREAM_CONTROL = 2,    /**< Control/command stream */
    USB_STREAM_DEBUG = 3       /**< Debug/logging stream */
} usb_stream_type_t;

/**
 * @brief USB transfer mode
 */
typedef enum {
    USB_MODE_CDC = 0,          /**< CDC ACM mode (serial) */
    USB_MODE_BULK = 1          /**< Bulk transfer mode */
} usb_transfer_mode_t;

/**
 * @brief USB connection status
 */
typedef enum {
    USB_STATUS_DISCONNECTED = 0,  /**< USB not connected */
    USB_STATUS_CONNECTED = 1,     /**< USB connected but not configured */
    USB_STATUS_CONFIGURED = 2,    /**< USB configured and ready */
    USB_STATUS_SUSPENDED = 3      /**< USB suspended */
} usb_connection_status_t;

/**
 * @brief USB streamer configuration
 */
typedef struct {
    usb_transfer_mode_t mode;      /**< Transfer mode */
    size_t tx_buffer_size;         /**< TX buffer size in bytes */
    size_t rx_buffer_size;         /**< RX buffer size in bytes */
    uint32_t tx_timeout_ms;        /**< TX timeout in milliseconds */
    bool enable_flow_control;      /**< Enable flow control */
} usb_streamer_config_t;

/**
 * @brief USB streamer statistics
 */
typedef struct {
    uint64_t bytes_sent;           /**< Total bytes sent */
    uint64_t bytes_received;       /**< Total bytes received */
    uint64_t packets_sent;         /**< Total packets sent */
    uint64_t packets_received;     /**< Total packets received */
    uint64_t tx_errors;            /**< TX errors count */
    uint64_t rx_errors;            /**< RX errors count */
    uint64_t packets_dropped;      /**< Packets dropped due to buffer full */
    float tx_throughput_mbps;      /**< TX throughput in Mbps */
    float rx_throughput_mbps;      /**< RX throughput in Mbps */
    uint32_t tx_buffer_usage;      /**< TX buffer usage percentage */
    uint32_t rx_buffer_usage;      /**< RX buffer usage percentage */
    usb_connection_status_t status; /**< Current USB status */
} usb_streamer_stats_t;

/**
 * @brief USB connection state callback
 *
 * Called when USB connection state changes.
 *
 * @param status New connection status
 * @param user_ctx User context pointer
 */
typedef void (*usb_connection_callback_t)(usb_connection_status_t status, void *user_ctx);

/**
 * @brief USB data received callback
 *
 * Called when data is received from Android.
 *
 * @param stream_type Stream type
 * @param data Received data
 * @param data_len Data length
 * @param user_ctx User context pointer
 */
typedef void (*usb_rx_callback_t)(
    usb_stream_type_t stream_type,
    const uint8_t *data,
    size_t data_len,
    void *user_ctx
);

/**
 * @brief Initialize USB streamer
 *
 * @param config Streamer configuration
 * @param conn_callback Connection state callback (optional)
 * @param rx_callback Data received callback (optional)
 * @param callback_ctx User context for callbacks
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_ARG if config is invalid
 *         ESP_ERR_NO_MEM if memory allocation fails
 */
esp_err_t usb_streamer_init(
    const usb_streamer_config_t *config,
    usb_connection_callback_t conn_callback,
    usb_rx_callback_t rx_callback,
    void *callback_ctx
);

/**
 * @brief Deinitialize USB streamer
 *
 * @return ESP_OK on success
 */
esp_err_t usb_streamer_deinit(void);

/**
 * @brief Send data via USB
 *
 * Sends data to Android device. May block if TX buffer is full.
 *
 * @param stream_type Stream type
 * @param data Data to send
 * @param data_len Data length
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if USB not connected
 *         ESP_ERR_TIMEOUT if timeout occurred
 *         ESP_ERR_NO_MEM if buffer full
 */
esp_err_t usb_streamer_send(
    usb_stream_type_t stream_type,
    const uint8_t *data,
    size_t data_len
);

/**
 * @brief Send data via USB (non-blocking)
 *
 * Attempts to send data without blocking. Returns immediately if buffer is full.
 *
 * @param stream_type Stream type
 * @param data Data to send
 * @param data_len Data length
 * @param bytes_sent Pointer to store actual bytes sent (optional)
 * @return ESP_OK on success
 *         ESP_ERR_INVALID_STATE if USB not connected
 *         ESP_ERR_NO_MEM if buffer full
 */
esp_err_t usb_streamer_send_nonblocking(
    usb_stream_type_t stream_type,
    const uint8_t *data,
    size_t data_len,
    size_t *bytes_sent
);

/**
 * @brief Get USB connection status
 *
 * @param status Pointer to store status
 * @return ESP_OK on success
 */
esp_err_t usb_streamer_get_status(usb_connection_status_t *status);

/**
 * @brief Check if USB is ready for transmission
 *
 * @return true if USB is configured and ready
 */
bool usb_streamer_is_ready(void);

/**
 * @brief Get available TX buffer space
 *
 * @param available Pointer to store available bytes
 * @return ESP_OK on success
 */
esp_err_t usb_streamer_get_tx_available(size_t *available);

/**
 * @brief Flush TX buffer
 *
 * Ensures all pending data is sent.
 *
 * @param timeout_ms Maximum time to wait (0 = no wait)
 * @return ESP_OK on success
 *         ESP_ERR_TIMEOUT if timeout occurred
 */
esp_err_t usb_streamer_flush(uint32_t timeout_ms);

/**
 * @brief Get streamer statistics
 *
 * @param stats Pointer to statistics structure
 * @return ESP_OK on success
 */
esp_err_t usb_streamer_get_stats(usb_streamer_stats_t *stats);

/**
 * @brief Reset streamer statistics
 *
 * @return ESP_OK on success
 */
esp_err_t usb_streamer_reset_stats(void);

/**
 * @brief Set flow control state
 *
 * Enables or disables flow control (backpressure).
 *
 * @param enable True to enable flow control
 * @return ESP_OK on success
 */
esp_err_t usb_streamer_set_flow_control(bool enable);

#ifdef __cplusplus
}
#endif

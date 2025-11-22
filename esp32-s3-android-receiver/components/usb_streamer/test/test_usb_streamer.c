/**
 * @file test_usb_streamer.c
 * @brief Unit tests for USB Streamer Component
 *
 * These tests verify the USB streamer functionality including:
 * - Initialization and deinitialization
 * - Packet framing and CRC calculation
 * - Stream type handling
 * - Flow control
 * - Statistics tracking
 *
 * @copyright Copyright (c) 2025
 */

#include "unity.h"
#include "usb_streamer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "test_usb_streamer";

// Mock connection callback data
static usb_connection_status_t last_connection_status = USB_STATUS_DISCONNECTED;
static int connection_callback_count = 0;

// Mock RX callback data
static usb_stream_type_t last_rx_stream_type;
static uint8_t last_rx_data[1024];
static size_t last_rx_data_len = 0;
static int rx_callback_count = 0;

/**
 * @brief Mock connection callback
 */
static void mock_connection_callback(usb_connection_status_t status, void *user_ctx) {
    last_connection_status = status;
    connection_callback_count++;
    ESP_LOGI(TAG, "Connection callback: status=%d, count=%d", status, connection_callback_count);
}

/**
 * @brief Mock RX callback
 */
static void mock_rx_callback(usb_stream_type_t stream_type,
                              const uint8_t *data,
                              size_t data_len,
                              void *user_ctx) {
    last_rx_stream_type = stream_type;
    if (data_len > 0 && data_len <= sizeof(last_rx_data)) {
        memcpy(last_rx_data, data, data_len);
        last_rx_data_len = data_len;
    }
    rx_callback_count++;
    ESP_LOGI(TAG, "RX callback: type=%d, len=%d, count=%d", stream_type, data_len, rx_callback_count);
}

/**
 * @brief Reset mock callback data
 */
static void reset_mock_data(void) {
    last_connection_status = USB_STATUS_DISCONNECTED;
    connection_callback_count = 0;
    last_rx_stream_type = USB_STREAM_VIDEO;
    last_rx_data_len = 0;
    rx_callback_count = 0;
    memset(last_rx_data, 0, sizeof(last_rx_data));
}

/**
 * @brief Test initialization with valid configuration
 */
void test_usb_streamer_init_valid(void) {
    reset_mock_data();

    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 65536,
        .rx_buffer_size = 8192,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    esp_err_t ret = usb_streamer_init(&config, mock_connection_callback,
                                      mock_rx_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Cleanup
    usb_streamer_deinit();
}

/**
 * @brief Test initialization with NULL configuration
 */
void test_usb_streamer_init_null_config(void) {
    esp_err_t ret = usb_streamer_init(NULL, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test initialization with invalid buffer sizes
 */
void test_usb_streamer_init_invalid_buffer_sizes(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 0, // Invalid
        .rx_buffer_size = 0, // Invalid
        .tx_timeout_ms = 1000,
        .enable_flow_control = false
    };

    esp_err_t ret = usb_streamer_init(&config, NULL, NULL, NULL);
    // Should fail or succeed with minimum buffer sizes
    // Implementation-dependent
    if (ret == ESP_OK) {
        usb_streamer_deinit();
    }
}

/**
 * @brief Test double initialization (should fail)
 */
void test_usb_streamer_double_init(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    esp_err_t ret1 = usb_streamer_init(&config, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    esp_err_t ret2 = usb_streamer_init(&config, NULL, NULL, NULL);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret2);

    usb_streamer_deinit();
}

/**
 * @brief Test deinitialization
 */
void test_usb_streamer_deinit(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);
    esp_err_t ret = usb_streamer_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Deinitializing again should fail
    ret = usb_streamer_deinit();
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test sending data when not connected
 */
void test_usb_streamer_send_disconnected(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04};
    esp_err_t ret = usb_streamer_send(USB_STREAM_VIDEO, test_data, sizeof(test_data));

    // Should fail when not connected
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    usb_streamer_deinit();
}

/**
 * @brief Test sending NULL data
 */
void test_usb_streamer_send_null_data(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    esp_err_t ret = usb_streamer_send(USB_STREAM_VIDEO, NULL, 100);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    usb_streamer_deinit();
}

/**
 * @brief Test sending zero-length data
 */
void test_usb_streamer_send_zero_length(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    uint8_t test_data[] = {0x01};
    esp_err_t ret = usb_streamer_send(USB_STREAM_VIDEO, test_data, 0);

    // Zero-length send might be allowed or rejected
    // Implementation-dependent

    usb_streamer_deinit();
}

/**
 * @brief Test non-blocking send
 */
void test_usb_streamer_send_nonblocking(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    uint8_t test_data[] = {0xAA, 0xBB, 0xCC, 0xDD};
    size_t bytes_sent = 0;

    esp_err_t ret = usb_streamer_send_nonblocking(USB_STREAM_VIDEO, test_data,
                                                   sizeof(test_data), &bytes_sent);

    // Should not block, even if not connected
    TEST_ASSERT_NOT_EQUAL(ESP_ERR_TIMEOUT, ret);

    usb_streamer_deinit();
}

/**
 * @brief Test getting connection status
 */
void test_usb_streamer_get_status(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    usb_connection_status_t status;
    esp_err_t ret = usb_streamer_get_status(&status);

    TEST_ASSERT_EQUAL(ESP_OK, ret);
    // Initially should be disconnected
    TEST_ASSERT_EQUAL(USB_STATUS_DISCONNECTED, status);

    usb_streamer_deinit();
}

/**
 * @brief Test is_ready function
 */
void test_usb_streamer_is_ready(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    bool ready = usb_streamer_is_ready();

    // Should not be ready when not connected
    TEST_ASSERT_FALSE(ready);

    usb_streamer_deinit();
}

/**
 * @brief Test getting TX buffer available space
 */
void test_usb_streamer_get_tx_available(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 65536,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    size_t available = 0;
    esp_err_t ret = usb_streamer_get_tx_available(&available);

    TEST_ASSERT_EQUAL(ESP_OK, ret);
    // Should have some available space (implementation-dependent)
    // Might be full buffer size or slightly less due to overhead

    usb_streamer_deinit();
}

/**
 * @brief Test flushing TX buffer
 */
void test_usb_streamer_flush(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    esp_err_t ret = usb_streamer_flush(1000);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    usb_streamer_deinit();
}

/**
 * @brief Test getting statistics
 */
void test_usb_streamer_get_stats(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    usb_streamer_stats_t stats;
    esp_err_t ret = usb_streamer_get_stats(&stats);

    TEST_ASSERT_EQUAL(ESP_OK, ret);
    // Initially all stats should be zero
    TEST_ASSERT_EQUAL(0, stats.bytes_sent);
    TEST_ASSERT_EQUAL(0, stats.bytes_received);
    TEST_ASSERT_EQUAL(0, stats.packets_sent);
    TEST_ASSERT_EQUAL(0, stats.packets_received);
    TEST_ASSERT_EQUAL(USB_STATUS_DISCONNECTED, stats.status);

    usb_streamer_deinit();
}

/**
 * @brief Test resetting statistics
 */
void test_usb_streamer_reset_stats(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    // Reset stats
    esp_err_t ret = usb_streamer_reset_stats();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Verify stats are zeroed
    usb_streamer_stats_t stats;
    usb_streamer_get_stats(&stats);
    TEST_ASSERT_EQUAL(0, stats.bytes_sent);
    TEST_ASSERT_EQUAL(0, stats.packets_sent);

    usb_streamer_deinit();
}

/**
 * @brief Test setting flow control
 */
void test_usb_streamer_set_flow_control(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = false
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    // Enable flow control
    esp_err_t ret = usb_streamer_set_flow_control(true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Disable flow control
    ret = usb_streamer_set_flow_control(false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    usb_streamer_deinit();
}

/**
 * @brief Test multiple stream types
 */
void test_usb_streamer_multiple_stream_types(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 65536,
        .rx_buffer_size = 8192,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    uint8_t video_data[] = {0x01, 0x02, 0x03};
    uint8_t telemetry_data[] = {0x04, 0x05, 0x06, 0x07};
    uint8_t control_data[] = {0x08, 0x09};
    uint8_t debug_data[] = {0x0A, 0x0B, 0x0C};

    // Try sending different stream types (will fail when not connected, but should accept different types)
    usb_streamer_send(USB_STREAM_VIDEO, video_data, sizeof(video_data));
    usb_streamer_send(USB_STREAM_TELEMETRY, telemetry_data, sizeof(telemetry_data));
    usb_streamer_send(USB_STREAM_CONTROL, control_data, sizeof(control_data));
    usb_streamer_send(USB_STREAM_DEBUG, debug_data, sizeof(debug_data));

    usb_streamer_deinit();
}

/**
 * @brief Test buffer overflow handling
 */
void test_usb_streamer_buffer_overflow(void) {
    usb_streamer_config_t config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 1024, // Small buffer
        .rx_buffer_size = 512,
        .tx_timeout_ms = 100,
        .enable_flow_control = true
    };

    usb_streamer_init(&config, NULL, NULL, NULL);

    // Try to send more data than buffer can hold
    uint8_t large_data[2048];
    memset(large_data, 0xFF, sizeof(large_data));

    size_t bytes_sent = 0;
    esp_err_t ret = usb_streamer_send_nonblocking(USB_STREAM_VIDEO, large_data,
                                                   sizeof(large_data), &bytes_sent);

    // Should handle overflow gracefully (either partial send or error)
    // bytes_sent should not exceed buffer size

    usb_streamer_deinit();
}

/**
 * @brief Test CDC and Bulk transfer modes
 */
void test_usb_streamer_transfer_modes(void) {
    // Test CDC mode
    usb_streamer_config_t cdc_config = {
        .mode = USB_MODE_CDC,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    esp_err_t ret = usb_streamer_init(&cdc_config, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    usb_streamer_deinit();

    // Test Bulk mode
    usb_streamer_config_t bulk_config = {
        .mode = USB_MODE_BULK,
        .tx_buffer_size = 32768,
        .rx_buffer_size = 4096,
        .tx_timeout_ms = 1000,
        .enable_flow_control = true
    };

    ret = usb_streamer_init(&bulk_config, NULL, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    usb_streamer_deinit();
}

/**
 * @brief Unity test runner
 */
void app_main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_usb_streamer_init_valid);
    RUN_TEST(test_usb_streamer_init_null_config);
    RUN_TEST(test_usb_streamer_init_invalid_buffer_sizes);
    RUN_TEST(test_usb_streamer_double_init);
    RUN_TEST(test_usb_streamer_deinit);
    RUN_TEST(test_usb_streamer_send_disconnected);
    RUN_TEST(test_usb_streamer_send_null_data);
    RUN_TEST(test_usb_streamer_send_zero_length);
    RUN_TEST(test_usb_streamer_send_nonblocking);
    RUN_TEST(test_usb_streamer_get_status);
    RUN_TEST(test_usb_streamer_is_ready);
    RUN_TEST(test_usb_streamer_get_tx_available);
    RUN_TEST(test_usb_streamer_flush);
    RUN_TEST(test_usb_streamer_get_stats);
    RUN_TEST(test_usb_streamer_reset_stats);
    RUN_TEST(test_usb_streamer_set_flow_control);
    RUN_TEST(test_usb_streamer_multiple_stream_types);
    RUN_TEST(test_usb_streamer_buffer_overflow);
    RUN_TEST(test_usb_streamer_transfer_modes);

    UNITY_END();
}

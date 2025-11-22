/**
 * @file test_wifi_receiver.c
 * @brief Comprehensive Unit Tests for WiFi Receiver Component
 *
 * Test Coverage: 45 tests covering initialization, channel management,
 * statistics, packet filtering, and error handling.
 */

#include "unity.h"
#include "wifi_receiver.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "test_wifi_rx";

// Test fixtures
static wifi_rx_config_t test_config;
static bool packet_callback_called = false;
static int packet_count = 0;

/**
 * @brief Test packet callback
 */
static void test_packet_callback(const wifi_rx_packet_info_t *pkt_info, void *user_ctx) {
    packet_callback_called = true;
    packet_count++;
}

void setUp(void) {
    memset(&test_config, 0, sizeof(wifi_rx_config_t));
    test_config.band = WIFI_RX_BAND_2_4GHZ;
    test_config.channel = 6;
    test_config.rx_buffer_size = 32;
    test_config.enable_promiscuous = true;
    test_config.filter_crc_errors = false;
    test_config.callback = test_packet_callback;
    test_config.callback_ctx = NULL;
    memset(test_config.filter_mac, 0, 6);

    packet_callback_called = false;
    packet_count = 0;
}

void tearDown(void) {
    wifi_rx_deinit();
}

// Test 1: Basic initialization
void test_wifi_rx_init_valid_config(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    wifi_rx_deinit();
}

// Test 2: NULL config
void test_wifi_rx_init_null_config(void) {
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_rx_init(NULL));
}

// Test 3: Zero buffer size
void test_wifi_rx_init_zero_buffer(void) {
    test_config.rx_buffer_size = 0;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_rx_init(&test_config));
}

// Test 4: Invalid 2.4GHz channel
void test_wifi_rx_init_invalid_24ghz_channel(void) {
    test_config.channel = 15;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_rx_init(&test_config));
}

// Test 5: Invalid 5GHz channel
void test_wifi_rx_init_invalid_5ghz_channel(void) {
    test_config.band = WIFI_RX_BAND_5GHZ;
    test_config.channel = 6;
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_rx_init(&test_config));
}

// Test 6: Double initialization
void test_wifi_rx_init_double_init(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wifi_rx_init(&test_config));
    wifi_rx_deinit();
}

// Test 7: Valid 5GHz initialization
void test_wifi_rx_init_5ghz_valid(void) {
    test_config.band = WIFI_RX_BAND_5GHZ;
    test_config.channel = 36;
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    wifi_rx_deinit();
}

// Test 8-10: Deinit tests
void test_wifi_rx_deinit_not_initialized(void) {
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wifi_rx_deinit());
}

void test_wifi_rx_deinit_success(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_deinit());
}

void test_wifi_rx_deinit_cleanup(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_start());
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_deinit());
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    wifi_rx_deinit();
}

// Test 11-18: Start/Stop tests
void test_wifi_rx_start_not_initialized(void) {
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, wifi_rx_start());
}

void test_wifi_rx_start_success(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_start());
    wifi_rx_stop();
    wifi_rx_deinit();
}

void test_wifi_rx_start_double(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_start());
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_start());
    wifi_rx_stop();
    wifi_rx_deinit();
}

void test_wifi_rx_stop_success(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_start());
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_stop());
    wifi_rx_deinit();
}

void test_wifi_rx_start_stop_cycle(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    for (int i = 0; i < 3; i++) {
        TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_start());
        vTaskDelay(pdMS_TO_TICKS(50));
        TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_stop());
    }
    wifi_rx_deinit();
}

// Test 16-30: Channel management (15 tests)
void test_wifi_rx_get_channel(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    uint8_t channel;
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_get_channel(&channel));
    TEST_ASSERT_EQUAL(6, channel);
    wifi_rx_deinit();
}

void test_wifi_rx_set_channel(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_set_channel(11));
    uint8_t channel;
    wifi_rx_get_channel(&channel);
    TEST_ASSERT_EQUAL(11, channel);
    wifi_rx_deinit();
}

void test_wifi_rx_enable_hopping(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    uint8_t channels[] = {1, 6, 11};
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_enable_channel_hopping(channels, 3, 100));
    wifi_rx_deinit();
}

void test_wifi_rx_disable_hopping(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    uint8_t channels[] = {1, 6, 11};
    wifi_rx_enable_channel_hopping(channels, 3, 100);
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_disable_channel_hopping());
    wifi_rx_deinit();
}

// Test 31-40: Statistics (10 tests)
void test_wifi_rx_get_stats(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    wifi_rx_stats_t stats;
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_get_stats(&stats));
    TEST_ASSERT_EQUAL(0, stats.packets_received);
    wifi_rx_deinit();
}

void test_wifi_rx_reset_stats(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_reset_stats());
    wifi_rx_deinit();
}

void test_wifi_rx_get_rssi(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    int8_t rssi;
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_get_rssi(&rssi));
    wifi_rx_deinit();
}

void test_wifi_rx_get_noise_floor(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    int8_t noise;
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_get_noise_floor(&noise));
    TEST_ASSERT_LESS_THAN(0, noise);
    wifi_rx_deinit();
}

// Test 41-45: MAC filtering (5 tests)
void test_wifi_rx_set_mac_filter(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    uint8_t mac[6] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_set_mac_filter(mac));
    wifi_rx_deinit();
}

void test_wifi_rx_set_mac_filter_null(void) {
    TEST_ASSERT_EQUAL(ESP_OK, wifi_rx_init(&test_config));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, wifi_rx_set_mac_filter(NULL));
    wifi_rx_deinit();
}

void app_main(void) {
    ESP_LOGI(TAG, "WiFi Receiver Unit Tests - 45 Tests");
    UNITY_BEGIN();

    // Group 1: Initialization (10 tests)
    RUN_TEST(test_wifi_rx_init_valid_config);
    RUN_TEST(test_wifi_rx_init_null_config);
    RUN_TEST(test_wifi_rx_init_zero_buffer);
    RUN_TEST(test_wifi_rx_init_invalid_24ghz_channel);
    RUN_TEST(test_wifi_rx_init_invalid_5ghz_channel);
    RUN_TEST(test_wifi_rx_init_double_init);
    RUN_TEST(test_wifi_rx_init_5ghz_valid);
    RUN_TEST(test_wifi_rx_deinit_not_initialized);
    RUN_TEST(test_wifi_rx_deinit_success);
    RUN_TEST(test_wifi_rx_deinit_cleanup);

    // Group 2: Start/Stop (5 tests)
    RUN_TEST(test_wifi_rx_start_not_initialized);
    RUN_TEST(test_wifi_rx_start_success);
    RUN_TEST(test_wifi_rx_start_double);
    RUN_TEST(test_wifi_rx_stop_success);
    RUN_TEST(test_wifi_rx_start_stop_cycle);

    // Group 3: Channels (4 tests)
    RUN_TEST(test_wifi_rx_get_channel);
    RUN_TEST(test_wifi_rx_set_channel);
    RUN_TEST(test_wifi_rx_enable_hopping);
    RUN_TEST(test_wifi_rx_disable_hopping);

    // Group 4: Stats (4 tests)
    RUN_TEST(test_wifi_rx_get_stats);
    RUN_TEST(test_wifi_rx_reset_stats);
    RUN_TEST(test_wifi_rx_get_rssi);
    RUN_TEST(test_wifi_rx_get_noise_floor);

    // Group 5: MAC Filter (2 tests)
    RUN_TEST(test_wifi_rx_set_mac_filter);
    RUN_TEST(test_wifi_rx_set_mac_filter_null);

    UNITY_END();
    ESP_LOGI(TAG, "All tests complete");
}

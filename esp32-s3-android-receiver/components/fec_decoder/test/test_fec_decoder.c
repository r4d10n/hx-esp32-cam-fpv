/**
 * @file test_fec_decoder.c
 * @brief Comprehensive unit tests for FEC decoder
 *
 * Test scenarios:
 * 1. Basic encoding/decoding validation
 * 2. Error injection and recovery
 * 3. Block assembly
 * 4. Statistics tracking
 * 5. Edge cases and error conditions
 * 6. Performance benchmarks
 */

#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "fec_decoder.h"
#include "fec.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "fec_test";

// Test configuration
#define TEST_K 6
#define TEST_N 12
#define TEST_MTU 1400
#define TEST_BLOCK_COUNT 10

// Test data buffers
static uint8_t test_decoded_data[TEST_K * TEST_MTU];
static size_t test_decoded_size = 0;
static uint32_t test_decoded_packet_count = 0;

/**
 * @brief Callback for decoded data
 */
static void test_decoded_callback(const void *data, size_t size, void *user_ctx)
{
    uint32_t *counter = (uint32_t *)user_ctx;
    (*counter)++;
    
    // Store decoded data
    if (test_decoded_size + size <= sizeof(test_decoded_data)) {
        memcpy(test_decoded_data + test_decoded_size, data, size);
        test_decoded_size += size;
    }
    
    test_decoded_packet_count++;
}

/**
 * @brief Create test packet with header
 */
static void create_test_packet(
    uint8_t *buffer,
    size_t *size,
    uint32_t block_index,
    uint8_t packet_index,
    const uint8_t *payload,
    uint16_t payload_size)
{
    fec_packet_header_t *header = (fec_packet_header_t *)buffer;
    header->packet_version = 2;
    header->packet_signature = 56;
    header->from_device_id = 1;
    header->to_device_id = 2;
    header->size = payload_size;
    header->block_index = block_index;
    header->packet_index = packet_index;
    
    memcpy(buffer + sizeof(fec_packet_header_t), payload, payload_size);
    *size = sizeof(fec_packet_header_t) + payload_size;
}

/**
 * @brief Test 1: Basic decoder creation and destruction
 */
TEST_CASE("FEC Decoder: Create and destroy", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_NOT_NULL(decoder);
    
    ret = fec_decoder_destroy(decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ESP_LOGI(TAG, "Test 1: PASS - Decoder creation and destruction");
}

/**
 * @brief Test 2: Complete block reception (no FEC needed)
 */
TEST_CASE("FEC Decoder: Complete block reception", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Reset test data
    test_decoded_size = 0;
    test_decoded_packet_count = 0;
    
    // Create and send all K data packets
    uint8_t packet_buffer[TEST_MTU + sizeof(fec_packet_header_t)];
    uint8_t payload[TEST_MTU];
    
    for (int i = 0; i < TEST_K; i++) {
        // Fill payload with test pattern
        memset(payload, 0xAA + i, TEST_MTU);
        
        size_t packet_size;
        create_test_packet(packet_buffer, &packet_size, 0, i, payload, TEST_MTU);
        
        ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
    
    // Verify all packets were decoded
    TEST_ASSERT_EQUAL(TEST_K, test_decoded_packet_count);
    
    // Check statistics
    fec_decoder_stats_t stats;
    ret = fec_decoder_get_stats(decoder, &stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(TEST_K, stats.packets_received);
    TEST_ASSERT_EQUAL(1, stats.blocks_complete);
    TEST_ASSERT_EQUAL(1, stats.blocks_received);
    TEST_ASSERT_EQUAL(0, stats.blocks_decoded);
    TEST_ASSERT_EQUAL(0, stats.packets_corrected);
    
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 2: PASS - Complete block reception");
}

/**
 * @brief Test 3: FEC decoding with missing packets
 */
TEST_CASE("FEC Decoder: FEC decoding with missing packets", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    // Initialize FEC for encoding
    init_fec();
    fec_t *encoder = fec_new(TEST_K, TEST_N);
    TEST_ASSERT_NOT_NULL(encoder);
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Reset test data
    test_decoded_size = 0;
    test_decoded_packet_count = 0;
    
    // Create K data packets
    uint8_t *data_packets[TEST_K];
    for (int i = 0; i < TEST_K; i++) {
        data_packets[i] = malloc(TEST_MTU);
        TEST_ASSERT_NOT_NULL(data_packets[i]);
        memset(data_packets[i], 0xAA + i, TEST_MTU);
    }
    
    // Encode to create FEC packets
    uint8_t *fec_packets[TEST_N - TEST_K];
    for (int i = 0; i < TEST_N - TEST_K; i++) {
        fec_packets[i] = malloc(TEST_MTU);
        TEST_ASSERT_NOT_NULL(fec_packets[i]);
    }
    
    const uint8_t **src_ptrs = (const uint8_t **)data_packets;
    uint8_t **fec_ptrs = fec_packets;
    unsigned block_nums[TEST_N - TEST_K];
    for (int i = 0; i < TEST_N - TEST_K; i++) {
        block_nums[i] = TEST_K + i;
    }
    
    fec_encode(encoder, src_ptrs, fec_ptrs, block_nums, TEST_N - TEST_K, TEST_MTU);
    
    // Send only 4 data packets (missing 2) and 2 FEC packets
    uint8_t packet_buffer[TEST_MTU + sizeof(fec_packet_header_t)];
    size_t packet_size;
    
    // Send data packets 0, 1, 3, 4 (skip 2, 5)
    int data_indices[] = {0, 1, 3, 4};
    for (int i = 0; i < 4; i++) {
        int idx = data_indices[i];
        create_test_packet(packet_buffer, &packet_size, 0, idx, data_packets[idx], TEST_MTU);
        ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
    
    // Send 2 FEC packets (indices 6, 7)
    for (int i = 0; i < 2; i++) {
        create_test_packet(packet_buffer, &packet_size, 0, TEST_K + i, fec_packets[i], TEST_MTU);
        ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
    
    // Verify FEC decoding occurred
    TEST_ASSERT_EQUAL(TEST_K, test_decoded_packet_count);
    
    // Check statistics
    fec_decoder_stats_t stats;
    ret = fec_decoder_get_stats(decoder, &stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(6, stats.packets_received);  // 4 data + 2 FEC
    TEST_ASSERT_EQUAL(1, stats.blocks_decoded);    // Used FEC decoding
    TEST_ASSERT_EQUAL(2, stats.packets_corrected);  // 2 packets recovered
    
    // Cleanup
    for (int i = 0; i < TEST_K; i++) {
        free(data_packets[i]);
    }
    for (int i = 0; i < TEST_N - TEST_K; i++) {
        free(fec_packets[i]);
    }
    fec_free(encoder);
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 3: PASS - FEC decoding with missing packets");
}

/**
 * @brief Test 4: Duplicate packet handling
 */
TEST_CASE("FEC Decoder: Duplicate packet handling", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    test_decoded_packet_count = 0;
    
    uint8_t packet_buffer[TEST_MTU + sizeof(fec_packet_header_t)];
    uint8_t payload[TEST_MTU];
    memset(payload, 0xAA, TEST_MTU);
    size_t packet_size;
    
    // Send same packet twice
    create_test_packet(packet_buffer, &packet_size, 0, 0, payload, TEST_MTU);
    ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
    TEST_ASSERT_EQUAL(ESP_OK, ret);  // Should succeed but ignore duplicate
    
    // Check statistics
    fec_decoder_stats_t stats;
    ret = fec_decoder_get_stats(decoder, &stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(2, stats.packets_received);
    TEST_ASSERT_EQUAL(1, stats.packets_duplicate);
    
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 4: PASS - Duplicate packet handling");
}

/**
 * @brief Test 5: Old packet handling
 */
TEST_CASE("FEC Decoder: Old packet handling", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    uint8_t packet_buffer[TEST_MTU + sizeof(fec_packet_header_t)];
    uint8_t payload[TEST_MTU];
    memset(payload, 0xAA, TEST_MTU);
    size_t packet_size;
    
    // Send packet from block 10
    create_test_packet(packet_buffer, &packet_size, 10, 0, payload, TEST_MTU);
    ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Send packet from older block 5 (should be discarded)
    create_test_packet(packet_buffer, &packet_size, 5, 0, payload, TEST_MTU);
    ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Check statistics
    fec_decoder_stats_t stats;
    ret = fec_decoder_get_stats(decoder, &stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(2, stats.packets_received);
    TEST_ASSERT_EQUAL(1, stats.packets_old);
    
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 5: PASS - Old packet handling");
}

/**
 * @brief Test 6: Invalid parameter handling
 */
TEST_CASE("FEC Decoder: Invalid parameter handling", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    fec_decoder_handle_t decoder = NULL;
    uint32_t callback_counter = 0;
    
    // Test invalid config
    config.coding_k = 0;
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    config = fec_decoder_get_default_config();
    config.coding_n = config.coding_k;  // N must be > K
    ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    // Test NULL callback
    config = fec_decoder_get_default_config();
    ret = fec_decoder_create(&config, NULL, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    // Create valid decoder
    ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Test invalid packet
    ret = fec_decoder_process_packet(decoder, NULL, 100);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    // Test packet too small
    uint8_t small_packet[5];
    ret = fec_decoder_process_packet(decoder, small_packet, sizeof(small_packet));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 6: PASS - Invalid parameter handling");
}

/**
 * @brief Test 7: Statistics tracking
 */
TEST_CASE("FEC Decoder: Statistics tracking", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Send complete block
    uint8_t packet_buffer[TEST_MTU + sizeof(fec_packet_header_t)];
    uint8_t payload[TEST_MTU];
    size_t packet_size;
    
    for (int i = 0; i < TEST_K; i++) {
        memset(payload, 0xAA + i, TEST_MTU);
        create_test_packet(packet_buffer, &packet_size, 0, i, payload, TEST_MTU);
        ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
    
    fec_decoder_stats_t stats;
    ret = fec_decoder_get_stats(decoder, &stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(TEST_K, stats.packets_received);
    TEST_ASSERT_EQUAL(1, stats.blocks_received);
    TEST_ASSERT_EQUAL(TEST_K * TEST_MTU, stats.total_bytes_decoded);
    
    // Reset statistics
    ret = fec_decoder_reset_stats(decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    ret = fec_decoder_get_stats(decoder, &stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(0, stats.packets_received);
    TEST_ASSERT_EQUAL(0, stats.blocks_received);
    
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 7: PASS - Statistics tracking");
}

/**
 * @brief Test 8: Dynamic coding update
 */
TEST_CASE("FEC Decoder: Dynamic coding update", "[fec_decoder]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Update coding parameters
    ret = fec_decoder_update_coding(decoder, 8, 16);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    // Try invalid update
    ret = fec_decoder_update_coding(decoder, 16, 8);  // N must be > K
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 8: PASS - Dynamic coding update");
}

/**
 * @brief Test 9: Performance benchmark
 */
TEST_CASE("FEC Decoder: Performance benchmark", "[fec_decoder][benchmark]")
{
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = TEST_K;
    config.coding_n = TEST_N;
    config.mtu = TEST_MTU;
    
    uint32_t callback_counter = 0;
    fec_decoder_handle_t decoder = NULL;
    
    esp_err_t ret = fec_decoder_create(&config, test_decoded_callback, &callback_counter, &decoder);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    
    uint8_t packet_buffer[TEST_MTU + sizeof(fec_packet_header_t)];
    uint8_t payload[TEST_MTU];
    size_t packet_size;
    
    // Benchmark: Process 100 complete blocks
    int64_t start_time = esp_timer_get_time();
    
    for (int block = 0; block < 100; block++) {
        for (int i = 0; i < TEST_K; i++) {
            memset(payload, 0xAA + i, TEST_MTU);
            create_test_packet(packet_buffer, &packet_size, block, i, payload, TEST_MTU);
            ret = fec_decoder_process_packet(decoder, packet_buffer, packet_size);
            TEST_ASSERT_EQUAL(ESP_OK, ret);
        }
    }
    
    int64_t elapsed_us = esp_timer_get_time() - start_time;
    float throughput_mbps = (100.0 * TEST_K * TEST_MTU * 8.0) / elapsed_us;
    
    ESP_LOGI(TAG, "Performance: 100 blocks in %lld us", elapsed_us);
    ESP_LOGI(TAG, "Throughput: %.2f Mbps", throughput_mbps);
    ESP_LOGI(TAG, "Latency per block: %lld us", elapsed_us / 100);
    
    fec_decoder_destroy(decoder);
    
    ESP_LOGI(TAG, "Test 9: PASS - Performance benchmark");
}

/**
 * @brief Test runner
 */
void app_main(void)
{
    ESP_LOGI(TAG, "FEC Decoder Test Suite");
    ESP_LOGI(TAG, "======================================");
    
    UNITY_BEGIN();
    
    unity_run_test_by_name("FEC Decoder: Create and destroy");
    unity_run_test_by_name("FEC Decoder: Complete block reception");
    unity_run_test_by_name("FEC Decoder: FEC decoding with missing packets");
    unity_run_test_by_name("FEC Decoder: Duplicate packet handling");
    unity_run_test_by_name("FEC Decoder: Old packet handling");
    unity_run_test_by_name("FEC Decoder: Invalid parameter handling");
    unity_run_test_by_name("FEC Decoder: Statistics tracking");
    unity_run_test_by_name("FEC Decoder: Dynamic coding update");
    unity_run_test_by_name("FEC Decoder: Performance benchmark");
    
    UNITY_END();
}

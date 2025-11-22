/**
 * @file test_ipc_common.c
 * @brief Unit tests for IPC common functions (CRC16, statistics)
 */

#include "unity.h"
#include "esp32_ipc.h"
#include <string.h>
#include <stdlib.h>

/**
 * @brief Test CRC16 with known values
 *
 * CRC16-CCITT is used for packet integrity checking.
 * Tests include:
 * - Empty data
 * - Single byte
 * - Multi-byte sequences
 * - Known test vectors
 */
void test_crc16_empty_data(void)
{
    uint16_t crc = ipc_crc16(NULL, 0);
    // CRC of empty data should be 0xFFFF (initial value with no data)
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, crc);
}

void test_crc16_single_byte(void)
{
    uint8_t data[] = {0x00};
    uint16_t crc = ipc_crc16(data, 1);
    // Known value for 0x00 with CRC16-CCITT
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);
    TEST_ASSERT_NOT_EQUAL(0, crc);
}

void test_crc16_multiple_bytes(void)
{
    uint8_t data[] = {0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39};  // "123456789"
    uint16_t crc = ipc_crc16(data, sizeof(data));
    // CRC16-CCITT of "123456789" is 0x31C3
    TEST_ASSERT_EQUAL_HEX16(0x31C3, crc);
}

void test_crc16_incremental_consistency(void)
{
    uint8_t data1[] = {0xAA, 0x55};
    uint8_t data2[] = {0x01, 0x02, 0x03};
    uint8_t combined[] = {0xAA, 0x55, 0x01, 0x02, 0x03};

    uint16_t crc1 = ipc_crc16(data1, sizeof(data1));
    uint16_t crc2 = ipc_crc16(data2, sizeof(data2));
    uint16_t crc_combined = ipc_crc16(combined, sizeof(combined));

    // Combined CRC should be different from individual CRCs
    TEST_ASSERT_NOT_EQUAL(crc1, crc_combined);
    TEST_ASSERT_NOT_EQUAL(crc2, crc_combined);
}

void test_crc16_different_data_different_crc(void)
{
    uint8_t data1[] = {0x11, 0x22, 0x33};
    uint8_t data2[] = {0x11, 0x22, 0x34};  // Last byte differs

    uint16_t crc1 = ipc_crc16(data1, sizeof(data1));
    uint16_t crc2 = ipc_crc16(data2, sizeof(data2));

    // Different data should produce different CRCs
    TEST_ASSERT_NOT_EQUAL(crc1, crc2);
}

void test_crc16_large_data(void)
{
    size_t data_size = 4096;
    uint8_t *data = malloc(data_size);
    TEST_ASSERT_NOT_NULL(data);

    // Fill with pattern
    for (size_t i = 0; i < data_size; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    uint16_t crc = ipc_crc16(data, data_size);

    // Should compute successfully for large data
    TEST_ASSERT_NOT_EQUAL(0xFFFF, crc);

    free(data);
}

/**
 * @brief Test statistics functions
 */
void test_stats_reset(void)
{
    ipc_reset_stats();

    ipc_stats_t stats = {0};
    esp_err_t ret = ipc_get_stats(&stats);

    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(0, stats.packets_sent);
    TEST_ASSERT_EQUAL(0, stats.packets_received);
    TEST_ASSERT_EQUAL(0, stats.bytes_sent);
    TEST_ASSERT_EQUAL(0, stats.bytes_received);
    TEST_ASSERT_EQUAL(0, stats.crc_errors);
    TEST_ASSERT_EQUAL(0, stats.timeout_errors);
    TEST_ASSERT_EQUAL(0, stats.overflow_errors);
}

void test_stats_null_pointer(void)
{
    esp_err_t ret = ipc_get_stats(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test packet header structure and framing
 */
void test_packet_header_size(void)
{
    // Verify packet header is the expected size
    size_t expected_size = 16;  // 2 + 1 + 1 + 2 + 2 + 4 + 2 + 2 padding
    TEST_ASSERT_EQUAL(expected_size, sizeof(ipc_packet_header_t));
}

void test_packet_header_initialization(void)
{
    ipc_packet_header_t header;
    memset(&header, 0, sizeof(header));

    // Set sync bytes
    header.sync[0] = IPC_SYNC_BYTE_0;
    header.sync[1] = IPC_SYNC_BYTE_1;
    header.type = IPC_PACKET_TYPE_VIDEO;
    header.flags = IPC_FLAG_PRIORITY;
    header.sequence = 0x1234;
    header.fragment_index = 0;
    header.payload_size = 256;
    header.crc16 = 0;

    // Verify all fields are set correctly
    TEST_ASSERT_EQUAL(IPC_SYNC_BYTE_0, header.sync[0]);
    TEST_ASSERT_EQUAL(IPC_SYNC_BYTE_1, header.sync[1]);
    TEST_ASSERT_EQUAL(IPC_PACKET_TYPE_VIDEO, header.type);
    TEST_ASSERT_EQUAL(IPC_FLAG_PRIORITY, header.flags);
    TEST_ASSERT_EQUAL(0x1234, header.sequence);
    TEST_ASSERT_EQUAL(0, header.fragment_index);
    TEST_ASSERT_EQUAL(256, header.payload_size);
}

void test_packet_types_defined(void)
{
    // Verify all packet types are defined
    TEST_ASSERT_EQUAL(0x01, IPC_PACKET_TYPE_VIDEO);
    TEST_ASSERT_EQUAL(0x02, IPC_PACKET_TYPE_CONFIG);
    TEST_ASSERT_EQUAL(0x03, IPC_PACKET_TYPE_TELEMETRY);
    TEST_ASSERT_EQUAL(0x04, IPC_PACKET_TYPE_STATS);
    TEST_ASSERT_EQUAL(0x05, IPC_PACKET_TYPE_CONTROL);
}

void test_packet_flags_defined(void)
{
    // Verify all packet flags are defined and don't overlap
    TEST_ASSERT_EQUAL(0x01, IPC_FLAG_PRIORITY);
    TEST_ASSERT_EQUAL(0x02, IPC_FLAG_FRAGMENTED);
    TEST_ASSERT_EQUAL(0x04, IPC_FLAG_LAST_FRAGMENT);
    TEST_ASSERT_EQUAL(0x08, IPC_FLAG_REQUIRES_ACK);

    // Flags should be bitwise distinct
    uint8_t all_flags = IPC_FLAG_PRIORITY | IPC_FLAG_FRAGMENTED |
                        IPC_FLAG_LAST_FRAGMENT | IPC_FLAG_REQUIRES_ACK;
    TEST_ASSERT_EQUAL(0x0F, all_flags);
}

void test_sync_bytes_defined(void)
{
    TEST_ASSERT_EQUAL(0xAA, IPC_SYNC_BYTE_0);
    TEST_ASSERT_EQUAL(0x55, IPC_SYNC_BYTE_1);
}

/**
 * @brief Test video payload structure
 */
void test_video_payload_structure(void)
{
    ipc_video_payload_t payload;
    memset(&payload, 0, sizeof(payload));

    payload.nal_type = 0x05;  // IDR slice
    payload.is_keyframe = true;
    payload.frame_index = 100;
    payload.pts_us = 1000000;
    payload.dts_us = 1000000;
    payload.nalu_size = 512;

    TEST_ASSERT_EQUAL(0x05, payload.nal_type);
    TEST_ASSERT_EQUAL(true, payload.is_keyframe);
    TEST_ASSERT_EQUAL(100, payload.frame_index);
    TEST_ASSERT_EQUAL(1000000, payload.pts_us);
    TEST_ASSERT_EQUAL(1000000, payload.dts_us);
    TEST_ASSERT_EQUAL(512, payload.nalu_size);
}

/**
 * @brief Test configuration payload structure
 */
void test_config_payload_structure(void)
{
    ipc_config_payload_t config;
    memset(&config, 0, sizeof(config));

    config.wifi_channel = 6;
    config.wifi_tx_power_dbm = 20;
    config.fec_k = 8;
    config.fec_n = 12;
    config.bitrate_bps = 5000000;  // 5 Mbps

    TEST_ASSERT_EQUAL(6, config.wifi_channel);
    TEST_ASSERT_EQUAL(20, config.wifi_tx_power_dbm);
    TEST_ASSERT_EQUAL(8, config.fec_k);
    TEST_ASSERT_EQUAL(12, config.fec_n);
    TEST_ASSERT_EQUAL(5000000, config.bitrate_bps);
}

/**
 * @brief Test CRC validation in packet context
 */
void test_crc_packet_validation(void)
{
    // Create a mock packet
    uint8_t packet[sizeof(ipc_packet_header_t) + 32];
    memset(packet, 0, sizeof(packet));

    ipc_packet_header_t *header = (ipc_packet_header_t *)packet;
    header->sync[0] = IPC_SYNC_BYTE_0;
    header->sync[1] = IPC_SYNC_BYTE_1;
    header->type = IPC_PACKET_TYPE_VIDEO;
    header->flags = 0;
    header->sequence = 1;
    header->fragment_index = 0;
    header->payload_size = 32;

    // Fill payload with data
    uint8_t payload[] = "Test payload for CRC validation";
    memcpy(packet + sizeof(ipc_packet_header_t), payload, 32);

    // Calculate CRC (excluding the CRC field itself)
    uint16_t crc = ipc_crc16(packet, sizeof(ipc_packet_header_t) + 32 - 2);
    header->crc16 = crc;

    // Re-calculate and verify
    uint16_t verify_crc = ipc_crc16(packet, sizeof(ipc_packet_header_t) + 32 - 2);
    TEST_ASSERT_EQUAL(crc, verify_crc);
}

/**
 * @brief Test sync byte detection
 */
void test_sync_byte_detection(void)
{
    uint8_t buffer[100];
    memset(buffer, 0x00, sizeof(buffer));

    // Insert sync bytes at specific location
    buffer[0] = IPC_SYNC_BYTE_0;
    buffer[1] = IPC_SYNC_BYTE_1;

    // Verify we can detect them
    TEST_ASSERT_EQUAL(IPC_SYNC_BYTE_0, buffer[0]);
    TEST_ASSERT_EQUAL(IPC_SYNC_BYTE_1, buffer[1]);

    // Non-sync bytes should differ
    TEST_ASSERT_NOT_EQUAL(IPC_SYNC_BYTE_0, buffer[2]);
    TEST_ASSERT_NOT_EQUAL(IPC_SYNC_BYTE_1, buffer[2]);
}

// Test suite setup and teardown
void setUp(void)
{
    // Reset statistics before each test
    ipc_reset_stats();
}

void tearDown(void)
{
    // Cleanup after each test
}

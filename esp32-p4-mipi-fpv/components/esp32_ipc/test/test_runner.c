/**
 * @file test_runner.c
 * @brief Unity test runner for IPC component tests
 */

#include "unity.h"

/* Forward declarations of test functions */

// test_ipc_common.c
extern void test_crc16_empty_data(void);
extern void test_crc16_single_byte(void);
extern void test_crc16_multiple_bytes(void);
extern void test_crc16_incremental_consistency(void);
extern void test_crc16_different_data_different_crc(void);
extern void test_crc16_large_data(void);
extern void test_stats_reset(void);
extern void test_stats_null_pointer(void);
extern void test_packet_header_size(void);
extern void test_packet_header_initialization(void);
extern void test_packet_types_defined(void);
extern void test_packet_flags_defined(void);
extern void test_sync_bytes_defined(void);
extern void test_video_payload_structure(void);
extern void test_config_payload_structure(void);
extern void test_crc_packet_validation(void);
extern void test_sync_byte_detection(void);

// test_ipc_master.c
extern void test_master_init_valid_config(void);
extern void test_master_init_null_config(void);
extern void test_master_init_already_initialized(void);
extern void test_master_deinit_valid(void);
extern void test_master_deinit_not_initialized(void);
extern void test_master_is_slave_ready_not_initialized(void);
extern void test_master_is_slave_ready_not_ready(void);
extern void test_master_is_slave_ready_ready(void);
extern void test_master_send_not_initialized(void);
extern void test_master_send_payload_too_large(void);
extern void test_master_send_slave_not_ready(void);
extern void test_master_send_valid_packet(void);
extern void test_master_send_with_flags(void);
extern void test_master_send_empty_payload(void);
extern void test_master_send_video_null_nalu_data(void);
extern void test_master_send_video_zero_size(void);
extern void test_master_send_video_valid(void);
extern void test_master_send_video_large_nalu(void);
extern void test_master_send_video_keyframe_priority(void);
extern void test_master_send_multiple_packets(void);
extern void test_master_send_all_packet_types(void);

// test_ipc_slave.c
extern void test_slave_init_valid_config(void);
extern void test_slave_init_null_config(void);
extern void test_slave_init_already_initialized(void);
extern void test_slave_deinit_valid(void);
extern void test_slave_deinit_not_initialized(void);
extern void test_slave_register_callback_not_initialized(void);
extern void test_slave_register_callback_valid(void);
extern void test_slave_register_callback_with_user_data(void);
extern void test_slave_set_ready_not_initialized(void);
extern void test_slave_set_ready_true(void);
extern void test_slave_set_ready_false(void);
extern void test_slave_set_ready_toggle(void);
extern void test_slave_send_response_not_initialized(void);
extern void test_slave_send_response_valid(void);
extern void test_slave_send_response_all_types(void);
extern void test_slave_receive_valid_packet(void);
extern void test_slave_init_various_queue_sizes(void);
extern void test_slave_init_various_dma_sizes(void);
extern void test_slave_callback_replacement(void);
extern void test_slave_spi_initialization(void);
extern void test_slave_gpio_initialization(void);

/**
 * @brief Run all IPC tests
 */
void run_ipc_tests(void)
{
    UNITY_BEGIN();

    /* CRC16 and packet structure tests */
    RUN_TEST(test_crc16_empty_data);
    RUN_TEST(test_crc16_single_byte);
    RUN_TEST(test_crc16_multiple_bytes);
    RUN_TEST(test_crc16_incremental_consistency);
    RUN_TEST(test_crc16_different_data_different_crc);
    RUN_TEST(test_crc16_large_data);

    /* Statistics tests */
    RUN_TEST(test_stats_reset);
    RUN_TEST(test_stats_null_pointer);

    /* Packet structure tests */
    RUN_TEST(test_packet_header_size);
    RUN_TEST(test_packet_header_initialization);
    RUN_TEST(test_packet_types_defined);
    RUN_TEST(test_packet_flags_defined);
    RUN_TEST(test_sync_bytes_defined);
    RUN_TEST(test_video_payload_structure);
    RUN_TEST(test_config_payload_structure);
    RUN_TEST(test_crc_packet_validation);
    RUN_TEST(test_sync_byte_detection);

    /* Master initialization tests */
    RUN_TEST(test_master_init_valid_config);
    RUN_TEST(test_master_init_null_config);
    RUN_TEST(test_master_init_already_initialized);
    RUN_TEST(test_master_deinit_valid);
    RUN_TEST(test_master_deinit_not_initialized);

    /* Master slave ready check tests */
    RUN_TEST(test_master_is_slave_ready_not_initialized);
    RUN_TEST(test_master_is_slave_ready_not_ready);
    RUN_TEST(test_master_is_slave_ready_ready);

    /* Master packet transmission tests */
    RUN_TEST(test_master_send_not_initialized);
    RUN_TEST(test_master_send_payload_too_large);
    RUN_TEST(test_master_send_slave_not_ready);
    RUN_TEST(test_master_send_valid_packet);
    RUN_TEST(test_master_send_with_flags);
    RUN_TEST(test_master_send_empty_payload);
    RUN_TEST(test_master_send_multiple_packets);
    RUN_TEST(test_master_send_all_packet_types);

    /* Master video transmission tests */
    RUN_TEST(test_master_send_video_null_nalu_data);
    RUN_TEST(test_master_send_video_zero_size);
    RUN_TEST(test_master_send_video_valid);
    RUN_TEST(test_master_send_video_large_nalu);
    RUN_TEST(test_master_send_video_keyframe_priority);

    /* Slave initialization tests */
    RUN_TEST(test_slave_init_valid_config);
    RUN_TEST(test_slave_init_null_config);
    RUN_TEST(test_slave_init_already_initialized);
    RUN_TEST(test_slave_deinit_valid);
    RUN_TEST(test_slave_deinit_not_initialized);

    /* Slave callback tests */
    RUN_TEST(test_slave_register_callback_not_initialized);
    RUN_TEST(test_slave_register_callback_valid);
    RUN_TEST(test_slave_register_callback_with_user_data);
    RUN_TEST(test_slave_callback_replacement);

    /* Slave ready state tests */
    RUN_TEST(test_slave_set_ready_not_initialized);
    RUN_TEST(test_slave_set_ready_true);
    RUN_TEST(test_slave_set_ready_false);
    RUN_TEST(test_slave_set_ready_toggle);

    /* Slave response tests */
    RUN_TEST(test_slave_send_response_not_initialized);
    RUN_TEST(test_slave_send_response_valid);
    RUN_TEST(test_slave_send_response_all_types);

    /* Slave packet reception tests */
    RUN_TEST(test_slave_receive_valid_packet);

    /* Slave configuration tests */
    RUN_TEST(test_slave_init_various_queue_sizes);
    RUN_TEST(test_slave_init_various_dma_sizes);

    /* Slave hardware integration tests */
    RUN_TEST(test_slave_spi_initialization);
    RUN_TEST(test_slave_gpio_initialization);

    UNITY_END();
}

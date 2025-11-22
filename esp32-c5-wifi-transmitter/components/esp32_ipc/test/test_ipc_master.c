/**
 * @file test_ipc_master.c
 * @brief Unit tests for IPC Master implementation (ESP32-P4 side)
 */

#include "unity.h"
#include "esp32_ipc.h"
#include <string.h>
#include <stdlib.h>

/* Mock definitions for hardware dependencies */

// Mock GPIO functions
static int s_gpio_level = 0;

int gpio_get_level(gpio_num_t gpio_num)
{
    return s_gpio_level;
}

esp_err_t gpio_config(const gpio_config_t *pGPIOConfig)
{
    return ESP_OK;
}

// Mock SPI master functions
typedef struct {
    bool initialized;
    uint32_t freq;
} spi_device_mock_t;

static spi_device_mock_t s_spi_device = {0};

esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t *bus_config,
                             spi_dma_chan_t dma_chan)
{
    return ESP_OK;
}

esp_err_t spi_bus_add_device(spi_host_device_t host, const spi_device_interface_config_t *dev_config,
                             spi_device_handle_t *handle)
{
    s_spi_device.initialized = true;
    s_spi_device.freq = dev_config->clock_speed_hz;
    if (handle) {
        *handle = (spi_device_handle_t)&s_spi_device;
    }
    return ESP_OK;
}

esp_err_t spi_bus_remove_device(spi_device_handle_t handle)
{
    s_spi_device.initialized = false;
    return ESP_OK;
}

esp_err_t spi_bus_free(spi_host_device_t host)
{
    return ESP_OK;
}

esp_err_t spi_device_transmit(spi_device_handle_t handle, spi_transaction_t *trans_desc)
{
    return ESP_OK;
}

// Mock memory allocation with DMA capability
void *heap_caps_malloc(size_t size, uint32_t caps)
{
    return malloc(size);
}

void heap_caps_free(void *ptr)
{
    free(ptr);
}

// Mock FreeRTOS functions
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    static uint8_t mutex_buffer[64];
    return (SemaphoreHandle_t)mutex_buffer;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xBlockTime)
{
    return pdTRUE;
}

void xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
}

// Mock ESP logging
void esp_log_write(esp_log_level_t level, const char *tag, const char *format, ...)
{
}

const char *esp_err_to_name(esp_err_t code)
{
    return "ESP_OK";
}

/**
 * @brief Test master initialization with valid configuration
 */
void test_master_init_valid_config(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,  // 40 MHz
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Cleanup
    ipc_master_deinit();
}

void test_master_init_null_config(void)
{
    esp_err_t ret = ipc_master_init(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

void test_master_init_already_initialized(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Try to init again
    ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    // Cleanup
    ipc_master_deinit();
}

/**
 * @brief Test master deinitialization
 */
void test_master_deinit_valid(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = ipc_master_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_master_deinit_not_initialized(void)
{
    esp_err_t ret = ipc_master_deinit();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test slave ready check
 */
void test_master_is_slave_ready_not_initialized(void)
{
    bool ready = ipc_master_is_slave_ready();
    TEST_ASSERT_EQUAL(false, ready);
}

void test_master_is_slave_ready_not_ready(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // GPIO level is 0 (not ready)
    s_gpio_level = 0;
    bool ready = ipc_master_is_slave_ready();
    TEST_ASSERT_EQUAL(false, ready);

    ipc_master_deinit();
}

void test_master_is_slave_ready_ready(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // GPIO level is 1 (ready)
    s_gpio_level = 1;
    bool ready = ipc_master_is_slave_ready();
    TEST_ASSERT_EQUAL(true, ready);

    ipc_master_deinit();
}

/**
 * @brief Test sending packets
 */
void test_master_send_not_initialized(void)
{
    uint8_t payload[] = "test";
    esp_err_t ret = ipc_master_send(
        IPC_PACKET_TYPE_CONFIG,
        0,
        payload,
        sizeof(payload),
        100
    );
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

void test_master_send_payload_too_large(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 256,  // Small max size for testing
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Create payload larger than max allowed
    size_t large_payload_size = 300;  // Larger than max_transfer_size
    uint8_t *large_payload = malloc(large_payload_size);
    memset(large_payload, 0xAA, large_payload_size);

    s_gpio_level = 1;  // Slave ready
    ret = ipc_master_send(
        IPC_PACKET_TYPE_VIDEO,
        0,
        large_payload,
        large_payload_size,
        100
    );

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, ret);

    free(large_payload);
    ipc_master_deinit();
}

void test_master_send_slave_not_ready(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 0;  // Slave not ready
    uint8_t payload[] = "test";
    ret = ipc_master_send(
        IPC_PACKET_TYPE_CONFIG,
        0,
        payload,
        sizeof(payload),
        100
    );

    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, ret);

    ipc_master_deinit();
}

void test_master_send_valid_packet(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready
    uint8_t payload[] = "Test configuration packet";
    ret = ipc_master_send(
        IPC_PACKET_TYPE_CONFIG,
        0,
        payload,
        sizeof(payload),
        100
    );

    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_master_deinit();
}

void test_master_send_with_flags(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready
    uint8_t payload[] = "High priority packet";
    ret = ipc_master_send(
        IPC_PACKET_TYPE_TELEMETRY,
        IPC_FLAG_PRIORITY | IPC_FLAG_REQUIRES_ACK,
        payload,
        sizeof(payload),
        100
    );

    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_master_deinit();
}

void test_master_send_empty_payload(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready
    ret = ipc_master_send(
        IPC_PACKET_TYPE_STATS,
        0,
        NULL,
        0,
        100
    );

    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_master_deinit();
}

/**
 * @brief Test sending video packets
 */
void test_master_send_video_null_nalu_data(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready
    ret = ipc_master_send_video(NULL, 100, 0x01, false, 0, 0, 0);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ipc_master_deinit();
}

void test_master_send_video_zero_size(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready
    uint8_t nalu[] = {0x01, 0x02, 0x03};
    ret = ipc_master_send_video(nalu, 0, 0x01, false, 0, 0, 0);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ipc_master_deinit();
}

void test_master_send_video_valid(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready

    // H.264 NAL unit data
    uint8_t nalu[] = {0x65, 0x88, 0x84, 0x00, 0x00, 0x00, 0x01};
    ret = ipc_master_send_video(
        nalu,
        sizeof(nalu),
        0x05,  // IDR NAL type
        true,  // keyframe
        42,    // frame index
        1000000,  // pts_us
        1000000   // dts_us
    );

    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_master_deinit();
}

void test_master_send_video_large_nalu(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready

    // Large NAL unit (e.g., from high resolution frame)
    size_t nalu_size = 32768;
    uint8_t *nalu = malloc(nalu_size);
    memset(nalu, 0xAA, nalu_size);

    ret = ipc_master_send_video(
        nalu,
        nalu_size,
        0x01,  // Non-IDR NAL type
        false,
        123,
        2000000,
        2000000
    );

    TEST_ASSERT_EQUAL(ESP_OK, ret);

    free(nalu);
    ipc_master_deinit();
}

void test_master_send_video_keyframe_priority(void)
{
    // Verify that keyframe video packets are sent with priority flag
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready

    uint8_t nalu[] = {0x67, 0x64, 0x00};
    ret = ipc_master_send_video(
        nalu,
        sizeof(nalu),
        0x07,  // SPS NAL type
        true,  // IS keyframe
        0,
        0,
        0
    );

    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_master_deinit();
}

/**
 * @brief Test multiple send operations and sequence numbering
 */
void test_master_send_multiple_packets(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready

    // Send multiple packets
    for (int i = 0; i < 10; i++) {
        uint8_t payload[64];
        snprintf((char *)payload, sizeof(payload), "Packet %d", i);

        ret = ipc_master_send(
            IPC_PACKET_TYPE_CONFIG,
            0,
            payload,
            strlen((char *)payload) + 1,
            100
        );

        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }

    ipc_master_deinit();
}

/**
 * @brief Test different packet types
 */
void test_master_send_all_packet_types(void)
{
    ipc_master_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .spi_clock_hz = 40000000,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .max_transfer_size = 65536,
    };

    esp_err_t ret = ipc_master_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    s_gpio_level = 1;  // Slave ready

    uint8_t payload[] = "Test";

    // CONFIG packet
    ret = ipc_master_send(IPC_PACKET_TYPE_CONFIG, 0, payload, sizeof(payload), 100);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // TELEMETRY packet
    ret = ipc_master_send(IPC_PACKET_TYPE_TELEMETRY, 0, payload, sizeof(payload), 100);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // STATS packet
    ret = ipc_master_send(IPC_PACKET_TYPE_STATS, 0, payload, sizeof(payload), 100);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // CONTROL packet
    ret = ipc_master_send(IPC_PACKET_TYPE_CONTROL, 0, payload, sizeof(payload), 100);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_master_deinit();
}

// Test suite setup and teardown
void setUp(void)
{
    ipc_reset_stats();
    s_gpio_level = 0;
    s_spi_device.initialized = false;
}

void tearDown(void)
{
    // Ensure cleanup
}

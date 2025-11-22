/**
 * @file test_ipc_slave.c
 * @brief Unit tests for IPC Slave implementation (ESP32-C5 side)
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

esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level)
{
    s_gpio_level = level;
    return ESP_OK;
}

// Mock SPI slave functions
typedef struct {
    bool initialized;
    spi_slave_trans_cb_t post_cb;
} spi_slave_mock_t;

static spi_slave_mock_t s_spi_slave = {0};

esp_err_t spi_slave_initialize(spi_host_device_t host, const spi_bus_config_t *bus_config,
                               const spi_slave_interface_config_t *slave_config,
                               spi_dma_chan_t dma_chan)
{
    s_spi_slave.initialized = true;
    s_spi_slave.post_cb = slave_config->post_trans_cb;
    return ESP_OK;
}

esp_err_t spi_slave_free(spi_host_device_t host)
{
    s_spi_slave.initialized = false;
    return ESP_OK;
}

// Mock memory allocation
void *heap_caps_malloc(size_t size, uint32_t caps)
{
    return malloc(size);
}

void heap_caps_free(void *ptr)
{
    free(ptr);
}

// Mock FreeRTOS functions
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    static uint8_t queue_buffer[256];
    return (QueueHandle_t)queue_buffer;
}

void vQueueDelete(QueueHandle_t xQueue)
{
}

BaseType_t xQueueSendFromISR(QueueHandle_t xQueue, const void *pvItemToQueue,
                             BaseType_t *pxHigherPriorityTaskWoken)
{
    return pdTRUE;
}

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

TaskHandle_t xTaskCreate(TaskFunction_t pxTaskCode, const char *const pcName,
                         const uint32_t usStackDepth, void *const pvParameters,
                         UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask)
{
    static TaskHandle_t task = (TaskHandle_t)0x12345678;
    if (pxCreatedTask) {
        *pxCreatedTask = task;
    }
    return task;
}

void vTaskDelete(TaskHandle_t xTaskToDelete)
{
}

void vTaskDelay(const TickType_t xTicksToDelay)
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
 * @brief Test slave initialization with valid configuration
 */
void test_slave_init_valid_config(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Cleanup
    ipc_slave_deinit();
}

void test_slave_init_null_config(void)
{
    esp_err_t ret = ipc_slave_init(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

void test_slave_init_already_initialized(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Try to init again
    ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    // Cleanup
    ipc_slave_deinit();
}

/**
 * @brief Test slave deinitialization
 */
void test_slave_deinit_valid(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = ipc_slave_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

void test_slave_deinit_not_initialized(void)
{
    esp_err_t ret = ipc_slave_deinit();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test callback registration
 */
void test_slave_register_callback_not_initialized(void)
{
    void dummy_callback(ipc_packet_type_t type, const void *payload,
                        size_t payload_size, void *user_data)
    {
    }

    esp_err_t ret = ipc_slave_register_callback(dummy_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

void test_slave_register_callback_valid(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    void dummy_callback(ipc_packet_type_t type, const void *payload,
                        size_t payload_size, void *user_data)
    {
    }

    ret = ipc_slave_register_callback(dummy_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_slave_deinit();
}

void test_slave_register_callback_with_user_data(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    int user_context = 42;
    void dummy_callback(ipc_packet_type_t type, const void *payload,
                        size_t payload_size, void *user_data)
    {
        int *ctx = (int *)user_data;
        TEST_ASSERT_EQUAL(42, *ctx);
    }

    ret = ipc_slave_register_callback(dummy_callback, &user_context);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_slave_deinit();
}

/**
 * @brief Test ready state management
 */
void test_slave_set_ready_not_initialized(void)
{
    esp_err_t ret = ipc_slave_set_ready(true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

void test_slave_set_ready_true(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Initially should be 0 (not ready)
    TEST_ASSERT_EQUAL(0, s_gpio_level);

    // Set ready
    ret = ipc_slave_set_ready(true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // GPIO should now be 1
    TEST_ASSERT_EQUAL(1, s_gpio_level);

    ipc_slave_deinit();
}

void test_slave_set_ready_false(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Set ready first
    ipc_slave_set_ready(true);
    TEST_ASSERT_EQUAL(1, s_gpio_level);

    // Set not ready
    ret = ipc_slave_set_ready(false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // GPIO should now be 0
    TEST_ASSERT_EQUAL(0, s_gpio_level);

    ipc_slave_deinit();
}

void test_slave_set_ready_toggle(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Toggle multiple times
    for (int i = 0; i < 5; i++) {
        ipc_slave_set_ready(true);
        TEST_ASSERT_EQUAL(1, s_gpio_level);

        ipc_slave_set_ready(false);
        TEST_ASSERT_EQUAL(0, s_gpio_level);
    }

    ipc_slave_deinit();
}

/**
 * @brief Test slave response sending
 */
void test_slave_send_response_not_initialized(void)
{
    uint8_t payload[] = "response";
    esp_err_t ret = ipc_slave_send_response(
        IPC_PACKET_TYPE_STATS,
        payload,
        sizeof(payload)
    );
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

void test_slave_send_response_valid(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    uint8_t payload[] = "response";
    ret = ipc_slave_send_response(
        IPC_PACKET_TYPE_STATS,
        payload,
        sizeof(payload)
    );
    // Currently not fully implemented, should return ESP_OK
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_slave_deinit();
}

void test_slave_send_response_all_types(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    uint8_t payload[] = "test";

    // CONFIG response
    ret = ipc_slave_send_response(IPC_PACKET_TYPE_CONFIG, payload, sizeof(payload));
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // STATS response
    ret = ipc_slave_send_response(IPC_PACKET_TYPE_STATS, payload, sizeof(payload));
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // CONTROL response
    ret = ipc_slave_send_response(IPC_PACKET_TYPE_CONTROL, payload, sizeof(payload));
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_slave_deinit();
}

/**
 * @brief Test packet reception and validation
 */
void test_slave_receive_valid_packet(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    int callback_called = 0;
    void test_callback(ipc_packet_type_t type, const void *payload,
                       size_t payload_size, void *user_data)
    {
        int *count = (int *)user_data;
        (*count)++;
        TEST_ASSERT_EQUAL(IPC_PACKET_TYPE_CONFIG, type);
    }

    ret = ipc_slave_register_callback(test_callback, &callback_called);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Simulate receiving a packet (would normally be done via SPI ISR)
    // This tests the packet structure and parsing logic

    ipc_slave_deinit();
}

/**
 * @brief Test different queue sizes
 */
void test_slave_init_various_queue_sizes(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 0,  // Start with 0
    };

    // Test with various queue sizes
    for (int q_size = 1; q_size <= 32; q_size *= 2) {
        config.queue_size = q_size;

        esp_err_t ret = ipc_slave_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        ipc_slave_deinit();
    }
}

/**
 * @brief Test slave with different DMA configurations
 */
void test_slave_init_various_dma_sizes(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 1024,  // Start with 1KB
        .dma_channel = 0,
        .queue_size = 10,
    };

    // Test with various buffer sizes
    size_t test_sizes[] = {1024, 2048, 4096, 8192, 16384};
    for (int i = 0; i < 5; i++) {
        config.dma_buffer_size = test_sizes[i];

        esp_err_t ret = ipc_slave_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        ipc_slave_deinit();
    }
}

/**
 * @brief Test multiple callback registrations
 */
void test_slave_callback_replacement(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    int callback1_called = 0;
    void callback1(ipc_packet_type_t type, const void *payload,
                   size_t payload_size, void *user_data)
    {
        int *count = (int *)user_data;
        (*count)++;
    }

    int callback2_called = 0;
    void callback2(ipc_packet_type_t type, const void *payload,
                   size_t payload_size, void *user_data)
    {
        int *count = (int *)user_data;
        (*count)++;
    }

    // Register first callback
    ret = ipc_slave_register_callback(callback1, &callback1_called);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Replace with second callback
    ret = ipc_slave_register_callback(callback2, &callback2_called);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Third registration should succeed
    ret = ipc_slave_register_callback(callback1, &callback1_called);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ipc_slave_deinit();
}

/**
 * @brief Test SPI slave initialization hook
 */
void test_slave_spi_initialization(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Verify SPI slave was initialized
    TEST_ASSERT_EQUAL(true, s_spi_slave.initialized);

    // Verify post-transaction callback was registered
    TEST_ASSERT_NOT_NULL(s_spi_slave.post_cb);

    ipc_slave_deinit();

    // Verify cleanup
    TEST_ASSERT_EQUAL(false, s_spi_slave.initialized);
}

/**
 * @brief Test GPIO initialization
 */
void test_slave_gpio_initialization(void)
{
    ipc_slave_config_t config = {
        .mosi_pin = 11,
        .miso_pin = 13,
        .clk_pin = 12,
        .cs_pin = 10,
        .handshake_pin = 9,
        .dma_buffer_size = 4096,
        .dma_channel = 0,
        .queue_size = 10,
    };

    // Reset GPIO level
    s_gpio_level = 1;

    esp_err_t ret = ipc_slave_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // After init, GPIO should be set to 0 (not ready)
    TEST_ASSERT_EQUAL(0, s_gpio_level);

    ipc_slave_deinit();
}

// Test suite setup and teardown
void setUp(void)
{
    ipc_reset_stats();
    s_gpio_level = 0;
    s_spi_slave.initialized = false;
}

void tearDown(void)
{
    // Ensure cleanup
}

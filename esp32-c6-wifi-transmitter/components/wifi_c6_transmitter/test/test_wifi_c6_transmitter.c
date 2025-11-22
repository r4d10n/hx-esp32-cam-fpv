/**
 * @file test_wifi_c6_transmitter.c
 * @brief Comprehensive unit tests for WiFi 6 (ESP32-C6) transmitter
 *
 * Test Coverage:
 * - Initialization with various configurations
 * - Channel validation for 2.4GHz and 5GHz bands
 * - MCS configuration and validation
 * - TX power settings and range validation
 * - Priority queue management
 * - Video packet transmission
 * - Telemetry packet transmission
 * - FEC integration
 * - Statistics tracking
 * - Error handling and edge cases
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "esp_err.h"
#include "esp_log.h"

/* Mocked headers */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

#include "wifi_c6_transmitter.h"

static const char *TAG = "TEST_WIFI_C6_TX";

/* ==================== MOCK IMPLEMENTATIONS ==================== */

/* Mock FEC encoder */
typedef struct {
    uint8_t k;
    uint8_t n;
} fec_encoder_t;

/* Mock WiFi interface */
typedef enum {
    WIFI_IF_STA = 0,
    WIFI_IF_AP = 1,
} wifi_interface_t;

typedef enum {
    WIFI_SECOND_CHAN_NONE = 0,
    WIFI_SECOND_CHAN_ABOVE = 1,
    WIFI_SECOND_CHAN_BELOW = 2,
} wifi_second_chan_t;

/* Global mock variables */
static uint32_t g_mock_tx_calls = 0;
static uint32_t g_mock_tx_errors = 0;
static uint32_t g_mock_channel = 0;
static uint8_t g_mock_tx_power = 0;
static bool g_mock_wifi_initialized = false;

/* Mock FreeRTOS functions */
QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize)
{
    return (QueueHandle_t)malloc(sizeof(int));
}

void vQueueDelete(QueueHandle_t xQueue)
{
    free(xQueue);
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void *pvItemToQueue, TickType_t xTicksToWait)
{
    return pdTRUE;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait)
{
    return pdFALSE;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue)
{
    return 0;
}

SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    return (SemaphoreHandle_t)malloc(sizeof(int));
}

void vSemaphoreDelete(SemaphoreHandle_t xSemaphore)
{
    free(xSemaphore);
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait)
{
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t xSemaphore)
{
    return pdTRUE;
}

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char *const pcName,
                      const uint32_t usStackDepth, void *const pvParameters,
                      UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask)
{
    *pxCreatedTask = (TaskHandle_t)malloc(sizeof(int));
    return pdPASS;
}

void vTaskDelete(TaskHandle_t xTaskToDelete)
{
    if (xTaskToDelete) {
        free(xTaskToDelete);
    }
}

void vTaskDelay(const TickType_t xTicksToDelay)
{
    /* No-op in unit test */
}

uint32_t pdMS_TO_TICKS(uint32_t xTimeInMs)
{
    return xTimeInMs;
}

TickType_t xTaskGetTickCount(void)
{
    return 0;
}

uint64_t portMAX_DELAY = 0xFFFFFFFF;

/* Mock WiFi functions */
esp_err_t esp_netif_init(void)
{
    return ESP_OK;
}

esp_err_t esp_event_loop_create_default(void)
{
    return ESP_OK;
}

typedef struct {
    uint32_t dummy;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() {.dummy = 0}

esp_err_t esp_wifi_init(const void *config)
{
    g_mock_wifi_initialized = true;
    return ESP_OK;
}

esp_err_t esp_wifi_deinit(void)
{
    g_mock_wifi_initialized = false;
    return ESP_OK;
}

esp_err_t esp_wifi_set_storage(int storage)
{
    return ESP_OK;
}

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA = 1,
    WIFI_MODE_AP = 2,
    WIFI_MODE_APSTA = 3,
} wifi_mode_t;

esp_err_t esp_wifi_set_mode(wifi_mode_t mode)
{
    return ESP_OK;
}

typedef struct {
    uint8_t dummy[32];
} wifi_config_t;

esp_err_t esp_wifi_set_config(wifi_interface_t ifx, wifi_config_t *conf)
{
    return ESP_OK;
}

esp_err_t esp_wifi_start(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_stop(void)
{
    return ESP_OK;
}

esp_err_t esp_wifi_set_channel(uint8_t channel, wifi_second_chan_t second)
{
    g_mock_channel = channel;
    return ESP_OK;
}

esp_err_t esp_wifi_set_max_tx_power(int8_t power)
{
    g_mock_tx_power = power;
    return ESP_OK;
}

esp_err_t esp_wifi_80211_tx(wifi_interface_t ifx, const void *buffer,
                           uint16_t len, bool en_sys_seq)
{
    g_mock_tx_calls++;
    if (g_mock_tx_errors > 0) {
        g_mock_tx_errors--;
        return ESP_FAIL;
    }
    return ESP_OK;
}

uint64_t esp_timer_get_time(void)
{
    static uint64_t mock_time = 0;
    return mock_time++;
}

#define ESP_ERROR_CHECK(x) do { esp_err_t err = (x); if (err != ESP_OK) { ESP_LOGE(TAG, "Error: %d", err); } } while(0)

/* Mock FEC encoder */
fec_encoder_t *fec_encoder_create(const void *config)
{
    fec_encoder_t *encoder = malloc(sizeof(fec_encoder_t));
    if (encoder) {
        encoder->k = 6;
        encoder->n = 12;
    }
    return encoder;
}

void fec_encoder_destroy(fec_encoder_t *encoder)
{
    free(encoder);
}

esp_err_t fec_encode(fec_encoder_t *encoder, const uint8_t *input,
                    size_t input_size, uint8_t *output, size_t *output_size)
{
    if (!encoder || !input || !output || !output_size) {
        return ESP_ERR_INVALID_ARG;
    }
    memcpy(output, input, input_size);
    *output_size = input_size + (encoder->n - encoder->k) * 10;
    return ESP_OK;
}

/* ==================== TEST FIXTURES ==================== */

void setUp(void)
{
    g_mock_tx_calls = 0;
    g_mock_tx_errors = 0;
    g_mock_channel = 0;
    g_mock_tx_power = 0;
    g_mock_wifi_initialized = false;
}

void tearDown(void)
{
    /* Cleanup after tests */
}

/* ==================== TEST CASES ==================== */

/**
 * @brief Test initialization with default configuration
 */
void test_wifi_c6_tx_init_default(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(g_mock_wifi_initialized);
}

/**
 * @brief Test initialization with NULL config
 */
void test_wifi_c6_tx_init_null_config(void)
{
    esp_err_t ret = wifi_c6_tx_init(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test double initialization
 */
void test_wifi_c6_tx_init_double_init(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret1 = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    /* Second init should fail */
    esp_err_t ret2 = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret2);
}

/**
 * @brief Test 2.4GHz channel validation
 */
void test_wifi_c6_tx_channel_2_4ghz_valid(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 1,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(1, g_mock_channel);

    config.channel = 13;
    wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(13, g_mock_channel);
}

/**
 * @brief Test 2.4GHz channel validation with invalid channel
 */
void test_wifi_c6_tx_channel_2_4ghz_invalid(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 0,  /* Invalid */
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    config.channel = 15;  /* Invalid */
    ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test 5GHz channel validation
 */
void test_wifi_c6_tx_channel_5ghz_valid(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_5GHZ,
        .channel = 36,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(36, g_mock_channel);

    config.channel = 165;
    wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(165, g_mock_channel);
}

/**
 * @brief Test 5GHz channel validation with invalid channel
 */
void test_wifi_c6_tx_channel_5ghz_invalid(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_5GHZ,
        .channel = 37,  /* Invalid */
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    config.channel = 166;  /* Invalid */
    ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test TX power validation - lower bound
 */
void test_wifi_c6_tx_power_min(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 5,  /* Minimum */
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(5 * 4, g_mock_tx_power);  /* Converted to 0.25dBm units */
}

/**
 * @brief Test TX power validation - upper bound
 */
void test_wifi_c6_tx_power_max(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 20,  /* Maximum */
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(20 * 4, g_mock_tx_power);
}

/**
 * @brief Test TX power validation - below minimum
 */
void test_wifi_c6_tx_power_below_min(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 4,  /* Below minimum */
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test TX power validation - above maximum
 */
void test_wifi_c6_tx_power_above_max(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 21,  /* Above maximum */
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test MCS configuration validation
 */
void test_wifi_c6_tx_mcs_all_indices(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    /* Test all valid MCS indices */
    for (int mcs = WIFI_C6_MCS_0; mcs <= WIFI_C6_MCS_11; mcs++) {
        setUp();  /* Reset mocks */
        config.mcs = (wifi_c6_mcs_t)mcs;
        esp_err_t ret = wifi_c6_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @brief Test FEC initialization when enabled
 */
void test_wifi_c6_tx_init_fec_enabled(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = true,
        .fec_k = 6,
        .fec_n = 12,
        .tx_queue_size = 32,
    };

    esp_err_t ret = wifi_c6_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test start transmission without initialization
 */
void test_wifi_c6_tx_start_not_initialized(void)
{
    esp_err_t ret = wifi_c6_tx_start();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test send video without initialization
 */
void test_wifi_c6_tx_send_video_not_initialized(void)
{
    uint8_t data[100] = {0};
    esp_err_t ret = wifi_c6_tx_send_video(data, sizeof(data), 1, false, 0, 0, 0,
                                          WIFI_C6_PRIORITY_NORMAL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test send video with NULL data
 */
void test_wifi_c6_tx_send_video_null_data(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);
    wifi_c6_tx_start();

    esp_err_t ret = wifi_c6_tx_send_video(NULL, 100, 1, false, 0, 0, 0,
                                          WIFI_C6_PRIORITY_NORMAL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test send video with zero size
 */
void test_wifi_c6_tx_send_video_zero_size(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);
    wifi_c6_tx_start();

    uint8_t data[100] = {0};
    esp_err_t ret = wifi_c6_tx_send_video(data, 0, 1, false, 0, 0, 0,
                                          WIFI_C6_PRIORITY_NORMAL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test send video with oversized data
 */
void test_wifi_c6_tx_send_video_oversized(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);
    wifi_c6_tx_start();

    uint8_t *data = malloc(2000);  /* > 1500 bytes */
    esp_err_t ret = wifi_c6_tx_send_video(data, 2000, 1, false, 0, 0, 0,
                                          WIFI_C6_PRIORITY_NORMAL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
    free(data);
}

/**
 * @brief Test send telemetry without initialization
 */
void test_wifi_c6_tx_send_telemetry_not_initialized(void)
{
    uint8_t data[100] = {0};
    esp_err_t ret = wifi_c6_tx_send_telemetry(data, sizeof(data));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test send telemetry with NULL data
 */
void test_wifi_c6_tx_send_telemetry_null_data(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);
    wifi_c6_tx_start();

    esp_err_t ret = wifi_c6_tx_send_telemetry(NULL, 100);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test get statistics with NULL pointer
 */
void test_wifi_c6_tx_get_stats_null(void)
{
    esp_err_t ret = wifi_c6_tx_get_stats(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test get statistics after init
 */
void test_wifi_c6_tx_get_stats_valid(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);

    wifi_c6_tx_stats_t stats;
    esp_err_t ret = wifi_c6_tx_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(0, stats.packets_sent);
    TEST_ASSERT_EQUAL(0, stats.bytes_sent);
}

/**
 * @brief Test reset statistics
 */
void test_wifi_c6_tx_reset_stats(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);

    esp_err_t ret = wifi_c6_tx_reset_stats();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    wifi_c6_tx_stats_t stats;
    wifi_c6_tx_get_stats(&stats);
    TEST_ASSERT_EQUAL(0, stats.packets_sent);
}

/**
 * @brief Test set channel without initialization
 */
void test_wifi_c6_tx_set_channel_not_initialized(void)
{
    esp_err_t ret = wifi_c6_tx_set_channel(6);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test set MCS without initialization
 */
void test_wifi_c6_tx_set_mcs_not_initialized(void)
{
    esp_err_t ret = wifi_c6_tx_set_mcs(WIFI_C6_MCS_7);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test set TX power without initialization
 */
void test_wifi_c6_tx_set_power_not_initialized(void)
{
    esp_err_t ret = wifi_c6_tx_set_power(15);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test set MCS with invalid value
 */
void test_wifi_c6_tx_set_mcs_invalid(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);

    /* Try to set MCS beyond valid range */
    esp_err_t ret = wifi_c6_tx_set_mcs(12);  /* Invalid */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test set power with invalid value
 */
void test_wifi_c6_tx_set_power_invalid(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);

    esp_err_t ret = wifi_c6_tx_set_power(25);  /* Above maximum */
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test video with keyframe priority
 */
void test_wifi_c6_tx_send_video_keyframe(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);
    wifi_c6_tx_start();

    uint8_t data[100] = {0};
    /* Keyframes should use high priority queue */
    esp_err_t ret = wifi_c6_tx_send_video(data, sizeof(data), 5, true, 0, 0, 0,
                                          WIFI_C6_PRIORITY_NORMAL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test priority queue management
 */
void test_wifi_c6_tx_priority_levels(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);
    wifi_c6_tx_start();

    uint8_t data[100] = {0};

    /* Test all priority levels */
    esp_err_t ret_low = wifi_c6_tx_send_video(data, sizeof(data), 1, false, 0, 0, 0,
                                              WIFI_C6_PRIORITY_LOW);
    TEST_ASSERT_EQUAL(ESP_OK, ret_low);

    esp_err_t ret_normal = wifi_c6_tx_send_video(data, sizeof(data), 1, false, 0, 0, 0,
                                                 WIFI_C6_PRIORITY_NORMAL);
    TEST_ASSERT_EQUAL(ESP_OK, ret_normal);

    esp_err_t ret_high = wifi_c6_tx_send_video(data, sizeof(data), 1, false, 0, 0, 0,
                                               WIFI_C6_PRIORITY_HIGH);
    TEST_ASSERT_EQUAL(ESP_OK, ret_high);
}

/**
 * @brief Test stop transmission
 */
void test_wifi_c6_tx_stop(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);
    wifi_c6_tx_start();

    esp_err_t ret = wifi_c6_tx_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test stop when not running
 */
void test_wifi_c6_tx_stop_not_running(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    wifi_c6_tx_init(&config);

    esp_err_t ret = wifi_c6_tx_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test various 2.4GHz channels
 */
void test_wifi_c6_tx_all_2_4ghz_channels(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    uint8_t valid_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

    for (int i = 0; i < sizeof(valid_channels); i++) {
        setUp();  /* Reset mocks */
        config.channel = valid_channels[i];
        esp_err_t ret = wifi_c6_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(valid_channels[i], g_mock_channel);
    }
}

/**
 * @brief Test various 5GHz channels
 */
void test_wifi_c6_tx_all_5ghz_channels(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_5GHZ,
        .mcs = WIFI_C6_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    uint8_t valid_channels[] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165};

    for (int i = 0; i < sizeof(valid_channels); i++) {
        setUp();  /* Reset mocks */
        config.channel = valid_channels[i];
        esp_err_t ret = wifi_c6_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(valid_channels[i], g_mock_channel);
    }
}

/**
 * @brief Test FEC K/N configurations
 */
void test_wifi_c6_tx_fec_configurations(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = true,
        .tx_queue_size = 32,
    };

    /* Test various FEC ratios */
    int fec_pairs[][2] = {{4, 8}, {6, 12}, {8, 16}};

    for (int i = 0; i < sizeof(fec_pairs) / sizeof(fec_pairs[0]); i++) {
        setUp();  /* Reset mocks */
        config.fec_k = fec_pairs[i][0];
        config.fec_n = fec_pairs[i][1];
        esp_err_t ret = wifi_c6_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @brief Test all TX power values
 */
void test_wifi_c6_tx_all_power_levels(void)
{
    wifi_c6_tx_config_t config = {
        .band = WIFI_C6_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_C6_MCS_5,
        .enable_fec = false,
        .tx_queue_size = 32,
    };

    for (int power = 5; power <= 20; power++) {
        setUp();  /* Reset mocks */
        config.tx_power_dbm = power;
        esp_err_t ret = wifi_c6_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(power * 4, g_mock_tx_power);
    }
}

/* ==================== TEST SUITE ==================== */

int run_wifi_c6_transmitter_tests(void)
{
    UNITY_BEGIN();

    /* Initialization tests */
    RUN_TEST(test_wifi_c6_tx_init_default);
    RUN_TEST(test_wifi_c6_tx_init_null_config);
    RUN_TEST(test_wifi_c6_tx_init_double_init);

    /* Channel validation tests */
    RUN_TEST(test_wifi_c6_tx_channel_2_4ghz_valid);
    RUN_TEST(test_wifi_c6_tx_channel_2_4ghz_invalid);
    RUN_TEST(test_wifi_c6_tx_channel_5ghz_valid);
    RUN_TEST(test_wifi_c6_tx_channel_5ghz_invalid);
    RUN_TEST(test_wifi_c6_tx_all_2_4ghz_channels);
    RUN_TEST(test_wifi_c6_tx_all_5ghz_channels);

    /* TX Power tests */
    RUN_TEST(test_wifi_c6_tx_power_min);
    RUN_TEST(test_wifi_c6_tx_power_max);
    RUN_TEST(test_wifi_c6_tx_power_below_min);
    RUN_TEST(test_wifi_c6_tx_power_above_max);
    RUN_TEST(test_wifi_c6_tx_all_power_levels);

    /* MCS tests */
    RUN_TEST(test_wifi_c6_tx_mcs_all_indices);

    /* FEC tests */
    RUN_TEST(test_wifi_c6_tx_init_fec_enabled);
    RUN_TEST(test_wifi_c6_tx_fec_configurations);

    /* Video transmission tests */
    RUN_TEST(test_wifi_c6_tx_start_not_initialized);
    RUN_TEST(test_wifi_c6_tx_send_video_not_initialized);
    RUN_TEST(test_wifi_c6_tx_send_video_null_data);
    RUN_TEST(test_wifi_c6_tx_send_video_zero_size);
    RUN_TEST(test_wifi_c6_tx_send_video_oversized);
    RUN_TEST(test_wifi_c6_tx_send_video_keyframe);

    /* Telemetry tests */
    RUN_TEST(test_wifi_c6_tx_send_telemetry_not_initialized);
    RUN_TEST(test_wifi_c6_tx_send_telemetry_null_data);

    /* Priority queue tests */
    RUN_TEST(test_wifi_c6_tx_priority_levels);

    /* Statistics tests */
    RUN_TEST(test_wifi_c6_tx_get_stats_null);
    RUN_TEST(test_wifi_c6_tx_get_stats_valid);
    RUN_TEST(test_wifi_c6_tx_reset_stats);

    /* Channel/MCS/Power configuration tests */
    RUN_TEST(test_wifi_c6_tx_set_channel_not_initialized);
    RUN_TEST(test_wifi_c6_tx_set_mcs_not_initialized);
    RUN_TEST(test_wifi_c6_tx_set_power_not_initialized);
    RUN_TEST(test_wifi_c6_tx_set_mcs_invalid);
    RUN_TEST(test_wifi_c6_tx_set_power_invalid);

    /* Lifecycle tests */
    RUN_TEST(test_wifi_c6_tx_stop);
    RUN_TEST(test_wifi_c6_tx_stop_not_running);

    return UNITY_END();
}

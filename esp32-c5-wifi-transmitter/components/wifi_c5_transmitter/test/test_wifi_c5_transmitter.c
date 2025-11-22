/**
 * @file test_wifi_c5_transmitter.c
 * @brief Comprehensive unit tests for WiFi 6 (ESP32-C5) transmitter
 *
 * Test Coverage:
 * - Initialization with various configurations
 * - Channel validation for 2.4GHz and 5GHz bands
 * - MCS configuration and validation
 * - TX power settings and range validation
 * - Video packet transmission
 * - Telemetry packet transmission
 * - OSD data transmission
 * - FEC integration
 * - Statistics tracking
 * - Channel scanning
 * - Channel information
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

#include "wifi_c5_transmitter.h"

static const char *TAG = "TEST_WIFI_C5_TX";

/* ==================== MOCK IMPLEMENTATIONS ==================== */

/* Mock FEC encoder */
typedef struct {
    uint8_t k;
    uint8_t n;
    size_t mtu;
} fec_encoder_t;

/* Mock WiFi types */
typedef enum {
    WIFI_STORAGE_FLASH = 0,
    WIFI_STORAGE_RAM = 1,
} wifi_storage_t;

typedef enum {
    WIFI_SECOND_CHAN_NONE = 0,
    WIFI_SECOND_CHAN_ABOVE = 1,
    WIFI_SECOND_CHAN_BELOW = 2,
} wifi_second_chan_t;

typedef struct {
    char cc[3];
    uint8_t schan;
    uint8_t nchan;
    int8_t max_tx_power;
    int8_t policy;
} wifi_country_t;

typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA = 1,
    WIFI_MODE_AP = 2,
    WIFI_MODE_APSTA = 3,
} wifi_mode_t;

typedef struct {
    uint8_t dummy[32];
} wifi_config_t;

typedef struct {
    uint32_t dummy;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() {.dummy = 0}

typedef enum {
    ESP_IF_WIFI_STA = 0,
    ESP_IF_WIFI_AP = 1,
} esp_interface_t;

typedef enum {
    WIFI_COUNTRY_POLICY_AUTO = 0,
    WIFI_COUNTRY_POLICY_MANUAL = 1,
} wifi_country_policy_t;

/* Global mock variables */
static uint32_t g_mock_tx_calls = 0;
static uint32_t g_mock_tx_errors = 0;
static uint32_t g_mock_channel = 0;
static uint8_t g_mock_tx_power = 0;
static bool g_mock_nvs_initialized = false;
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

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t pxTaskCode, const char *const pcName,
                                  const uint32_t usStackDepth, void *const pvParameters,
                                  UBaseType_t uxPriority, TaskHandle_t *const pxCreatedTask,
                                  const BaseType_t xCoreID)
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

/* Mock NVS functions */
typedef int nvs_handle_t;

esp_err_t nvs_flash_init(void)
{
    g_mock_nvs_initialized = true;
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void)
{
    return ESP_OK;
}

/* Mock WiFi functions */
esp_err_t esp_netif_init(void)
{
    return ESP_OK;
}

esp_err_t esp_event_loop_create_default(void)
{
    return ESP_OK;
}

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

esp_err_t esp_wifi_set_storage(wifi_storage_t storage)
{
    return ESP_OK;
}

esp_err_t esp_wifi_set_mode(wifi_mode_t mode)
{
    return ESP_OK;
}

esp_err_t esp_wifi_set_config(esp_interface_t ifx, wifi_config_t *conf)
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

esp_err_t esp_wifi_set_country(const wifi_country_t *country)
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

esp_err_t esp_wifi_80211_tx(esp_interface_t ifx, const void *buffer,
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
fec_encoder_t *fec_encoder_create(uint8_t k, uint8_t n, size_t mtu)
{
    fec_encoder_t *encoder = malloc(sizeof(fec_encoder_t));
    if (encoder) {
        encoder->k = k;
        encoder->n = n;
        encoder->mtu = mtu;
    }
    return encoder;
}

void fec_encoder_destroy(fec_encoder_t *encoder)
{
    free(encoder);
}

esp_err_t fec_encoder_encode(fec_encoder_t *enc, const uint8_t *data, size_t size)
{
    if (!enc || !data) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

/* ==================== TEST FIXTURES ==================== */

void setUp(void)
{
    g_mock_tx_calls = 0;
    g_mock_tx_errors = 0;
    g_mock_channel = 0;
    g_mock_tx_power = 0;
    g_mock_nvs_initialized = false;
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
void test_wifi_c5_tx_init_default(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .retry_count = 0,
        .air_device_id = 0x1234,
        .gs_device_id = 0,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    esp_err_t ret = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_TRUE(g_mock_nvs_initialized);
    TEST_ASSERT_TRUE(g_mock_wifi_initialized);
}

/**
 * @brief Test initialization with NULL config
 */
void test_wifi_c5_tx_init_null_config(void)
{
    esp_err_t ret = wifi_tx_init(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test double initialization
 */
void test_wifi_c5_tx_init_double_init(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .retry_count = 0,
        .air_device_id = 0x1234,
        .gs_device_id = 0,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    esp_err_t ret1 = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    esp_err_t ret2 = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret2);
}

/**
 * @brief Test 2.4GHz channel validation
 */
void test_wifi_c5_tx_channel_2_4ghz_valid(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 1,
        .mcs = WIFI_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    esp_err_t ret = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(1, g_mock_channel);
}

/**
 * @brief Test 5GHz channel validation
 */
void test_wifi_c5_tx_channel_5ghz_valid(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_5GHZ,
        .channel = 36,
        .mcs = WIFI_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    esp_err_t ret = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(36, g_mock_channel);
}

/**
 * @brief Test TX power validation - lower bound
 */
void test_wifi_c5_tx_power_min(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_5,
        .tx_power_dbm = 5,  /* Minimum */
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    esp_err_t ret = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(5 * 4, g_mock_tx_power);  /* Converted to 0.25dBm units */
}

/**
 * @brief Test TX power validation - upper bound
 */
void test_wifi_c5_tx_power_max(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_5,
        .tx_power_dbm = 20,  /* Maximum */
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    esp_err_t ret = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(20 * 4, g_mock_tx_power);
}

/**
 * @brief Test MCS configuration validation
 */
void test_wifi_c5_tx_mcs_all_indices(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    /* Test all valid MCS indices */
    for (int mcs = WIFI_MCS_0; mcs <= WIFI_MCS_11; mcs++) {
        setUp();  /* Reset mocks */
        config.mcs = (wifi_mcs_index_t)mcs;
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @brief Test FEC initialization when enabled
 */
void test_wifi_c5_tx_init_fec_enabled(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = true,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .retry_count = 0,
        .air_device_id = 0x1234,
        .gs_device_id = 0,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    esp_err_t ret = wifi_tx_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @brief Test start transmission without initialization
 */
void test_wifi_c5_tx_start_not_initialized(void)
{
    esp_err_t ret = wifi_tx_start();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test send video without initialization
 */
void test_wifi_c5_tx_send_video_not_initialized(void)
{
    uint8_t data[100] = {0};
    esp_err_t ret = wifi_tx_send_video(data, sizeof(data), 1, false, 0, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test send video with NULL data
 */
void test_wifi_c5_tx_send_video_null_data(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    wifi_tx_init(&config);
    wifi_tx_start();

    esp_err_t ret = wifi_tx_send_video(NULL, 100, 1, false, 0, 0);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test send telemetry without initialization
 */
void test_wifi_c5_tx_send_telemetry_not_initialized(void)
{
    uint8_t data[100] = {0};
    esp_err_t ret = wifi_tx_send_telemetry(data, sizeof(data));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test send OSD without initialization
 */
void test_wifi_c5_tx_send_osd_not_initialized(void)
{
    uint8_t data[100] = {0};
    esp_err_t ret = wifi_tx_send_osd(data, sizeof(data));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test get statistics with NULL pointer
 */
void test_wifi_c5_tx_get_stats_null(void)
{
    esp_err_t ret = wifi_tx_get_stats(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test get statistics after init
 */
void test_wifi_c5_tx_get_stats_valid(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    wifi_tx_init(&config);

    wifi_tx_stats_t stats;
    esp_err_t ret = wifi_tx_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(0, stats.packets_sent);
    TEST_ASSERT_EQUAL(0, stats.bytes_sent);
}

/**
 * @brief Test reset statistics
 */
void test_wifi_c5_tx_reset_stats(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    wifi_tx_init(&config);

    esp_err_t ret = wifi_tx_reset_stats();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    wifi_tx_stats_t stats;
    wifi_tx_get_stats(&stats);
    TEST_ASSERT_EQUAL(0, stats.packets_sent);
}

/**
 * @brief Test set channel without initialization
 */
void test_wifi_c5_tx_set_channel_not_initialized(void)
{
    esp_err_t ret = wifi_tx_set_channel(6);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test set MCS without initialization
 */
void test_wifi_c5_tx_set_mcs_not_initialized(void)
{
    esp_err_t ret = wifi_tx_set_mcs(WIFI_MCS_7);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test set TX power without initialization
 */
void test_wifi_c5_tx_set_power_not_initialized(void)
{
    esp_err_t ret = wifi_tx_set_power(15);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test stop transmission
 */
void test_wifi_c5_tx_stop(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    wifi_tx_init(&config);
    wifi_tx_start();

    esp_err_t ret = wifi_tx_stop();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @brief Test deinitialize
 */
void test_wifi_c5_tx_deinit(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    wifi_tx_init(&config);

    esp_err_t ret = wifi_tx_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_FALSE(g_mock_wifi_initialized);
}

/**
 * @brief Test get channel information
 */
void test_wifi_c5_tx_get_channel_info_2_4ghz(void)
{
    uint16_t freq_mhz = 0;
    int8_t max_power = 0;

    esp_err_t ret = wifi_tx_get_channel_info(6, &freq_mhz, &max_power);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(2407 + (6 * 5), freq_mhz);
    TEST_ASSERT_EQUAL(20, max_power);
}

/**
 * @brief Test get channel information for 5GHz
 */
void test_wifi_c5_tx_get_channel_info_5ghz(void)
{
    uint16_t freq_mhz = 0;
    int8_t max_power = 0;

    esp_err_t ret = wifi_tx_get_channel_info(36, &freq_mhz, &max_power);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(5000 + (36 * 5), freq_mhz);
    TEST_ASSERT_EQUAL(23, max_power);
}

/**
 * @brief Test get channel information with invalid channel
 */
void test_wifi_c5_tx_get_channel_info_invalid(void)
{
    uint16_t freq_mhz = 0;
    int8_t max_power = 0;

    esp_err_t ret = wifi_tx_get_channel_info(200, &freq_mhz, &max_power);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test get channel information with NULL pointers
 */
void test_wifi_c5_tx_get_channel_info_null(void)
{
    esp_err_t ret = wifi_tx_get_channel_info(6, NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test scan best channel
 */
void test_wifi_c5_tx_scan_best_channel(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    wifi_tx_init(&config);

    uint8_t best_channel = 0;
    int8_t noise_floor = 0;

    esp_err_t ret = wifi_tx_scan_best_channel(WIFI_BAND_2_4GHZ, &best_channel,
                                              &noise_floor, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(7, best_channel);  /* Default value */
    TEST_ASSERT_EQUAL(-90, noise_floor);
}

/**
 * @brief Test scan best channel with 5GHz
 */
void test_wifi_c5_tx_scan_best_channel_5ghz(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_5GHZ,
        .channel = 36,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    wifi_tx_init(&config);

    uint8_t best_channel = 0;
    int8_t noise_floor = 0;

    esp_err_t ret = wifi_tx_scan_best_channel(WIFI_BAND_5GHZ, &best_channel,
                                              &noise_floor, 1000);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(36, best_channel);  /* Default value */
}

/**
 * @brief Test scan best channel without initialization
 */
void test_wifi_c5_tx_scan_best_channel_not_initialized(void)
{
    uint8_t best_channel = 0;
    int8_t noise_floor = 0;

    esp_err_t ret = wifi_tx_scan_best_channel(WIFI_BAND_2_4GHZ, &best_channel,
                                              &noise_floor, 1000);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @brief Test all 2.4GHz channels
 */
void test_wifi_c5_tx_all_2_4ghz_channels(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .mcs = WIFI_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    uint8_t valid_channels[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

    for (int i = 0; i < sizeof(valid_channels); i++) {
        setUp();  /* Reset mocks */
        config.channel = valid_channels[i];
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(valid_channels[i], g_mock_channel);
    }
}

/**
 * @brief Test all 5GHz channels
 */
void test_wifi_c5_tx_all_5ghz_channels(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_5GHZ,
        .mcs = WIFI_MCS_5,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    uint8_t valid_channels[] = {36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140, 144, 149, 153, 157, 161, 165};

    for (int i = 0; i < sizeof(valid_channels); i++) {
        setUp();  /* Reset mocks */
        config.channel = valid_channels[i];
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(valid_channels[i], g_mock_channel);
    }
}

/**
 * @brief Test FEC configurations
 */
void test_wifi_c5_tx_fec_configurations(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = true,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    /* Test various FEC ratios */
    int fec_pairs[][2] = {{4, 8}, {6, 12}, {8, 16}};

    for (int i = 0; i < sizeof(fec_pairs) / sizeof(fec_pairs[0]); i++) {
        setUp();  /* Reset mocks */
        config.fec_k = fec_pairs[i][0];
        config.fec_n = fec_pairs[i][1];
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @brief Test all TX power values
 */
void test_wifi_c5_tx_all_power_levels(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_5,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    for (int power = 5; power <= 20; power++) {
        setUp();  /* Reset mocks */
        config.tx_power_dbm = power;
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        TEST_ASSERT_EQUAL(power * 4, g_mock_tx_power);
    }
}

/**
 * @brief Test different MTU sizes
 */
void test_wifi_c5_tx_various_mtu_sizes(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    size_t mtu_sizes[] = {512, 1024, 1500, 2048};

    for (int i = 0; i < sizeof(mtu_sizes) / sizeof(mtu_sizes[0]); i++) {
        setUp();  /* Reset mocks */
        config.mtu = mtu_sizes[i];
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @brief Test retry count configurations
 */
void test_wifi_c5_tx_retry_counts(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    for (int retry = 0; retry <= 15; retry++) {
        setUp();  /* Reset mocks */
        config.retry_count = retry;
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @brief Test device ID configurations
 */
void test_wifi_c5_tx_device_ids(void)
{
    wifi_tx_config_t config = {
        .band = WIFI_BAND_2_4GHZ,
        .channel = 6,
        .mcs = WIFI_MCS_7,
        .tx_power_dbm = 15,
        .enable_fec = false,
        .fec_k = 6,
        .fec_n = 12,
        .mtu = 1500,
        .tx_queue_size = 32,
        .tx_task_priority = 5,
        .tx_task_core = 0,
    };

    uint16_t device_ids[] = {0x0001, 0x1234, 0xFFFF};

    for (int i = 0; i < sizeof(device_ids) / sizeof(device_ids[0]); i++) {
        setUp();  /* Reset mocks */
        config.air_device_id = device_ids[i];
        config.gs_device_id = device_ids[i];
        esp_err_t ret = wifi_tx_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/* ==================== TEST SUITE ==================== */

int run_wifi_c5_transmitter_tests(void)
{
    UNITY_BEGIN();

    /* Initialization tests */
    RUN_TEST(test_wifi_c5_tx_init_default);
    RUN_TEST(test_wifi_c5_tx_init_null_config);
    RUN_TEST(test_wifi_c5_tx_init_double_init);

    /* Channel validation tests */
    RUN_TEST(test_wifi_c5_tx_channel_2_4ghz_valid);
    RUN_TEST(test_wifi_c5_tx_channel_5ghz_valid);
    RUN_TEST(test_wifi_c5_tx_all_2_4ghz_channels);
    RUN_TEST(test_wifi_c5_tx_all_5ghz_channels);

    /* TX Power tests */
    RUN_TEST(test_wifi_c5_tx_power_min);
    RUN_TEST(test_wifi_c5_tx_power_max);
    RUN_TEST(test_wifi_c5_tx_all_power_levels);

    /* MCS tests */
    RUN_TEST(test_wifi_c5_tx_mcs_all_indices);

    /* FEC tests */
    RUN_TEST(test_wifi_c5_tx_init_fec_enabled);
    RUN_TEST(test_wifi_c5_tx_fec_configurations);

    /* Video transmission tests */
    RUN_TEST(test_wifi_c5_tx_start_not_initialized);
    RUN_TEST(test_wifi_c5_tx_send_video_not_initialized);
    RUN_TEST(test_wifi_c5_tx_send_video_null_data);

    /* Telemetry and OSD tests */
    RUN_TEST(test_wifi_c5_tx_send_telemetry_not_initialized);
    RUN_TEST(test_wifi_c5_tx_send_osd_not_initialized);

    /* Statistics tests */
    RUN_TEST(test_wifi_c5_tx_get_stats_null);
    RUN_TEST(test_wifi_c5_tx_get_stats_valid);
    RUN_TEST(test_wifi_c5_tx_reset_stats);

    /* Channel/MCS/Power configuration tests */
    RUN_TEST(test_wifi_c5_tx_set_channel_not_initialized);
    RUN_TEST(test_wifi_c5_tx_set_mcs_not_initialized);
    RUN_TEST(test_wifi_c5_tx_set_power_not_initialized);

    /* Channel information tests */
    RUN_TEST(test_wifi_c5_tx_get_channel_info_2_4ghz);
    RUN_TEST(test_wifi_c5_tx_get_channel_info_5ghz);
    RUN_TEST(test_wifi_c5_tx_get_channel_info_invalid);
    RUN_TEST(test_wifi_c5_tx_get_channel_info_null);

    /* Channel scanning tests */
    RUN_TEST(test_wifi_c5_tx_scan_best_channel);
    RUN_TEST(test_wifi_c5_tx_scan_best_channel_5ghz);
    RUN_TEST(test_wifi_c5_tx_scan_best_channel_not_initialized);

    /* Configuration tests */
    RUN_TEST(test_wifi_c5_tx_fec_configurations);
    RUN_TEST(test_wifi_c5_tx_various_mtu_sizes);
    RUN_TEST(test_wifi_c5_tx_retry_counts);
    RUN_TEST(test_wifi_c5_tx_device_ids);

    /* Lifecycle tests */
    RUN_TEST(test_wifi_c5_tx_stop);
    RUN_TEST(test_wifi_c5_tx_deinit);

    return UNITY_END();
}

// ESP-IDF stub headers for syntax checking
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Error codes
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM -2
#define ESP_ERR_INVALID_ARG -3
#define ESP_ERR_INVALID_STATE -4
#define ESP_ERR_NOT_FOUND -5

// Logging stubs
#define ESP_LOGI(tag, fmt, ...) printf("[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) printf("[%s] WARN: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, fmt, ...) printf("[%s] ERROR: " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...)
#define ESP_LOGV(tag, fmt, ...)

#define ESP_ERROR_CHECK(x) do { esp_err_t __err = (x); if (__err != ESP_OK) { printf("Error: %d\n", __err); } } while(0)

// Memory
#define MALLOC_CAP_INTERNAL 0x01
#define MALLOC_CAP_SPIRAM 0x02
#define MALLOC_CAP_8BIT 0x04
#define MALLOC_CAP_DMA 0x08

static inline void* heap_caps_malloc(size_t size, uint32_t caps) { return malloc(size); }
static inline size_t heap_caps_get_free_size(uint32_t caps) { return 1024*1024; }
static inline uint32_t esp_get_free_heap_size(void) { return 1024*1024; }

// WiFi stubs
typedef enum {
    WIFI_MODE_NULL = 0,
    WIFI_MODE_STA,
    WIFI_MODE_AP,
    WIFI_MODE_APSTA
} wifi_mode_t;

typedef enum {
    WIFI_SECOND_CHAN_NONE = 0,
    WIFI_SECOND_CHAN_ABOVE,
    WIFI_SECOND_CHAN_BELOW
} wifi_second_chan_t;

typedef struct {
    int rssi;
    int sig_len;
} wifi_rx_ctrl_t;

typedef struct {
    wifi_rx_ctrl_t rx_ctrl;
    uint8_t payload[0];
} wifi_promiscuous_pkt_t;

typedef enum {
    WIFI_PKT_MGMT,
    WIFI_PKT_CTRL,
    WIFI_PKT_DATA,
    WIFI_PKT_MISC
} wifi_promiscuous_pkt_type_t;

typedef struct {
    uint32_t filter_mask;
} wifi_promiscuous_filter_t;

#define WIFI_PROMIS_FILTER_MASK_DATA 0x01

typedef void (*wifi_promiscuous_cb_t)(void *buf, wifi_promiscuous_pkt_type_t type);

typedef struct {
    int magic;
} wifi_init_config_t;

#define WIFI_INIT_CONFIG_DEFAULT() {0}

static inline esp_err_t esp_wifi_init(const wifi_init_config_t *config) { return ESP_OK; }
static inline esp_err_t esp_wifi_set_mode(wifi_mode_t mode) { return ESP_OK; }
static inline esp_err_t esp_wifi_start(void) { return ESP_OK; }
static inline esp_err_t esp_wifi_stop(void) { return ESP_OK; }
static inline esp_err_t esp_wifi_set_channel(uint8_t primary, wifi_second_chan_t second) { return ESP_OK; }
static inline esp_err_t esp_wifi_set_promiscuous(bool en) { return ESP_OK; }
static inline esp_err_t esp_wifi_set_promiscuous_filter(const wifi_promiscuous_filter_t *filter) { return ESP_OK; }
static inline esp_err_t esp_wifi_set_promiscuous_rx_cb(wifi_promiscuous_cb_t cb) { return ESP_OK; }

// FreeRTOS stubs
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;
typedef void* QueueHandle_t;
typedef int BaseType_t;
typedef unsigned int TickType_t;
typedef unsigned int UBaseType_t;

#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY 0xFFFFFFFF
#define portTICK_PERIOD_MS 1
#define configMAX_PRIORITIES 25

static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void*)1; }
static inline void vSemaphoreDelete(SemaphoreHandle_t sem) {}
static inline BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t wait) { return pdTRUE; }
static inline BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) { return pdTRUE; }
static inline TickType_t xTaskGetTickCount(void) { return 0; }
static inline void vTaskDelay(TickType_t ticks) {}
static inline void vTaskDelete(TaskHandle_t task) {}

#define pdMS_TO_TICKS(ms) (ms)

typedef void (*TaskFunction_t)(void*);
static inline BaseType_t xTaskCreatePinnedToCore(
    TaskFunction_t fn, const char* name, uint32_t stack,
    void* param, UBaseType_t priority, TaskHandle_t* handle, int core) {
    return pdTRUE;
}

// NVS stubs
static inline esp_err_t nvs_flash_init(void) { return ESP_OK; }
static inline esp_err_t nvs_flash_erase(void) { return ESP_OK; }
#define ESP_ERR_NVS_NO_FREE_PAGES -100
#define ESP_ERR_NVS_NEW_VERSION_FOUND -101

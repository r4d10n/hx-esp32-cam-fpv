/**
 * @file esp32_ipc_master.c
 * @brief IPC Master Implementation (ESP32-P4 side)
 */

#include "esp32_ipc.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "IPC_MASTER";

// External stats functions
extern void ipc_update_stats_tx(size_t bytes);
extern void ipc_update_stats_error(uint32_t error_type);

typedef struct {
    ipc_master_config_t config;
    bool initialized;

    spi_device_handle_t spi_handle;
    SemaphoreHandle_t mutex;

    uint16_t tx_sequence;
    uint8_t *tx_buffer;
} ipc_master_context_t;

static ipc_master_context_t s_ctx = {0};

/**
 * @brief Check if slave is ready
 */
bool ipc_master_is_slave_ready(void)
{
    if (!s_ctx.initialized) {
        return false;
    }

    // Check handshake GPIO
    return gpio_get_level(s_ctx.config.handshake_pin) == 1;
}

/**
 * @brief Initialize IPC master
 */
esp_err_t ipc_master_init(const ipc_master_config_t *config)
{
    if (s_ctx.initialized) {
        ESP_LOGE(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing IPC master...");

    memcpy(&s_ctx.config, config, sizeof(ipc_master_config_t));

    // Configure handshake GPIO as input
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->handshake_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->mosi_pin,
        .miso_io_num = config->miso_pin,
        .sclk_io_num = config->clk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->max_transfer_size,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure SPI device
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = config->spi_clock_hz,
        .mode = 0,  // SPI mode 0
        .spics_io_num = config->cs_pin,
        .queue_size = 4,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &s_ctx.spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        spi_bus_free(SPI2_HOST);
        return ret;
    }

    // Allocate TX buffer
    s_ctx.tx_buffer = heap_caps_malloc(config->max_transfer_size, MALLOC_CAP_DMA);
    if (!s_ctx.tx_buffer) {
        ESP_LOGE(TAG, "Failed to allocate TX buffer");
        spi_bus_remove_device(s_ctx.spi_handle);
        spi_bus_free(SPI2_HOST);
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    s_ctx.mutex = xSemaphoreCreateMutex();
    if (!s_ctx.mutex) {
        heap_caps_free(s_ctx.tx_buffer);
        spi_bus_remove_device(s_ctx.spi_handle);
        spi_bus_free(SPI2_HOST);
        return ESP_ERR_NO_MEM;
    }

    s_ctx.tx_sequence = 0;
    s_ctx.initialized = true;

    ESP_LOGI(TAG, "IPC master initialized (SPI @ %u Hz)", config->spi_clock_hz);
    return ESP_OK;
}

/**
 * @brief Deinitialize IPC master
 */
esp_err_t ipc_master_deinit(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.mutex) {
        vSemaphoreDelete(s_ctx.mutex);
        s_ctx.mutex = NULL;
    }

    if (s_ctx.tx_buffer) {
        heap_caps_free(s_ctx.tx_buffer);
        s_ctx.tx_buffer = NULL;
    }

    if (s_ctx.spi_handle) {
        spi_bus_remove_device(s_ctx.spi_handle);
        s_ctx.spi_handle = NULL;
    }

    spi_bus_free(SPI2_HOST);

    s_ctx.initialized = false;
    ESP_LOGI(TAG, "IPC master deinitialized");

    return ESP_OK;
}

/**
 * @brief Send packet
 */
esp_err_t ipc_master_send(
    ipc_packet_type_t type,
    uint8_t flags,
    const void *payload,
    size_t payload_size,
    uint32_t timeout_ms)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (payload_size > s_ctx.config.max_transfer_size - sizeof(ipc_packet_header_t)) {
        ESP_LOGE(TAG, "Payload too large: %u", payload_size);
        return ESP_ERR_INVALID_SIZE;
    }

    // Wait for slave to be ready
    if (!ipc_master_is_slave_ready()) {
        ESP_LOGW(TAG, "Slave not ready");
        return ESP_ERR_TIMEOUT;
    }

    if (xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    // Build packet
    ipc_packet_header_t *header = (ipc_packet_header_t *)s_ctx.tx_buffer;
    header->sync[0] = IPC_SYNC_BYTE_0;
    header->sync[1] = IPC_SYNC_BYTE_1;
    header->type = type;
    header->flags = flags;
    header->sequence = s_ctx.tx_sequence++;
    header->fragment_index = 0;
    header->payload_size = payload_size;

    // Copy payload
    if (payload && payload_size > 0) {
        memcpy(s_ctx.tx_buffer + sizeof(ipc_packet_header_t), payload, payload_size);
    }

    // Calculate CRC
    header->crc16 = ipc_crc16(s_ctx.tx_buffer, sizeof(ipc_packet_header_t) + payload_size - 2);

    // Send via SPI
    size_t total_size = sizeof(ipc_packet_header_t) + payload_size;

    spi_transaction_t trans = {
        .length = total_size * 8,  // bits
        .tx_buffer = s_ctx.tx_buffer,
        .rx_buffer = NULL,
    };

    esp_err_t ret = spi_device_transmit(s_ctx.spi_handle, &trans);

    xSemaphoreGive(s_ctx.mutex);

    if (ret == ESP_OK) {
        ipc_update_stats_tx(total_size);
    } else {
        ESP_LOGE(TAG, "SPI transmit failed: %s", esp_err_to_name(ret));
        ipc_update_stats_error(1);  // Timeout error
    }

    return ret;
}

/**
 * @brief Send video packet
 */
esp_err_t ipc_master_send_video(
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint8_t nal_type,
    bool is_keyframe,
    uint32_t frame_index,
    uint64_t pts_us,
    uint64_t dts_us)
{
    if (!nalu_data || nalu_size == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    // Build video payload
    size_t payload_size = sizeof(ipc_video_payload_t) + nalu_size;
    uint8_t *payload_buffer = malloc(payload_size);

    if (!payload_buffer) {
        return ESP_ERR_NO_MEM;
    }

    ipc_video_payload_t *video_payload = (ipc_video_payload_t *)payload_buffer;
    video_payload->nal_type = nal_type;
    video_payload->is_keyframe = is_keyframe;
    video_payload->frame_index = frame_index;
    video_payload->pts_us = pts_us;
    video_payload->dts_us = dts_us;
    video_payload->nalu_size = nalu_size;

    // Copy NAL data
    memcpy(payload_buffer + sizeof(ipc_video_payload_t), nalu_data, nalu_size);

    // Send packet
    esp_err_t ret = ipc_master_send(
        IPC_PACKET_TYPE_VIDEO,
        is_keyframe ? IPC_FLAG_PRIORITY : 0,
        payload_buffer,
        payload_size,
        100  // 100ms timeout
    );

    free(payload_buffer);
    return ret;
}

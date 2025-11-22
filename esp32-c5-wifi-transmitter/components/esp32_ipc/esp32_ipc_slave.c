/**
 * @file esp32_ipc_slave.c
 * @brief IPC Slave Implementation (ESP32-C5 side)
 */

#include "esp32_ipc.h"
#include "esp_log.h"
#include "driver/spi_slave.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "IPC_SLAVE";

// External stats functions
extern void ipc_update_stats_rx(size_t bytes);
extern void ipc_update_stats_tx(size_t bytes);
extern void ipc_update_stats_error(uint32_t error_type);

typedef struct {
    ipc_slave_config_t config;
    bool initialized;
    bool running;

    ipc_receive_cb_t receive_callback;
    void *receive_callback_user_data;

    QueueHandle_t rx_queue;
    TaskHandle_t rx_task_handle;

    uint8_t *rx_buffer;
    uint8_t *tx_buffer;

    SemaphoreHandle_t ready_mutex;
    bool ready;
} ipc_slave_context_t;

static ipc_slave_context_t s_ctx = {0};

// Received packet structure
typedef struct {
    ipc_packet_type_t type;
    size_t payload_size;
    uint8_t *payload_data;
} rx_packet_t;

/**
 * @brief RX task - process received packets
 */
static void rx_task(void *pvParameters)
{
    ESP_LOGI(TAG, "RX task started");

    rx_packet_t packet;

    while (s_ctx.running) {
        if (xQueueReceive(s_ctx.rx_queue, &packet, portMAX_DELAY) == pdTRUE) {
            // Call user callback
            if (s_ctx.receive_callback) {
                s_ctx.receive_callback(
                    packet.type,
                    packet.payload_data,
                    packet.payload_size,
                    s_ctx.receive_callback_user_data
                );
            }

            // Free payload
            if (packet.payload_data) {
                free(packet.payload_data);
            }
        }
    }

    ESP_LOGI(TAG, "RX task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief SPI post-transaction callback
 */
static void IRAM_ATTR spi_post_trans_cb(spi_slave_transaction_t *trans)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if (trans->trans_len > 0) {
        // Parse received data
        ipc_packet_header_t *header = (ipc_packet_header_t *)s_ctx.rx_buffer;

        // Verify sync bytes
        if (header->sync[0] == IPC_SYNC_BYTE_0 && header->sync[1] == IPC_SYNC_BYTE_1) {
            // Verify CRC
            uint16_t calc_crc = ipc_crc16(s_ctx.rx_buffer,
                                         sizeof(ipc_packet_header_t) + header->payload_size - 2);

            if (calc_crc == header->crc16) {
                // Valid packet - queue for processing
                rx_packet_t packet = {
                    .type = (ipc_packet_type_t)header->type,
                    .payload_size = header->payload_size,
                    .payload_data = NULL,
                };

                if (header->payload_size > 0) {
                    packet.payload_data = malloc(header->payload_size);
                    if (packet.payload_data) {
                        memcpy(packet.payload_data,
                              s_ctx.rx_buffer + sizeof(ipc_packet_header_t),
                              header->payload_size);
                    }
                }

                xQueueSendFromISR(s_ctx.rx_queue, &packet, &xHigherPriorityTaskWoken);

                ipc_update_stats_rx(sizeof(ipc_packet_header_t) + header->payload_size);
            } else {
                ipc_update_stats_error(0);  // CRC error
            }
        }
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief Initialize IPC slave
 */
esp_err_t ipc_slave_init(const ipc_slave_config_t *config)
{
    if (s_ctx.initialized) {
        ESP_LOGE(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing IPC slave...");

    memcpy(&s_ctx.config, config, sizeof(ipc_slave_config_t));

    // Configure handshake GPIO as output
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->handshake_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(config->handshake_pin, 0);  // Not ready initially

    // Configure SPI slave
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->mosi_pin,
        .miso_io_num = config->miso_pin,
        .sclk_io_num = config->clk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };

    spi_slave_interface_config_t slvcfg = {
        .mode = 0,
        .spics_io_num = config->cs_pin,
        .queue_size = 3,
        .flags = 0,
        .post_setup_cb = NULL,
        .post_trans_cb = spi_post_trans_cb,
    };

    esp_err_t ret = spi_slave_initialize(SPI2_HOST, &buscfg, &slvcfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI slave init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Allocate buffers
    size_t buffer_size = 4096;  // Default buffer size

    s_ctx.rx_buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);
    s_ctx.tx_buffer = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA);

    if (!s_ctx.rx_buffer || !s_ctx.tx_buffer) {
        ESP_LOGE(TAG, "Failed to allocate buffers");
        if (s_ctx.rx_buffer) heap_caps_free(s_ctx.rx_buffer);
        if (s_ctx.tx_buffer) heap_caps_free(s_ctx.tx_buffer);
        spi_slave_free(SPI2_HOST);
        return ESP_ERR_NO_MEM;
    }

    // Create RX queue
    s_ctx.rx_queue = xQueueCreate(config->queue_size, sizeof(rx_packet_t));
    if (!s_ctx.rx_queue) {
        heap_caps_free(s_ctx.rx_buffer);
        heap_caps_free(s_ctx.tx_buffer);
        spi_slave_free(SPI2_HOST);
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    s_ctx.ready_mutex = xSemaphoreCreateMutex();
    if (!s_ctx.ready_mutex) {
        vQueueDelete(s_ctx.rx_queue);
        heap_caps_free(s_ctx.rx_buffer);
        heap_caps_free(s_ctx.tx_buffer);
        spi_slave_free(SPI2_HOST);
        return ESP_ERR_NO_MEM;
    }

    s_ctx.initialized = true;
    s_ctx.running = true;
    s_ctx.ready = false;

    // Create RX task
    xTaskCreate(rx_task, "ipc_rx", 8192, NULL, configMAX_PRIORITIES - 2,
                &s_ctx.rx_task_handle);

    ESP_LOGI(TAG, "IPC slave initialized");
    return ESP_OK;
}

/**
 * @brief Deinitialize IPC slave
 */
esp_err_t ipc_slave_deinit(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.running = false;

    if (s_ctx.rx_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_ctx.rx_task_handle = NULL;
    }

    if (s_ctx.rx_queue) {
        vQueueDelete(s_ctx.rx_queue);
        s_ctx.rx_queue = NULL;
    }

    if (s_ctx.ready_mutex) {
        vSemaphoreDelete(s_ctx.ready_mutex);
        s_ctx.ready_mutex = NULL;
    }

    if (s_ctx.rx_buffer) {
        heap_caps_free(s_ctx.rx_buffer);
        s_ctx.rx_buffer = NULL;
    }

    if (s_ctx.tx_buffer) {
        heap_caps_free(s_ctx.tx_buffer);
        s_ctx.tx_buffer = NULL;
    }

    spi_slave_free(SPI2_HOST);

    s_ctx.initialized = false;
    ESP_LOGI(TAG, "IPC slave deinitialized");

    return ESP_OK;
}

/**
 * @brief Register receive callback
 */
esp_err_t ipc_slave_register_callback(
    ipc_receive_cb_t callback,
    void *user_data)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.receive_callback = callback;
    s_ctx.receive_callback_user_data = user_data;

    return ESP_OK;
}

/**
 * @brief Set ready state
 */
esp_err_t ipc_slave_set_ready(bool ready)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_ctx.ready_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        s_ctx.ready = ready;
        gpio_set_level(s_ctx.config.handshake_pin, ready ? 1 : 0);
        xSemaphoreGive(s_ctx.ready_mutex);
    }

    return ESP_OK;
}

/**
 * @brief Send response (not commonly used in current design)
 */
esp_err_t ipc_slave_send_response(
    ipc_packet_type_t type,
    const void *payload,
    size_t payload_size)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Implement slave-to-master communication if needed
    ESP_LOGW(TAG, "Slave-to-master not fully implemented");

    return ESP_OK;
}

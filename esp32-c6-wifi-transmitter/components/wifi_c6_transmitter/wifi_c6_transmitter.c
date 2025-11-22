/**
 * @file wifi_c6_transmitter.c
 * @brief WiFi 6 Transmitter Implementation for ESP32-C6
 */

#include "wifi_c6_transmitter.h"
#include "fec_encoder.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "wifi_c6_tx";

#define WIFI_C6_TX_TASK_STACK_SIZE  (8192)
#define WIFI_C6_TX_TASK_PRIORITY    (configMAX_PRIORITIES - 2)
#define WIFI_C6_MAX_PACKET_SIZE     (1500)
#define WIFI_C6_STATS_UPDATE_MS     (1000)

/**
 * @brief WiFi packet structure for TX queue
 */
typedef struct {
    uint8_t data[WIFI_C6_MAX_PACKET_SIZE];
    size_t size;
    wifi_c6_priority_t priority;
    uint64_t timestamp_us;
} wifi_c6_tx_packet_t;

/**
 * @brief WiFi transmitter context
 */
typedef struct {
    wifi_c6_tx_config_t config;
    bool initialized;
    bool running;

    // TX queue (priority queue)
    QueueHandle_t tx_queue_high;
    QueueHandle_t tx_queue_normal;
    QueueHandle_t tx_queue_low;

    TaskHandle_t tx_task;
    SemaphoreHandle_t stats_mutex;

    // FEC encoder
    fec_encoder_t *fec_encoder;

    // Statistics
    wifi_c6_tx_stats_t stats;
    uint64_t last_stats_update_us;
    uint64_t bytes_sent_last_interval;

} wifi_c6_tx_context_t;

static wifi_c6_tx_context_t s_tx_ctx = {0};

/**
 * @brief Channel mapping for 2.4GHz band
 */
static const uint8_t s_channels_2_4ghz[] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14
};

/**
 * @brief Channel mapping for 5GHz band
 */
static const uint8_t s_channels_5ghz[] = {
    36, 40, 44, 48, 52, 56, 60, 64,
    100, 104, 108, 112, 116, 120, 124, 128,
    132, 136, 140, 144, 149, 153, 157, 161, 165
};

/**
 * @brief Validate channel for selected band
 */
static bool wifi_c6_validate_channel(wifi_c6_band_t band, uint8_t channel)
{
    if (band == WIFI_C6_BAND_2_4GHZ) {
        for (int i = 0; i < sizeof(s_channels_2_4ghz); i++) {
            if (s_channels_2_4ghz[i] == channel) {
                return true;
            }
        }
    } else {
        for (int i = 0; i < sizeof(s_channels_5ghz); i++) {
            if (s_channels_5ghz[i] == channel) {
                return true;
            }
        }
    }
    return false;
}

/**
 * @brief WiFi transmission task
 */
static void wifi_c6_tx_task(void *arg)
{
    wifi_c6_tx_packet_t packet;
    TickType_t max_wait = pdMS_TO_TICKS(10);

    ESP_LOGI(TAG, "WiFi TX task started");

    while (s_tx_ctx.running) {
        bool packet_received = false;

        // Check queues in priority order
        if (xQueueReceive(s_tx_ctx.tx_queue_high, &packet, 0) == pdTRUE) {
            packet_received = true;
        } else if (xQueueReceive(s_tx_ctx.tx_queue_normal, &packet, 0) == pdTRUE) {
            packet_received = true;
        } else if (xQueueReceive(s_tx_ctx.tx_queue_low, &packet, max_wait) == pdTRUE) {
            packet_received = true;
        }

        if (packet_received) {
            // Apply FEC if enabled
            if (s_tx_ctx.config.enable_fec && s_tx_ctx.fec_encoder) {
                uint8_t fec_output[WIFI_C6_MAX_PACKET_SIZE * 2];
                size_t fec_output_size = 0;

                esp_err_t ret = fec_encode(
                    s_tx_ctx.fec_encoder,
                    packet.data,
                    packet.size,
                    fec_output,
                    &fec_output_size
                );

                if (ret == ESP_OK) {
                    // Send FEC-encoded packet
                    esp_err_t tx_ret = esp_wifi_80211_tx(
                        WIFI_IF_STA,
                        fec_output,
                        fec_output_size,
                        false
                    );

                    if (tx_ret == ESP_OK) {
                        xSemaphoreTake(s_tx_ctx.stats_mutex, portMAX_DELAY);
                        s_tx_ctx.stats.packets_sent++;
                        s_tx_ctx.stats.bytes_sent += fec_output_size;
                        s_tx_ctx.stats.fec_blocks_sent += (s_tx_ctx.config.fec_n - s_tx_ctx.config.fec_k);
                        s_tx_ctx.bytes_sent_last_interval += fec_output_size;
                        xSemaphoreGive(s_tx_ctx.stats_mutex);
                    } else {
                        xSemaphoreTake(s_tx_ctx.stats_mutex, portMAX_DELAY);
                        s_tx_ctx.stats.packets_failed++;
                        xSemaphoreGive(s_tx_ctx.stats_mutex);
                        ESP_LOGW(TAG, "WiFi TX failed: %d", tx_ret);
                    }
                } else {
                    ESP_LOGE(TAG, "FEC encode failed: %d", ret);
                }
            } else {
                // Send packet without FEC
                esp_err_t tx_ret = esp_wifi_80211_tx(
                    WIFI_IF_STA,
                    packet.data,
                    packet.size,
                    false
                );

                if (tx_ret == ESP_OK) {
                    xSemaphoreTake(s_tx_ctx.stats_mutex, portMAX_DELAY);
                    s_tx_ctx.stats.packets_sent++;
                    s_tx_ctx.stats.bytes_sent += packet.size;
                    s_tx_ctx.bytes_sent_last_interval += packet.size;
                    xSemaphoreGive(s_tx_ctx.stats_mutex);
                } else {
                    xSemaphoreTake(s_tx_ctx.stats_mutex, portMAX_DELAY);
                    s_tx_ctx.stats.packets_failed++;
                    xSemaphoreGive(s_tx_ctx.stats_mutex);
                    ESP_LOGW(TAG, "WiFi TX failed: %d", tx_ret);
                }
            }
        }

        // Update throughput statistics
        uint64_t now_us = esp_timer_get_time();
        uint64_t elapsed_us = now_us - s_tx_ctx.last_stats_update_us;

        if (elapsed_us >= (WIFI_C6_STATS_UPDATE_MS * 1000)) {
            xSemaphoreTake(s_tx_ctx.stats_mutex, portMAX_DELAY);

            // Calculate throughput in Mbps
            s_tx_ctx.stats.throughput_mbps =
                (s_tx_ctx.bytes_sent_last_interval * 8.0f * 1000000.0f) /
                (elapsed_us * 1024.0f * 1024.0f);

            // Calculate queue usage
            UBaseType_t high_count = uxQueueMessagesWaiting(s_tx_ctx.tx_queue_high);
            UBaseType_t normal_count = uxQueueMessagesWaiting(s_tx_ctx.tx_queue_normal);
            UBaseType_t low_count = uxQueueMessagesWaiting(s_tx_ctx.tx_queue_low);
            UBaseType_t total_count = high_count + normal_count + low_count;

            s_tx_ctx.stats.queue_usage_percent =
                (total_count * 100) / s_tx_ctx.config.tx_queue_size;

            s_tx_ctx.bytes_sent_last_interval = 0;
            s_tx_ctx.last_stats_update_us = now_us;

            xSemaphoreGive(s_tx_ctx.stats_mutex);
        }
    }

    ESP_LOGI(TAG, "WiFi TX task stopped");
    vTaskDelete(NULL);
}

esp_err_t wifi_c6_tx_init(const wifi_c6_tx_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_tx_ctx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate configuration
    if (!wifi_c6_validate_channel(config->band, config->channel)) {
        ESP_LOGE(TAG, "Invalid channel %d for band %d", config->channel, config->band);
        return ESP_ERR_INVALID_ARG;
    }

    if (config->tx_power_dbm < 5 || config->tx_power_dbm > 20) {
        ESP_LOGE(TAG, "Invalid TX power: %d dBm", config->tx_power_dbm);
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_tx_ctx.config, config, sizeof(wifi_c6_tx_config_t));

    // Create queues
    size_t queue_size_per_priority = config->tx_queue_size / 3;
    s_tx_ctx.tx_queue_high = xQueueCreate(queue_size_per_priority, sizeof(wifi_c6_tx_packet_t));
    s_tx_ctx.tx_queue_normal = xQueueCreate(queue_size_per_priority, sizeof(wifi_c6_tx_packet_t));
    s_tx_ctx.tx_queue_low = xQueueCreate(queue_size_per_priority, sizeof(wifi_c6_tx_packet_t));

    if (!s_tx_ctx.tx_queue_high || !s_tx_ctx.tx_queue_normal || !s_tx_ctx.tx_queue_low) {
        ESP_LOGE(TAG, "Failed to create TX queues");
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    s_tx_ctx.stats_mutex = xSemaphoreCreateMutex();
    if (!s_tx_ctx.stats_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }

    // Initialize FEC encoder if enabled
    if (config->enable_fec) {
        fec_encoder_config_t fec_config = {
            .k = config->fec_k,
            .n = config->fec_n
        };

        s_tx_ctx.fec_encoder = fec_encoder_create(&fec_config);
        if (!s_tx_ctx.fec_encoder) {
            ESP_LOGE(TAG, "Failed to create FEC encoder");
            return ESP_ERR_NO_MEM;
        }
    }

    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Configure WiFi for monitor mode / packet injection
    wifi_config_t wifi_config = {0};
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Set channel
    ESP_ERROR_CHECK(esp_wifi_set_channel(config->channel, WIFI_SECOND_CHAN_NONE));

    // Set TX power
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(config->tx_power_dbm * 4)); // Convert dBm to 0.25dBm units

    ESP_LOGI(TAG, "WiFi C6 TX initialized: band=%d, channel=%d, MCS=%d, power=%d dBm",
             config->band, config->channel, config->mcs, config->tx_power_dbm);

    if (config->enable_fec) {
        ESP_LOGI(TAG, "FEC enabled: K=%d, N=%d", config->fec_k, config->fec_n);
    }

    s_tx_ctx.initialized = true;
    s_tx_ctx.last_stats_update_us = esp_timer_get_time();

    return ESP_OK;
}

esp_err_t wifi_c6_tx_start(void)
{
    if (!s_tx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_tx_ctx.running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }

    s_tx_ctx.running = true;

    // Create TX task
    BaseType_t ret = xTaskCreate(
        wifi_c6_tx_task,
        "wifi_c6_tx",
        WIFI_C6_TX_TASK_STACK_SIZE,
        NULL,
        WIFI_C6_TX_TASK_PRIORITY,
        &s_tx_ctx.tx_task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TX task");
        s_tx_ctx.running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WiFi C6 TX started");
    return ESP_OK;
}

esp_err_t wifi_c6_tx_stop(void)
{
    if (!s_tx_ctx.running) {
        return ESP_OK;
    }

    s_tx_ctx.running = false;

    // Wait for task to stop
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "WiFi C6 TX stopped");
    return ESP_OK;
}

esp_err_t wifi_c6_tx_send_video(
    const uint8_t *nalu_data, size_t nalu_size,
    uint8_t nal_type, bool is_keyframe,
    uint32_t frame_index, uint64_t pts_us, uint64_t dts_us,
    wifi_c6_priority_t priority)
{
    if (!s_tx_ctx.initialized || !s_tx_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!nalu_data || nalu_size == 0 || nalu_size > WIFI_C6_MAX_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_c6_tx_packet_t packet;
    memcpy(packet.data, nalu_data, nalu_size);
    packet.size = nalu_size;
    packet.priority = priority;
    packet.timestamp_us = esp_timer_get_time();

    // Select queue based on priority (keyframes always go to high priority)
    QueueHandle_t queue;
    if (is_keyframe || priority == WIFI_C6_PRIORITY_HIGH) {
        queue = s_tx_ctx.tx_queue_high;
    } else if (priority == WIFI_C6_PRIORITY_NORMAL) {
        queue = s_tx_ctx.tx_queue_normal;
    } else {
        queue = s_tx_ctx.tx_queue_low;
    }

    // Send to queue (don't block if full, just drop)
    if (xQueueSend(queue, &packet, 0) != pdTRUE) {
        ESP_LOGW(TAG, "TX queue full, packet dropped");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t wifi_c6_tx_send_telemetry(
    const uint8_t *telemetry_data, size_t telemetry_size)
{
    if (!s_tx_ctx.initialized || !s_tx_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!telemetry_data || telemetry_size == 0 || telemetry_size > WIFI_C6_MAX_PACKET_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_c6_tx_packet_t packet;
    memcpy(packet.data, telemetry_data, telemetry_size);
    packet.size = telemetry_size;
    packet.priority = WIFI_C6_PRIORITY_NORMAL;
    packet.timestamp_us = esp_timer_get_time();

    if (xQueueSend(s_tx_ctx.tx_queue_normal, &packet, 0) != pdTRUE) {
        ESP_LOGW(TAG, "TX queue full, telemetry dropped");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t wifi_c6_tx_get_stats(wifi_c6_tx_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_tx_ctx.stats_mutex, portMAX_DELAY);
    memcpy(stats, &s_tx_ctx.stats, sizeof(wifi_c6_tx_stats_t));
    xSemaphoreGive(s_tx_ctx.stats_mutex);

    return ESP_OK;
}

esp_err_t wifi_c6_tx_reset_stats(void)
{
    xSemaphoreTake(s_tx_ctx.stats_mutex, portMAX_DELAY);
    memset(&s_tx_ctx.stats, 0, sizeof(wifi_c6_tx_stats_t));
    s_tx_ctx.bytes_sent_last_interval = 0;
    s_tx_ctx.last_stats_update_us = esp_timer_get_time();
    xSemaphoreGive(s_tx_ctx.stats_mutex);

    return ESP_OK;
}

esp_err_t wifi_c6_tx_set_channel(uint8_t channel)
{
    if (!s_tx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!wifi_c6_validate_channel(s_tx_ctx.config.band, channel)) {
        ESP_LOGE(TAG, "Invalid channel %d for band %d", channel, s_tx_ctx.config.band);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (ret == ESP_OK) {
        s_tx_ctx.config.channel = channel;
        ESP_LOGI(TAG, "Channel changed to %d", channel);
    }

    return ret;
}

esp_err_t wifi_c6_tx_set_mcs(wifi_c6_mcs_t mcs)
{
    if (!s_tx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (mcs > WIFI_C6_MCS_11) {
        return ESP_ERR_INVALID_ARG;
    }

    s_tx_ctx.config.mcs = mcs;
    ESP_LOGI(TAG, "MCS changed to %d", mcs);

    return ESP_OK;
}

esp_err_t wifi_c6_tx_set_power(uint8_t tx_power_dbm)
{
    if (!s_tx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (tx_power_dbm < 5 || tx_power_dbm > 20) {
        ESP_LOGE(TAG, "Invalid TX power: %d dBm", tx_power_dbm);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_wifi_set_max_tx_power(tx_power_dbm * 4); // Convert to 0.25dBm units
    if (ret == ESP_OK) {
        s_tx_ctx.config.tx_power_dbm = tx_power_dbm;
        ESP_LOGI(TAG, "TX power changed to %d dBm", tx_power_dbm);
    }

    return ret;
}

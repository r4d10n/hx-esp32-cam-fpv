/**
 * @file wifi_c5_transmitter.c
 * @brief WiFi 6 Transmitter Implementation for ESP32-C5
 */

#include "wifi_c5_transmitter.h"
#include "fec_encoder_wifi.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "WIFI_TX";

// Extern FEC functions (placeholder - would use actual common/fec implementation)
typedef struct fec_encoder_s fec_encoder_t;
extern fec_encoder_t* fec_encoder_create(uint8_t k, uint8_t n, size_t mtu);
extern void fec_encoder_destroy(fec_encoder_t *enc);
extern esp_err_t fec_encoder_encode(fec_encoder_t *enc, const uint8_t *data, size_t size);

typedef struct {
    wifi_tx_config_t config;
    bool initialized;
    bool running;

    QueueHandle_t tx_queue;
    TaskHandle_t tx_task_handle;

    fec_encoder_t *fec_encoder;

    wifi_tx_stats_t stats;
    uint64_t stats_start_time_us;

    SemaphoreHandle_t mutex;
} wifi_tx_context_t;

static wifi_tx_context_t s_ctx = {0};

// TX packet structure
typedef struct {
    uint8_t *data;
    size_t size;
    uint8_t nal_type;
    bool is_keyframe;
    uint32_t frame_index;
    uint64_t pts_us;
} tx_packet_t;

/**
 * @brief WiFi packet injection callback (from FEC encoder)
 */
static esp_err_t wifi_inject_packet(const uint8_t *packet, size_t size)
{
    // TODO: Use actual ESP32-C5 packet injection API
    // This would call esp_wifi_80211_tx() or similar

    esp_err_t ret = ESP_OK;

    // Simulate packet injection
    // In real implementation:
    // ret = esp_wifi_80211_tx(ESP_IF_WIFI_STA, packet, size, true);

    if (ret == ESP_OK) {
        s_ctx.stats.packets_sent++;
        s_ctx.stats.bytes_sent += size;
    } else {
        s_ctx.stats.wifi_tx_errors++;
    }

    return ret;
}

/**
 * @brief TX task
 */
static void tx_task(void *pvParameters)
{
    ESP_LOGI(TAG, "TX task started");

    tx_packet_t *packet = NULL;

    while (s_ctx.running) {
        if (xQueueReceive(s_ctx.tx_queue, &packet, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (!packet) continue;

            // Encode with FEC
            if (s_ctx.config.enable_fec && s_ctx.fec_encoder) {
                fec_encoder_encode(s_ctx.fec_encoder, packet->data, packet->size);
            } else {
                // Direct transmission without FEC
                wifi_inject_packet(packet->data, packet->size);
            }

            // Update statistics
            if (packet->nal_type == 5) {  // IDR frame
                s_ctx.stats.video_packets_sent++;
            }

            // Free packet
            free(packet->data);
            free(packet);
            packet = NULL;
        }
    }

    ESP_LOGI(TAG, "TX task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Initialize WiFi transmitter
 */
esp_err_t wifi_tx_init(const wifi_tx_config_t *config)
{
    if (s_ctx.initialized) {
        ESP_LOGE(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing WiFi transmitter...");
    ESP_LOGI(TAG, "Band: %s, Channel: %d, MCS: %d",
             config->band == WIFI_BAND_2_4GHZ ? "2.4GHz" : "5GHz",
             config->channel, config->mcs);

    memcpy(&s_ctx.config, config, sizeof(wifi_tx_config_t));

    // Initialize NVS (required for WiFi)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set channel
    wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    ESP_ERROR_CHECK(esp_wifi_set_channel(config->channel, WIFI_SECOND_CHAN_NONE));

    // Set TX power
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(config->tx_power_dbm * 4));  // Convert dBm to 0.25dBm units

    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // Create FEC encoder if enabled
    if (config->enable_fec) {
        s_ctx.fec_encoder = fec_encoder_create(config->fec_k, config->fec_n, config->mtu);
        if (!s_ctx.fec_encoder) {
            ESP_LOGE(TAG, "Failed to create FEC encoder");
            return ESP_ERR_NO_MEM;
        }
        ESP_LOGI(TAG, "FEC enabled: %d/%d", config->fec_k, config->fec_n);
    }

    // Create TX queue
    s_ctx.tx_queue = xQueueCreate(config->tx_queue_size, sizeof(tx_packet_t*));
    if (!s_ctx.tx_queue) {
        ESP_LOGE(TAG, "Failed to create TX queue");
        if (s_ctx.fec_encoder) fec_encoder_destroy(s_ctx.fec_encoder);
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    s_ctx.mutex = xSemaphoreCreateMutex();
    if (!s_ctx.mutex) {
        vQueueDelete(s_ctx.tx_queue);
        if (s_ctx.fec_encoder) fec_encoder_destroy(s_ctx.fec_encoder);
        return ESP_ERR_NO_MEM;
    }

    // Initialize statistics
    memset(&s_ctx.stats, 0, sizeof(wifi_tx_stats_t));
    s_ctx.stats_start_time_us = esp_timer_get_time();

    s_ctx.initialized = true;

    ESP_LOGI(TAG, "WiFi transmitter initialized");
    return ESP_OK;
}

/**
 * @brief Deinitialize WiFi transmitter
 */
esp_err_t wifi_tx_deinit(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_ctx.running) {
        wifi_tx_stop();
    }

    if (s_ctx.mutex) {
        vSemaphoreDelete(s_ctx.mutex);
        s_ctx.mutex = NULL;
    }

    if (s_ctx.tx_queue) {
        // Clear queue
        tx_packet_t *packet;
        while (xQueueReceive(s_ctx.tx_queue, &packet, 0) == pdTRUE) {
            if (packet) {
                free(packet->data);
                free(packet);
            }
        }
        vQueueDelete(s_ctx.tx_queue);
        s_ctx.tx_queue = NULL;
    }

    if (s_ctx.fec_encoder) {
        fec_encoder_destroy(s_ctx.fec_encoder);
        s_ctx.fec_encoder = NULL;
    }

    esp_wifi_stop();
    esp_wifi_deinit();

    s_ctx.initialized = false;
    ESP_LOGI(TAG, "WiFi transmitter deinitialized");

    return ESP_OK;
}

/**
 * @brief Start WiFi transmission
 */
esp_err_t wifi_tx_start(void)
{
    if (!s_ctx.initialized || s_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Starting WiFi transmission...");

    s_ctx.running = true;

    // Create TX task
    BaseType_t ret = xTaskCreatePinnedToCore(
        tx_task,
        "wifi_tx",
        8192,
        NULL,
        s_ctx.config.tx_task_priority,
        &s_ctx.tx_task_handle,
        s_ctx.config.tx_task_core
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create TX task");
        s_ctx.running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WiFi transmission started");
    return ESP_OK;
}

/**
 * @brief Stop WiFi transmission
 */
esp_err_t wifi_tx_stop(void)
{
    if (!s_ctx.running) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Stopping WiFi transmission...");

    s_ctx.running = false;

    if (s_ctx.tx_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_ctx.tx_task_handle = NULL;
    }

    ESP_LOGI(TAG, "WiFi transmission stopped");
    return ESP_OK;
}

/**
 * @brief Send video NAL unit
 */
esp_err_t wifi_tx_send_video(
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint8_t nal_type,
    bool is_keyframe,
    uint32_t frame_index,
    uint64_t pts_us)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Create packet
    tx_packet_t *packet = malloc(sizeof(tx_packet_t));
    if (!packet) {
        return ESP_ERR_NO_MEM;
    }

    packet->data = malloc(nalu_size);
    if (!packet->data) {
        free(packet);
        return ESP_ERR_NO_MEM;
    }

    memcpy(packet->data, nalu_data, nalu_size);
    packet->size = nalu_size;
    packet->nal_type = nal_type;
    packet->is_keyframe = is_keyframe;
    packet->frame_index = frame_index;
    packet->pts_us = pts_us;

    // Queue packet
    if (xQueueSend(s_ctx.tx_queue, &packet, 0) != pdTRUE) {
        ESP_LOGW(TAG, "TX queue full, dropping packet");
        free(packet->data);
        free(packet);
        s_ctx.stats.packets_dropped++;
        return ESP_FAIL;
    }

    return ESP_OK;
}

/**
 * @brief Send telemetry data
 */
esp_err_t wifi_tx_send_telemetry(const uint8_t *data, size_t size)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Implement telemetry packet transmission
    s_ctx.stats.telemetry_packets_sent++;

    return ESP_OK;
}

/**
 * @brief Send OSD data
 */
esp_err_t wifi_tx_send_osd(const uint8_t *osd_buffer, size_t size)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // TODO: Implement OSD packet transmission

    return ESP_OK;
}

/**
 * @brief Set WiFi channel
 */
esp_err_t wifi_tx_set_channel(uint8_t channel)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config.channel = channel;
    return esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
}

/**
 * @brief Set TX power
 */
esp_err_t wifi_tx_set_power(int8_t power_dbm)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config.tx_power_dbm = power_dbm;
    return esp_wifi_set_max_tx_power(power_dbm * 4);
}

/**
 * @brief Set MCS rate
 */
esp_err_t wifi_tx_set_mcs(wifi_mcs_index_t mcs)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_ctx.config.mcs = mcs;
    // TODO: Set WiFi MCS rate (API-specific)

    return ESP_OK;
}

/**
 * @brief Get transmitter statistics
 */
esp_err_t wifi_tx_get_stats(wifi_tx_stats_t *stats)
{
    if (!s_ctx.initialized || !stats) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        // Calculate throughput
        uint64_t now = esp_timer_get_time();
        uint64_t elapsed_us = now - s_ctx.stats_start_time_us;

        if (elapsed_us > 0) {
            s_ctx.stats.throughput_mbps = (float)s_ctx.stats.bytes_sent * 8 * 1000000.0f /
                                         elapsed_us / 1000000.0f;
        }

        // Calculate queue usage
        UBaseType_t queue_items = uxQueueMessagesWaiting(s_ctx.tx_queue);
        s_ctx.stats.queue_usage_percent = (queue_items * 100) / s_ctx.config.tx_queue_size;

        memcpy(stats, &s_ctx.stats, sizeof(wifi_tx_stats_t));
        xSemaphoreGive(s_ctx.mutex);
    }

    return ESP_OK;
}

/**
 * @brief Reset statistics
 */
esp_err_t wifi_tx_reset_stats(void)
{
    if (!s_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (xSemaphoreTake(s_ctx.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memset(&s_ctx.stats, 0, sizeof(wifi_tx_stats_t));
        s_ctx.stats_start_time_us = esp_timer_get_time();
        xSemaphoreGive(s_ctx.mutex);
    }

    return ESP_OK;
}

/**
 * @brief Get channel information
 */
esp_err_t wifi_tx_get_channel_info(
    uint8_t channel,
    uint16_t *freq_mhz,
    int8_t *max_power_dbm)
{
    if (!freq_mhz || !max_power_dbm) {
        return ESP_ERR_INVALID_ARG;
    }

    // 2.4GHz channels
    if (channel >= 1 && channel <= 14) {
        *freq_mhz = 2407 + (channel * 5);
        *max_power_dbm = 20;  // Typical max
        return ESP_OK;
    }

    // 5GHz channels
    if (channel >= 36 && channel <= 165) {
        *freq_mhz = 5000 + (channel * 5);
        *max_power_dbm = 23;  // Typical max
        return ESP_OK;
    }

    return ESP_ERR_INVALID_ARG;
}

/**
 * @brief Scan for best channel
 */
esp_err_t wifi_tx_scan_best_channel(
    wifi_band_t band,
    uint8_t *best_channel,
    int8_t *noise_floor,
    uint32_t timeout_ms)
{
    if (!s_ctx.initialized || !best_channel || !noise_floor) {
        return ESP_ERR_INVALID_ARG;
    }

    // TODO: Implement channel scanning
    // Would use esp_wifi_scan_start() and analyze results

    // For now, return default channel
    *best_channel = band == WIFI_BAND_2_4GHZ ? 7 : 36;
    *noise_floor = -90;

    ESP_LOGI(TAG, "Best channel: %d, Noise: %d dBm", *best_channel, *noise_floor);

    return ESP_OK;
}

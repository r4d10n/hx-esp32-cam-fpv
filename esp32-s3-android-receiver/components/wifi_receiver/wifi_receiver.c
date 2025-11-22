/**
 * @file wifi_receiver.c
 * @brief WiFi Receiver Implementation for ESP32-S3
 */

#include "wifi_receiver.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi_rx";

#define WIFI_RX_TASK_STACK_SIZE     (8192)
#define WIFI_RX_TASK_PRIORITY       (configMAX_PRIORITIES - 2)
#define WIFI_RX_MAX_PAYLOAD_SIZE    (2346)  /* Max WiFi packet payload */
#define WIFI_RX_STATS_UPDATE_MS     (1000)

/**
 * @brief Internal packet structure for RX queue
 */
typedef struct {
    uint8_t payload[WIFI_RX_MAX_PAYLOAD_SIZE];
    size_t payload_len;
    wifi_rx_pkt_type_t pkt_type;
    int8_t rssi;
    uint8_t channel;
    uint8_t rate;
    uint64_t timestamp_us;
    uint32_t sequence_num;
    bool crc_valid;
    uint8_t src_mac[6];
    uint8_t dst_mac[6];
} wifi_rx_internal_packet_t;

/**
 * @brief WiFi receiver context
 */
typedef struct {
    wifi_rx_config_t config;
    bool initialized;
    bool running;

    // RX queue and task
    QueueHandle_t rx_queue;
    TaskHandle_t rx_task;
    TaskHandle_t hop_task;
    SemaphoreHandle_t stats_mutex;

    // Channel hopping
    bool hopping_enabled;
    uint8_t *hop_channels;
    size_t hop_channels_count;
    uint32_t hop_dwell_ms;
    size_t hop_index;

    // Statistics
    wifi_rx_stats_t stats;
    uint64_t last_stats_update_us;
    uint64_t bytes_received_last_interval;
    int32_t rssi_sum;
    uint32_t rssi_count;

    // Current state
    uint8_t current_channel;
    int8_t last_rssi;
    int8_t noise_floor;

} wifi_rx_context_t;

static wifi_rx_context_t s_rx_ctx = {0};

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
static bool wifi_rx_validate_channel(wifi_rx_band_t band, uint8_t channel)
{
    if (band == WIFI_RX_BAND_2_4GHZ) {
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
 * @brief Check if MAC address matches filter
 */
static bool wifi_rx_mac_matches_filter(const uint8_t *mac)
{
    // Check if filter is disabled (all zeros)
    bool filter_disabled = true;
    for (int i = 0; i < 6; i++) {
        if (s_rx_ctx.config.filter_mac[i] != 0) {
            filter_disabled = false;
            break;
        }
    }

    if (filter_disabled) {
        return true;
    }

    // Check if MAC matches filter
    return memcmp(mac, s_rx_ctx.config.filter_mac, 6) == 0;
}

/**
 * @brief WiFi promiscuous mode RX callback
 */
static void wifi_rx_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (!s_rx_ctx.running) {
        return;
    }

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;

    // Update statistics
    xSemaphoreTake(s_rx_ctx.stats_mutex, portMAX_DELAY);
    s_rx_ctx.stats.packets_received++;

    // Check CRC
    bool crc_ok = (pkt->rx_ctrl.sig_len > 0);
    if (crc_ok) {
        s_rx_ctx.stats.packets_valid++;
    } else {
        s_rx_ctx.stats.packets_crc_error++;
    }

    // Update RSSI statistics
    s_rx_ctx.last_rssi = pkt->rx_ctrl.rssi;
    s_rx_ctx.rssi_sum += pkt->rx_ctrl.rssi;
    s_rx_ctx.rssi_count++;

    if (pkt->rx_ctrl.rssi < s_rx_ctx.stats.min_rssi_dbm || s_rx_ctx.stats.packets_received == 1) {
        s_rx_ctx.stats.min_rssi_dbm = pkt->rx_ctrl.rssi;
    }
    if (pkt->rx_ctrl.rssi > s_rx_ctx.stats.max_rssi_dbm || s_rx_ctx.stats.packets_received == 1) {
        s_rx_ctx.stats.max_rssi_dbm = pkt->rx_ctrl.rssi;
    }

    xSemaphoreGive(s_rx_ctx.stats_mutex);

    // Filter CRC errors if configured
    if (s_rx_ctx.config.filter_crc_errors && !crc_ok) {
        return;
    }

    // Extract MAC addresses from payload (assuming 802.11 header)
    const uint8_t *payload = pkt->payload;
    uint8_t src_mac[6] = {0};
    uint8_t dst_mac[6] = {0};

    if (pkt->rx_ctrl.sig_len >= 24) {  // Minimum 802.11 header size
        // Destination MAC (Address 1)
        memcpy(dst_mac, &payload[4], 6);
        // Source MAC (Address 2)
        memcpy(src_mac, &payload[10], 6);
    }

    // Apply MAC filter
    if (!wifi_rx_mac_matches_filter(src_mac) && !wifi_rx_mac_matches_filter(dst_mac)) {
        return;
    }

    // Determine packet type
    wifi_rx_pkt_type_t pkt_type;
    switch (type) {
        case WIFI_PKT_MGMT:
            pkt_type = WIFI_RX_PKT_MGMT;
            break;
        case WIFI_PKT_CTRL:
            pkt_type = WIFI_RX_PKT_CTRL;
            break;
        case WIFI_PKT_DATA:
            pkt_type = WIFI_RX_PKT_DATA;
            break;
        default:
            pkt_type = WIFI_RX_PKT_UNKNOWN;
            break;
    }

    // Create internal packet
    wifi_rx_internal_packet_t int_pkt;
    int_pkt.payload_len = pkt->rx_ctrl.sig_len;
    if (int_pkt.payload_len > WIFI_RX_MAX_PAYLOAD_SIZE) {
        int_pkt.payload_len = WIFI_RX_MAX_PAYLOAD_SIZE;
    }

    memcpy(int_pkt.payload, pkt->payload, int_pkt.payload_len);
    int_pkt.pkt_type = pkt_type;
    int_pkt.rssi = pkt->rx_ctrl.rssi;
    int_pkt.channel = pkt->rx_ctrl.channel;
    int_pkt.rate = pkt->rx_ctrl.rate;
    int_pkt.timestamp_us = esp_timer_get_time();
    int_pkt.sequence_num = 0;  // Could extract from 802.11 header if needed
    int_pkt.crc_valid = crc_ok;
    memcpy(int_pkt.src_mac, src_mac, 6);
    memcpy(int_pkt.dst_mac, dst_mac, 6);

    // Send to queue
    if (xQueueSend(s_rx_ctx.rx_queue, &int_pkt, 0) != pdTRUE) {
        xSemaphoreTake(s_rx_ctx.stats_mutex, portMAX_DELAY);
        s_rx_ctx.stats.packets_dropped++;
        xSemaphoreGive(s_rx_ctx.stats_mutex);
        ESP_LOGW(TAG, "RX queue full, packet dropped");
    }
}

/**
 * @brief WiFi reception task
 */
static void wifi_rx_task(void *arg)
{
    wifi_rx_internal_packet_t int_pkt;
    TickType_t max_wait = pdMS_TO_TICKS(10);

    ESP_LOGI(TAG, "WiFi RX task started");

    while (s_rx_ctx.running) {
        if (xQueueReceive(s_rx_ctx.rx_queue, &int_pkt, max_wait) == pdTRUE) {
            // Update byte statistics
            xSemaphoreTake(s_rx_ctx.stats_mutex, portMAX_DELAY);
            s_rx_ctx.stats.bytes_received += int_pkt.payload_len;
            s_rx_ctx.bytes_received_last_interval += int_pkt.payload_len;
            xSemaphoreGive(s_rx_ctx.stats_mutex);

            // Call user callback if registered
            if (s_rx_ctx.config.callback) {
                wifi_rx_packet_info_t pkt_info = {
                    .payload = int_pkt.payload,
                    .payload_len = int_pkt.payload_len,
                    .pkt_type = int_pkt.pkt_type,
                    .rssi = int_pkt.rssi,
                    .channel = int_pkt.channel,
                    .rate = int_pkt.rate,
                    .timestamp_us = int_pkt.timestamp_us,
                    .sequence_num = int_pkt.sequence_num,
                    .crc_valid = int_pkt.crc_valid,
                };
                memcpy(pkt_info.src_mac, int_pkt.src_mac, 6);
                memcpy(pkt_info.dst_mac, int_pkt.dst_mac, 6);

                s_rx_ctx.config.callback(&pkt_info, s_rx_ctx.config.callback_ctx);
            }
        }

        // Update throughput statistics
        uint64_t now_us = esp_timer_get_time();
        uint64_t elapsed_us = now_us - s_rx_ctx.last_stats_update_us;

        if (elapsed_us >= (WIFI_RX_STATS_UPDATE_MS * 1000)) {
            xSemaphoreTake(s_rx_ctx.stats_mutex, portMAX_DELAY);

            // Calculate throughput in Mbps
            s_rx_ctx.stats.throughput_mbps =
                (s_rx_ctx.bytes_received_last_interval * 8.0f * 1000000.0f) /
                (elapsed_us * 1024.0f * 1024.0f);

            // Calculate average RSSI
            if (s_rx_ctx.rssi_count > 0) {
                s_rx_ctx.stats.avg_rssi_dbm = (int8_t)(s_rx_ctx.rssi_sum / s_rx_ctx.rssi_count);
            }

            // Calculate buffer usage
            UBaseType_t queue_count = uxQueueMessagesWaiting(s_rx_ctx.rx_queue);
            s_rx_ctx.stats.buffer_usage_percent =
                (queue_count * 100) / s_rx_ctx.config.rx_buffer_size;

            s_rx_ctx.bytes_received_last_interval = 0;
            s_rx_ctx.last_stats_update_us = now_us;

            xSemaphoreGive(s_rx_ctx.stats_mutex);
        }
    }

    ESP_LOGI(TAG, "WiFi RX task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Channel hopping task
 */
static void wifi_rx_hop_task(void *arg)
{
    ESP_LOGI(TAG, "WiFi RX hop task started");

    while (s_rx_ctx.running && s_rx_ctx.hopping_enabled) {
        if (s_rx_ctx.hop_channels && s_rx_ctx.hop_channels_count > 0) {
            uint8_t channel = s_rx_ctx.hop_channels[s_rx_ctx.hop_index];

            esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
            s_rx_ctx.current_channel = channel;

            ESP_LOGD(TAG, "Hopped to channel %d", channel);

            // Move to next channel
            s_rx_ctx.hop_index = (s_rx_ctx.hop_index + 1) % s_rx_ctx.hop_channels_count;

            // Dwell on this channel
            vTaskDelay(pdMS_TO_TICKS(s_rx_ctx.hop_dwell_ms));
        } else {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    ESP_LOGI(TAG, "WiFi RX hop task stopped");
    vTaskDelete(NULL);
}

esp_err_t wifi_rx_init(const wifi_rx_config_t *config)
{
    if (!config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (s_rx_ctx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // Validate configuration
    if (!wifi_rx_validate_channel(config->band, config->channel)) {
        ESP_LOGE(TAG, "Invalid channel %d for band %d", config->channel, config->band);
        return ESP_ERR_INVALID_ARG;
    }

    if (config->rx_buffer_size == 0) {
        ESP_LOGE(TAG, "RX buffer size must be > 0");
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&s_rx_ctx.config, config, sizeof(wifi_rx_config_t));

    // Create RX queue
    s_rx_ctx.rx_queue = xQueueCreate(config->rx_buffer_size, sizeof(wifi_rx_internal_packet_t));
    if (!s_rx_ctx.rx_queue) {
        ESP_LOGE(TAG, "Failed to create RX queue");
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    s_rx_ctx.stats_mutex = xSemaphoreCreateMutex();
    if (!s_rx_ctx.stats_mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        vQueueDelete(s_rx_ctx.rx_queue);
        return ESP_ERR_NO_MEM;
    }

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

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    wifi_config_t wifi_config = {0};
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Set channel
    s_rx_ctx.current_channel = config->channel;
    ESP_ERROR_CHECK(esp_wifi_set_channel(config->channel, WIFI_SECOND_CHAN_NONE));

    // Enable promiscuous mode if configured
    if (config->enable_promiscuous) {
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_rx_promiscuous_cb));

        // Set promiscuous filter to receive all packet types
        wifi_promiscuous_filter_t filter = {
            .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
        };
        ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));
    }

    // Initialize statistics
    memset(&s_rx_ctx.stats, 0, sizeof(wifi_rx_stats_t));
    s_rx_ctx.stats.min_rssi_dbm = 0;
    s_rx_ctx.stats.max_rssi_dbm = -100;
    s_rx_ctx.last_stats_update_us = esp_timer_get_time();
    s_rx_ctx.noise_floor = -95;  // Typical value

    ESP_LOGI(TAG, "WiFi RX initialized: band=%d, channel=%d, promiscuous=%d, buffer_size=%zu",
             config->band, config->channel, config->enable_promiscuous, config->rx_buffer_size);

    s_rx_ctx.initialized = true;

    return ESP_OK;
}

esp_err_t wifi_rx_deinit(void)
{
    if (!s_rx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Stop if running
    if (s_rx_ctx.running) {
        wifi_rx_stop();
    }

    // Disable promiscuous mode
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();
    esp_wifi_deinit();

    // Free resources
    if (s_rx_ctx.rx_queue) {
        vQueueDelete(s_rx_ctx.rx_queue);
        s_rx_ctx.rx_queue = NULL;
    }

    if (s_rx_ctx.stats_mutex) {
        vSemaphoreDelete(s_rx_ctx.stats_mutex);
        s_rx_ctx.stats_mutex = NULL;
    }

    if (s_rx_ctx.hop_channels) {
        free(s_rx_ctx.hop_channels);
        s_rx_ctx.hop_channels = NULL;
    }

    s_rx_ctx.initialized = false;

    ESP_LOGI(TAG, "WiFi RX deinitialized");
    return ESP_OK;
}

esp_err_t wifi_rx_start(void)
{
    if (!s_rx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_rx_ctx.running) {
        ESP_LOGW(TAG, "Already running");
        return ESP_OK;
    }

    s_rx_ctx.running = true;

    // Create RX task
    BaseType_t ret = xTaskCreate(
        wifi_rx_task,
        "wifi_rx",
        WIFI_RX_TASK_STACK_SIZE,
        NULL,
        WIFI_RX_TASK_PRIORITY,
        &s_rx_ctx.rx_task
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create RX task");
        s_rx_ctx.running = false;
        return ESP_FAIL;
    }

    // Create hopping task if enabled
    if (s_rx_ctx.hopping_enabled) {
        ret = xTaskCreate(
            wifi_rx_hop_task,
            "wifi_rx_hop",
            WIFI_RX_TASK_STACK_SIZE / 2,
            NULL,
            WIFI_RX_TASK_PRIORITY - 1,
            &s_rx_ctx.hop_task
        );

        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create hop task");
            s_rx_ctx.running = false;
            vTaskDelete(s_rx_ctx.rx_task);
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "WiFi RX started");
    return ESP_OK;
}

esp_err_t wifi_rx_stop(void)
{
    if (!s_rx_ctx.running) {
        return ESP_OK;
    }

    s_rx_ctx.running = false;

    // Wait for tasks to stop
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "WiFi RX stopped");
    return ESP_OK;
}

esp_err_t wifi_rx_set_channel(uint8_t channel)
{
    if (!s_rx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_rx_ctx.hopping_enabled) {
        ESP_LOGW(TAG, "Cannot set channel while hopping is enabled");
        return ESP_ERR_INVALID_STATE;
    }

    if (!wifi_rx_validate_channel(s_rx_ctx.config.band, channel)) {
        ESP_LOGE(TAG, "Invalid channel %d for band %d", channel, s_rx_ctx.config.band);
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (ret == ESP_OK) {
        s_rx_ctx.current_channel = channel;
        s_rx_ctx.config.channel = channel;
        ESP_LOGI(TAG, "Channel changed to %d", channel);
    }

    return ret;
}

esp_err_t wifi_rx_get_channel(uint8_t *channel)
{
    if (!channel) {
        return ESP_ERR_INVALID_ARG;
    }

    *channel = s_rx_ctx.current_channel;
    return ESP_OK;
}

esp_err_t wifi_rx_set_mac_filter(const uint8_t mac[6])
{
    if (!mac) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(s_rx_ctx.config.filter_mac, mac, 6);

    ESP_LOGI(TAG, "MAC filter set to %02x:%02x:%02x:%02x:%02x:%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    return ESP_OK;
}

esp_err_t wifi_rx_set_promiscuous(bool enable)
{
    if (!s_rx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_wifi_set_promiscuous(enable);
    if (ret == ESP_OK) {
        s_rx_ctx.config.enable_promiscuous = enable;

        if (enable) {
            esp_wifi_set_promiscuous_rx_cb(wifi_rx_promiscuous_cb);
            wifi_promiscuous_filter_t filter = {
                .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
            };
            esp_wifi_set_promiscuous_filter(&filter);
        }

        ESP_LOGI(TAG, "Promiscuous mode %s", enable ? "enabled" : "disabled");
    }

    return ret;
}

esp_err_t wifi_rx_get_stats(wifi_rx_stats_t *stats)
{
    if (!stats) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_rx_ctx.stats_mutex, portMAX_DELAY);
    memcpy(stats, &s_rx_ctx.stats, sizeof(wifi_rx_stats_t));
    xSemaphoreGive(s_rx_ctx.stats_mutex);

    return ESP_OK;
}

esp_err_t wifi_rx_reset_stats(void)
{
    xSemaphoreTake(s_rx_ctx.stats_mutex, portMAX_DELAY);
    memset(&s_rx_ctx.stats, 0, sizeof(wifi_rx_stats_t));
    s_rx_ctx.stats.min_rssi_dbm = 0;
    s_rx_ctx.stats.max_rssi_dbm = -100;
    s_rx_ctx.bytes_received_last_interval = 0;
    s_rx_ctx.rssi_sum = 0;
    s_rx_ctx.rssi_count = 0;
    s_rx_ctx.last_stats_update_us = esp_timer_get_time();
    xSemaphoreGive(s_rx_ctx.stats_mutex);

    ESP_LOGI(TAG, "Statistics reset");
    return ESP_OK;
}

esp_err_t wifi_rx_get_rssi(int8_t *rssi)
{
    if (!rssi) {
        return ESP_ERR_INVALID_ARG;
    }

    *rssi = s_rx_ctx.last_rssi;
    return ESP_OK;
}

esp_err_t wifi_rx_get_noise_floor(int8_t *noise_floor)
{
    if (!noise_floor) {
        return ESP_ERR_INVALID_ARG;
    }

    *noise_floor = s_rx_ctx.noise_floor;
    return ESP_OK;
}

esp_err_t wifi_rx_enable_channel_hopping(const uint8_t *channels, size_t num_channels, uint32_t dwell_time_ms)
{
    if (!channels || num_channels == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_rx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Validate all channels
    for (size_t i = 0; i < num_channels; i++) {
        if (!wifi_rx_validate_channel(s_rx_ctx.config.band, channels[i])) {
            ESP_LOGE(TAG, "Invalid channel %d in hop list", channels[i]);
            return ESP_ERR_INVALID_ARG;
        }
    }

    // Allocate and copy channel list
    if (s_rx_ctx.hop_channels) {
        free(s_rx_ctx.hop_channels);
    }

    s_rx_ctx.hop_channels = malloc(num_channels);
    if (!s_rx_ctx.hop_channels) {
        ESP_LOGE(TAG, "Failed to allocate hop channels");
        return ESP_ERR_NO_MEM;
    }

    memcpy(s_rx_ctx.hop_channels, channels, num_channels);
    s_rx_ctx.hop_channels_count = num_channels;
    s_rx_ctx.hop_dwell_ms = dwell_time_ms;
    s_rx_ctx.hop_index = 0;
    s_rx_ctx.hopping_enabled = true;

    ESP_LOGI(TAG, "Channel hopping enabled: %zu channels, %lu ms dwell",
             num_channels, (unsigned long)dwell_time_ms);

    // If already running, restart hopping task
    if (s_rx_ctx.running && !s_rx_ctx.hop_task) {
        BaseType_t ret = xTaskCreate(
            wifi_rx_hop_task,
            "wifi_rx_hop",
            WIFI_RX_TASK_STACK_SIZE / 2,
            NULL,
            WIFI_RX_TASK_PRIORITY - 1,
            &s_rx_ctx.hop_task
        );

        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create hop task");
            s_rx_ctx.hopping_enabled = false;
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t wifi_rx_disable_channel_hopping(void)
{
    if (!s_rx_ctx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_rx_ctx.hopping_enabled = false;

    // Wait for hopping task to stop
    if (s_rx_ctx.hop_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_rx_ctx.hop_task = NULL;
    }

    ESP_LOGI(TAG, "Channel hopping disabled");
    return ESP_OK;
}

/**
 * ESP32 FPV Ground Station - Packet TX
 *
 * Transmits Ground2Air packets using raw 802.11 injection.
 */

#include "packet_tx.h"
#include "fpv_gs.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "packet_tx";

// Protocol constants
#define PACKET_VERSION      3
#define PACKET_SIGNATURE    56
#define GROUND2AIR_MAX_MTU  64

// Packet types
typedef enum {
    G2A_TYPE_TELEMETRY = 0,
    G2A_TYPE_CONFIG = 1,
    G2A_TYPE_CONNECT = 2,
} ground2air_type_t;

// Ground2Air header (matches air unit)
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t size;
    uint8_t crc;
    uint8_t packet_version;
    uint16_t air_device_id;
    uint16_t gs_device_id;
} ground2air_header_t;

// Ground2Air config packet
typedef struct __attribute__((packed)) {
    ground2air_header_t header;
    uint8_t ping;
    camera_config_t camera;
    data_channel_config_t data_channel;
    misc_config_t misc;
} ground2air_config_packet_t;

// FEC packet header
typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t signature;
    uint16_t from_device_id;
    uint16_t to_device_id;
    uint16_t size;
    uint32_t block_packet_index;
} fec_header_t;

// 802.11 header for Ground to Air
static const uint8_t IEEE80211_HEADER[] = {
    0x08, 0x01, 0x00, 0x00,             // Frame control
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // BSSID (broadcast)
    0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Source address
    0x66, 0x55, 0x44, 0x33, 0x22, 0x11, // Destination address
    0x10, 0x86                          // Sequence control
};

// TX state
static struct {
    bool initialized;
    bool running;
    bool connected;
    bool config_dirty;
    uint16_t gs_device_id;
    uint16_t air_device_id;
    uint8_t ping_counter;
    uint8_t pong_received;
    int64_t last_ping_time;
    uint32_t latency_ms;
    uint32_t block_index;
    camera_config_t camera_config;
    data_channel_config_t data_channel_config;
    misc_config_t misc_config;
    air_stats_t air_stats;
    TaskHandle_t tx_task;
    SemaphoreHandle_t mutex;
} s_tx = {0};

// CRC8 calculation
static uint8_t crc8(const uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Default camera configuration
static void init_default_camera_config(void) {
    s_tx.camera_config.resolution = RESOLUTION_SVGA;
    s_tx.camera_config.fps_limit = 60;
    s_tx.camera_config.quality = 0;  // Auto
    s_tx.camera_config.brightness = 0;
    s_tx.camera_config.contrast = 0;
    s_tx.camera_config.saturation = 1;
    s_tx.camera_config.sharpness = 0;
    s_tx.camera_config.denoise = 0;
    s_tx.camera_config.special_effect = 0;
    s_tx.camera_config.awb = true;
    s_tx.camera_config.awb_gain = true;
    s_tx.camera_config.wb_mode = 0;
    s_tx.camera_config.aec = true;
    s_tx.camera_config.aec2 = true;
    s_tx.camera_config.ae_level = 1;
    s_tx.camera_config.aec_value = 204;
    s_tx.camera_config.agc = true;
    s_tx.camera_config.agc_gain = 0;
    s_tx.camera_config.gainceiling = 0;
    s_tx.camera_config.bpc = true;
    s_tx.camera_config.wpc = true;
    s_tx.camera_config.raw_gma = true;
    s_tx.camera_config.lenc = true;
    s_tx.camera_config.hmirror = false;
    s_tx.camera_config.vflip = false;
    s_tx.camera_config.dcw = true;
}

// Default data channel configuration
static void init_default_data_channel_config(void) {
    s_tx.data_channel_config.wifi_power = 20;
    s_tx.data_channel_config.wifi_rate = RATE_N_26M_MCS3;
    s_tx.data_channel_config.wifi_channel = g_config.channel;
    s_tx.data_channel_config.fec_codec_k = g_config.fec_k;
    s_tx.data_channel_config.fec_codec_n = g_config.fec_n;
    s_tx.data_channel_config.fec_codec_mtu = 1464;
}

// Build and send a packet
static esp_err_t send_raw_packet(const uint8_t *payload, size_t payload_len) {
    // Build complete frame: 802.11 header + FEC header + payload
    size_t fec_header_size = sizeof(fec_header_t);
    size_t total_size = sizeof(IEEE80211_HEADER) + fec_header_size + payload_len;

    uint8_t *frame = malloc(total_size);
    if (frame == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Copy 802.11 header
    memcpy(frame, IEEE80211_HEADER, sizeof(IEEE80211_HEADER));

    // Build FEC header
    fec_header_t *fec = (fec_header_t *)(frame + sizeof(IEEE80211_HEADER));
    fec->version = PACKET_VERSION;
    fec->signature = PACKET_SIGNATURE;
    fec->from_device_id = s_tx.gs_device_id;
    fec->to_device_id = s_tx.air_device_id;
    fec->size = payload_len;
    fec->block_packet_index = (s_tx.block_index & 0xFFFFFF) | (0 << 24);  // packet_index = 0
    s_tx.block_index++;

    // Copy payload
    memcpy(frame + sizeof(IEEE80211_HEADER) + fec_header_size, payload, payload_len);

    // Send via raw 802.11 TX
    esp_err_t ret = esp_wifi_80211_tx(WIFI_IF_STA, frame, total_size, false);

    free(frame);
    return ret;
}

// Build config packet
static size_t build_config_packet(uint8_t *buffer) {
    ground2air_config_packet_t *pkt = (ground2air_config_packet_t *)buffer;

    pkt->header.type = G2A_TYPE_CONFIG;
    pkt->header.size = sizeof(ground2air_config_packet_t);
    pkt->header.packet_version = PACKET_VERSION;
    pkt->header.air_device_id = s_tx.air_device_id;
    pkt->header.gs_device_id = s_tx.gs_device_id;

    pkt->ping = s_tx.ping_counter++;

    // Copy configurations
    memcpy(&pkt->camera, &s_tx.camera_config, sizeof(camera_config_t));
    memcpy(&pkt->data_channel, &s_tx.data_channel_config, sizeof(data_channel_config_t));
    memcpy(&pkt->misc, &s_tx.misc_config, sizeof(misc_config_t));

    // Calculate CRC (over header excluding crc field)
    pkt->header.crc = crc8((uint8_t *)&pkt->header + 6, sizeof(ground2air_header_t) - 6);

    return sizeof(ground2air_config_packet_t);
}

// Build connect packet
static size_t build_connect_packet(uint8_t *buffer) {
    ground2air_header_t *pkt = (ground2air_header_t *)buffer;

    pkt->type = G2A_TYPE_CONNECT;
    pkt->size = sizeof(ground2air_header_t);
    pkt->packet_version = PACKET_VERSION;
    pkt->air_device_id = s_tx.air_device_id;
    pkt->gs_device_id = s_tx.gs_device_id;
    pkt->crc = crc8((uint8_t *)pkt + 6, sizeof(ground2air_header_t) - 6);

    return sizeof(ground2air_header_t);
}

// TX task - sends packets periodically
static void tx_task(void *arg) {
    (void)arg;
    uint8_t buffer[GROUND2AIR_MAX_MTU];
    TickType_t last_send = 0;
    const TickType_t send_interval = pdMS_TO_TICKS(500);  // 2 Hz

    ESP_LOGI(TAG, "TX task started");

    while (s_tx.running) {
        TickType_t now = xTaskGetTickCount();

        // Send at regular intervals or when config changed
        if ((now - last_send >= send_interval) || s_tx.config_dirty) {
            xSemaphoreTake(s_tx.mutex, portMAX_DELAY);

            size_t len;
            if (!s_tx.connected) {
                // Send connect packet until we get a response
                len = build_connect_packet(buffer);
            } else {
                // Send config packet
                len = build_config_packet(buffer);
                s_tx.last_ping_time = esp_timer_get_time();
            }

            s_tx.config_dirty = false;
            xSemaphoreGive(s_tx.mutex);

            esp_err_t ret = send_raw_packet(buffer, len);
            if (ret == ESP_OK) {
                last_send = now;
            } else {
                ESP_LOGW(TAG, "Failed to send packet: %s", esp_err_to_name(ret));
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }

    ESP_LOGI(TAG, "TX task exiting");
    vTaskDelete(NULL);
}

// Process received Air2Ground packet (called from packet_rx)
void packet_tx_process_air_response(const uint8_t *data, size_t len) {
    // Parse pong for latency measurement
    if (len >= 6) {
        uint8_t pong = data[5];  // pong field offset
        if (pong == s_tx.ping_counter - 1) {
            int64_t now = esp_timer_get_time();
            s_tx.latency_ms = (uint32_t)((now - s_tx.last_ping_time) / 1000);
        }
    }

    // Mark as connected
    if (!s_tx.connected) {
        s_tx.connected = true;
        ESP_LOGI(TAG, "Connected to air unit");
    }
}

// Process received air stats (from OSD packet)
void packet_tx_update_air_stats(const air_stats_t *stats) {
    if (stats) {
        xSemaphoreTake(s_tx.mutex, portMAX_DELAY);
        memcpy(&s_tx.air_stats, stats, sizeof(air_stats_t));
        xSemaphoreGive(s_tx.mutex);
    }
}

// Public API

esp_err_t packet_tx_init(void) {
    if (s_tx.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_tx.mutex = xSemaphoreCreateMutex();
    if (s_tx.mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // Generate unique GS device ID
    s_tx.gs_device_id = (uint16_t)(esp_random() & 0xFFFF);
    if (s_tx.gs_device_id == 0) s_tx.gs_device_id = 1;

    // Initialize default configurations
    init_default_camera_config();
    init_default_data_channel_config();

    s_tx.initialized = true;
    ESP_LOGI(TAG, "Packet TX initialized, GS ID=%04X", s_tx.gs_device_id);

    return ESP_OK;
}

esp_err_t packet_tx_start(void) {
    if (!s_tx.initialized || s_tx.running) {
        return ESP_ERR_INVALID_STATE;
    }

    s_tx.running = true;

    BaseType_t ret = xTaskCreatePinnedToCore(
        tx_task,
        "pkt_tx",
        4096,
        NULL,
        configMAX_PRIORITIES - 3,
        &s_tx.tx_task,
        1  // Core 1
    );

    if (ret != pdPASS) {
        s_tx.running = false;
        ESP_LOGE(TAG, "Failed to create TX task");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Packet TX started");
    return ESP_OK;
}

void packet_tx_stop(void) {
    s_tx.running = false;

    if (s_tx.tx_task) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_tx.tx_task = NULL;
    }
}

void packet_tx_set_air_device_id(uint16_t device_id) {
    s_tx.air_device_id = device_id;
    s_tx.connected = false;  // Need to reconnect
}

camera_config_t *packet_tx_get_camera_config(void) {
    return &s_tx.camera_config;
}

data_channel_config_t *packet_tx_get_data_channel_config(void) {
    return &s_tx.data_channel_config;
}

air_stats_t *packet_tx_get_air_stats(void) {
    return &s_tx.air_stats;
}

void packet_tx_config_changed(void) {
    s_tx.config_dirty = true;
}

bool packet_tx_is_connected(void) {
    return s_tx.connected;
}

esp_err_t packet_tx_send_connect(void) {
    uint8_t buffer[GROUND2AIR_MAX_MTU];
    size_t len = build_connect_packet(buffer);
    return send_raw_packet(buffer, len);
}

esp_err_t packet_tx_send_config(void) {
    uint8_t buffer[GROUND2AIR_MAX_MTU];

    xSemaphoreTake(s_tx.mutex, portMAX_DELAY);
    size_t len = build_config_packet(buffer);
    xSemaphoreGive(s_tx.mutex);

    return send_raw_packet(buffer, len);
}

uint32_t packet_tx_get_latency_ms(void) {
    return s_tx.latency_ms;
}

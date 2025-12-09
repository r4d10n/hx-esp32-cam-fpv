/**
 * @file unified_rx.cpp
 * @brief Unified receiver implementation for WFB-NG and ESP32-FPV
 */

#include "unified_rx.h"

#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#ifdef ESP_PLATFORM
#include "esp_log.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#else
#include <stdio.h>
#define ESP_LOGI(tag, ...) printf("[%s] ", tag); printf(__VA_ARGS__); printf("\n")
#define ESP_LOGW(tag, ...) printf("[%s] WARN: ", tag); printf(__VA_ARGS__); printf("\n")
#define ESP_LOGE(tag, ...) printf("[%s] ERROR: ", tag); printf(__VA_ARGS__); printf("\n")
#endif

// Include protocol-specific headers
#include "wfb_rx.h"
#include "wfb_protocol.h"
#include "fec_decoder.h"
#include "frame_assembler.h"

static const char* TAG = "unified_rx";

// IEEE 802.11 header constants
#define IEEE80211_HDR_LEN           24
#define IEEE80211_RADIOTAP_MIN      8

// Protocol detection magic bytes
#define WFB_MAGIC_1                 0x57  // 'W'
#define WFB_MAGIC_2                 0x42  // 'B'
#define ESP32FPV_FEC_HEADER_SIZE    6

// Frame buffer size
#define DEFAULT_FRAME_BUFFER_SIZE   (256 * 1024)
#define MAX_FRAME_SIZE              (150 * 1024)

// State
static struct {
    rx_config_t config;
    rx_stats_t stats;

    // Callbacks
    rx_video_callback_t video_cb;
    void* video_cb_user;
    rx_telemetry_callback_t telemetry_cb;
    void* telemetry_cb_user;
    rx_osd_callback_t osd_cb;
    void* osd_cb_user;

    // WFB-NG receiver
    WfbReceiver* wfb_rx;

    // ESP32-FPV FEC decoder
    FecDecoder* esp_fec;
    FrameAssembler* frame_assembler;

    // Frame buffer (for both protocols)
    uint8_t* frame_buffer;
    size_t frame_buffer_size;

    // Protocol detection
    uint32_t wfb_packet_count;
    uint32_t esp_packet_count;
    uint32_t last_detect_time;

    // FPS calculation
    uint32_t frame_count_for_fps;
    uint32_t last_fps_time;

#ifdef ESP_PLATFORM
    SemaphoreHandle_t mutex;
#endif

    bool initialized;
    bool running;
} g_rx = {0};

// Forward declarations
static void process_esp32fpv_packet(const uint8_t* data, size_t len);
static void process_wfb_packet(const uint8_t* data, size_t len, int8_t rssi, int8_t noise);
static rx_protocol_t detect_protocol(const uint8_t* data, size_t len);
static void on_wfb_data(const uint8_t* data, size_t len, void* user_data);
static void on_esp_frame_decoded(const uint8_t* data, size_t len, uint32_t frame_index,
                                  uint8_t resolution, void* user_data);
static void on_esp_telemetry(const uint8_t* data, size_t len, void* user_data);
static void on_esp_osd(const uint8_t* data, size_t len, void* user_data);
static void update_fps_stats(void);

/**
 * @brief Initialize the unified receiver
 */
int unified_rx_init(const rx_config_t* config)
{
    if (g_rx.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return -1;
    }

    // Apply configuration
    if (config) {
        g_rx.config = *config;
    } else {
        g_rx.config.protocol = PROTOCOL_AUTO;
        g_rx.config.wifi_channel = 7;
        g_rx.config.device_id = 0;
        g_rx.config.gs_key_path = "/sdcard/gs.key";
        g_rx.config.wfb_channel_id = 0;
        g_rx.config.fec_k = 6;
        g_rx.config.fec_n = 12;
        g_rx.config.mtu = 1464;
        g_rx.config.frame_buffer_size = DEFAULT_FRAME_BUFFER_SIZE;
        g_rx.config.max_concurrent_blocks = 16;
    }

    // Allocate frame buffer in PSRAM if available
#ifdef ESP_PLATFORM
    g_rx.frame_buffer = (uint8_t*)heap_caps_malloc(g_rx.config.frame_buffer_size,
                                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!g_rx.frame_buffer) {
        g_rx.frame_buffer = (uint8_t*)malloc(g_rx.config.frame_buffer_size);
    }
#else
    g_rx.frame_buffer = (uint8_t*)malloc(g_rx.config.frame_buffer_size);
#endif
    if (!g_rx.frame_buffer) {
        ESP_LOGE(TAG, "Failed to allocate frame buffer");
        return -1;
    }
    g_rx.frame_buffer_size = g_rx.config.frame_buffer_size;

    // Initialize WFB-NG receiver
    g_rx.wfb_rx = new WfbReceiver();
    if (!g_rx.wfb_rx->init(g_rx.config.gs_key_path)) {
        ESP_LOGW(TAG, "WFB-NG init failed (continuing without encryption support)");
    }
    g_rx.wfb_rx->set_data_callback(on_wfb_data, nullptr);

    // Initialize ESP32-FPV FEC decoder
    FecDecoderConfig fec_config = {
        .coding_k = g_rx.config.fec_k,
        .coding_n = g_rx.config.fec_n,
        .mtu = g_rx.config.mtu,
        .use_psram = true,
        .device_id = g_rx.config.device_id,
        .block_timeout_ms = 100
    };

    g_rx.esp_fec = new FecDecoder();
    if (g_rx.esp_fec->init(fec_config) != 0) {
        ESP_LOGE(TAG, "Failed to init ESP32-FPV FEC decoder");
        delete g_rx.wfb_rx;
        free(g_rx.frame_buffer);
        return -1;
    }

    // Initialize frame assembler
    g_rx.frame_assembler = new FrameAssembler();
    FrameAssemblerConfig fa_config = {
        .max_frame_size = MAX_FRAME_SIZE,
        .use_psram = true,
        .timeout_ms = 200
    };
    g_rx.frame_assembler->init(fa_config);
    g_rx.frame_assembler->set_frame_callback(on_esp_frame_decoded, nullptr);
    g_rx.frame_assembler->set_telemetry_callback(on_esp_telemetry, nullptr);
    g_rx.frame_assembler->set_osd_callback(on_esp_osd, nullptr);

#ifdef ESP_PLATFORM
    g_rx.mutex = xSemaphoreCreateMutex();
#endif

    memset(&g_rx.stats, 0, sizeof(g_rx.stats));
    g_rx.initialized = true;

    ESP_LOGI(TAG, "Unified receiver initialized (protocol=%d, channel=%d)",
             g_rx.config.protocol, g_rx.config.wifi_channel);

    return 0;
}

/**
 * @brief Start receiving
 */
int unified_rx_start(void)
{
    if (!g_rx.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return -1;
    }

    if (g_rx.running) {
        ESP_LOGW(TAG, "Already running");
        return 0;
    }

#ifdef ESP_PLATFORM
    // Set WiFi channel
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_wifi_set_channel(g_rx.config.wifi_channel, second);
#endif

    g_rx.running = true;
    ESP_LOGI(TAG, "Unified receiver started");

    return 0;
}

/**
 * @brief Stop receiving
 */
void unified_rx_stop(void)
{
    if (!g_rx.running) return;

    g_rx.running = false;
    ESP_LOGI(TAG, "Unified receiver stopped");
}

/**
 * @brief Detect protocol from packet
 */
static rx_protocol_t detect_protocol(const uint8_t* data, size_t len)
{
    if (len < IEEE80211_HDR_LEN + 10) {
        return PROTOCOL_UNKNOWN;
    }

    // Skip IEEE 802.11 header
    const uint8_t* payload = data + IEEE80211_HDR_LEN;

    // Check for WFB-NG magic in MAC address
    // WFB-NG uses 0x57 0x42 ('WB') in first two bytes of transmitter MAC
    if (data[10] == WFB_MAGIC_1 && data[11] == WFB_MAGIC_2) {
        return PROTOCOL_WFB_NG;
    }

    // ESP32-FPV has 6-byte FEC header: block_index (4) + packet_index (1) + flags (1)
    // Check if it looks like an ESP32-FPV packet
    if (len >= IEEE80211_HDR_LEN + ESP32FPV_FEC_HEADER_SIZE + 3) {
        // Check for packet header after FEC header
        const uint8_t* pkt_hdr = payload + ESP32FPV_FEC_HEADER_SIZE;
        uint8_t flags = pkt_hdr[0];
        uint16_t size = (pkt_hdr[1] << 8) | pkt_hdr[2];

        // Valid ESP32-FPV packet if size is reasonable
        if (size > 0 && size < 1500 && (flags & 0xF0) == 0) {
            return PROTOCOL_ESP32_FPV;
        }
    }

    return PROTOCOL_UNKNOWN;
}

/**
 * @brief Process a raw WiFi packet
 */
void unified_rx_process_packet(const uint8_t* data, size_t len,
                                int8_t rssi, int8_t noise)
{
    if (!g_rx.running || len < IEEE80211_HDR_LEN) return;

#ifdef ESP_PLATFORM
    xSemaphoreTake(g_rx.mutex, portMAX_DELAY);
#endif

    g_rx.stats.packets_received++;
    g_rx.stats.rssi = rssi;
    g_rx.stats.noise_floor = noise;
    g_rx.stats.snr = rssi - noise;

    rx_protocol_t protocol = g_rx.config.protocol;

    // Auto-detect protocol if needed
    if (protocol == PROTOCOL_AUTO) {
        protocol = detect_protocol(data, len);

        // Update detection counts
        if (protocol == PROTOCOL_WFB_NG) {
            g_rx.wfb_packet_count++;
        } else if (protocol == PROTOCOL_ESP32_FPV) {
            g_rx.esp_packet_count++;
        }

        // Switch active protocol based on majority
        uint32_t now = 0;
#ifdef ESP_PLATFORM
        now = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif
        if (now - g_rx.last_detect_time > 1000) {
            if (g_rx.wfb_packet_count > g_rx.esp_packet_count * 2) {
                g_rx.stats.active_protocol = PROTOCOL_WFB_NG;
            } else if (g_rx.esp_packet_count > g_rx.wfb_packet_count * 2) {
                g_rx.stats.active_protocol = PROTOCOL_ESP32_FPV;
            }
            g_rx.wfb_packet_count = 0;
            g_rx.esp_packet_count = 0;
            g_rx.last_detect_time = now;
        }
    } else {
        g_rx.stats.active_protocol = protocol;
    }

    // Route to appropriate handler
    switch (protocol) {
        case PROTOCOL_WFB_NG:
            process_wfb_packet(data, len, rssi, noise);
            break;
        case PROTOCOL_ESP32_FPV:
            process_esp32fpv_packet(data, len);
            break;
        default:
            // Try both handlers
            process_wfb_packet(data, len, rssi, noise);
            process_esp32fpv_packet(data, len);
            break;
    }

#ifdef ESP_PLATFORM
    xSemaphoreGive(g_rx.mutex);
#endif
}

/**
 * @brief Process ESP32-FPV protocol packet
 */
static void process_esp32fpv_packet(const uint8_t* data, size_t len)
{
    if (len < IEEE80211_HDR_LEN + ESP32FPV_FEC_HEADER_SIZE) return;

    const uint8_t* payload = data + IEEE80211_HDR_LEN;
    size_t payload_len = len - IEEE80211_HDR_LEN;

    // Parse FEC header
    uint32_t block_index = (payload[0] << 24) | (payload[1] << 16) |
                           (payload[2] << 8) | payload[3];
    uint8_t packet_index = payload[4];

    // Pass to FEC decoder
    const uint8_t* fec_payload = payload + ESP32FPV_FEC_HEADER_SIZE;
    size_t fec_payload_len = payload_len - ESP32FPV_FEC_HEADER_SIZE;

    // Add to FEC decoder block
    if (g_rx.esp_fec->add_packet(block_index, packet_index, fec_payload, fec_payload_len) == 0) {
        g_rx.stats.packets_valid++;

        // Try to decode ready blocks
        g_rx.esp_fec->process();
    }
}

/**
 * @brief Process WFB-NG protocol packet
 */
static void process_wfb_packet(const uint8_t* data, size_t len, int8_t rssi, int8_t noise)
{
    if (!g_rx.wfb_rx) return;

    // WFB receiver handles full packet including 802.11 header
    g_rx.wfb_rx->process_packet(data, len, rssi, noise);
}

/**
 * @brief Callback when WFB-NG data is decoded
 */
static void on_wfb_data(const uint8_t* data, size_t len, void* user_data)
{
    if (!g_rx.video_cb || len < 2) return;

    g_rx.stats.packets_valid++;

    // WFB-NG typically outputs RTP packets or raw NAL units
    // Check for RTP header (version 2)
    uint8_t version = (data[0] >> 6) & 0x03;

    rx_video_metadata_t metadata = {0};
    const uint8_t* video_data = data;
    size_t video_len = len;

    if (version == 2) {
        // RTP packet - parse header
        uint8_t pt = data[1] & 0x7F;
        uint32_t timestamp = (data[4] << 24) | (data[5] << 16) |
                             (data[6] << 8) | data[7];

        // Skip RTP header (minimum 12 bytes)
        size_t rtp_hdr_len = 12;
        uint8_t cc = data[0] & 0x0F;
        rtp_hdr_len += cc * 4;

        // Check for extension
        if (data[0] & 0x10) {
            if (len >= rtp_hdr_len + 4) {
                uint16_t ext_len = (data[rtp_hdr_len + 2] << 8) | data[rtp_hdr_len + 3];
                rtp_hdr_len += 4 + ext_len * 4;
            }
        }

        if (len > rtp_hdr_len) {
            video_data = data + rtp_hdr_len;
            video_len = len - rtp_hdr_len;
        }

        metadata.timestamp = timestamp;
        metadata.codec = (pt == 96) ? CODEC_H264 : CODEC_H265;
    } else {
        // Raw NAL unit
        metadata.codec = CODEC_H264;  // Assume H.264 for raw NAL
    }

    // Detect keyframe
    if (metadata.codec == CODEC_H264 && video_len > 0) {
        uint8_t nal_type = video_data[0] & 0x1F;
        metadata.is_keyframe = (nal_type == 5);  // IDR
    } else if (metadata.codec == CODEC_H265 && video_len > 0) {
        uint8_t nal_type = (video_data[0] >> 1) & 0x3F;
        metadata.is_keyframe = (nal_type >= 16 && nal_type <= 21);
    }

    g_rx.stats.active_codec = metadata.codec;
    if (metadata.is_keyframe) g_rx.stats.keyframes++;

    // Invoke callback
    g_rx.video_cb(video_data, video_len, &metadata, g_rx.video_cb_user);

    update_fps_stats();
}

/**
 * @brief Callback when ESP32-FPV frame is decoded
 */
static void on_esp_frame_decoded(const uint8_t* data, size_t len, uint32_t frame_index,
                                  uint8_t resolution, void* user_data)
{
    if (!g_rx.video_cb || len == 0) return;

    g_rx.stats.frames_complete++;

    rx_video_metadata_t metadata = {0};
    metadata.codec = CODEC_MJPEG;
    metadata.frame_index = frame_index;
    metadata.is_keyframe = true;  // All MJPEG frames are keyframes

    // Parse resolution
    // Resolution enum: QVGA=0, CIF=1, HVGA=2, VGA=3, VGA16=4, SVGA=5, SVGA16=6...
    static const uint16_t widths[] = {320, 400, 480, 640, 640, 800, 800, 1024, 1024, 1280, 1280, 1600};
    static const uint16_t heights[] = {240, 296, 320, 480, 360, 600, 456, 768, 576, 960, 720, 1200};
    if (resolution < 12) {
        metadata.width = widths[resolution];
        metadata.height = heights[resolution];
    }

    g_rx.stats.active_codec = CODEC_MJPEG;
    g_rx.stats.keyframes++;

    // Invoke callback
    g_rx.video_cb(data, len, &metadata, g_rx.video_cb_user);

    update_fps_stats();
}

/**
 * @brief Callback for ESP32-FPV telemetry
 */
static void on_esp_telemetry(const uint8_t* data, size_t len, void* user_data)
{
    if (g_rx.telemetry_cb) {
        g_rx.telemetry_cb(data, len, g_rx.telemetry_cb_user);
    }
}

/**
 * @brief Callback for ESP32-FPV OSD
 */
static void on_esp_osd(const uint8_t* data, size_t len, void* user_data)
{
    if (g_rx.osd_cb) {
        g_rx.osd_cb(data, len, g_rx.osd_cb_user);
    }
}

/**
 * @brief Update FPS statistics
 */
static void update_fps_stats(void)
{
    g_rx.frame_count_for_fps++;

    uint32_t now = 0;
#ifdef ESP_PLATFORM
    now = xTaskGetTickCount() * portTICK_PERIOD_MS;
#endif

    uint32_t elapsed = now - g_rx.last_fps_time;
    if (elapsed >= 1000) {
        g_rx.stats.fps = (float)g_rx.frame_count_for_fps * 1000.0f / elapsed;
        g_rx.frame_count_for_fps = 0;
        g_rx.last_fps_time = now;
    }
}

/**
 * @brief Set video callback
 */
void unified_rx_set_video_callback(rx_video_callback_t callback, void* user_data)
{
    g_rx.video_cb = callback;
    g_rx.video_cb_user = user_data;
}

/**
 * @brief Set telemetry callback
 */
void unified_rx_set_telemetry_callback(rx_telemetry_callback_t callback, void* user_data)
{
    g_rx.telemetry_cb = callback;
    g_rx.telemetry_cb_user = user_data;
}

/**
 * @brief Set OSD callback
 */
void unified_rx_set_osd_callback(rx_osd_callback_t callback, void* user_data)
{
    g_rx.osd_cb = callback;
    g_rx.osd_cb_user = user_data;
}

/**
 * @brief Get statistics
 */
void unified_rx_get_stats(rx_stats_t* stats)
{
    if (stats) {
#ifdef ESP_PLATFORM
        xSemaphoreTake(g_rx.mutex, portMAX_DELAY);
#endif
        *stats = g_rx.stats;
#ifdef ESP_PLATFORM
        xSemaphoreGive(g_rx.mutex);
#endif
    }
}

/**
 * @brief Set WiFi channel
 */
int unified_rx_set_channel(uint8_t channel)
{
    g_rx.config.wifi_channel = channel;

#ifdef ESP_PLATFORM
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_err_t ret = esp_wifi_set_channel(channel, second);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set channel %d: %d", channel, ret);
        return -1;
    }
#endif

    ESP_LOGI(TAG, "Set WiFi channel to %d", channel);
    return 0;
}

/**
 * @brief Set protocol
 */
void unified_rx_set_protocol(rx_protocol_t protocol)
{
    g_rx.config.protocol = protocol;
    ESP_LOGI(TAG, "Set protocol to %d", protocol);
}

/**
 * @brief Get current protocol
 */
rx_protocol_t unified_rx_get_protocol(void)
{
    return g_rx.stats.active_protocol;
}

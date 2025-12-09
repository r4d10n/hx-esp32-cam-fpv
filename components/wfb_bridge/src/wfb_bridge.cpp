/**
 * @file wfb_bridge.cpp
 * @brief Bridge implementation connecting WFB-NG receiver to WebSocket server
 */

#include "wfb_bridge.h"
#include "wfb_rx.h"
#include "video_ws_server.h"

#include <string.h>
#include <inttypes.h>

#ifdef ESP_PLATFORM
#include <esp_log.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#else
#include <stdio.h>
#define ESP_LOGI(tag, ...) printf("[%s] ", tag); printf(__VA_ARGS__); printf("\n")
#define ESP_LOGW(tag, ...) printf("[%s] WARN: ", tag); printf(__VA_ARGS__); printf("\n")
#define ESP_LOGE(tag, ...) printf("[%s] ERROR: ", tag); printf(__VA_ARGS__); printf("\n")
#endif

static const char* TAG = "wfb_bridge";

// RTP header structure (12 bytes)
typedef struct __attribute__((packed)) {
    uint8_t  version_flags;   // Version(2), P(1), X(1), CC(4)
    uint8_t  marker_pt;       // M(1), PT(7)
    uint16_t sequence;
    uint32_t timestamp;
    uint32_t ssrc;
} rtp_header_t;

#define RTP_VERSION         2
#define RTP_PT_H264         96
#define RTP_PT_H265         97

// H.264 NAL unit types
#define NAL_TYPE_MASK       0x1F
#define NAL_TYPE_STAP_A     24
#define NAL_TYPE_FU_A       28

// Bridge state
static struct {
    wfb_bridge_config_t config;
    wfb_bridge_stats_t stats;
    WfbReceiver* receiver;
    bool initialized;
    bool running;

    // Codec detection state
    uint8_t  detected_codec;
    uint16_t video_width;
    uint16_t video_height;
    uint8_t  video_fps;

    // SPS/PPS cache for codec config
    uint8_t* sps_data;
    size_t   sps_len;
    uint8_t* pps_data;
    size_t   pps_len;
    uint8_t* vps_data;
    size_t   vps_len;
    bool     config_sent;

    // FU-A reassembly buffer
    uint8_t* fu_buffer;
    size_t   fu_len;
    size_t   fu_capacity;
    bool     fu_started;
    uint8_t  fu_nal_type;

    // Statistics
    uint32_t rtp_sequence;
    uint32_t frame_count;
} g_bridge;

// Forward declarations
static void on_wfb_data(const uint8_t* data, size_t len, uint8_t flags);
static void process_rtp_packet(const uint8_t* data, size_t len);
static void process_h264_nal(const uint8_t* data, size_t len, uint32_t timestamp);
static void process_h265_nal(const uint8_t* data, size_t len, uint32_t timestamp);
static void extract_h264_sps_info(const uint8_t* sps, size_t len);
static void send_codec_config(void);

/**
 * @brief Initialize the WFB bridge
 */
int wfb_bridge_init(const wfb_bridge_config_t* config)
{
    if (g_bridge.initialized) {
        ESP_LOGW(TAG, "Bridge already initialized");
        return -1;
    }

    // Apply configuration
    if (config) {
        g_bridge.config = *config;
    } else {
        g_bridge.config.gs_key_path = "/sdcard/gs.key";
        g_bridge.config.ws_port = 8080;
        g_bridge.config.channel_id = 0;
        g_bridge.config.auto_detect_codec = true;
    }

    // Initialize WFB receiver
    g_bridge.receiver = new WfbReceiver();
    if (!g_bridge.receiver->init(true)) {  // use PSRAM
        ESP_LOGE(TAG, "Failed to initialize WFB receiver");
        delete g_bridge.receiver;
        g_bridge.receiver = nullptr;
        return -1;
    }

    // Load key if path specified
    if (g_bridge.config.gs_key_path) {
        // Key loading would happen here from filesystem
        ESP_LOGI(TAG, "Key path: %s (loading not implemented)", g_bridge.config.gs_key_path);
    }

    // Set data callback
    g_bridge.receiver->set_data_callback(on_wfb_data);

    // Initialize WebSocket server
    video_ws_server_config_t ws_config = {
        .port = g_bridge.config.ws_port,
        .max_clients = 4,
        .send_buffer_size = 64 * 1024,
        .enable_cors = true
    };

    if (video_ws_server_init(&ws_config) != 0) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket server");
        delete g_bridge.receiver;
        g_bridge.receiver = nullptr;
        return -1;
    }

    // Allocate FU-A reassembly buffer
    g_bridge.fu_capacity = 256 * 1024;  // 256KB for large NAL units
    g_bridge.fu_buffer = (uint8_t*)malloc(g_bridge.fu_capacity);
    if (!g_bridge.fu_buffer) {
        ESP_LOGE(TAG, "Failed to allocate FU buffer");
        delete g_bridge.receiver;
        g_bridge.receiver = nullptr;
        return -1;
    }

    memset(&g_bridge.stats, 0, sizeof(g_bridge.stats));
    g_bridge.initialized = true;

    ESP_LOGI(TAG, "WFB bridge initialized (port=%d)", g_bridge.config.ws_port);
    return 0;
}

/**
 * @brief Start the bridge
 */
int wfb_bridge_start(void)
{
    if (!g_bridge.initialized) {
        ESP_LOGE(TAG, "Bridge not initialized");
        return -1;
    }

    if (g_bridge.running) {
        ESP_LOGW(TAG, "Bridge already running");
        return 0;
    }

    // Start WebSocket server
    if (video_ws_server_start() != 0) {
        ESP_LOGE(TAG, "Failed to start WebSocket server");
        return -1;
    }

    g_bridge.running = true;
    ESP_LOGI(TAG, "WFB bridge started");

    return 0;
}

/**
 * @brief Stop the bridge
 */
void wfb_bridge_stop(void)
{
    if (!g_bridge.running) return;

    video_ws_server_stop();
    g_bridge.running = false;

    ESP_LOGI(TAG, "WFB bridge stopped");
}

/**
 * @brief Process a raw WiFi packet from monitor mode
 */
void wfb_bridge_process_packet(const uint8_t* data, size_t len,
                               int8_t rssi, int8_t noise)
{
    if (!g_bridge.running || !g_bridge.receiver) return;

    g_bridge.stats.packets_received++;
    g_bridge.stats.rssi = rssi;
    g_bridge.stats.snr = rssi - noise;

    // Pass to WFB receiver for parsing, decryption, and FEC
    g_bridge.receiver->process_packet(data, len, rssi, noise);
}

/**
 * @brief Callback when WFB receiver outputs decoded data
 */
static void on_wfb_data(const uint8_t* data, size_t len, uint8_t flags)
{
    (void)flags;  // unused for now

    if (len < sizeof(rtp_header_t)) {
        return;
    }

    g_bridge.stats.packets_decrypted++;

    // Check for RTP packet
    const rtp_header_t* rtp = (const rtp_header_t*)data;
    uint8_t version = (rtp->version_flags >> 6) & 0x03;

    if (version == RTP_VERSION) {
        process_rtp_packet(data, len);
    } else {
        // Raw NAL unit (no RTP encapsulation)
        if (g_bridge.detected_codec == VIDEO_CODEC_H264 ||
            g_bridge.config.auto_detect_codec) {
            process_h264_nal(data, len, 0);
        }
    }
}

/**
 * @brief Process an RTP packet
 */
static void process_rtp_packet(const uint8_t* data, size_t len)
{
    const rtp_header_t* rtp = (const rtp_header_t*)data;

    uint8_t pt = rtp->marker_pt & 0x7F;
    uint16_t seq = __builtin_bswap16(rtp->sequence);
    uint32_t timestamp = __builtin_bswap32(rtp->timestamp);

    // Check for sequence discontinuity
    if (g_bridge.rtp_sequence != 0 &&
        seq != (uint16_t)(g_bridge.rtp_sequence + 1)) {
        ESP_LOGW(TAG, "RTP sequence discontinuity: expected %u, got %u",
                 (unsigned)(g_bridge.rtp_sequence + 1), seq);
        // Reset FU-A reassembly on discontinuity
        g_bridge.fu_started = false;
        g_bridge.fu_len = 0;
    }
    g_bridge.rtp_sequence = seq;

    // Calculate payload offset (skip RTP header + CSRC)
    uint8_t cc = rtp->version_flags & 0x0F;
    size_t header_len = sizeof(rtp_header_t) + cc * 4;

    // Check for extension header
    if (rtp->version_flags & 0x10) {
        if (len < header_len + 4) return;
        uint16_t ext_len = __builtin_bswap16(*(uint16_t*)(data + header_len + 2));
        header_len += 4 + ext_len * 4;
    }

    if (len <= header_len) return;

    const uint8_t* payload = data + header_len;
    size_t payload_len = len - header_len;

    // Route based on payload type
    if (pt == RTP_PT_H264 || pt == 96) {
        g_bridge.detected_codec = VIDEO_CODEC_H264;
        process_h264_nal(payload, payload_len, timestamp);
    } else if (pt == RTP_PT_H265 || pt == 97) {
        g_bridge.detected_codec = VIDEO_CODEC_H265;
        process_h265_nal(payload, payload_len, timestamp);
    }
}

/**
 * @brief Process H.264 NAL unit (handles RTP packetization modes)
 */
static void process_h264_nal(const uint8_t* data, size_t len, uint32_t timestamp)
{
    if (len < 1) return;

    uint8_t nal_header = data[0];
    uint8_t nal_type = nal_header & NAL_TYPE_MASK;

    if (nal_type >= 1 && nal_type <= 23) {
        // Single NAL unit packet
        bool is_keyframe = (nal_type == 5);  // IDR

        // Cache SPS/PPS
        if (nal_type == 7 && len < 256) {  // SPS
            if (g_bridge.sps_data) free(g_bridge.sps_data);
            g_bridge.sps_data = (uint8_t*)malloc(len);
            if (g_bridge.sps_data) {
                memcpy(g_bridge.sps_data, data, len);
                g_bridge.sps_len = len;
                extract_h264_sps_info(data, len);
            }
            return;  // Don't send parameter sets as frames
        } else if (nal_type == 8 && len < 64) {  // PPS
            if (g_bridge.pps_data) free(g_bridge.pps_data);
            g_bridge.pps_data = (uint8_t*)malloc(len);
            if (g_bridge.pps_data) {
                memcpy(g_bridge.pps_data, data, len);
                g_bridge.pps_len = len;
            }
            return;  // Don't send parameter sets as frames
        }

        // Send codec config before first keyframe
        if (is_keyframe && !g_bridge.config_sent) {
            send_codec_config();
        }

        // Send NAL unit to WebSocket clients
        video_ws_server_send_frame(data, len, VIDEO_CODEC_H264,
                                   timestamp, is_keyframe);

        g_bridge.stats.frames_extracted++;
        if (is_keyframe) g_bridge.stats.keyframes++;

    } else if (nal_type == NAL_TYPE_STAP_A) {
        // STAP-A: Single-time aggregation packet
        const uint8_t* ptr = data + 1;
        const uint8_t* end = data + len;

        while (ptr + 2 <= end) {
            uint16_t nalu_size = (ptr[0] << 8) | ptr[1];
            ptr += 2;

            if (ptr + nalu_size > end) break;

            // Recursively process each NAL unit
            process_h264_nal(ptr, nalu_size, timestamp);
            ptr += nalu_size;
        }

    } else if (nal_type == NAL_TYPE_FU_A) {
        // FU-A: Fragmentation unit
        if (len < 2) return;

        uint8_t fu_header = data[1];
        bool start = (fu_header & 0x80) != 0;
        bool end = (fu_header & 0x40) != 0;
        uint8_t fu_type = fu_header & 0x1F;

        if (start) {
            // Start of fragmented NAL
            g_bridge.fu_started = true;
            g_bridge.fu_len = 0;
            g_bridge.fu_nal_type = fu_type;

            // Reconstruct NAL header
            uint8_t reconstructed_header = (nal_header & 0xE0) | fu_type;
            if (g_bridge.fu_len + 1 <= g_bridge.fu_capacity) {
                g_bridge.fu_buffer[g_bridge.fu_len++] = reconstructed_header;
            }
        }

        if (g_bridge.fu_started) {
            // Append payload (skip FU indicator and header)
            const uint8_t* payload = data + 2;
            size_t payload_len = len - 2;

            if (g_bridge.fu_len + payload_len <= g_bridge.fu_capacity) {
                memcpy(g_bridge.fu_buffer + g_bridge.fu_len, payload, payload_len);
                g_bridge.fu_len += payload_len;
            }

            if (end) {
                // Complete NAL unit
                bool is_keyframe = (g_bridge.fu_nal_type == 5);

                // Send codec config before first keyframe
                if (is_keyframe && !g_bridge.config_sent) {
                    send_codec_config();
                }

                video_ws_server_send_frame(g_bridge.fu_buffer, g_bridge.fu_len,
                                           VIDEO_CODEC_H264, timestamp, is_keyframe);

                g_bridge.stats.frames_extracted++;
                if (is_keyframe) g_bridge.stats.keyframes++;

                g_bridge.fu_started = false;
                g_bridge.fu_len = 0;
            }
        }
    }
}

/**
 * @brief Process H.265 NAL unit
 */
static void process_h265_nal(const uint8_t* data, size_t len, uint32_t timestamp)
{
    if (len < 2) return;

    // H.265 NAL header is 2 bytes
    uint8_t nal_type = (data[0] >> 1) & 0x3F;

    bool is_keyframe = (nal_type >= 16 && nal_type <= 21);  // IDR, CRA

    // Cache VPS/SPS/PPS
    if (nal_type == 32 && len < 256) {  // VPS
        if (g_bridge.vps_data) free(g_bridge.vps_data);
        g_bridge.vps_data = (uint8_t*)malloc(len);
        if (g_bridge.vps_data) {
            memcpy(g_bridge.vps_data, data, len);
            g_bridge.vps_len = len;
        }
        return;
    } else if (nal_type == 33 && len < 256) {  // SPS
        if (g_bridge.sps_data) free(g_bridge.sps_data);
        g_bridge.sps_data = (uint8_t*)malloc(len);
        if (g_bridge.sps_data) {
            memcpy(g_bridge.sps_data, data, len);
            g_bridge.sps_len = len;
        }
        return;
    } else if (nal_type == 34 && len < 64) {  // PPS
        if (g_bridge.pps_data) free(g_bridge.pps_data);
        g_bridge.pps_data = (uint8_t*)malloc(len);
        if (g_bridge.pps_data) {
            memcpy(g_bridge.pps_data, data, len);
            g_bridge.pps_len = len;
        }
        return;
    }

    // Send codec config before first keyframe
    if (is_keyframe && !g_bridge.config_sent) {
        send_codec_config();
    }

    video_ws_server_send_frame(data, len, VIDEO_CODEC_H265,
                               timestamp, is_keyframe);

    g_bridge.stats.frames_extracted++;
    if (is_keyframe) g_bridge.stats.keyframes++;
}

/**
 * @brief Extract video dimensions from H.264 SPS
 */
static void extract_h264_sps_info(const uint8_t* sps, size_t len)
{
    // Simplified SPS parsing - full implementation would use exp-golomb decoder
    // For now, use common defaults
    g_bridge.video_width = 1920;
    g_bridge.video_height = 1080;
    g_bridge.video_fps = 30;

    ESP_LOGI(TAG, "H.264 SPS detected (assuming %dx%d @ %dfps)",
             g_bridge.video_width, g_bridge.video_height, g_bridge.video_fps);
}

/**
 * @brief Send codec configuration to connected clients
 */
static void send_codec_config(void)
{
    if (!g_bridge.sps_data || !g_bridge.pps_data) {
        ESP_LOGW(TAG, "Cannot send codec config: missing SPS/PPS");
        return;
    }

    int sent = video_ws_server_send_config(
        g_bridge.detected_codec,
        g_bridge.video_width,
        g_bridge.video_height,
        g_bridge.video_fps,
        g_bridge.sps_data, g_bridge.sps_len,
        g_bridge.pps_data, g_bridge.pps_len,
        g_bridge.vps_data, g_bridge.vps_len
    );

    if (sent > 0) {
        g_bridge.config_sent = true;
        ESP_LOGI(TAG, "Sent codec config to %d clients", sent);
    }
}

/**
 * @brief Get bridge statistics
 */
void wfb_bridge_get_stats(wfb_bridge_stats_t* stats)
{
    if (!stats) return;

    *stats = g_bridge.stats;
    stats->clients_connected = video_ws_server_get_client_count();

    if (g_bridge.receiver) {
        const WfbRxStats& rx_stats = g_bridge.receiver->get_stats();
        stats->fec_recovered = rx_stats.packets_fec_recovered;
    }
}

/**
 * @brief Set WiFi channel
 */
int wfb_bridge_set_channel(uint8_t channel)
{
#ifdef ESP_PLATFORM
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    esp_err_t ret = esp_wifi_set_channel(channel, second);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set channel %d: %d", channel, ret);
        return -1;
    }
    ESP_LOGI(TAG, "Set WiFi channel to %d", channel);
#endif
    return 0;
}

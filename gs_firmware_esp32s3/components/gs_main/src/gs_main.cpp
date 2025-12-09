/**
 * @file gs_main.cpp
 * @brief ESP32-S3 Ground Station main implementation
 */

#include "gs_main.h"
#include "usb_ncm.h"
#include "unified_rx.h"
#include "frame_buffer.h"
#include "video_ws_server.h"

#include <string.h>
#include <inttypes.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "gs_main";

// Default configuration
#define DEFAULT_HTTP_PORT       80
#define DEFAULT_WIFI_CHANNEL    7
#define DEFAULT_FEC_K           6
#define DEFAULT_FEC_N           12
#define DEFAULT_LATENCY_MS      100

// Embedded web files
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t player_js_start[] asm("_binary_player_js_start");
extern const uint8_t player_js_end[] asm("_binary_player_js_end");

// State
static struct {
    gs_config_t config;
    gs_stats_t stats;
    httpd_handle_t http_server;
    TaskHandle_t process_task;
    bool initialized;
    bool running;
} g_gs;

// Forward declarations
static void video_frame_callback(const uint8_t* data, size_t len,
                                  const rx_video_metadata_t* metadata,
                                  void* user_data);
static void frame_output_callback(const uint8_t* data, size_t len,
                                   const fb_frame_info_t* info,
                                   void* user_data);
static void process_task(void* param);
static esp_err_t start_http_server(void);
static void stop_http_server(void);

// HTTP handlers
static esp_err_t index_handler(httpd_req_t* req);
static esp_err_t player_js_handler(httpd_req_t* req);
static esp_err_t stats_handler(httpd_req_t* req);
static esp_err_t config_handler(httpd_req_t* req);

/**
 * @brief Initialize ground station
 */
int gs_init(const gs_config_t* config)
{
    if (g_gs.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return -1;
    }

    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Apply configuration
    if (config) {
        g_gs.config = *config;
    } else {
        g_gs.config.http_port = DEFAULT_HTTP_PORT;
        g_gs.config.ws_port = DEFAULT_HTTP_PORT;
        g_gs.config.wifi_channel = DEFAULT_WIFI_CHANNEL;
        g_gs.config.device_id = 0;
        g_gs.config.gs_key_path = "/sdcard/gs.key";
        g_gs.config.fec_k = DEFAULT_FEC_K;
        g_gs.config.fec_n = DEFAULT_FEC_N;
        g_gs.config.target_latency_ms = DEFAULT_LATENCY_MS;
    }

    // Initialize USB NCM
    usb_ncm_config_t ncm_config = {
        .ip_addr = "192.168.7.1",
        .netmask = "255.255.255.0",
        .gw_addr = "192.168.7.1",
        .hostname = "esp32-fpv-gs"
    };
    if (usb_ncm_init(&ncm_config) != 0) {
        ESP_LOGE(TAG, "Failed to init USB NCM");
        return -1;
    }

    // Initialize unified receiver
    rx_config_t rx_config = {
        .protocol = PROTOCOL_AUTO,
        .wifi_channel = g_gs.config.wifi_channel,
        .device_id = g_gs.config.device_id,
        .gs_key_path = g_gs.config.gs_key_path,
        .wfb_channel_id = 0,
        .fec_k = g_gs.config.fec_k,
        .fec_n = g_gs.config.fec_n,
        .mtu = 1464,
        .frame_buffer_size = 256 * 1024,
        .max_concurrent_blocks = 16
    };
    if (unified_rx_init(&rx_config) != 0) {
        ESP_LOGE(TAG, "Failed to init unified receiver");
        return -1;
    }
    unified_rx_set_video_callback(video_frame_callback, NULL);

    // Initialize frame buffer
    fb_config_t fb_config = {
        .max_frame_size = 256 * 1024,
        .max_frames = 30,
        .target_latency_ms = g_gs.config.target_latency_ms,
        .max_latency_ms = 500,
        .reorder_frames = true,
        .use_psram = true
    };
    if (frame_buffer_init(&fb_config) != 0) {
        ESP_LOGE(TAG, "Failed to init frame buffer");
        return -1;
    }
    frame_buffer_set_output_callback(frame_output_callback, NULL);

    // Initialize WebSocket video server
    video_ws_server_config_t ws_config = {
        .port = g_gs.config.ws_port,
        .max_clients = 4,
        .send_buffer_size = 64 * 1024,
        .enable_cors = true
    };
    if (video_ws_server_init(&ws_config) != 0) {
        ESP_LOGE(TAG, "Failed to init WebSocket server");
        return -1;
    }

    memset(&g_gs.stats, 0, sizeof(g_gs.stats));
    g_gs.initialized = true;

    ESP_LOGI(TAG, "Ground station initialized");
    return 0;
}

/**
 * @brief Start ground station
 */
int gs_start(void)
{
    if (!g_gs.initialized) {
        ESP_LOGE(TAG, "Not initialized");
        return -1;
    }

    if (g_gs.running) {
        ESP_LOGW(TAG, "Already running");
        return 0;
    }

    // Start USB NCM
    if (usb_ncm_start() != 0) {
        ESP_LOGE(TAG, "Failed to start USB NCM");
        return -1;
    }

    // Initialize WiFi in monitor mode
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Set channel
    esp_wifi_set_channel(g_gs.config.wifi_channel, WIFI_SECOND_CHAN_NONE);

    // Enable promiscuous mode
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA
    };
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(
        (wifi_promiscuous_cb_t)gs_wifi_rx_callback));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    // Start HTTP server
    if (start_http_server() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return -1;
    }

    // Start WebSocket server
    if (video_ws_server_start() != 0) {
        ESP_LOGE(TAG, "Failed to start WebSocket server");
        return -1;
    }

    // Start unified receiver
    if (unified_rx_start() != 0) {
        ESP_LOGE(TAG, "Failed to start receiver");
        return -1;
    }

    // Create processing task
    xTaskCreatePinnedToCore(process_task, "gs_process", 4096, NULL,
                            5, &g_gs.process_task, 1);

    g_gs.running = true;
    ESP_LOGI(TAG, "Ground station started on channel %d", g_gs.config.wifi_channel);

    return 0;
}

/**
 * @brief Stop ground station
 */
void gs_stop(void)
{
    if (!g_gs.running) return;

    // Stop processing task
    if (g_gs.process_task) {
        vTaskDelete(g_gs.process_task);
        g_gs.process_task = NULL;
    }

    // Stop services
    unified_rx_stop();
    video_ws_server_stop();
    stop_http_server();

    // Disable promiscuous mode
    esp_wifi_set_promiscuous(false);
    esp_wifi_stop();

    usb_ncm_stop();

    g_gs.running = false;
    ESP_LOGI(TAG, "Ground station stopped");
}

/**
 * @brief WiFi promiscuous mode callback
 */
void gs_wifi_rx_callback(void* buf, int type)
{
    if (!g_gs.running) return;

    wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;

    // Extract RSSI and noise
    int8_t rssi = pkt->rx_ctrl.rssi;
    int8_t noise = -95;  // Typical noise floor

    // Pass to unified receiver
    unified_rx_process_packet(pkt->payload, pkt->rx_ctrl.sig_len, rssi, noise);
}

/**
 * @brief Video frame callback from unified receiver
 */
static void video_frame_callback(const uint8_t* data, size_t len,
                                  const rx_video_metadata_t* metadata,
                                  void* user_data)
{
    // Convert to frame buffer format
    fb_frame_info_t info = {
        .codec = (fb_codec_t)metadata->codec,
        .frame_index = metadata->frame_index,
        .timestamp = metadata->timestamp,
        .dts = metadata->timestamp,
        .width = metadata->width,
        .height = metadata->height,
        .flags = (uint8_t)(metadata->is_keyframe ? FB_FLAG_KEYFRAME : 0),
        .nal_type = 0
    };

    // Push to frame buffer
    frame_buffer_push(data, len, &info);

    g_gs.stats.frames_received++;
}

/**
 * @brief Frame output callback from frame buffer
 */
static void frame_output_callback(const uint8_t* data, size_t len,
                                   const fb_frame_info_t* info,
                                   void* user_data)
{
    // Send to WebSocket clients
    uint8_t codec = VIDEO_CODEC_MJPEG;
    switch (info->codec) {
        case FB_CODEC_H264: codec = VIDEO_CODEC_H264; break;
        case FB_CODEC_H265: codec = VIDEO_CODEC_H265; break;
        default: codec = VIDEO_CODEC_MJPEG; break;
    }

    bool is_keyframe = (info->flags & FB_FLAG_KEYFRAME) != 0;
    video_ws_server_send_frame(data, len, codec, info->timestamp, is_keyframe);

    g_gs.stats.frames_output++;
    if (is_keyframe) g_gs.stats.keyframes++;
}

/**
 * @brief Processing task
 */
static void process_task(void* param)
{
    while (g_gs.running) {
        // Process frame buffer
        frame_buffer_process();

        // Update statistics
        rx_stats_t rx_stats;
        unified_rx_get_stats(&rx_stats);

        fb_stats_t fb_stats;
        frame_buffer_get_stats(&fb_stats);

        g_gs.stats.packets_received = rx_stats.packets_received;
        g_gs.stats.packets_valid = rx_stats.packets_valid;
        g_gs.stats.packets_lost = rx_stats.packets_lost;
        g_gs.stats.fec_recovered = rx_stats.packets_fec_recovered;
        g_gs.stats.fps = rx_stats.fps;
        g_gs.stats.bitrate_kbps = rx_stats.bitrate_kbps;
        g_gs.stats.rssi = rx_stats.rssi;
        g_gs.stats.snr = rx_stats.snr;
        g_gs.stats.active_protocol = rx_stats.active_protocol;
        g_gs.stats.active_codec = rx_stats.active_codec;
        g_gs.stats.latency_ms = fb_stats.current_latency_ms;
        g_gs.stats.frames_dropped = fb_stats.frames_dropped;
        g_gs.stats.ws_clients = video_ws_server_get_client_count();
        g_gs.stats.usb_connected = usb_ncm_is_connected();

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

/**
 * @brief Get statistics
 */
void gs_get_stats(gs_stats_t* stats)
{
    if (stats) {
        *stats = g_gs.stats;
    }
}

/**
 * @brief Set WiFi channel
 */
int gs_set_channel(uint8_t channel)
{
    g_gs.config.wifi_channel = channel;
    esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    unified_rx_set_channel(channel);
    ESP_LOGI(TAG, "Set channel to %d", channel);
    return 0;
}

/**
 * @brief Get current channel
 */
uint8_t gs_get_channel(void)
{
    return g_gs.config.wifi_channel;
}

/**
 * @brief Set latency
 */
void gs_set_latency(uint16_t latency_ms)
{
    g_gs.config.target_latency_ms = latency_ms;
    frame_buffer_set_latency(latency_ms);
}

// ============================================================================
// HTTP Server
// ============================================================================

static esp_err_t index_handler(httpd_req_t* req)
{
    size_t html_size = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char*)index_html_start, html_size);
    return ESP_OK;
}

static esp_err_t player_js_handler(httpd_req_t* req)
{
    size_t js_size = player_js_end - player_js_start;
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char*)player_js_start, js_size);
    return ESP_OK;
}

static esp_err_t stats_handler(httpd_req_t* req)
{
    char json[512];
    snprintf(json, sizeof(json),
        "{\"packets\":%" PRIu32 ",\"frames\":%" PRIu32 ",\"fps\":%.1f,"
        "\"bitrate\":%" PRIu32 ",\"latency\":%" PRIu16 ",\"rssi\":%" PRId8 ","
        "\"snr\":%" PRIu8 ",\"protocol\":%" PRIu8 ",\"codec\":%" PRIu8 ","
        "\"clients\":%" PRIu32 ",\"usb\":%s}",
        g_gs.stats.packets_received,
        g_gs.stats.frames_output,
        g_gs.stats.fps,
        g_gs.stats.bitrate_kbps,
        g_gs.stats.latency_ms,
        g_gs.stats.rssi,
        g_gs.stats.snr,
        g_gs.stats.active_protocol,
        g_gs.stats.active_codec,
        g_gs.stats.ws_clients,
        g_gs.stats.usb_connected ? "true" : "false");

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));
    return ESP_OK;
}

static esp_err_t config_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET) {
        char json[256];
        snprintf(json, sizeof(json),
            "{\"channel\":%" PRIu8 ",\"latency\":%" PRIu16 "}",
            g_gs.config.wifi_channel,
            g_gs.config.target_latency_ms);

        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json, strlen(json));
    } else {
        // POST - update config
        char buf[100];
        int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (ret > 0) {
            buf[ret] = '\0';
            // Simple parsing (would use cJSON in production)
            uint8_t channel;
            uint16_t latency;
            if (sscanf(buf, "{\"channel\":%" SCNu8 ",\"latency\":%" SCNu16 "}",
                       &channel, &latency) >= 1) {
                if (channel >= 1 && channel <= 14) {
                    gs_set_channel(channel);
                }
                if (latency >= 10 && latency <= 1000) {
                    gs_set_latency(latency);
                }
            }
        }
        httpd_resp_send(req, "OK", 2);
    }
    return ESP_OK;
}

static esp_err_t start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = g_gs.config.http_port;
    config.lru_purge_enable = true;
    config.max_open_sockets = 7;

    esp_err_t ret = httpd_start(&g_gs.http_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %d", ret);
        return ret;
    }

    // Register handlers
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler
    };
    httpd_register_uri_handler(g_gs.http_server, &index_uri);

    httpd_uri_t player_uri = {
        .uri = "/player.js",
        .method = HTTP_GET,
        .handler = player_js_handler
    };
    httpd_register_uri_handler(g_gs.http_server, &player_uri);

    httpd_uri_t stats_uri = {
        .uri = "/api/stats",
        .method = HTTP_GET,
        .handler = stats_handler
    };
    httpd_register_uri_handler(g_gs.http_server, &stats_uri);

    httpd_uri_t config_get_uri = {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = config_handler
    };
    httpd_register_uri_handler(g_gs.http_server, &config_get_uri);

    httpd_uri_t config_post_uri = {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = config_handler
    };
    httpd_register_uri_handler(g_gs.http_server, &config_post_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", g_gs.config.http_port);
    return ESP_OK;
}

static void stop_http_server(void)
{
    if (g_gs.http_server) {
        httpd_stop(g_gs.http_server);
        g_gs.http_server = NULL;
    }
}

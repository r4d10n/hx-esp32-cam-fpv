/**
 * @file video_ws_server.cpp
 * @brief WebSocket server implementation for video streaming
 *
 * Uses ESP-IDF's HTTP server with WebSocket support to stream
 * raw H.264/H.265 NAL units to browser clients.
 */

#include "video_ws_server.h"

#ifdef ESP_PLATFORM
#include <esp_http_server.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>
#include <inttypes.h>

static const char* TAG = "video_ws_server";

// Default configuration
#define DEFAULT_PORT            8080
#define DEFAULT_MAX_CLIENTS     4
#define DEFAULT_SEND_BUFFER     (64 * 1024)
#define MAX_FRAME_SIZE          (128 * 1024)

// Client tracking
typedef struct {
    int fd;                     // Socket file descriptor
    bool connected;
    bool needs_config;          // Send codec config on next keyframe
    uint32_t bytes_sent;
    uint32_t frames_sent;
    uint32_t frames_dropped;
} ws_client_t;

// Server state
static struct {
    httpd_handle_t server;
    video_ws_server_config_t config;
    video_ws_server_stats_t stats;
    ws_client_t clients[8];     // Max 8 clients
    int client_count;
    SemaphoreHandle_t mutex;
    video_ws_client_callback_t client_callback;

    // Cached codec configuration
    uint8_t  codec_type;
    uint16_t video_width;
    uint16_t video_height;
    uint8_t  video_fps;
    uint8_t* sps_data;
    uint16_t sps_len;
    uint8_t* pps_data;
    uint16_t pps_len;
    uint8_t* vps_data;
    uint16_t vps_len;
    bool config_valid;

    bool initialized;
    bool running;
} g_server = {0};

// Forward declarations
static esp_err_t ws_handler(httpd_req_t* req);
static void send_to_client(ws_client_t* client, httpd_handle_t server,
                          const uint8_t* data, size_t len);
static int find_client_by_fd(int fd);
static int add_client(int fd);
static void remove_client(int fd);

/**
 * @brief WebSocket handler for incoming connections and messages
 */
static esp_err_t ws_handler(httpd_req_t* req)
{
    if (req->method == HTTP_GET) {
        // New WebSocket connection
        ESP_LOGI(TAG, "WebSocket handshake from client");
        return ESP_OK;
    }

    // Handle WebSocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(ws_pkt));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    // Get frame info first
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get frame info: %d", ret);
        return ret;
    }

    if (ws_pkt.len > 0) {
        // Allocate buffer for payload
        uint8_t* buf = (uint8_t*)malloc(ws_pkt.len + 1);
        if (!buf) {
            ESP_LOGE(TAG, "Failed to allocate buffer");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to receive frame: %d", ret);
            free(buf);
            return ret;
        }

        // Handle text messages (commands from client)
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
            buf[ws_pkt.len] = '\0';
            ESP_LOGI(TAG, "Received message: %s", buf);

            // Handle commands
            if (strcmp((char*)buf, "start") == 0) {
                // Client requesting stream start
                int fd = httpd_req_to_sockfd(req);
                int idx = find_client_by_fd(fd);
                if (idx >= 0) {
                    xSemaphoreTake(g_server.mutex, portMAX_DELAY);
                    g_server.clients[idx].needs_config = true;
                    xSemaphoreGive(g_server.mutex);
                    ESP_LOGI(TAG, "Client %d requested stream start", fd);
                }
            } else if (strcmp((char*)buf, "stop") == 0) {
                ESP_LOGI(TAG, "Client requested stream stop");
            }
        }

        free(buf);
    }

    return ESP_OK;
}

/**
 * @brief Callback when client connects/disconnects
 */
static esp_err_t ws_open_callback(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "WebSocket client connected: fd=%d", sockfd);

    int idx = add_client(sockfd);
    if (idx >= 0 && g_server.client_callback) {
        g_server.client_callback(sockfd, true);
    }
    return ESP_OK;
}

static void ws_close_callback(httpd_handle_t hd, int sockfd)
{
    ESP_LOGI(TAG, "WebSocket client disconnected: fd=%d", sockfd);

    remove_client(sockfd);
    if (g_server.client_callback) {
        g_server.client_callback(sockfd, false);
    }
}

/**
 * @brief Find client index by file descriptor
 */
static int find_client_by_fd(int fd)
{
    for (int i = 0; i < g_server.config.max_clients; i++) {
        if (g_server.clients[i].connected && g_server.clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Add a new client
 */
static int add_client(int fd)
{
    xSemaphoreTake(g_server.mutex, portMAX_DELAY);

    for (int i = 0; i < g_server.config.max_clients; i++) {
        if (!g_server.clients[i].connected) {
            g_server.clients[i].fd = fd;
            g_server.clients[i].connected = true;
            g_server.clients[i].needs_config = true;
            g_server.clients[i].bytes_sent = 0;
            g_server.clients[i].frames_sent = 0;
            g_server.clients[i].frames_dropped = 0;
            g_server.client_count++;
            g_server.stats.clients_connected++;

            xSemaphoreGive(g_server.mutex);
            return i;
        }
    }

    xSemaphoreGive(g_server.mutex);
    ESP_LOGW(TAG, "No room for new client");
    return -1;
}

/**
 * @brief Remove a client
 */
static void remove_client(int fd)
{
    xSemaphoreTake(g_server.mutex, portMAX_DELAY);

    int idx = -1;
    for (int i = 0; i < g_server.config.max_clients; i++) {
        if (g_server.clients[i].connected && g_server.clients[i].fd == fd) {
            idx = i;
            break;
        }
    }

    if (idx >= 0) {
        g_server.clients[idx].connected = false;
        g_server.client_count--;
    }

    xSemaphoreGive(g_server.mutex);
}

/**
 * @brief Send data to a specific client
 */
static void send_to_client(ws_client_t* client, httpd_handle_t server,
                          const uint8_t* data, size_t len)
{
    if (!client->connected) return;

    httpd_ws_frame_t ws_pkt = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = (uint8_t*)data,
        .len = len
    };

    esp_err_t ret = httpd_ws_send_frame_async(server, client->fd, &ws_pkt);
    if (ret == ESP_OK) {
        client->bytes_sent += len;
        client->frames_sent++;
        g_server.stats.total_bytes_sent += len;
    } else {
        client->frames_dropped++;
        g_server.stats.frames_dropped++;
        g_server.stats.send_errors++;
        ESP_LOGW(TAG, "Failed to send to client %d: %d", client->fd, ret);
    }
}

/**
 * @brief Initialize the video WebSocket server
 */
int video_ws_server_init(const video_ws_server_config_t* config)
{
    if (g_server.initialized) {
        ESP_LOGW(TAG, "Server already initialized");
        return -1;
    }

    // Set configuration
    if (config) {
        g_server.config = *config;
    } else {
        g_server.config.port = DEFAULT_PORT;
        g_server.config.max_clients = DEFAULT_MAX_CLIENTS;
        g_server.config.send_buffer_size = DEFAULT_SEND_BUFFER;
        g_server.config.enable_cors = true;
    }

    // Limit max clients
    if (g_server.config.max_clients > 8) {
        g_server.config.max_clients = 8;
    }

    // Create mutex
    g_server.mutex = xSemaphoreCreateMutex();
    if (!g_server.mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return -1;
    }

    // Initialize client array
    memset(g_server.clients, 0, sizeof(g_server.clients));
    g_server.client_count = 0;

    // Initialize stats
    memset(&g_server.stats, 0, sizeof(g_server.stats));

    g_server.initialized = true;
    ESP_LOGI(TAG, "Video WebSocket server initialized on port %d", g_server.config.port);

    return 0;
}

/**
 * @brief Start the WebSocket server
 */
int video_ws_server_start(void)
{
    if (!g_server.initialized) {
        ESP_LOGE(TAG, "Server not initialized");
        return -1;
    }

    if (g_server.running) {
        ESP_LOGW(TAG, "Server already running");
        return 0;
    }

    // Configure HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = g_server.config.port;
    config.max_open_sockets = g_server.config.max_clients + 1;
    config.lru_purge_enable = true;
    config.open_fn = ws_open_callback;
    config.close_fn = ws_close_callback;

    // Start server
    esp_err_t ret = httpd_start(&g_server.server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start server: %d", ret);
        return -1;
    }

    // Register WebSocket handler
    httpd_uri_t ws_uri = {
        .uri = "/video",
        .method = HTTP_GET,
        .handler = ws_handler,
        .user_ctx = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true
    };

    ret = httpd_register_uri_handler(g_server.server, &ws_uri);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register WebSocket handler: %d", ret);
        httpd_stop(g_server.server);
        return -1;
    }

    g_server.running = true;
    ESP_LOGI(TAG, "Video WebSocket server started on ws://0.0.0.0:%d/video",
             g_server.config.port);

    return 0;
}

/**
 * @brief Stop the WebSocket server
 */
void video_ws_server_stop(void)
{
    if (!g_server.running) return;

    httpd_stop(g_server.server);
    g_server.server = NULL;
    g_server.running = false;

    // Clear client list
    xSemaphoreTake(g_server.mutex, portMAX_DELAY);
    for (int i = 0; i < g_server.config.max_clients; i++) {
        g_server.clients[i].connected = false;
    }
    g_server.client_count = 0;
    xSemaphoreGive(g_server.mutex);

    ESP_LOGI(TAG, "Video WebSocket server stopped");
}

/**
 * @brief Send video frame to all connected clients
 */
int video_ws_server_send_frame(const uint8_t* data, size_t len,
                               uint8_t codec, uint32_t timestamp,
                               bool is_keyframe)
{
    if (!g_server.running || g_server.client_count == 0) {
        return 0;
    }

    // Build frame message
    size_t msg_len = sizeof(video_ws_frame_hdr_t) + len;
    uint8_t* msg = (uint8_t*)malloc(msg_len);
    if (!msg) {
        ESP_LOGE(TAG, "Failed to allocate frame message");
        return -1;
    }

    video_ws_frame_hdr_t* hdr = (video_ws_frame_hdr_t*)msg;
    hdr->msg_type = VIDEO_WS_MSG_FRAME;
    hdr->codec = codec;
    hdr->flags = is_keyframe ? VIDEO_WS_FLAG_KEYFRAME : 0;
    hdr->reserved = 0;
    hdr->timestamp = timestamp;
    hdr->frame_index = g_server.stats.frames_sent;
    hdr->data_len = len;

    memcpy(msg + sizeof(video_ws_frame_hdr_t), data, len);

    // Send to all clients
    int sent_count = 0;
    xSemaphoreTake(g_server.mutex, portMAX_DELAY);

    for (int i = 0; i < g_server.config.max_clients; i++) {
        ws_client_t* client = &g_server.clients[i];
        if (!client->connected) continue;

        // If client needs config, only send on keyframe
        if (client->needs_config) {
            if (is_keyframe && g_server.config_valid) {
                // Send config first
                video_ws_server_send_config(
                    g_server.codec_type,
                    g_server.video_width,
                    g_server.video_height,
                    g_server.video_fps,
                    g_server.sps_data, g_server.sps_len,
                    g_server.pps_data, g_server.pps_len,
                    g_server.vps_data, g_server.vps_len);
                client->needs_config = false;
            } else {
                // Skip non-keyframes until we send config
                continue;
            }
        }

        send_to_client(client, g_server.server, msg, msg_len);
        sent_count++;
    }

    g_server.stats.frames_sent++;
    xSemaphoreGive(g_server.mutex);

    free(msg);
    return sent_count;
}

/**
 * @brief Send codec configuration to all connected clients
 */
int video_ws_server_send_config(uint8_t codec, uint16_t width, uint16_t height,
                                uint8_t fps,
                                const uint8_t* sps, uint16_t sps_len,
                                const uint8_t* pps, uint16_t pps_len,
                                const uint8_t* vps, uint16_t vps_len)
{
    if (!g_server.running) return 0;

    // Cache config for new clients
    g_server.codec_type = codec;
    g_server.video_width = width;
    g_server.video_height = height;
    g_server.video_fps = fps;

    // Cache SPS
    if (g_server.sps_data) free(g_server.sps_data);
    g_server.sps_data = sps_len ? (uint8_t*)malloc(sps_len) : NULL;
    if (g_server.sps_data) {
        memcpy(g_server.sps_data, sps, sps_len);
        g_server.sps_len = sps_len;
    }

    // Cache PPS
    if (g_server.pps_data) free(g_server.pps_data);
    g_server.pps_data = pps_len ? (uint8_t*)malloc(pps_len) : NULL;
    if (g_server.pps_data) {
        memcpy(g_server.pps_data, pps, pps_len);
        g_server.pps_len = pps_len;
    }

    // Cache VPS (H.265 only)
    if (g_server.vps_data) free(g_server.vps_data);
    g_server.vps_data = vps_len ? (uint8_t*)malloc(vps_len) : NULL;
    if (g_server.vps_data) {
        memcpy(g_server.vps_data, vps, vps_len);
        g_server.vps_len = vps_len;
    }

    g_server.config_valid = true;

    // Build config message
    size_t msg_len = sizeof(video_ws_codec_config_t) + vps_len + sps_len + pps_len;
    uint8_t* msg = (uint8_t*)malloc(msg_len);
    if (!msg) return -1;

    video_ws_codec_config_t* cfg = (video_ws_codec_config_t*)msg;
    cfg->msg_type = VIDEO_WS_MSG_CODEC_CONFIG;
    cfg->codec = codec;
    cfg->width = width;
    cfg->height = height;
    cfg->fps = fps;
    cfg->profile = 0;
    cfg->sps_len = sps_len;
    cfg->pps_len = pps_len;
    cfg->vps_len = vps_len;

    uint8_t* ptr = msg + sizeof(video_ws_codec_config_t);
    if (vps && vps_len > 0) {
        memcpy(ptr, vps, vps_len);
        ptr += vps_len;
    }
    if (sps && sps_len > 0) {
        memcpy(ptr, sps, sps_len);
        ptr += sps_len;
    }
    if (pps && pps_len > 0) {
        memcpy(ptr, pps, pps_len);
    }

    // Send to all clients
    int sent_count = 0;
    xSemaphoreTake(g_server.mutex, portMAX_DELAY);

    for (int i = 0; i < g_server.config.max_clients; i++) {
        if (g_server.clients[i].connected) {
            send_to_client(&g_server.clients[i], g_server.server, msg, msg_len);
            sent_count++;
        }
    }

    xSemaphoreGive(g_server.mutex);
    free(msg);

    ESP_LOGI(TAG, "Sent codec config: %dx%d @ %d fps, codec=%d",
             width, height, fps, codec);

    return sent_count;
}

/**
 * @brief Send statistics update
 */
int video_ws_server_send_stats(const video_ws_stats_t* stats)
{
    if (!g_server.running || !stats) return 0;

    int sent_count = 0;
    xSemaphoreTake(g_server.mutex, portMAX_DELAY);

    for (int i = 0; i < g_server.config.max_clients; i++) {
        if (g_server.clients[i].connected) {
            send_to_client(&g_server.clients[i], g_server.server,
                          (const uint8_t*)stats, sizeof(*stats));
            sent_count++;
        }
    }

    xSemaphoreGive(g_server.mutex);
    return sent_count;
}

/**
 * @brief Get server statistics
 */
void video_ws_server_get_stats(video_ws_server_stats_t* stats)
{
    if (stats) {
        xSemaphoreTake(g_server.mutex, portMAX_DELAY);
        *stats = g_server.stats;
        xSemaphoreGive(g_server.mutex);
    }
}

/**
 * @brief Set client connection callback
 */
void video_ws_server_set_client_callback(video_ws_client_callback_t callback)
{
    g_server.client_callback = callback;
}

/**
 * @brief Get number of connected clients
 */
int video_ws_server_get_client_count(void)
{
    return g_server.client_count;
}

/**
 * @brief Check if NAL unit is a keyframe
 */
bool video_ws_is_keyframe(const uint8_t* data, size_t len, uint8_t codec)
{
    if (!data || len == 0) return false;

    if (codec == VIDEO_CODEC_H264) {
        uint8_t nal_type = data[0] & 0x1F;
        return nal_type == H264_NAL_IDR;
    } else if (codec == VIDEO_CODEC_H265) {
        uint8_t nal_type = (data[0] >> 1) & 0x3F;
        return nal_type == H265_NAL_IDR_W_RADL ||
               nal_type == H265_NAL_IDR_N_LP ||
               nal_type == H265_NAL_CRA_NUT;
    } else if (codec == VIDEO_CODEC_MJPEG) {
        // All MJPEG frames are keyframes
        return true;
    }

    return false;
}

/**
 * @brief Check if NAL unit is a parameter set
 */
bool video_ws_is_parameter_set(const uint8_t* data, size_t len, uint8_t codec)
{
    if (!data || len == 0) return false;

    if (codec == VIDEO_CODEC_H264) {
        uint8_t nal_type = data[0] & 0x1F;
        return nal_type == H264_NAL_SPS || nal_type == H264_NAL_PPS;
    } else if (codec == VIDEO_CODEC_H265) {
        uint8_t nal_type = (data[0] >> 1) & 0x3F;
        return nal_type == H265_NAL_VPS ||
               nal_type == H265_NAL_SPS ||
               nal_type == H265_NAL_PPS;
    }

    return false;
}

#else // !ESP_PLATFORM

// Stub implementation for non-ESP32 platforms
#include <stdio.h>

int video_ws_server_init(const video_ws_server_config_t* config)
{
    printf("video_ws_server: stub init\n");
    return 0;
}

int video_ws_server_start(void) { return 0; }
void video_ws_server_stop(void) {}

int video_ws_server_send_frame(const uint8_t* data, size_t len,
                               uint8_t codec, uint32_t timestamp,
                               bool is_keyframe)
{
    return 0;
}

int video_ws_server_send_config(uint8_t codec, uint16_t width, uint16_t height,
                                uint8_t fps,
                                const uint8_t* sps, uint16_t sps_len,
                                const uint8_t* pps, uint16_t pps_len,
                                const uint8_t* vps, uint16_t vps_len)
{
    return 0;
}

int video_ws_server_send_stats(const video_ws_stats_t* stats) { return 0; }
void video_ws_server_get_stats(video_ws_server_stats_t* stats) {}
void video_ws_server_set_client_callback(video_ws_client_callback_t cb) {}
int video_ws_server_get_client_count(void) { return 0; }
bool video_ws_is_keyframe(const uint8_t* data, size_t len, uint8_t codec) { return false; }
bool video_ws_is_parameter_set(const uint8_t* data, size_t len, uint8_t codec) { return false; }

#endif // ESP_PLATFORM

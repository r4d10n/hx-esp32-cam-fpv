/**
 * ESP32 FPV Ground Station - Web Server
 */

#include "web_server.h"
#include "frame_buffer.h"
#include "config_manager.h"
#include "esp_http_server.h"
#include "esp_timer.h"

static const char *TAG = "web_srv";

// Embedded files (from frontend/)
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[] asm("_binary_app_js_end");
extern const uint8_t style_css_start[] asm("_binary_style_css_start");
extern const uint8_t style_css_end[] asm("_binary_style_css_end");

static httpd_handle_t s_server = NULL;
static TaskHandle_t s_stream_task = NULL;
static volatile bool s_streaming = false;

// WebSocket client tracking
#define MAX_WS_CLIENTS 4
static int s_ws_fds[MAX_WS_CLIENTS] = {-1, -1, -1, -1};
static uint8_t s_ws_client_count = 0;
static SemaphoreHandle_t s_ws_mutex = NULL;

// Forward declarations
static esp_err_t index_handler(httpd_req_t *req);
static esp_err_t js_handler(httpd_req_t *req);
static esp_err_t css_handler(httpd_req_t *req);
static esp_err_t ws_handler(httpd_req_t *req);
static esp_err_t api_config_handler(httpd_req_t *req);
static esp_err_t api_stats_handler(httpd_req_t *req);
static esp_err_t api_channel_handler(httpd_req_t *req);
static void stream_task(void *arg);

// URI handlers
static const httpd_uri_t uri_index = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
};

static const httpd_uri_t uri_js = {
    .uri = "/app.js",
    .method = HTTP_GET,
    .handler = js_handler,
};

static const httpd_uri_t uri_css = {
    .uri = "/style.css",
    .method = HTTP_GET,
    .handler = css_handler,
};

static const httpd_uri_t uri_ws = {
    .uri = "/ws",
    .method = HTTP_GET,
    .handler = ws_handler,
    .is_websocket = true,
};

static const httpd_uri_t uri_api_config = {
    .uri = "/api/config",
    .method = HTTP_GET,
    .handler = api_config_handler,
};

static const httpd_uri_t uri_api_config_post = {
    .uri = "/api/config",
    .method = HTTP_POST,
    .handler = api_config_handler,
};

static const httpd_uri_t uri_api_stats = {
    .uri = "/api/stats",
    .method = HTTP_GET,
    .handler = api_stats_handler,
};

static const httpd_uri_t uri_api_channel = {
    .uri = "/api/channel",
    .method = HTTP_POST,
    .handler = api_channel_handler,
};

esp_err_t web_server_init(void)
{
    s_ws_mutex = xSemaphoreCreateMutex();
    if (s_ws_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create WebSocket mutex");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Web server initialized");
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (s_server != NULL) {
        return ESP_OK;  // Already running
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 10;
    config.max_open_sockets = MAX_WS_CLIENTS + 2;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register URI handlers
    httpd_register_uri_handler(s_server, &uri_index);
    httpd_register_uri_handler(s_server, &uri_js);
    httpd_register_uri_handler(s_server, &uri_css);
    httpd_register_uri_handler(s_server, &uri_ws);
    httpd_register_uri_handler(s_server, &uri_api_config);
    httpd_register_uri_handler(s_server, &uri_api_config_post);
    httpd_register_uri_handler(s_server, &uri_api_stats);
    httpd_register_uri_handler(s_server, &uri_api_channel);

    // Start streaming task
    s_streaming = true;
    xTaskCreatePinnedToCore(
        stream_task,
        "ws_stream",
        4096,
        NULL,
        4,
        &s_stream_task,
        1  // Core 1 (opposite of packet RX)
    );

    ESP_LOGI(TAG, "Web server started on port %d", config.server_port);
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    s_streaming = false;

    if (s_stream_task != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100));
        s_stream_task = NULL;
    }

    if (s_server != NULL) {
        httpd_stop(s_server);
        s_server = NULL;
    }

    return ESP_OK;
}

uint8_t web_server_get_client_count(void)
{
    return s_ws_client_count;
}

// Static file handlers
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)index_html_start,
                    index_html_end - index_html_start);
    return ESP_OK;
}

static esp_err_t js_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_send(req, (const char *)app_js_start,
                    app_js_end - app_js_start);
    return ESP_OK;
}

static esp_err_t css_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/css");
    httpd_resp_send(req, (const char *)style_css_start,
                    style_css_end - style_css_start);
    return ESP_OK;
}

// WebSocket handler
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket handshake");
        return ESP_OK;
    }

    // Handle WebSocket frame
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    // Client connected - add to list
    int fd = httpd_req_to_sockfd(req);

    xSemaphoreTake(s_ws_mutex, portMAX_DELAY);

    bool added = false;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] < 0) {
            s_ws_fds[i] = fd;
            s_ws_client_count++;
            added = true;
            ESP_LOGI(TAG, "WebSocket client connected, fd=%d, total=%d", fd, s_ws_client_count);
            break;
        }
    }

    xSemaphoreGive(s_ws_mutex);

    if (!added) {
        ESP_LOGW(TAG, "Max WebSocket clients reached");
    }

    g_stats.websocket_clients = s_ws_client_count;

    return ESP_OK;
}

// API handlers
static esp_err_t api_config_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        char buf[512];
        int len = config_to_json(buf, sizeof(buf));
        if (len > 0) {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, buf, len);
        } else {
            httpd_resp_send_500(req);
        }
    } else {
        // POST - update config
        char buf[256];
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';
            config_from_json(buf);
            httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
        } else {
            httpd_resp_send_400(req);
        }
    }
    return ESP_OK;
}

static esp_err_t api_stats_handler(httpd_req_t *req)
{
    char buf[512];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"packets_received\":%lu,"
        "\"packets_valid\":%lu,"
        "\"packets_invalid\":%lu,"
        "\"frames_complete\":%lu,"
        "\"frames_incomplete\":%lu,"
        "\"websocket_clients\":%lu,"
        "\"rssi_dbm\":%d,"
        "\"channel\":%d"
        "}",
        (unsigned long)g_stats.packets_received,
        (unsigned long)g_stats.packets_valid,
        (unsigned long)g_stats.packets_invalid,
        (unsigned long)g_stats.frames_complete,
        (unsigned long)g_stats.frames_incomplete,
        (unsigned long)g_stats.websocket_clients,
        g_stats.rssi_dbm,
        g_stats.channel
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}

static esp_err_t api_channel_handler(httpd_req_t *req)
{
    char buf[32];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len > 0) {
        buf[len] = '\0';

        // Parse channel number
        const char *ch_ptr = strstr(buf, "\"channel\":");
        if (ch_ptr) {
            int channel = atoi(ch_ptr + 10);
            if (channel >= 1 && channel <= 14) {
                config_set_channel((uint8_t)channel);
                httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
                return ESP_OK;
            }
        }
    }

    httpd_resp_send_400(req);
    return ESP_OK;
}

// Frame streaming task
static void stream_task(void *arg)
{
    ESP_LOGI(TAG, "Streaming task started");

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    while (s_streaming) {
        // Check for new frame
        if (!frame_buffer_has_new_frame()) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        frame_info_t *frame = frame_buffer_get_latest();
        if (frame == NULL) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        // Send to all connected WebSocket clients
        ws_pkt.payload = frame->data;
        ws_pkt.len = frame->size;

        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);

        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_ws_fds[i] >= 0) {
                esp_err_t ret = httpd_ws_send_frame_async(
                    s_server, s_ws_fds[i], &ws_pkt);

                if (ret != ESP_OK) {
                    // Client disconnected
                    ESP_LOGI(TAG, "WebSocket client disconnected, fd=%d", s_ws_fds[i]);
                    s_ws_fds[i] = -1;
                    if (s_ws_client_count > 0) s_ws_client_count--;
                    g_stats.websocket_clients = s_ws_client_count;
                } else {
                    g_stats.websocket_frames_sent++;
                }
            }
        }

        xSemaphoreGive(s_ws_mutex);

        // Release frame
        frame_buffer_release(frame);

        // Small delay to prevent overwhelming clients
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGI(TAG, "Streaming task exiting");
    vTaskDelete(NULL);
}

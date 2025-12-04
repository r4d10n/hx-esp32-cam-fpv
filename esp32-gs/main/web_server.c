/**
 * ESP32 FPV Ground Station - Web Server
 * With captive portal, client IP logging, and FEC stats logging
 */

#include "web_server.h"
#include "frame_buffer.h"
#include "config_manager.h"
#include "packet_tx.h"
#include "fec_decoder.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "web_srv";

// FEC stats logging interval (5 seconds)
#define STATS_LOG_INTERVAL_MS 5000
static esp_timer_handle_t s_stats_timer = NULL;

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
typedef struct {
    int fd;
    char ip[16];
} ws_client_t;
static ws_client_t s_ws_clients[MAX_WS_CLIENTS];
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
static esp_err_t api_camera_handler(httpd_req_t *req);
static esp_err_t api_air_stats_handler(httpd_req_t *req);
static esp_err_t captive_portal_handler(httpd_req_t *req);
static void stream_task(void *arg);
static void stats_log_callback(void *arg);
static void get_client_ip(httpd_req_t *req, char *ip_str, size_t len);

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

static const httpd_uri_t uri_api_camera = {
    .uri = "/api/camera",
    .method = HTTP_GET,
    .handler = api_camera_handler,
};

static const httpd_uri_t uri_api_camera_post = {
    .uri = "/api/camera",
    .method = HTTP_POST,
    .handler = api_camera_handler,
};

static const httpd_uri_t uri_api_air_stats = {
    .uri = "/api/air",
    .method = HTTP_GET,
    .handler = api_air_stats_handler,
};

// Captive portal handlers - redirect to main page
// Android captive portal detection
static const httpd_uri_t uri_generate_204 = {
    .uri = "/generate_204",
    .method = HTTP_GET,
    .handler = captive_portal_handler,
};

// Apple/iOS captive portal detection
static const httpd_uri_t uri_hotspot_detect = {
    .uri = "/hotspot-detect.html",
    .method = HTTP_GET,
    .handler = captive_portal_handler,
};

// Windows captive portal detection
static const httpd_uri_t uri_ncsi = {
    .uri = "/ncsi.txt",
    .method = HTTP_GET,
    .handler = captive_portal_handler,
};

// Generic connectivity check
static const httpd_uri_t uri_connectivity_check = {
    .uri = "/connectivitycheck",
    .method = HTTP_GET,
    .handler = captive_portal_handler,
};

// Firefox captive portal detection
static const httpd_uri_t uri_success_txt = {
    .uri = "/success.txt",
    .method = HTTP_GET,
    .handler = captive_portal_handler,
};

esp_err_t web_server_init(void)
{
    // Initialize client tracking
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        s_ws_clients[i].fd = -1;
        s_ws_clients[i].ip[0] = '\0';
    }

    s_ws_mutex = xSemaphoreCreateMutex();
    if (s_ws_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create WebSocket mutex");
        return ESP_ERR_NO_MEM;
    }

    // Create periodic stats logging timer
    const esp_timer_create_args_t timer_args = {
        .callback = stats_log_callback,
        .name = "stats_log"
    };
    esp_err_t ret = esp_timer_create(&timer_args, &s_stats_timer);
    if (ret == ESP_OK) {
        esp_timer_start_periodic(s_stats_timer, STATS_LOG_INTERVAL_MS * 1000);
        ESP_LOGI(TAG, "Stats logging enabled (every %d ms)", STATS_LOG_INTERVAL_MS);
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
    config.max_uri_handlers = 20;  // Increased for captive portal
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
    httpd_register_uri_handler(s_server, &uri_api_camera);
    httpd_register_uri_handler(s_server, &uri_api_camera_post);
    httpd_register_uri_handler(s_server, &uri_api_air_stats);

    // Register captive portal handlers
    httpd_register_uri_handler(s_server, &uri_generate_204);
    httpd_register_uri_handler(s_server, &uri_hotspot_detect);
    httpd_register_uri_handler(s_server, &uri_ncsi);
    httpd_register_uri_handler(s_server, &uri_connectivity_check);
    httpd_register_uri_handler(s_server, &uri_success_txt);

    // Start streaming task - higher priority for smooth video
    s_streaming = true;
    xTaskCreatePinnedToCore(
        stream_task,
        "ws_stream",
        6144,   // Larger stack
        NULL,
        configMAX_PRIORITIES - 3,  // High priority, just below packet RX
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

// Helper to get client IP address from request
static void get_client_ip(httpd_req_t *req, char *ip_str, size_t len)
{
    int fd = httpd_req_to_sockfd(req);
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if (getpeername(fd, (struct sockaddr *)&addr, &addr_len) == 0) {
        inet_ntoa_r(addr.sin_addr, ip_str, len);
    } else {
        strncpy(ip_str, "unknown", len);
    }
}

// WebSocket handler
static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        // WebSocket handshake - add client to list
        int fd = httpd_req_to_sockfd(req);

        // Get client IP for logging
        char client_ip[16];
        get_client_ip(req, client_ip, sizeof(client_ip));

        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);

        bool added = false;
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_ws_clients[i].fd < 0) {
                s_ws_clients[i].fd = fd;
                strncpy(s_ws_clients[i].ip, client_ip, sizeof(s_ws_clients[i].ip) - 1);
                s_ws_clients[i].ip[sizeof(s_ws_clients[i].ip) - 1] = '\0';
                s_ws_client_count++;
                added = true;
                ESP_LOGI(TAG, "WS client connected: %s (fd=%d) total=%d",
                         client_ip, fd, s_ws_client_count);
                break;
            }
        }

        xSemaphoreGive(s_ws_mutex);

        if (!added) {
            ESP_LOGW(TAG, "Max WS clients reached (from %s)", client_ip);
        }

        g_stats.websocket_clients = s_ws_client_count;
        return ESP_OK;
    }

    // Handle incoming WebSocket frame (if client sends anything)
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        return ret;
    }

    // Just acknowledge any message received
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
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
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

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid channel");
    return ESP_OK;
}

// Frame streaming task - optimized for low latency
static void stream_task(void *arg)
{
    (void)arg;  // Unused
    ESP_LOGI(TAG, "Streaming task started");

    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_BINARY;

    TickType_t last_send = 0;
    const TickType_t min_frame_interval = pdMS_TO_TICKS(16);  // ~60fps max

    while (s_streaming) {
        // Check for new frame - minimal delay for responsiveness
        if (!frame_buffer_has_new_frame()) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        // Rate limiting - don't send faster than client can handle
        TickType_t now = xTaskGetTickCount();
        if (now - last_send < min_frame_interval) {
            vTaskDelay(pdMS_TO_TICKS(1));
            continue;
        }

        frame_info_t *frame = frame_buffer_get_latest();
        if (frame == NULL) {
            continue;
        }

        // Send to all connected WebSocket clients
        ws_pkt.payload = frame->data;
        ws_pkt.len = frame->size;

        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);

        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_ws_clients[i].fd >= 0) {
                esp_err_t ret = httpd_ws_send_frame_async(
                    s_server, s_ws_clients[i].fd, &ws_pkt);

                if (ret != ESP_OK) {
                    // Client disconnected or send failed
                    ESP_LOGI(TAG, "WS client disconnected: %s", s_ws_clients[i].ip);
                    s_ws_clients[i].fd = -1;
                    s_ws_clients[i].ip[0] = '\0';
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
        last_send = now;

        // Yield to other tasks
        taskYIELD();
    }

    ESP_LOGI(TAG, "Streaming task exiting");
    vTaskDelete(NULL);
}

// Camera control API handler
static esp_err_t api_camera_handler(httpd_req_t *req)
{
    camera_config_t *cam = packet_tx_get_camera_config();

    if (req->method == HTTP_GET) {
        // Return current camera configuration as JSON
        char buf[1024];
        int len = snprintf(buf, sizeof(buf),
            "{"
            "\"resolution\":%d,"
            "\"fps_limit\":%d,"
            "\"quality\":%d,"
            "\"brightness\":%d,"
            "\"contrast\":%d,"
            "\"saturation\":%d,"
            "\"sharpness\":%d,"
            "\"denoise\":%d,"
            "\"special_effect\":%d,"
            "\"awb\":%s,"
            "\"awb_gain\":%s,"
            "\"wb_mode\":%d,"
            "\"aec\":%s,"
            "\"aec2\":%s,"
            "\"ae_level\":%d,"
            "\"aec_value\":%d,"
            "\"agc\":%s,"
            "\"agc_gain\":%d,"
            "\"gainceiling\":%d,"
            "\"bpc\":%s,"
            "\"wpc\":%s,"
            "\"raw_gma\":%s,"
            "\"lenc\":%s,"
            "\"hmirror\":%s,"
            "\"vflip\":%s,"
            "\"connected\":%s,"
            "\"latency_ms\":%lu"
            "}",
            cam->resolution,
            cam->fps_limit,
            cam->quality,
            cam->brightness,
            cam->contrast,
            cam->saturation,
            cam->sharpness,
            cam->denoise,
            cam->special_effect,
            cam->awb ? "true" : "false",
            cam->awb_gain ? "true" : "false",
            cam->wb_mode,
            cam->aec ? "true" : "false",
            cam->aec2 ? "true" : "false",
            cam->ae_level,
            cam->aec_value,
            cam->agc ? "true" : "false",
            cam->agc_gain,
            cam->gainceiling,
            cam->bpc ? "true" : "false",
            cam->wpc ? "true" : "false",
            cam->raw_gma ? "true" : "false",
            cam->lenc ? "true" : "false",
            cam->hmirror ? "true" : "false",
            cam->vflip ? "true" : "false",
            packet_tx_is_connected() ? "true" : "false",
            (unsigned long)packet_tx_get_latency_ms()
        );

        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, buf, len);
    } else {
        // POST - update camera configuration
        char buf[512];
        int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = '\0';

            // Simple JSON parsing for camera parameters
            char *p;

            // Resolution
            if ((p = strstr(buf, "\"resolution\":")) != NULL) {
                cam->resolution = (uint8_t)atoi(p + 13);
            }
            // Quality
            if ((p = strstr(buf, "\"quality\":")) != NULL) {
                cam->quality = (uint8_t)atoi(p + 10);
            }
            // Brightness
            if ((p = strstr(buf, "\"brightness\":")) != NULL) {
                cam->brightness = (int8_t)atoi(p + 13);
            }
            // Contrast
            if ((p = strstr(buf, "\"contrast\":")) != NULL) {
                cam->contrast = (int8_t)atoi(p + 11);
            }
            // Saturation
            if ((p = strstr(buf, "\"saturation\":")) != NULL) {
                cam->saturation = (int8_t)atoi(p + 13);
            }
            // Sharpness
            if ((p = strstr(buf, "\"sharpness\":")) != NULL) {
                cam->sharpness = (int8_t)atoi(p + 12);
            }
            // AE Level
            if ((p = strstr(buf, "\"ae_level\":")) != NULL) {
                cam->ae_level = (int8_t)atoi(p + 11);
            }
            // AEC Value (manual exposure)
            if ((p = strstr(buf, "\"aec_value\":")) != NULL) {
                cam->aec_value = (uint16_t)atoi(p + 12);
            }
            // AGC Gain
            if ((p = strstr(buf, "\"agc_gain\":")) != NULL) {
                cam->agc_gain = (uint8_t)atoi(p + 11);
            }
            // Gain ceiling
            if ((p = strstr(buf, "\"gainceiling\":")) != NULL) {
                cam->gainceiling = (uint8_t)atoi(p + 14);
            }
            // WB Mode
            if ((p = strstr(buf, "\"wb_mode\":")) != NULL) {
                cam->wb_mode = (uint8_t)atoi(p + 10);
            }
            // FPS limit
            if ((p = strstr(buf, "\"fps_limit\":")) != NULL) {
                cam->fps_limit = (uint8_t)atoi(p + 12);
            }

            // Boolean parameters
            if ((p = strstr(buf, "\"aec\":")) != NULL) {
                cam->aec = (strstr(p + 6, "true") == p + 6);
            }
            if ((p = strstr(buf, "\"aec2\":")) != NULL) {
                cam->aec2 = (strstr(p + 7, "true") == p + 7);
            }
            if ((p = strstr(buf, "\"agc\":")) != NULL) {
                cam->agc = (strstr(p + 6, "true") == p + 6);
            }
            if ((p = strstr(buf, "\"awb\":")) != NULL) {
                cam->awb = (strstr(p + 6, "true") == p + 6);
            }
            if ((p = strstr(buf, "\"hmirror\":")) != NULL) {
                cam->hmirror = (strstr(p + 10, "true") == p + 10);
            }
            if ((p = strstr(buf, "\"vflip\":")) != NULL) {
                cam->vflip = (strstr(p + 8, "true") == p + 8);
            }

            // Mark config as changed to trigger immediate send
            packet_tx_config_changed();

            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
        } else {
            httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        }
    }
    return ESP_OK;
}

// Air unit statistics API handler
static esp_err_t api_air_stats_handler(httpd_req_t *req)
{
    air_stats_t *air = packet_tx_get_air_stats();
    fec_stats_t fec_stats;
    fec_decoder_get_stats(&fec_stats);

    char buf[1024];
    int len = snprintf(buf, sizeof(buf),
        "{"
        "\"connected\":%s,"
        "\"latency_ms\":%lu,"
        "\"air\":{"
            "\"rssi_dbm\":%d,"
            "\"noise_floor_dbm\":%d,"
            "\"temperature\":%d,"
            "\"capture_fps\":%d,"
            "\"resolution\":%d,"
            "\"quality\":%d,"
            "\"wifi_channel\":%d,"
            "\"wifi_rate\":%d,"
            "\"out_packet_rate\":%d,"
            "\"in_packet_rate\":%d,"
            "\"sd_detected\":%s,"
            "\"sd_error\":%s,"
            "\"recording\":%s,"
            "\"sd_free_gb\":%.2f,"
            "\"sd_total_gb\":%.2f,"
            "\"cam_ovf_count\":%d,"
            "\"frame_size_min\":%d,"
            "\"frame_size_max\":%d,"
            "\"overheat\":%s,"
            "\"is_ov5640\":%s"
        "},"
        "\"fec\":{"
            "\"blocks_received\":%lu,"
            "\"blocks_complete\":%lu,"
            "\"blocks_recovered\":%lu,"
            "\"blocks_failed\":%lu,"
            "\"packets_recovered\":%lu"
        "}"
        "}",
        packet_tx_is_connected() ? "true" : "false",
        (unsigned long)packet_tx_get_latency_ms(),
        air->rssi_dbm,
        air->noise_floor_dbm,
        air->temperature,
        air->capture_fps,
        air->resolution,
        air->curr_quality,
        air->wifi_channel,
        air->curr_wifi_rate,
        air->out_packet_rate,
        air->in_packet_rate,
        air->sd_detected ? "true" : "false",
        air->sd_error ? "true" : "false",
        air->air_record_state ? "true" : "false",
        (float)air->sd_free_space_gb16 / 16.0f,
        (float)air->sd_total_space_gb16 / 16.0f,
        air->cam_ovf_count,
        air->cam_frame_size_min,
        air->cam_frame_size_max,
        air->overheat_throttling ? "true" : "false",
        air->is_ov5640 ? "true" : "false",
        (unsigned long)fec_stats.blocks_received,
        (unsigned long)fec_stats.blocks_complete,
        (unsigned long)fec_stats.blocks_recovered,
        (unsigned long)fec_stats.blocks_failed,
        (unsigned long)fec_stats.packets_recovered
    );

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, len);
    return ESP_OK;
}

// Captive portal handler - redirect to main page
static esp_err_t captive_portal_handler(httpd_req_t *req)
{
    char client_ip[16];
    get_client_ip(req, client_ip, sizeof(client_ip));
    ESP_LOGI(TAG, "Captive portal request from %s: %s", client_ip, req->uri);

    // Redirect to the main page with 302 Found
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");

    // Send a small HTML body with redirect for browsers that don't follow 302
    const char *redirect_html =
        "<!DOCTYPE html><html><head>"
        "<meta http-equiv=\"refresh\" content=\"0;url=/\">"
        "<title>FPV Ground Station</title></head>"
        "<body><p>Redirecting to <a href=\"/\">FPV Ground Station</a>...</p></body></html>";

    httpd_resp_send(req, redirect_html, strlen(redirect_html));
    return ESP_OK;
}

// Resolution name lookup
static const char *get_resolution_name(uint8_t res)
{
    static const char *names[] = {
        "QVGA", "CIF", "HVGA", "VGA", "VGA16:9", "SVGA", "SVGA16:9",
        "XGA", "XGA16:9", "SXGA", "HD720", "UXGA"
    };
    if (res < sizeof(names) / sizeof(names[0])) {
        return names[res];
    }
    return "???";
}

// Periodic stats logging callback
static void stats_log_callback(void *arg)
{
    (void)arg;

    fec_stats_t fec;
    fec_decoder_get_stats(&fec);
    air_stats_t *air = packet_tx_get_air_stats();

    // Calculate FEC recovery rate
    uint32_t total_blocks = fec.blocks_complete + fec.blocks_recovered + fec.blocks_failed;
    float recovery_rate = 0.0f;
    if (total_blocks > 0) {
        recovery_rate = (float)(fec.blocks_complete + fec.blocks_recovered) * 100.0f / total_blocks;
    }

    // Header
    printf("\n=== FPV GS Stats ===\n");

    // Link status
    printf("CH:%d RSSI:%ddBm %s\n",
           g_stats.channel, g_stats.rssi_dbm,
           packet_tx_is_connected() ? "CONNECTED" : "NO-LINK");

    // FEC stats
    printf("FEC: blk=%lu ok=%lu rec=%lu fail=%lu (%.1f%%)\n",
           (unsigned long)total_blocks,
           (unsigned long)fec.blocks_complete,
           (unsigned long)fec.blocks_recovered,
           (unsigned long)fec.blocks_failed,
           recovery_rate);

    // Frame stats
    printf("PKT: rx=%lu valid=%lu | FRM: ok=%lu lost=%lu\n",
           (unsigned long)g_stats.packets_received,
           (unsigned long)g_stats.packets_valid,
           (unsigned long)g_stats.frames_complete,
           (unsigned long)g_stats.frames_incomplete);

    // Air unit stats (if connected)
    if (packet_tx_is_connected()) {
        printf("AIR: %s Q:%d FPS:%d Temp:%dC\n",
               get_resolution_name(air->resolution),
               air->curr_quality,
               air->capture_fps,
               air->temperature);
    }

    // Web clients
    if (s_ws_client_count > 0) {
        printf("WS[%d]:", s_ws_client_count);
        xSemaphoreTake(s_ws_mutex, pdMS_TO_TICKS(10));
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_ws_clients[i].fd >= 0) {
                printf(" %s", s_ws_clients[i].ip);
            }
        }
        xSemaphoreGive(s_ws_mutex);
        printf("\n");
    } else {
        printf("WS: no clients\n");
    }

    printf("====================\n");
}

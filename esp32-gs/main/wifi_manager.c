/**
 * ESP32 FPV Ground Station - WiFi Manager
 */

#include "wifi_manager.h"
#include "packet_rx.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include <assert.h>

static const char *TAG = "wifi_mgr";

static esp_netif_t *s_ap_netif = NULL;
static uint8_t s_station_count = 0;

// Promiscuous mode callback - forward to packet_rx module
static void IRAM_ATTR wifi_promiscuous_cb(void *buf, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_DATA && type != WIFI_PKT_MGMT) {
        return;
    }

    const wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buf;

    // Forward to packet processing
    packet_rx_handle(pkt->payload, pkt->rx_ctrl.sig_len, pkt->rx_ctrl.rssi);
}

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;  // Unused

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_AP_START:
                ESP_LOGI(TAG, "AP started on channel %d", g_config.channel);
                break;

            case WIFI_EVENT_AP_STOP:
                ESP_LOGI(TAG, "AP stopped");
                break;

            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
                s_station_count++;
                g_stats.websocket_clients = s_station_count;
                ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x connected, total=%u",
                         (unsigned)event->mac[0], (unsigned)event->mac[1],
                         (unsigned)event->mac[2], (unsigned)event->mac[3],
                         (unsigned)event->mac[4], (unsigned)event->mac[5],
                         (unsigned)s_station_count);
                break;
            }

            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
                if (s_station_count > 0) s_station_count--;
                g_stats.websocket_clients = s_station_count;
                ESP_LOGI(TAG, "Station %02x:%02x:%02x:%02x:%02x:%02x disconnected, total=%u",
                         (unsigned)event->mac[0], (unsigned)event->mac[1],
                         (unsigned)event->mac[2], (unsigned)event->mac[3],
                         (unsigned)event->mac[4], (unsigned)event->mac[5],
                         (unsigned)s_station_count);
                break;
            }

            default:
                break;
        }
    }
}

esp_err_t wifi_manager_init(void)
{
    esp_err_t ret;

    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Create AP network interface
    s_ap_netif = esp_netif_create_default_wifi_ap();
    assert(s_ap_netif);

    // Initialize WiFi with default config
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = false;  // We manage NVS ourselves

    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    // Set WiFi mode to AP
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .channel = g_config.channel,
            .max_connection = CONFIG_FPV_GS_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    // Set SSID and password
    strncpy((char *)ap_config.ap.ssid, g_config.ap_ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(g_config.ap_ssid);

    if (strlen(g_config.ap_password) >= 8) {
        strncpy((char *)ap_config.ap.password, g_config.ap_password, sizeof(ap_config.ap.password));
    } else {
        // Open network if password too short
        ap_config.ap.authmode = WIFI_AUTH_OPEN;
        ESP_LOGW(TAG, "Password too short, using open network");
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // Set country for proper channel support
    wifi_country_t country = {
        .cc = "US",
        .schan = 1,
        .nchan = 11,
        .policy = WIFI_COUNTRY_POLICY_AUTO,
    };
    esp_wifi_set_country(&country);

    ESP_LOGI(TAG, "WiFi initialized: SSID=%s, CH=%d", g_config.ap_ssid, g_config.channel);

    return ESP_OK;
}

esp_err_t wifi_manager_start(void)
{
    esp_err_t ret;

    // Start WiFi
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WiFi: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set channel explicitly (AP config might not apply immediately)
    ret = wifi_set_channel(g_config.channel);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set channel: %s", esp_err_to_name(ret));
    }

    // Enable promiscuous mode with filter
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_DATA | WIFI_PROMIS_FILTER_MASK_DATA_MPDU,
    };
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_filter(&filter));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(&wifi_promiscuous_cb));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    ESP_LOGI(TAG, "WiFi started with promiscuous mode enabled");

    g_stats.channel = g_config.channel;

    return ESP_OK;
}

esp_err_t wifi_manager_stop(void)
{
    esp_wifi_set_promiscuous(false);
    return esp_wifi_stop();
}

esp_err_t wifi_set_channel(uint8_t channel)
{
    if (channel < 1 || channel > 14) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (ret == ESP_OK) {
        g_config.channel = channel;
        g_stats.channel = channel;
        ESP_LOGI(TAG, "Channel set to %d", channel);
    } else {
        ESP_LOGE(TAG, "Failed to set channel %d: %s", channel, esp_err_to_name(ret));
    }

    return ret;
}

uint8_t wifi_get_channel(void)
{
    uint8_t primary;
    wifi_second_chan_t second;

    if (esp_wifi_get_channel(&primary, &second) == ESP_OK) {
        return primary;
    }
    return g_config.channel;
}

esp_err_t wifi_set_promiscuous(bool enable)
{
    return esp_wifi_set_promiscuous(enable);
}

uint8_t wifi_get_station_count(void)
{
    return s_station_count;
}

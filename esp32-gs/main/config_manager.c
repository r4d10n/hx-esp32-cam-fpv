/**
 * ESP32 FPV Ground Station - Configuration Manager
 */

#include "config_manager.h"
#include "wifi_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "config_mgr";

#define NVS_NAMESPACE "fpv_gs"
#define NVS_KEY_SSID "ap_ssid"
#define NVS_KEY_PASS "ap_pass"
#define NVS_KEY_CHANNEL "channel"
#define NVS_KEY_FEC_K "fec_k"
#define NVS_KEY_FEC_N "fec_n"
#define NVS_KEY_FEC_EN "fec_en"

esp_err_t config_manager_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Load configuration
    return config_load();
}

esp_err_t config_load(void)
{
    nvs_handle_t handle;
    esp_err_t ret;

    // Set defaults first
    config_reset_defaults();

    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved config, using defaults");
        return ESP_OK;
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    // Load SSID
    size_t len = sizeof(g_config.ap_ssid);
    nvs_get_str(handle, NVS_KEY_SSID, g_config.ap_ssid, &len);

    // Load password
    len = sizeof(g_config.ap_password);
    nvs_get_str(handle, NVS_KEY_PASS, g_config.ap_password, &len);

    // Load channel
    uint8_t channel;
    if (nvs_get_u8(handle, NVS_KEY_CHANNEL, &channel) == ESP_OK) {
        g_config.channel = channel;
    }

    // Load FEC settings
    nvs_get_u8(handle, NVS_KEY_FEC_K, &g_config.fec_k);
    nvs_get_u8(handle, NVS_KEY_FEC_N, &g_config.fec_n);

    uint8_t fec_en;
    if (nvs_get_u8(handle, NVS_KEY_FEC_EN, &fec_en) == ESP_OK) {
        g_config.fec_enabled = fec_en ? true : false;
    }

    nvs_close(handle);

    ESP_LOGI(TAG, "Config loaded: SSID=%s, CH=%d, FEC=%s (K=%d, N=%d)",
             g_config.ap_ssid, g_config.channel,
             g_config.fec_enabled ? "on" : "off",
             g_config.fec_k, g_config.fec_n);

    return ESP_OK;
}

esp_err_t config_save(void)
{
    nvs_handle_t handle;
    esp_err_t ret;

    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for write: %s", esp_err_to_name(ret));
        return ret;
    }

    nvs_set_str(handle, NVS_KEY_SSID, g_config.ap_ssid);
    nvs_set_str(handle, NVS_KEY_PASS, g_config.ap_password);
    nvs_set_u8(handle, NVS_KEY_CHANNEL, g_config.channel);
    nvs_set_u8(handle, NVS_KEY_FEC_K, g_config.fec_k);
    nvs_set_u8(handle, NVS_KEY_FEC_N, g_config.fec_n);
    nvs_set_u8(handle, NVS_KEY_FEC_EN, g_config.fec_enabled ? 1 : 0);

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Config saved");
    } else {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t config_reset_defaults(void)
{
    strncpy(g_config.ap_ssid, CONFIG_FPV_GS_DEFAULT_AP_SSID, sizeof(g_config.ap_ssid) - 1);
    strncpy(g_config.ap_password, CONFIG_FPV_GS_DEFAULT_AP_PASS, sizeof(g_config.ap_password) - 1);
    g_config.channel = CONFIG_FPV_GS_DEFAULT_CHANNEL;
    g_config.fec_k = CONFIG_FPV_GS_FEC_K;
    g_config.fec_n = CONFIG_FPV_GS_FEC_N;
    g_config.fec_enabled = false;  // Start with primary packets only

    ESP_LOGI(TAG, "Config reset to defaults");
    return ESP_OK;
}

esp_err_t config_set_channel(uint8_t channel)
{
    if (channel < 1 || channel > 14) {
        ESP_LOGE(TAG, "Invalid channel: %d", channel);
        return ESP_ERR_INVALID_ARG;
    }

    g_config.channel = channel;
    g_stats.channel = channel;

    // Apply channel change immediately (promiscuous mode is always enabled)
    esp_err_t ret = wifi_set_channel(channel);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Channel changed to %d", channel);
        config_save();
    }
    return ret;
}

esp_err_t config_set_ap_credentials(const char *ssid, const char *password)
{
    if (ssid == NULL || strlen(ssid) == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (password != NULL && strlen(password) > 0 && strlen(password) < 8) {
        ESP_LOGE(TAG, "Password must be at least 8 characters");
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(g_config.ap_ssid, ssid, sizeof(g_config.ap_ssid) - 1);
    g_config.ap_ssid[sizeof(g_config.ap_ssid) - 1] = '\0';

    if (password != NULL) {
        strncpy(g_config.ap_password, password, sizeof(g_config.ap_password) - 1);
        g_config.ap_password[sizeof(g_config.ap_password) - 1] = '\0';
    }

    ESP_LOGI(TAG, "AP credentials updated (restart required)");
    return config_save();
}

int config_to_json(char *buf, size_t buf_len)
{
    return snprintf(buf, buf_len,
        "{"
        "\"ssid\":\"%s\","
        "\"channel\":%d,"
        "\"fec_enabled\":%s,"
        "\"fec_k\":%d,"
        "\"fec_n\":%d,"
        "\"stats\":{"
            "\"packets_received\":%lu,"
            "\"packets_valid\":%lu,"
            "\"frames_complete\":%lu,"
            "\"frames_incomplete\":%lu,"
            "\"websocket_clients\":%lu,"
            "\"rssi_dbm\":%d"
        "}"
        "}",
        g_config.ap_ssid,
        g_config.channel,
        g_config.fec_enabled ? "true" : "false",
        g_config.fec_k,
        g_config.fec_n,
        (unsigned long)g_stats.packets_received,
        (unsigned long)g_stats.packets_valid,
        (unsigned long)g_stats.frames_complete,
        (unsigned long)g_stats.frames_incomplete,
        (unsigned long)g_stats.websocket_clients,
        g_stats.rssi_dbm
    );
}

// Simple JSON parser for config updates
esp_err_t config_from_json(const char *json)
{
    // Parse channel
    const char *ch_ptr = strstr(json, "\"channel\":");
    if (ch_ptr) {
        int channel = atoi(ch_ptr + 10);
        if (channel >= 1 && channel <= 14) {
            config_set_channel((uint8_t)channel);
        }
    }

    // Parse SSID
    const char *ssid_ptr = strstr(json, "\"ssid\":\"");
    if (ssid_ptr) {
        ssid_ptr += 8;
        const char *end = strchr(ssid_ptr, '"');
        if (end) {
            size_t ssid_len = (size_t)(end - ssid_ptr);
            if (ssid_len < sizeof(g_config.ap_ssid)) {
                // Note: password change requires separate call
                memcpy(g_config.ap_ssid, ssid_ptr, ssid_len);
                g_config.ap_ssid[ssid_len] = '\0';
                config_save();
            }
        }
    }

    // Parse FEC enabled
    const char *fec_ptr = strstr(json, "\"fec_enabled\":");
    if (fec_ptr) {
        g_config.fec_enabled = (strstr(fec_ptr, "true") == fec_ptr + 14);
        config_save();
    }

    return ESP_OK;
}

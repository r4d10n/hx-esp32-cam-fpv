/**
 * @file example.c
 * @brief Example usage of FEC decoder component
 *
 * This example demonstrates how to integrate the FEC decoder
 * with a WiFi receiver for video streaming.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "fec_decoder.h"

static const char *TAG = "fec_example";

// Global decoder handle
static fec_decoder_handle_t s_decoder = NULL;

// Statistics display task
static void stats_task(void *arg)
{
    fec_decoder_stats_t stats;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000)); // Every 5 seconds
        
        if (fec_decoder_get_stats(s_decoder, &stats) == ESP_OK) {
            ESP_LOGI(TAG, "=== FEC Decoder Statistics ===");
            ESP_LOGI(TAG, "Blocks received: %u", stats.blocks_received);
            ESP_LOGI(TAG, "  - Complete: %u", stats.blocks_complete);
            ESP_LOGI(TAG, "  - Decoded: %u", stats.blocks_decoded);
            ESP_LOGI(TAG, "  - Uncorrectable: %u", stats.blocks_uncorrectable);
            ESP_LOGI(TAG, "  - Abandoned: %u", stats.blocks_abandoned);
            ESP_LOGI(TAG, "Packets received: %u", stats.packets_received);
            ESP_LOGI(TAG, "  - Corrected: %u", stats.packets_corrected);
            ESP_LOGI(TAG, "  - Duplicate: %u", stats.packets_duplicate);
            ESP_LOGI(TAG, "  - Old: %u", stats.packets_old);
            ESP_LOGI(TAG, "Total bytes: %llu", stats.total_bytes_decoded);
            
            // Calculate packet loss rate
            if (stats.blocks_received > 0) {
                float fec_rate = (100.0f * stats.blocks_decoded) / stats.blocks_received;
                ESP_LOGI(TAG, "FEC usage: %.1f%%", fec_rate);
            }
        }
    }
}

/**
 * @brief Callback for decoded video data
 */
static void decoded_data_callback(const void *data, size_t size, void *user_ctx)
{
    // In a real application, this would:
    // 1. Assemble video frames from packets
    // 2. Decode H.264/JPEG
    // 3. Display on screen or save to file
    
    static uint32_t packet_count = 0;
    packet_count++;
    
    // Log every 100th packet
    if (packet_count % 100 == 0) {
        ESP_LOGI(TAG, "Decoded packet #%u, size: %d bytes", packet_count, size);
    }
}

/**
 * @brief WiFi packet receive callback (promiscuous mode)
 */
static void wifi_rx_callback(void *buffer, wifi_promiscuous_pkt_type_t type)
{
    if (type != WIFI_PKT_DATA) {
        return;
    }
    
    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t *)buffer;
    
    // Filter by MAC address or other criteria if needed
    // For this example, we process all data packets
    
    // Process packet through FEC decoder
    esp_err_t ret = fec_decoder_process_packet(
        s_decoder,
        pkt->payload,
        pkt->rx_ctrl.sig_len
    );
    
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "Packet processing failed: %s", esp_err_to_name(ret));
    }
}

/**
 * @brief Initialize WiFi in promiscuous mode
 */
static esp_err_t wifi_init_promiscuous(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    // Set WiFi channel (match transmitter)
    ESP_ERROR_CHECK(esp_wifi_set_channel(6, WIFI_SECOND_CHAN_NONE));
    
    // Enable promiscuous mode
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous_rx_cb(wifi_rx_callback));
    
    // Start WiFi
    ESP_ERROR_CHECK(esp_wifi_start());
    
    ESP_LOGI(TAG, "WiFi initialized in promiscuous mode on channel 6");
    
    return ESP_OK;
}

/**
 * @brief Initialize FEC decoder
 */
static esp_err_t fec_decoder_init(void)
{
    // Configure decoder
    fec_decoder_config_t config = fec_decoder_get_default_config();
    config.coding_k = 6;   // 6 data packets per block
    config.coding_n = 12;  // 12 total packets (6 FEC)
    config.mtu = 1400;     // Standard WiFi MTU
    config.enable_stats = true;
    
    ESP_LOGI(TAG, "Creating FEC decoder (K=%d, N=%d, MTU=%d)",
             config.coding_k, config.coding_n, config.mtu);
    
    // Create decoder
    esp_err_t ret = fec_decoder_create(
        &config,
        decoded_data_callback,
        NULL,  // User context (optional)
        &s_decoder
    );
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create FEC decoder: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ESP_LOGI(TAG, "FEC decoder created successfully");
    
    return ESP_OK;
}

/**
 * @brief Main application
 */
void app_main(void)
{
    ESP_LOGI(TAG, "FEC Decoder Example Starting...");
    
    // Initialize FEC decoder
    ESP_ERROR_CHECK(fec_decoder_init());
    
    // Initialize WiFi receiver
    ESP_ERROR_CHECK(wifi_init_promiscuous());
    
    // Start statistics task
    xTaskCreate(stats_task, "stats", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "System initialized. Receiving FEC-encoded packets...");
    
    // Main loop (could add additional processing here)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Example: Dynamically adjust FEC based on packet loss
        fec_decoder_stats_t stats;
        if (fec_decoder_get_stats(s_decoder, &stats) == ESP_OK) {
            if (stats.blocks_received > 100) {
                float error_rate = (100.0f * stats.blocks_uncorrectable) / stats.blocks_received;
                
                if (error_rate > 5.0f) {
                    ESP_LOGW(TAG, "High error rate (%.1f%%), consider increasing FEC", error_rate);
                    // Could call fec_decoder_update_coding() here
                }
            }
        }
    }
}

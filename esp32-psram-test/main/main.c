/**
 * ESP32-S3 PSRAM Test
 *
 * Simple test program to verify PSRAM availability and functionality.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "esp_timer.h"

static const char *TAG = "psram_test";

static void print_chip_info(void)
{
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);

    ESP_LOGI(TAG, "=== Chip Info ===");
    ESP_LOGI(TAG, "Model: %s", CONFIG_IDF_TARGET);
    ESP_LOGI(TAG, "Cores: %d", chip_info.cores);
    ESP_LOGI(TAG, "Features: %s%s%s",
             (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "WiFi " : "",
             (chip_info.features & CHIP_FEATURE_BLE) ? "BLE " : "",
             (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "PSRAM" : "");
    ESP_LOGI(TAG, "Revision: %d.%d", chip_info.revision / 100, chip_info.revision % 100);
}

static void print_memory_info(void)
{
    ESP_LOGI(TAG, "=== Memory Info ===");

    // Internal RAM
    size_t internal_total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    size_t internal_largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);

    ESP_LOGI(TAG, "Internal RAM:");
    ESP_LOGI(TAG, "  Total: %7u bytes (%u KB)", (unsigned)internal_total, (unsigned)(internal_total/1024));
    ESP_LOGI(TAG, "  Free:  %7u bytes (%u KB)", (unsigned)internal_free, (unsigned)(internal_free/1024));
    ESP_LOGI(TAG, "  Largest block: %u bytes", (unsigned)internal_largest);

    // PSRAM
    size_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

    ESP_LOGI(TAG, "PSRAM:");
    ESP_LOGI(TAG, "  Total: %7u bytes (%u KB)", (unsigned)psram_total, (unsigned)(psram_total/1024));
    ESP_LOGI(TAG, "  Free:  %7u bytes (%u KB)", (unsigned)psram_free, (unsigned)(psram_free/1024));
    ESP_LOGI(TAG, "  Largest block: %u bytes", (unsigned)psram_largest);

    if (psram_total == 0) {
        ESP_LOGW(TAG, "*** PSRAM NOT DETECTED OR NOT ENABLED! ***");
    }
}

static bool test_psram_allocation(size_t size_kb)
{
    size_t size = size_kb * 1024;
    ESP_LOGI(TAG, "Testing allocation of %u KB from PSRAM...", (unsigned)size_kb);

    uint8_t *buf = heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "  FAILED to allocate %u KB!", (unsigned)size_kb);
        return false;
    }

    ESP_LOGI(TAG, "  Allocated at %p", buf);

    // Write pattern
    ESP_LOGI(TAG, "  Writing test pattern...");
    for (size_t i = 0; i < size; i++) {
        buf[i] = (uint8_t)(i ^ (i >> 8) ^ (i >> 16));
    }

    // Verify pattern
    ESP_LOGI(TAG, "  Verifying test pattern...");
    bool pass = true;
    for (size_t i = 0; i < size; i++) {
        uint8_t expected = (uint8_t)(i ^ (i >> 8) ^ (i >> 16));
        if (buf[i] != expected) {
            ESP_LOGE(TAG, "  VERIFY FAILED at offset %u: got 0x%02X, expected 0x%02X",
                     (unsigned)i, buf[i], expected);
            pass = false;
            break;
        }
    }

    heap_caps_free(buf);

    if (pass) {
        ESP_LOGI(TAG, "  PASSED - %u KB verified", (unsigned)size_kb);
    }

    return pass;
}

static void test_psram_speed(void)
{
    ESP_LOGI(TAG, "=== PSRAM Speed Test ===");

    size_t test_size = 256 * 1024;  // 256KB
    uint8_t *buf = heap_caps_malloc(test_size, MALLOC_CAP_SPIRAM);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate buffer for speed test");
        return;
    }

    // Write speed test
    int64_t start = esp_timer_get_time();
    memset(buf, 0xAA, test_size);
    int64_t write_time = esp_timer_get_time() - start;

    // Read speed test
    volatile uint32_t sum = 0;
    start = esp_timer_get_time();
    for (size_t i = 0; i < test_size; i += 4) {
        sum += *(uint32_t*)(buf + i);
    }
    int64_t read_time = esp_timer_get_time() - start;

    (void)sum;  // Prevent optimization

    float write_speed = (float)test_size / write_time;  // MB/s
    float read_speed = (float)test_size / read_time;

    ESP_LOGI(TAG, "Write: %u KB in %lld us (%.2f MB/s)",
             (unsigned)(test_size/1024), write_time, write_speed);
    ESP_LOGI(TAG, "Read:  %u KB in %lld us (%.2f MB/s)",
             (unsigned)(test_size/1024), read_time, read_speed);

    heap_caps_free(buf);
}

void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "    ESP32-S3 PSRAM Test Program");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    print_chip_info();
    ESP_LOGI(TAG, "");

    print_memory_info();
    ESP_LOGI(TAG, "");

    // Check if PSRAM is available
    size_t psram_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    if (psram_size == 0) {
        ESP_LOGE(TAG, "=== PSRAM TEST FAILED ===");
        ESP_LOGE(TAG, "PSRAM not detected. Check:");
        ESP_LOGE(TAG, "  1. CONFIG_SPIRAM=y in sdkconfig");
        ESP_LOGE(TAG, "  2. Correct SPIRAM mode (OCT vs QUAD)");
        ESP_LOGE(TAG, "  3. Correct SPIRAM speed (80M vs 40M)");
        ESP_LOGE(TAG, "  4. Hardware has PSRAM chip");
        return;
    }

    ESP_LOGI(TAG, "=== PSRAM Allocation Tests ===");

    // Test various allocation sizes
    bool all_pass = true;
    all_pass &= test_psram_allocation(64);    // 64KB
    all_pass &= test_psram_allocation(256);   // 256KB
    all_pass &= test_psram_allocation(512);   // 512KB
    all_pass &= test_psram_allocation(1024);  // 1MB

    // Try to allocate most of PSRAM
    size_t large_size = (psram_size * 90) / 100 / 1024;  // 90% of PSRAM in KB
    all_pass &= test_psram_allocation(large_size);

    ESP_LOGI(TAG, "");
    test_psram_speed();

    ESP_LOGI(TAG, "");
    print_memory_info();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    if (all_pass) {
        ESP_LOGI(TAG, "    ALL PSRAM TESTS PASSED!");
    } else {
        ESP_LOGE(TAG, "    SOME PSRAM TESTS FAILED!");
    }
    ESP_LOGI(TAG, "========================================");

    // Keep running to allow monitoring
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "PSRAM Free: %u KB",
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
    }
}

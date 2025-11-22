/**
 * @file fec_decoder.c
 * @brief Forward Error Correction Decoder Implementation for ESP32-S3
 *
 * This implementation provides Reed-Solomon FEC decoding optimized for
 * ESP32-S3 real-time video streaming applications.
 */

#include "fec_decoder.h"
#include "fec.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "fec_decoder";

/**
 * @brief Packet structure for internal use
 */
typedef struct {
    uint8_t *data;              ///< Packet data (payload only, no header)
    uint32_t size;              ///< Payload size
    uint32_t block_index;       ///< Block index
    uint8_t packet_index;       ///< Packet index within block
    bool is_processed;          ///< Whether packet has been output
} fec_packet_t;

/**
 * @brief Block structure for assembling packets
 */
typedef struct {
    uint32_t block_index;                       ///< Block sequence number
    fec_packet_t data_packets[FEC_DECODER_MAX_K]; ///< Data packets (index < K)
    fec_packet_t fec_packets[FEC_DECODER_MAX_N - FEC_DECODER_MAX_K]; ///< FEC packets (index >= K)
    uint8_t data_packet_count;                  ///< Number of data packets received
    uint8_t fec_packet_count;                   ///< Number of FEC packets received
    bool data_packet_present[FEC_DECODER_MAX_K]; ///< Bitmap of received data packets
} fec_block_t;

/**
 * @brief FEC decoder internal structure
 */
struct fec_decoder_s {
    fec_decoder_config_t config;        ///< Configuration
    fec_t *fec;                         ///< FEC codec handle
    fec_block_t current_block;          ///< Current block being assembled
    fec_decoder_stats_t stats;          ///< Statistics
    fec_decoder_data_cb_t data_callback; ///< Data output callback
    void *user_ctx;                     ///< User context for callback
    SemaphoreHandle_t mutex;            ///< Thread safety mutex

    // Memory pools for FEC decoding
    uint8_t *decode_buffer[FEC_DECODER_MAX_K];  ///< Buffers for decoded packets
    const uint8_t *fec_src_ptrs[FEC_DECODER_MAX_K];  ///< Source pointers for FEC
    uint8_t *fec_dst_ptrs[FEC_DECODER_MAX_K];   ///< Destination pointers for FEC
    uint32_t indices[FEC_DECODER_MAX_K];        ///< Packet indices for FEC decode
};

// Forward declarations
static esp_err_t process_complete_block(fec_decoder_handle_t handle);
static esp_err_t process_fec_decode(fec_decoder_handle_t handle);
static void reset_block(fec_decoder_handle_t handle);
static bool validate_packet_header(const fec_packet_header_t *header);

/**
 * @brief Get default decoder configuration
 */
fec_decoder_config_t fec_decoder_get_default_config(void)
{
    fec_decoder_config_t config = {
        .coding_k = 6,
        .coding_n = 12,
        .mtu = FEC_DECODER_DEFAULT_MTU,
        .max_blocks_pending = 4,
        .enable_stats = true
    };
    return config;
}

/**
 * @brief Create and initialize FEC decoder
 */
esp_err_t fec_decoder_create(
    const fec_decoder_config_t *config,
    fec_decoder_data_cb_t data_callback,
    void *user_ctx,
    fec_decoder_handle_t *handle)
{
    if (!config || !data_callback || !handle) {
        ESP_LOGE(TAG, "Invalid parameters");
        return ESP_ERR_INVALID_ARG;
    }

    // Validate configuration
    if (config->coding_k == 0 || config->coding_k > FEC_DECODER_MAX_K) {
        ESP_LOGE(TAG, "Invalid coding_k: %d (max: %d)", config->coding_k, FEC_DECODER_MAX_K);
        return ESP_ERR_INVALID_ARG;
    }
    if (config->coding_n <= config->coding_k || config->coding_n > FEC_DECODER_MAX_N) {
        ESP_LOGE(TAG, "Invalid coding_n: %d (must be > k and <= %d)",
                 config->coding_n, FEC_DECODER_MAX_N);
        return ESP_ERR_INVALID_ARG;
    }
    if (config->mtu == 0 || config->mtu > 2048) {
        ESP_LOGE(TAG, "Invalid MTU: %d", config->mtu);
        return ESP_ERR_INVALID_ARG;
    }

    // Allocate decoder structure
    fec_decoder_handle_t decoder = heap_caps_calloc(1, sizeof(struct fec_decoder_s),
                                                     MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (!decoder) {
        ESP_LOGE(TAG, "Failed to allocate decoder structure");
        return ESP_ERR_NO_MEM;
    }

    // Copy configuration
    memcpy(&decoder->config, config, sizeof(fec_decoder_config_t));
    decoder->data_callback = data_callback;
    decoder->user_ctx = user_ctx;

    // Create mutex
    decoder->mutex = xSemaphoreCreateMutex();
    if (!decoder->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(decoder);
        return ESP_ERR_NO_MEM;
    }

    // Initialize FEC library (if not already initialized)
    init_fec();

    // Create FEC codec
    decoder->fec = fec_new(config->coding_k, config->coding_n);
    if (!decoder->fec) {
        ESP_LOGE(TAG, "Failed to create FEC codec");
        vSemaphoreDelete(decoder->mutex);
        free(decoder);
        return ESP_ERR_NO_MEM;
    }

    // Allocate decode buffers
    for (int i = 0; i < FEC_DECODER_MAX_K; i++) {
        decoder->decode_buffer[i] = heap_caps_malloc(config->mtu,
                                                      MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
        if (!decoder->decode_buffer[i]) {
            ESP_LOGE(TAG, "Failed to allocate decode buffer %d", i);
            // Cleanup
            for (int j = 0; j < i; j++) {
                free(decoder->decode_buffer[j]);
            }
            fec_free(decoder->fec);
            vSemaphoreDelete(decoder->mutex);
            free(decoder);
            return ESP_ERR_NO_MEM;
        }
    }

    // Initialize block
    reset_block(decoder);

    // Initialize statistics
    memset(&decoder->stats, 0, sizeof(fec_decoder_stats_t));

    *handle = decoder;

    ESP_LOGI(TAG, "FEC decoder created: K=%d, N=%d, MTU=%d",
             config->coding_k, config->coding_n, config->mtu);

    return ESP_OK;
}

/**
 * @brief Destroy FEC decoder
 */
esp_err_t fec_decoder_destroy(fec_decoder_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    // Free decode buffers
    for (int i = 0; i < FEC_DECODER_MAX_K; i++) {
        if (handle->decode_buffer[i]) {
            free(handle->decode_buffer[i]);
        }
    }

    // Free FEC codec
    if (handle->fec) {
        fec_free(handle->fec);
    }

    // Delete mutex
    if (handle->mutex) {
        vSemaphoreDelete(handle->mutex);
    }

    // Free decoder structure
    free(handle);

    ESP_LOGI(TAG, "FEC decoder destroyed");

    return ESP_OK;
}

/**
 * @brief Validate packet header
 */
static bool validate_packet_header(const fec_packet_header_t *header)
{
    if (header->packet_version != 2) {
        ESP_LOGW(TAG, "Invalid packet version: %d", header->packet_version);
        return false;
    }
    if (header->packet_signature != 56) {
        ESP_LOGW(TAG, "Invalid packet signature: %d", header->packet_signature);
        return false;
    }
    return true;
}

/**
 * @brief Reset current block
 */
static void reset_block(fec_decoder_handle_t handle)
{
    memset(&handle->current_block, 0, sizeof(fec_block_t));
    handle->current_block.block_index = 0xFFFFFFFF; // Invalid
}

/**
 * @brief Process incoming encoded packet
 */
esp_err_t fec_decoder_process_packet(
    fec_decoder_handle_t handle,
    const void *data,
    size_t size)
{
    if (!handle || !data || size < sizeof(fec_packet_header_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const fec_packet_header_t *header = (const fec_packet_header_t *)data;

    // Validate header
    if (!validate_packet_header(header)) {
        return ESP_ERR_INVALID_ARG;
    }

    // Extract packet information
    uint32_t block_index = header->block_index;
    uint8_t packet_index = header->packet_index;
    uint16_t payload_size = header->size;
    const uint8_t *payload = (const uint8_t *)data + sizeof(fec_packet_header_t);

    // Validate packet index
    if (packet_index >= handle->config.coding_n) {
        ESP_LOGW(TAG, "Invalid packet index: %d (max: %d)",
                 packet_index, handle->config.coding_n - 1);
        return ESP_ERR_INVALID_ARG;
    }

    // Validate payload size
    if (payload_size > handle->config.mtu) {
        ESP_LOGW(TAG, "Payload size %d exceeds MTU %d", payload_size, handle->config.mtu);
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    if (handle->config.enable_stats) {
        handle->stats.packets_received++;
    }

    // Check if this is a new block
    if (handle->current_block.block_index == 0xFFFFFFFF) {
        // First packet, initialize block
        handle->current_block.block_index = block_index;
        if (handle->config.enable_stats) {
            handle->stats.current_block_index = block_index;
        }
    }
    else if (block_index < handle->current_block.block_index) {
        // Old packet
        if (block_index + 100 < handle->current_block.block_index) {
            // Very old - might be new session, reset
            ESP_LOGW(TAG, "Very old block %u (current: %u), resetting",
                     block_index, handle->current_block.block_index);
            reset_block(handle);
            handle->current_block.block_index = block_index;
            if (handle->config.enable_stats) {
                handle->stats.current_block_index = block_index;
            }
        } else {
            // Just old
            if (handle->config.enable_stats) {
                handle->stats.packets_old++;
            }
            xSemaphoreGive(handle->mutex);
            return ESP_OK; // Discard old packet
        }
    }
    else if (block_index > handle->current_block.block_index) {
        // New block, abandon current
        if (handle->current_block.data_packet_count > 0 ||
            handle->current_block.fec_packet_count > 0) {
            if (handle->config.enable_stats) {
                handle->stats.blocks_abandoned++;
            }
            ESP_LOGW(TAG, "Abandoning block %u for new block %u (data: %d, fec: %d)",
                     handle->current_block.block_index, block_index,
                     handle->current_block.data_packet_count,
                     handle->current_block.fec_packet_count);
        }
        reset_block(handle);
        handle->current_block.block_index = block_index;
        if (handle->config.enable_stats) {
            handle->stats.current_block_index = block_index;
        }
    }

    // Store packet
    if (packet_index < handle->config.coding_k) {
        // Data packet
        if (handle->current_block.data_packet_present[packet_index]) {
            // Duplicate
            if (handle->config.enable_stats) {
                handle->stats.packets_duplicate++;
            }
            xSemaphoreGive(handle->mutex);
            return ESP_OK;
        }

        fec_packet_t *pkt = &handle->current_block.data_packets[packet_index];
        pkt->data = (uint8_t *)payload;
        pkt->size = payload_size;
        pkt->block_index = block_index;
        pkt->packet_index = packet_index;
        pkt->is_processed = false;

        handle->current_block.data_packet_present[packet_index] = true;
        handle->current_block.data_packet_count++;
    }
    else {
        // FEC packet
        uint8_t fec_index = packet_index - handle->config.coding_k;

        // Check for duplicate (simple linear search, OK for small N-K)
        for (int i = 0; i < handle->current_block.fec_packet_count; i++) {
            if (handle->current_block.fec_packets[i].packet_index == packet_index) {
                // Duplicate
                if (handle->config.enable_stats) {
                    handle->stats.packets_duplicate++;
                }
                xSemaphoreGive(handle->mutex);
                return ESP_OK;
            }
        }

        if (handle->current_block.fec_packet_count >= (handle->config.coding_n - handle->config.coding_k)) {
            ESP_LOGW(TAG, "Too many FEC packets");
            xSemaphoreGive(handle->mutex);
            return ESP_ERR_INVALID_STATE;
        }

        fec_packet_t *pkt = &handle->current_block.fec_packets[handle->current_block.fec_packet_count];
        pkt->data = (uint8_t *)payload;
        pkt->size = payload_size;
        pkt->block_index = block_index;
        pkt->packet_index = packet_index;
        pkt->is_processed = false;

        handle->current_block.fec_packet_count++;
    }

    esp_err_t ret = ESP_OK;

    // Check if we can process the block
    if (handle->current_block.data_packet_count >= handle->config.coding_k) {
        // Complete block received, no FEC needed
        ret = process_complete_block(handle);
    }
    else if (handle->current_block.data_packet_count + handle->current_block.fec_packet_count
             >= handle->config.coding_k) {
        // Enough packets for FEC decode
        ret = process_fec_decode(handle);
    }

    xSemaphoreGive(handle->mutex);
    return ret;
}

/**
 * @brief Process complete block (all data packets received)
 */
static esp_err_t process_complete_block(fec_decoder_handle_t handle)
{
    if (handle->config.enable_stats) {
        handle->stats.blocks_complete++;
        handle->stats.blocks_received++;
    }

    // Output all data packets in order
    for (int i = 0; i < handle->config.coding_k; i++) {
        if (handle->current_block.data_packet_present[i]) {
            fec_packet_t *pkt = &handle->current_block.data_packets[i];
            if (!pkt->is_processed && handle->data_callback) {
                handle->data_callback(pkt->data, pkt->size, handle->user_ctx);
                pkt->is_processed = true;

                if (handle->config.enable_stats) {
                    handle->stats.total_bytes_decoded += pkt->size;
                }
            }
        }
    }

    // Move to next block
    reset_block(handle);
    handle->current_block.block_index = handle->stats.current_block_index + 1;
    if (handle->config.enable_stats) {
        handle->stats.current_block_index++;
    }

    return ESP_OK;
}

/**
 * @brief Process block with FEC decoding
 */
static esp_err_t process_fec_decode(fec_decoder_handle_t handle)
{
    // Build source packet array and indices
    int src_idx = 0;
    int missing_count = 0;
    int missing_indices[FEC_DECODER_MAX_K];

    for (int i = 0; i < handle->config.coding_k; i++) {
        if (handle->current_block.data_packet_present[i]) {
            // Data packet present
            handle->fec_src_ptrs[src_idx] = handle->current_block.data_packets[i].data;
            handle->indices[src_idx] = i;
            src_idx++;
        } else {
            // Missing data packet
            missing_indices[missing_count++] = i;
        }
    }

    // Fill remaining with FEC packets
    int fec_used = 0;
    for (int i = src_idx; i < handle->config.coding_k; i++) {
        if (fec_used >= handle->current_block.fec_packet_count) {
            ESP_LOGE(TAG, "Not enough packets for FEC decode");
            if (handle->config.enable_stats) {
                handle->stats.blocks_uncorrectable++;
            }
            reset_block(handle);
            return ESP_ERR_INVALID_STATE;
        }

        handle->fec_src_ptrs[i] = handle->current_block.fec_packets[fec_used].data;
        handle->indices[i] = handle->current_block.fec_packets[fec_used].packet_index;
        fec_used++;
    }

    // Prepare destination buffers for missing packets
    for (int i = 0; i < missing_count; i++) {
        handle->fec_dst_ptrs[i] = handle->decode_buffer[i];
    }

    // Perform FEC decode
    fec_decode(handle->fec,
               handle->fec_src_ptrs,
               handle->fec_dst_ptrs,
               handle->indices,
               handle->config.mtu);

    if (handle->config.enable_stats) {
        handle->stats.blocks_decoded++;
        handle->stats.blocks_received++;
        handle->stats.packets_corrected += missing_count;
    }

    // Output all packets in order
    int decoded_idx = 0;
    for (int i = 0; i < handle->config.coding_k; i++) {
        const uint8_t *data;
        size_t size;

        if (handle->current_block.data_packet_present[i]) {
            // Use received data packet
            fec_packet_t *pkt = &handle->current_block.data_packets[i];
            data = pkt->data;
            size = pkt->size;
        } else {
            // Use FEC-decoded packet
            data = handle->decode_buffer[decoded_idx];
            size = handle->config.mtu; // Decoded packets are full MTU size
            decoded_idx++;
        }

        if (handle->data_callback) {
            handle->data_callback(data, size, handle->user_ctx);

            if (handle->config.enable_stats) {
                handle->stats.total_bytes_decoded += size;
            }
        }
    }

    // Move to next block
    reset_block(handle);
    handle->current_block.block_index = handle->stats.current_block_index + 1;
    if (handle->config.enable_stats) {
        handle->stats.current_block_index++;
    }

    return ESP_OK;
}

/**
 * @brief Get decoder statistics
 */
esp_err_t fec_decoder_get_stats(
    fec_decoder_handle_t handle,
    fec_decoder_stats_t *stats)
{
    if (!handle || !stats) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    memcpy(stats, &handle->stats, sizeof(fec_decoder_stats_t));
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

/**
 * @brief Reset decoder statistics
 */
esp_err_t fec_decoder_reset_stats(fec_decoder_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    uint32_t current_block = handle->stats.current_block_index;
    memset(&handle->stats, 0, sizeof(fec_decoder_stats_t));
    handle->stats.current_block_index = current_block;

    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Statistics reset");

    return ESP_OK;
}

/**
 * @brief Update decoder coding parameters
 */
esp_err_t fec_decoder_update_coding(
    fec_decoder_handle_t handle,
    uint8_t coding_k,
    uint8_t coding_n)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (coding_k == 0 || coding_k > FEC_DECODER_MAX_K) {
        ESP_LOGE(TAG, "Invalid coding_k: %d", coding_k);
        return ESP_ERR_INVALID_ARG;
    }
    if (coding_n <= coding_k || coding_n > FEC_DECODER_MAX_N) {
        ESP_LOGE(TAG, "Invalid coding_n: %d", coding_n);
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    // Flush current block
    reset_block(handle);

    // Free old FEC codec
    if (handle->fec) {
        fec_free(handle->fec);
    }

    // Create new FEC codec
    handle->fec = fec_new(coding_k, coding_n);
    if (!handle->fec) {
        ESP_LOGE(TAG, "Failed to create new FEC codec");
        xSemaphoreGive(handle->mutex);
        return ESP_ERR_NO_MEM;
    }

    handle->config.coding_k = coding_k;
    handle->config.coding_n = coding_n;

    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Coding updated to K=%d, N=%d", coding_k, coding_n);

    return ESP_OK;
}

/**
 * @brief Flush decoder
 */
esp_err_t fec_decoder_flush(fec_decoder_handle_t handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    reset_block(handle);
    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Decoder flushed");

    return ESP_OK;
}

/**
 * @brief Get current block index
 */
esp_err_t fec_decoder_get_current_block(
    fec_decoder_handle_t handle,
    uint32_t *block_index)
{
    if (!handle || !block_index) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);
    *block_index = handle->stats.current_block_index;
    xSemaphoreGive(handle->mutex);

    return ESP_OK;
}

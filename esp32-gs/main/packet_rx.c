/**
 * ESP32 FPV Ground Station - Packet RX Handler
 * With FEC decoder integration
 */

#include "packet_rx.h"
#include "frame_buffer.h"
#include "fec_decoder.h"
#include "packet_tx.h"
#include "esp_timer.h"

static const char *TAG = "packet_rx";

// Packet queue item
typedef struct {
    uint8_t *data;
    size_t len;
    int8_t rssi;
} packet_item_t;

// Packet buffer pool (pre-allocated to avoid malloc in ISR context)
#define PACKET_POOL_SIZE CONFIG_FPV_GS_PACKET_QUEUE_SIZE
#define MAX_PACKET_SIZE 1500

static uint8_t s_packet_pool[PACKET_POOL_SIZE][MAX_PACKET_SIZE];
static volatile uint8_t s_pool_write_idx = 0;
static QueueHandle_t s_packet_queue = NULL;
static TaskHandle_t s_process_task = NULL;
static volatile bool s_running = false;

// RSSI averaging
static int32_t s_rssi_sum = 0;
static int16_t s_rssi_count = 0;
static int8_t s_rssi_avg = -100;

// Frame assembly state
typedef struct {
    uint32_t frame_index;
    uint8_t parts_received;
    uint8_t last_part_index;
    bool complete;
} frame_state_t;

static frame_state_t s_current_frame = {0};

// Forward declarations
static void packet_process_task(void *arg);
static bool process_fpv_packet(const uint8_t *data, size_t len);
static int find_fec_header(const uint8_t *data, size_t len);
static void process_video_payload(const uint8_t *payload, size_t payload_len);
static void fec_packet_callback(uint32_t block_index, uint8_t packet_index,
                                 const uint8_t *data, size_t size, bool recovered);

esp_err_t packet_rx_init(void)
{
    // Create packet queue
    s_packet_queue = xQueueCreate(PACKET_POOL_SIZE, sizeof(packet_item_t));
    if (s_packet_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create packet queue");
        return ESP_ERR_NO_MEM;
    }

    // Initialize FEC decoder
    // K=8 data packets, N=12 total (4 FEC), block buffer count, timeout
    esp_err_t ret = fec_decoder_init(g_config.fec_k, g_config.fec_n,
                                      CONFIG_FPV_GS_FEC_BLOCK_BUFFER_COUNT,
                                      CONFIG_FPV_GS_FEC_TIMEOUT_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init FEC decoder: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set FEC callback for decoded packets
    fec_decoder_set_callback(fec_packet_callback);

    ESP_LOGI(TAG, "Packet RX initialized, queue=%d, FEC K=%d N=%d",
             PACKET_POOL_SIZE, g_config.fec_k, g_config.fec_n);
    return ESP_OK;
}

void IRAM_ATTR packet_rx_handle(const uint8_t *data, size_t len, int8_t rssi)
{
    if (!s_running || s_packet_queue == NULL) {
        return;
    }

    // Quick filter: minimum packet size check
    if (len < 60) {  // Min: 802.11 header + FEC header + video header
        return;
    }

    // Update RSSI average
    s_rssi_sum += rssi;
    s_rssi_count++;
    if (s_rssi_count >= 100) {
        s_rssi_avg = (int8_t)(s_rssi_sum / s_rssi_count);
        s_rssi_sum = 0;
        s_rssi_count = 0;
    }
    g_stats.rssi_dbm = s_rssi_avg;

    // Quick signature check before queuing
    // Look for FPV signature in expected range (after 802.11 header)
    bool found_sig = false;
    size_t search_end = (len > 60) ? 60 : len;
    for (size_t i = 24; i + 1 < search_end; i++) {
        if (data[i + 1] == FPV_PACKET_SIGNATURE) {
            found_sig = true;
            break;
        }
    }
    if (!found_sig) {
        return;
    }

    g_stats.packets_received++;

    // Get buffer from pool (circular)
    uint8_t idx = s_pool_write_idx;
    s_pool_write_idx = (s_pool_write_idx + 1) % PACKET_POOL_SIZE;

    // Copy packet data
    size_t copy_len = (len < MAX_PACKET_SIZE) ? len : MAX_PACKET_SIZE;
    memcpy(s_packet_pool[idx], data, copy_len);

    // Queue for processing
    packet_item_t item = {
        .data = s_packet_pool[idx],
        .len = copy_len,
        .rssi = rssi,
    };

    // Non-blocking queue send (drop if full)
    if (xQueueSendFromISR(s_packet_queue, &item, NULL) != pdTRUE) {
        // Queue full, packet dropped
    }
}

esp_err_t packet_rx_start(void)
{
    if (s_process_task != NULL) {
        return ESP_OK;  // Already running
    }

    s_running = true;

    // Create processing task (pinned to Core 0 for WiFi affinity)
    // High priority (configMAX_PRIORITIES-2) to ensure packets are processed quickly
    BaseType_t ret = xTaskCreatePinnedToCore(
        packet_process_task,
        "pkt_proc",
        8192,   // Larger stack for faster processing
        NULL,
        configMAX_PRIORITIES - 2,  // Very high priority
        &s_process_task,
        0   // Core 0
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create packet processing task");
        s_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Packet processing started");
    return ESP_OK;
}

void packet_rx_stop(void)
{
    s_running = false;

    if (s_process_task != NULL) {
        // Wait for task to exit
        vTaskDelay(pdMS_TO_TICKS(100));
        s_process_task = NULL;
    }

    ESP_LOGI(TAG, "Packet processing stopped");
}

int8_t packet_rx_get_rssi(void)
{
    return s_rssi_avg;
}

// Packet processing task
static void packet_process_task(void *arg)
{
    (void)arg;  // Unused
    packet_item_t item;
    uint32_t process_counter = 0;

    ESP_LOGI(TAG, "Packet processing task started");

    while (s_running) {
        // Wait for packet with short timeout
        if (xQueueReceive(s_packet_queue, &item, pdMS_TO_TICKS(10)) == pdTRUE) {
            // Process FPV packet
            if (process_fpv_packet(item.data, item.len)) {
                g_stats.packets_valid++;
            } else {
                g_stats.packets_invalid++;
            }
        }

        // Process FEC blocks periodically (every ~10 packets or every 50ms)
        process_counter++;
        if (process_counter >= 10) {
            fec_decoder_process();
            process_counter = 0;
        }
    }

    ESP_LOGI(TAG, "Packet processing task exiting");
    vTaskDelete(NULL);
}

// Find FEC header signature in packet
static int find_fec_header(const uint8_t *data, size_t len)
{
    // Search in expected range (after 802.11 header, before payload)
    size_t search_end = (len > 100) ? 100 : len;
    for (size_t i = 24; i + 1 < search_end; i++) {
        if (data[i + 1] == FPV_PACKET_SIGNATURE) {
            return (int)i;
        }
    }
    return -1;
}

// Process a single FPV packet
static bool process_fpv_packet(const uint8_t *data, size_t len)
{
    // Find FEC header
    int fec_offset = find_fec_header(data, len);
    if (fec_offset < 0) {
        return false;
    }

    // Parse FEC header
    if ((size_t)fec_offset + FPV_PACKET_HEADER_SIZE > len) {
        return false;
    }

    const fpv_fec_header_t *fec = (const fpv_fec_header_t *)(data + fec_offset);

    // Validate header
    if (fec->signature != FPV_PACKET_SIGNATURE) {
        return false;
    }

    // Mark connection as active (we received a valid FPV packet)
    packet_tx_process_air_response(data, len);

    uint8_t packet_index = FPV_GET_PACKET_INDEX(fec);
    uint32_t block_index = FPV_GET_BLOCK_INDEX(fec);

    // Get payload
    size_t payload_offset = fec_offset + FPV_PACKET_HEADER_SIZE;
    size_t payload_len = fec->size;

    if (payload_offset + payload_len > len) {
        payload_len = len - payload_offset;
    }

    if (payload_len < 1) {
        return false;
    }

    const uint8_t *payload = data + payload_offset;

    // Feed ALL packets to FEC decoder (both data and parity)
    fec_decoder_add_packet(block_index, packet_index, payload, payload_len);

    return true;
}

// FEC callback - called when a packet is ready (from complete block or recovered)
static void fec_packet_callback(uint32_t block_index, uint8_t packet_index,
                                 const uint8_t *data, size_t size, bool recovered)
{
    (void)block_index;
    (void)packet_index;

    // Process the video payload
    process_video_payload(data, size);
}

// Process video payload from FEC-decoded packet
static void process_video_payload(const uint8_t *payload, size_t payload_len)
{
    if (payload_len < FPV_VIDEO_HEADER_SIZE) {
        return;
    }

    // Parse video header
    const fpv_video_header_t *vh = (const fpv_video_header_t *)payload;

    // Check packet type
    if (vh->type != FPV_PACKET_TYPE_VIDEO) {
        // Not a video packet (might be OSD, telemetry, etc.)
        return;
    }

    // Extract video part info
    uint32_t frame_index = vh->frame_index;
    uint8_t part_index = FPV_GET_PART_INDEX(vh);
    bool last_part = FPV_GET_LAST_PART(vh);

    // Get JPEG data
    const uint8_t *jpeg_data = payload + FPV_VIDEO_HEADER_SIZE;
    size_t jpeg_len = payload_len - FPV_VIDEO_HEADER_SIZE;

    if (jpeg_len == 0) {
        return;
    }

    // Handle frame assembly
    if (frame_index != s_current_frame.frame_index) {
        // New frame starting
        if (s_current_frame.frame_index > 0 && !s_current_frame.complete) {
            // Previous frame was incomplete
            g_stats.frames_incomplete++;
            frame_buffer_discard_current();
        }

        // Start new frame
        s_current_frame.frame_index = frame_index;
        s_current_frame.parts_received = 0;
        s_current_frame.last_part_index = 0xFF;
        s_current_frame.complete = false;

        frame_buffer_start_frame(frame_index);
    }

    // Add part to frame buffer
    if (frame_buffer_add_part(part_index, jpeg_data, jpeg_len)) {
        s_current_frame.parts_received++;

        if (last_part) {
            s_current_frame.last_part_index = part_index;
        }

        // Check if frame is complete
        if (s_current_frame.last_part_index != 0xFF) {
            // We know the total parts count
            if (s_current_frame.parts_received == s_current_frame.last_part_index + 1) {
                // Frame complete!
                if (frame_buffer_commit_frame()) {
                    s_current_frame.complete = true;
                    g_stats.frames_complete++;
                }
            }
        }
    }
}

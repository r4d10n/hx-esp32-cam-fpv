/**
 * ESP32 FPV Ground Station - Common Definitions
 *
 * Captures FPV video packets in promiscuous mode and serves
 * JPEG frames via WebSocket to connected web clients.
 */

#ifndef FPV_GS_H
#define FPV_GS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"

// Protocol constants (must match air unit)
#define FPV_PACKET_SIGNATURE    0x38    // 56 decimal
#define FPV_PACKET_VERSION      3
#define FPV_PACKET_HEADER_SIZE  12      // FEC header size
#define FPV_VIDEO_HEADER_SIZE   18      // Video packet header size

// Packet types
typedef enum {
    FPV_PACKET_TYPE_VIDEO = 0,
    FPV_PACKET_TYPE_TELEMETRY = 1,
    FPV_PACKET_TYPE_OSD = 2,
    FPV_PACKET_TYPE_CONFIG = 3,
} fpv_packet_type_t;

// FEC Packet Header (12 bytes) - matches Packet_Header in fec.h
typedef struct __attribute__((packed)) {
    uint8_t version;
    uint8_t signature;
    uint16_t from_device_id;
    uint16_t to_device_id;
    uint16_t size;
    uint32_t block_packet_index;  // block_index (24 bits) | packet_index (8 bits)
} fpv_fec_header_t;

// Video Packet Header (18 bytes) - matches Air2Ground_Video_Packet
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint32_t size;
    uint8_t pong;
    uint8_t version;
    uint8_t crc;
    uint16_t air_device_id;
    uint16_t gs_device_id;
    uint8_t resolution;
    uint8_t part_last;          // part_index (7 bits) | last_part (1 bit)
    uint32_t frame_index;
} fpv_video_header_t;

// Extracted video part info
typedef struct {
    uint32_t frame_index;
    uint8_t part_index;
    bool last_part;
    uint8_t *data;
    size_t data_len;
} fpv_video_part_t;

// Ground station configuration
typedef struct {
    char ap_ssid[32];
    char ap_password[64];
    uint8_t channel;
    uint8_t fec_k;
    uint8_t fec_n;
    bool fec_enabled;
} fpv_gs_config_t;

// Statistics
typedef struct {
    uint32_t packets_received;
    uint32_t packets_valid;
    uint32_t packets_invalid;
    uint32_t frames_complete;
    uint32_t frames_incomplete;
    uint32_t websocket_clients;
    uint32_t websocket_frames_sent;
    int8_t rssi_dbm;
    uint8_t channel;
} fpv_gs_stats_t;

// Helper macros
#define FPV_GET_BLOCK_INDEX(header)   ((header)->block_packet_index & 0xFFFFFF)
#define FPV_GET_PACKET_INDEX(header)  (((header)->block_packet_index >> 24) & 0xFF)
#define FPV_GET_PART_INDEX(vh)        ((vh)->part_last & 0x7F)
#define FPV_GET_LAST_PART(vh)         (((vh)->part_last >> 7) & 0x01)

// Global configuration and stats (defined in main.c)
extern fpv_gs_config_t g_config;
extern fpv_gs_stats_t g_stats;

#endif // FPV_GS_H

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Inter-Processor Communication between ESP32-P4 and ESP32-C5
 *
 * High-throughput, low-latency communication over SPI.
 * ESP32-P4 acts as SPI master, ESP32-C5 as SPI slave.
 */

/**
 * @brief Packet types
 */
typedef enum {
    IPC_PACKET_TYPE_VIDEO = 0x01,      ///< H.264 NAL unit
    IPC_PACKET_TYPE_CONFIG = 0x02,     ///< Configuration data
    IPC_PACKET_TYPE_TELEMETRY = 0x03,  ///< Telemetry data (Mavlink/MSP)
    IPC_PACKET_TYPE_STATS = 0x04,      ///< Statistics
    IPC_PACKET_TYPE_CONTROL = 0x05,    ///< Control commands
} ipc_packet_type_t;

/**
 * @brief Packet flags
 */
#define IPC_FLAG_PRIORITY     (1 << 0)  ///< High priority packet
#define IPC_FLAG_FRAGMENTED   (1 << 1)  ///< Packet is fragmented
#define IPC_FLAG_LAST_FRAGMENT (1 << 2) ///< Last fragment in sequence
#define IPC_FLAG_REQUIRES_ACK (1 << 3)  ///< Requires acknowledgment

/**
 * @brief IPC packet header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t sync[2];                    ///< Sync bytes (0xAA, 0x55)
    uint8_t type;                       ///< IPC_PACKET_TYPE_*
    uint8_t flags;                      ///< IPC_FLAG_*
    uint16_t sequence;                  ///< Sequence number
    uint16_t fragment_index;            ///< Fragment index (if fragmented)
    uint32_t payload_size;              ///< Size of payload
    uint16_t crc16;                     ///< CRC16 of header + payload
} ipc_packet_header_t;
#pragma pack(pop)

#define IPC_HEADER_SIZE sizeof(ipc_packet_header_t)
#define IPC_SYNC_BYTE_0 0xAA
#define IPC_SYNC_BYTE_1 0x55

/**
 * @brief Video packet payload
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t nal_type;                   ///< H.264 NAL type
    bool is_keyframe;                   ///< True if keyframe
    uint32_t frame_index;               ///< Frame number
    uint64_t pts_us;                    ///< Presentation timestamp
    uint64_t dts_us;                    ///< Decode timestamp
    uint32_t nalu_size;                 ///< Size of NAL unit data following
    // NAL unit data follows
} ipc_video_payload_t;
#pragma pack(pop)

/**
 * @brief Configuration packet payload
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t wifi_channel;
    int8_t wifi_tx_power_dbm;
    uint8_t fec_k;
    uint8_t fec_n;
    uint32_t bitrate_bps;
    uint8_t reserved[16];
} ipc_config_payload_t;
#pragma pack(pop)

/**
 * @brief Statistics packet payload
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t packets_sent;
    uint32_t packets_dropped;
    uint64_t bytes_sent;
    int8_t wifi_rssi_dbm;
    uint8_t wifi_tx_failures;
    uint32_t spi_errors;
    float actual_bitrate_mbps;
} ipc_stats_payload_t;
#pragma pack(pop)

/**
 * @brief Master (ESP32-P4) configuration
 */
typedef struct {
    uint8_t mosi_pin;
    uint8_t miso_pin;
    uint8_t clk_pin;
    uint8_t cs_pin;
    uint8_t handshake_pin;              ///< GPIO for slave ready signal

    uint32_t spi_clock_hz;              ///< SPI clock frequency (up to 80MHz)
    size_t dma_buffer_size;             ///< DMA buffer size in bytes
    uint8_t dma_channel;                ///< DMA channel to use

    size_t max_transfer_size;           ///< Maximum transfer size
} ipc_master_config_t;

/**
 * @brief Slave (ESP32-C5) configuration
 */
typedef struct {
    uint8_t mosi_pin;
    uint8_t miso_pin;
    uint8_t clk_pin;
    uint8_t cs_pin;
    uint8_t handshake_pin;              ///< GPIO to signal ready to master

    size_t dma_buffer_size;             ///< DMA buffer size
    uint8_t dma_channel;                ///< DMA channel to use

    size_t queue_size;                  ///< Receive queue size (packets)
} ipc_slave_config_t;

/**
 * @brief Packet receive callback (slave side)
 *
 * @param packet_type Packet type
 * @param payload Payload data
 * @param payload_size Size of payload
 * @param user_data User data
 */
typedef void (*ipc_receive_cb_t)(
    ipc_packet_type_t packet_type,
    const void *payload,
    size_t payload_size,
    void *user_data
);

// ========== MASTER (ESP32-P4) API ==========

/**
 * @brief Initialize IPC master
 *
 * @param config Master configuration
 * @return ESP_OK on success
 */
esp_err_t ipc_master_init(const ipc_master_config_t *config);

/**
 * @brief Deinitialize IPC master
 *
 * @return ESP_OK on success
 */
esp_err_t ipc_master_deinit(void);

/**
 * @brief Send packet to slave
 *
 * @param type Packet type
 * @param flags Packet flags
 * @param payload Payload data
 * @param payload_size Size of payload
 * @param timeout_ms Timeout in milliseconds (0 = no wait)
 * @return ESP_OK on success
 */
esp_err_t ipc_master_send(
    ipc_packet_type_t type,
    uint8_t flags,
    const void *payload,
    size_t payload_size,
    uint32_t timeout_ms
);

/**
 * @brief Send video packet (NAL unit)
 *
 * @param nalu_data NAL unit data
 * @param nalu_size NAL unit size
 * @param nal_type NAL type
 * @param is_keyframe True if keyframe
 * @param frame_index Frame number
 * @param pts_us Presentation timestamp
 * @param dts_us Decode timestamp
 * @return ESP_OK on success
 */
esp_err_t ipc_master_send_video(
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint8_t nal_type,
    bool is_keyframe,
    uint32_t frame_index,
    uint64_t pts_us,
    uint64_t dts_us
);

/**
 * @brief Check if slave is ready
 *
 * @return true if slave is ready
 */
bool ipc_master_is_slave_ready(void);

// ========== SLAVE (ESP32-C5) API ==========

/**
 * @brief Initialize IPC slave
 *
 * @param config Slave configuration
 * @return ESP_OK on success
 */
esp_err_t ipc_slave_init(const ipc_slave_config_t *config);

/**
 * @brief Deinitialize IPC slave
 *
 * @return ESP_OK on success
 */
esp_err_t ipc_slave_deinit(void);

/**
 * @brief Register packet receive callback
 *
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return ESP_OK on success
 */
esp_err_t ipc_slave_register_callback(
    ipc_receive_cb_t callback,
    void *user_data
);

/**
 * @brief Signal master that slave is ready
 *
 * @param ready True to signal ready, false otherwise
 * @return ESP_OK on success
 */
esp_err_t ipc_slave_set_ready(bool ready);

/**
 * @brief Send response to master (optional)
 *
 * @param type Packet type
 * @param payload Payload data
 * @param payload_size Payload size
 * @return ESP_OK on success
 */
esp_err_t ipc_slave_send_response(
    ipc_packet_type_t type,
    const void *payload,
    size_t payload_size
);

// ========== COMMON API ==========

/**
 * @brief IPC statistics
 */
typedef struct {
    uint64_t packets_sent;
    uint64_t packets_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
    uint32_t crc_errors;
    uint32_t timeout_errors;
    uint32_t overflow_errors;
    float throughput_mbps;
} ipc_stats_t;

/**
 * @brief Get IPC statistics
 *
 * @param[out] stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t ipc_get_stats(ipc_stats_t *stats);

/**
 * @brief Reset statistics
 *
 * @return ESP_OK on success
 */
esp_err_t ipc_reset_stats(void);

/**
 * @brief Calculate CRC16 for data
 *
 * @param data Data buffer
 * @param len Data length
 * @return CRC16 value
 */
uint16_t ipc_crc16(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

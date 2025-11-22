/**
 * @file stats_tracker.h
 * @brief System-wide Statistics and Performance Monitoring
 *
 * This component aggregates and tracks performance metrics from all subsystems.
 * Features:
 * - Real-time performance monitoring
 * - System health tracking
 * - Memory usage monitoring
 * - CPU utilization tracking
 * - Network statistics aggregation
 * - Latency measurements
 * - Event logging and reporting
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief System health status
 */
typedef enum {
    HEALTH_GOOD = 0,           /**< All systems operating normally */
    HEALTH_WARNING = 1,        /**< Minor issues detected */
    HEALTH_CRITICAL = 2,       /**< Critical issues, degraded performance */
    HEALTH_FAILURE = 3         /**< System failure */
} system_health_t;

/**
 * @brief Memory statistics
 */
typedef struct {
    uint32_t total_heap;       /**< Total heap size */
    uint32_t free_heap;        /**< Free heap size */
    uint32_t min_free_heap;    /**< Minimum free heap ever */
    uint32_t largest_block;    /**< Largest free block */
    uint32_t spiram_total;     /**< Total SPIRAM size */
    uint32_t spiram_free;      /**< Free SPIRAM */
} memory_stats_t;

/**
 * @brief CPU statistics
 */
typedef struct {
    uint8_t cpu0_usage;        /**< CPU0 usage percentage */
    uint8_t cpu1_usage;        /**< CPU1 usage percentage */
    uint32_t cpu_freq_mhz;     /**< CPU frequency in MHz */
    uint32_t task_count;       /**< Number of active tasks */
} cpu_stats_t;

/**
 * @brief Network performance statistics
 */
typedef struct {
    float rx_throughput_mbps;  /**< RX throughput in Mbps */
    float tx_throughput_mbps;  /**< TX throughput in Mbps */
    uint64_t packets_received; /**< Total packets received */
    uint64_t packets_sent;     /**< Total packets sent */
    uint64_t packets_dropped;  /**< Total packets dropped */
    int8_t rssi_dbm;           /**< Current RSSI */
    float packet_loss_rate;    /**< Packet loss rate */
} network_stats_t;

/**
 * @brief Latency statistics
 */
typedef struct {
    uint32_t wifi_rx_latency_us;    /**< WiFi RX latency (microseconds) */
    uint32_t fec_decode_latency_us; /**< FEC decode latency */
    uint32_t frame_assembly_latency_us; /**< Frame assembly latency */
    uint32_t usb_tx_latency_us;     /**< USB TX latency */
    uint32_t end_to_end_latency_us; /**< Total end-to-end latency */
} latency_stats_t;

/**
 * @brief Comprehensive system statistics
 */
typedef struct {
    system_health_t health;    /**< Overall system health */
    memory_stats_t memory;     /**< Memory statistics */
    cpu_stats_t cpu;           /**< CPU statistics */
    network_stats_t network;   /**< Network statistics */
    latency_stats_t latency;   /**< Latency statistics */
    uint64_t uptime_ms;        /**< System uptime in milliseconds */
    uint32_t temperature_c;    /**< CPU temperature in Celsius */
} system_stats_t;

/**
 * @brief Stats tracker configuration
 */
typedef struct {
    uint32_t update_interval_ms;   /**< Stats update interval */
    bool enable_cpu_profiling;     /**< Enable CPU profiling */
    bool enable_memory_tracking;   /**< Enable memory tracking */
    bool enable_logging;           /**< Enable stats logging */
} stats_tracker_config_t;

/**
 * @brief Stats update callback
 *
 * Called periodically with updated statistics.
 *
 * @param stats Current system statistics
 * @param user_ctx User context pointer
 */
typedef void (*stats_update_callback_t)(const system_stats_t *stats, void *user_ctx);

/**
 * @brief Initialize statistics tracker
 *
 * @param config Tracker configuration
 * @param callback Stats update callback (optional)
 * @param callback_ctx User context for callback
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_init(
    const stats_tracker_config_t *config,
    stats_update_callback_t callback,
    void *callback_ctx
);

/**
 * @brief Deinitialize statistics tracker
 *
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_deinit(void);

/**
 * @brief Start statistics collection
 *
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_start(void);

/**
 * @brief Stop statistics collection
 *
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_stop(void);

/**
 * @brief Get current system statistics
 *
 * @param stats Pointer to statistics structure
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_get_stats(system_stats_t *stats);

/**
 * @brief Reset all statistics
 *
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_reset(void);

/**
 * @brief Record latency measurement
 *
 * Records a latency measurement for a specific stage.
 *
 * @param stage Stage identifier (e.g., "wifi_rx", "fec_decode")
 * @param latency_us Latency in microseconds
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_record_latency(const char *stage, uint32_t latency_us);

/**
 * @brief Record packet event
 *
 * Records a packet reception, transmission, or drop event.
 *
 * @param received Number of packets received
 * @param sent Number of packets sent
 * @param dropped Number of packets dropped
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_record_packet_event(uint64_t received, uint64_t sent, uint64_t dropped);

/**
 * @brief Get system health status
 *
 * @param health Pointer to store health status
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_get_health(system_health_t *health);

/**
 * @brief Get memory statistics
 *
 * @param mem_stats Pointer to memory statistics structure
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_get_memory_stats(memory_stats_t *mem_stats);

/**
 * @brief Get CPU statistics
 *
 * @param cpu_stats Pointer to CPU statistics structure
 * @return ESP_OK on success
 */
esp_err_t stats_tracker_get_cpu_stats(cpu_stats_t *cpu_stats);

/**
 * @brief Print statistics summary
 *
 * Prints a formatted summary of current statistics to console.
 */
void stats_tracker_print_summary(void);

/**
 * @brief Export statistics as JSON
 *
 * Exports current statistics in JSON format.
 *
 * @param buffer Buffer to store JSON string
 * @param buffer_size Buffer size
 * @return Number of bytes written, or -1 on error
 */
int stats_tracker_export_json(char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

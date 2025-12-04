/**
 * ESP32 FPV Ground Station - Packet TX
 *
 * Transmits Ground2Air packets to the FPV air unit.
 * Supports camera control, configuration, and telemetry.
 */

#ifndef PACKET_TX_H
#define PACKET_TX_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// Resolution options (must match air unit)
typedef enum {
    RESOLUTION_QVGA = 0,    // 320x240
    RESOLUTION_CIF,         // 400x296
    RESOLUTION_HVGA,        // 480x320
    RESOLUTION_VGA,         // 640x480
    RESOLUTION_VGA16,       // 640x360
    RESOLUTION_SVGA,        // 800x600
    RESOLUTION_SVGA16,      // 800x456
    RESOLUTION_XGA,         // 1024x768
    RESOLUTION_XGA16,       // 1024x576
    RESOLUTION_SXGA,        // 1280x960
    RESOLUTION_HD,          // 1280x720
    RESOLUTION_UXGA,        // 1600x1200
    RESOLUTION_COUNT
} camera_resolution_t;

// WiFi rates (must match air unit)
typedef enum {
    RATE_B_2M_CCK = 0,
    RATE_B_2M_CCK_S,
    RATE_B_5_5M_CCK,
    RATE_B_5_5M_CCK_S,
    RATE_B_11M_CCK,
    RATE_B_11M_CCK_S,
    RATE_G_6M_ODFM,
    RATE_G_9M_ODFM,
    RATE_G_12M_ODFM,
    RATE_G_18M_ODFM,
    RATE_G_24M_ODFM,
    RATE_G_36M_ODFM,
    RATE_G_48M_ODFM,
    RATE_G_54M_ODFM,
    RATE_N_6_5M_MCS0,
    RATE_N_7_2M_MCS0_S,
    RATE_N_13M_MCS1,
    RATE_N_14_4M_MCS1_S,
    RATE_N_19_5M_MCS2,
    RATE_N_21_7M_MCS2_S,
    RATE_N_26M_MCS3,
    RATE_N_28_9M_MCS3_S,
    RATE_N_39M_MCS4,
    RATE_N_43_3M_MCS4_S,
    RATE_N_52M_MCS5,
    RATE_N_57_8M_MCS5_S,
    RATE_N_58M_MCS6,
    RATE_N_65M_MCS6_S,
    RATE_N_65M_MCS7,
    RATE_N_72M_MCS7_S,
} wifi_rate_t;

// Camera configuration (matches air unit CameraConfig)
typedef struct __attribute__((packed)) {
    uint8_t resolution;     // camera_resolution_t
    uint8_t fps_limit;      // 0-255, 0=no limit
    uint8_t quality;        // 0-63, 0=auto
    int8_t brightness;      // -2 to 2
    int8_t contrast;        // -2 to 2
    int8_t saturation;      // -2 to 2
    int8_t sharpness;       // -2 to 3
    uint8_t denoise;        // 0-8 (OV5640 only)
    uint8_t special_effect; // 0-6
    bool awb;               // Auto white balance
    bool awb_gain;
    uint8_t wb_mode;        // 0-4
    bool aec;               // Auto exposure control
    bool aec2;              // DSP AEC / Night mode
    int8_t ae_level;        // -2 to 2 (when aec=true)
    uint16_t aec_value;     // 0-1200 ISO (when aec=false)
    bool agc;               // Auto gain control
    uint8_t agc_gain;       // 0-30 (when agc=false)
    uint8_t gainceiling;    // 0-6 (when agc=true): 2x,4x,8x,16x,32x,64x,128x
    bool bpc;               // Bad pixel correction
    bool wpc;               // White point correction
    bool raw_gma;           // Raw gamma
    bool lenc;              // Lens correction
    bool hmirror;           // Horizontal mirror
    bool vflip;             // Vertical flip
    bool dcw;               // Downsize
    bool ov2640_high_fps;
    bool ov5640_high_fps;
    bool ov5640_night_mode;
} camera_config_t;

// Data channel configuration
typedef struct __attribute__((packed)) {
    int8_t wifi_power;      // dBm (-20 to 20)
    uint8_t wifi_rate;      // wifi_rate_t
    uint8_t wifi_channel;   // 1-14
    uint8_t fec_codec_k;    // FEC K value
    uint8_t fec_codec_n;    // FEC N value
    uint16_t fec_codec_mtu; // FEC MTU
} data_channel_config_t;

// Miscellaneous configuration
typedef struct __attribute__((packed)) {
    uint8_t air_record_btn;     // Incremented on button press
    uint8_t profile1_btn;
    uint8_t profile2_btn;
    uint8_t camera_stop_channel : 5;
    uint8_t autostart_record : 1;
    uint8_t mavlink2msp_rc : 1;
    uint8_t reserved1 : 1;
    uint32_t osd_font_crc32;
} misc_config_t;

// Air unit statistics (received from air unit)
typedef struct __attribute__((packed)) {
    uint8_t sd_detected : 1;
    uint8_t sd_slow : 1;
    uint8_t sd_error : 1;
    uint8_t curr_wifi_rate : 5;
    uint8_t wifi_queue_min : 7;
    uint8_t air_record_state : 1;
    uint8_t wifi_queue_max;
    uint32_t sd_free_space_gb16 : 12;
    uint32_t sd_total_space_gb16 : 12;
    uint32_t curr_quality : 6;
    uint32_t wifi_ovf : 1;
    uint32_t is_ov5640 : 1;
    uint16_t out_packet_rate;
    uint16_t in_packet_rate;
    uint16_t in_rejected_packet_rate;
    uint8_t rssi_dbm;
    uint8_t noise_floor_dbm;
    uint8_t capture_fps;
    uint8_t cam_ovf_count;
    uint16_t cam_frame_size_min;
    uint16_t cam_frame_size_max;
    uint16_t in_mavlink_rate;
    uint16_t out_mavlink_rate;
    uint8_t rc_period_max;
    uint8_t wifi_channel : 4;
    uint8_t resolution : 4;
    uint8_t temperature : 7;
    uint8_t overheat_throttling : 1;
    uint8_t reserved : 7;
    uint8_t suspended : 1;
    uint8_t fec_codec_k : 4;
    uint8_t in_session : 1;
    uint8_t screen_aspect_ratio : 3;
    int16_t osd_brightness : 3;
    int16_t osd_contrast : 3;
    int16_t osd_saturation : 3;
    int16_t osd_sharpness : 3;
    int16_t osd_ae_level : 3;
    int16_t osd_reserved1 : 1;
} air_stats_t;

/**
 * Initialize packet TX
 */
esp_err_t packet_tx_init(void);

/**
 * Start packet TX task (periodic config transmission)
 */
esp_err_t packet_tx_start(void);

/**
 * Stop packet TX
 */
void packet_tx_stop(void);

/**
 * Set target air device ID
 */
void packet_tx_set_air_device_id(uint16_t device_id);

/**
 * Get current camera configuration
 */
camera_config_t *packet_tx_get_camera_config(void);

/**
 * Get current data channel configuration
 */
data_channel_config_t *packet_tx_get_data_channel_config(void);

/**
 * Get latest air unit statistics
 */
air_stats_t *packet_tx_get_air_stats(void);

/**
 * Mark configuration as changed (triggers immediate send)
 */
void packet_tx_config_changed(void);

/**
 * Check if connected to air unit
 */
bool packet_tx_is_connected(void);

/**
 * Send connection request
 */
esp_err_t packet_tx_send_connect(void);

/**
 * Send current configuration to air unit
 */
esp_err_t packet_tx_send_config(void);

/**
 * Get latency measurement (ping in ms)
 */
uint32_t packet_tx_get_latency_ms(void);

/**
 * Process air unit response (updates connection state and stats)
 */
void packet_tx_process_air_response(const uint8_t *data, size_t len);

#endif // PACKET_TX_H

# ESP32-P4 + ESP32-C5 High-Resolution FPV Implementation Plan

## Executive Summary

This document outlines the complete implementation of a next-generation FPV system using:
- **ESP32-P4** for high-resolution MIPI camera capture and video processing
- **ESP32-C5** for 2.4GHz/5GHz WiFi transmission
- Inter-processor communication via SPI/SDIO
- Support for resolutions up to 1920x1080 @ 60fps

## Hardware Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     AIR UNIT (ESP32-P4 + ESP32-C5)          │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────────────────────────┐                  │
│  │        ESP32-P4 (Main Processor)     │                  │
│  │  - Dual-core RISC-V @ 400MHz         │                  │
│  │  - 32MB PSRAM                        │                  │
│  │  - H.264 hardware encoder            │                  │
│  │  - 2D acceleration                   │                  │
│  └───────┬──────────────────────┬───────┘                  │
│          │                      │                           │
│          │ MIPI CSI-2          │ SPI/SDIO                 │
│          │ (4-lane, 2.5Gbps)   │ (50MHz)                  │
│          │                      │                           │
│  ┌───────▼──────────┐   ┌───────▼───────────────┐         │
│  │   MIPI Camera    │   │    ESP32-C5           │         │
│  │   (High Res)     │   │  - WiFi 6 (802.11ax)  │         │
│  │                  │   │  - 2.4GHz + 5GHz      │         │
│  │  Options:        │   │  - Packet injection   │         │
│  │  - IMX219        │   │  - FEC encoder        │         │
│  │  - IMX477        │   └───────────────────────┘         │
│  │  - OV5647        │                                      │
│  └──────────────────┘                                      │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## Supported MIPI Camera Modules

### IMX219 (Recommended)

**Specifications:**
- **Sensor:** Sony IMX219 CMOS
- **Resolution:** 8MP (3280 × 2464)
- **Video Modes:**
  - 1920x1080 @ 60fps
  - 1920x1080 @ 30fps
  - 1280x720 @ 120fps
  - 1640x1232 @ 40fps
- **Interface:** MIPI CSI-2 (2-lane)
- **Features:**
  - On-chip 10-bit ADC
  - Rolling shutter
  - HDR mode support
- **Availability:** Raspberry Pi Camera Module V2
- **Cost:** ~$25

### IMX477 (High Quality)

**Specifications:**
- **Sensor:** Sony IMX477 CMOS
- **Resolution:** 12.3MP (4056 × 3040)
- **Video Modes:**
  - 1920x1080 @ 60fps
  - 2028x1520 @ 40fps
  - 4056x3040 @ 10fps
- **Interface:** MIPI CSI-2 (2-lane or 4-lane)
- **Features:**
  - 12-bit ADC
  - Back-illuminated sensor
  - C/CS-mount (interchangeable lenses)
  - Excellent low-light performance
- **Availability:** Raspberry Pi HQ Camera Module
- **Cost:** ~$50

### OV5647 (Budget Option)

**Specifications:**
- **Sensor:** OmniVision OV5647 CMOS
- **Resolution:** 5MP (2592 × 1944)
- **Video Modes:**
  - 1920x1080 @ 30fps
  - 1280x720 @ 60fps
  - 640x480 @ 90fps
- **Interface:** MIPI CSI-2 (2-lane)
- **Availability:** Raspberry Pi Camera Module V1
- **Cost:** ~$15

## System Capabilities Comparison

| Feature | Current (ESP32-S3 + OV5640) | New (ESP32-P4 + IMX219) |
|---------|----------------------------|-------------------------|
| Max Resolution | 1280x720 @ 30fps | 1920x1080 @ 60fps |
| Video Encoding | MJPEG (sensor) | H.264 (hardware) |
| Typical Bitrate | 8-10 Mbps | 5-8 Mbps (better compression) |
| Latency | 90-110ms | 60-80ms (target) |
| Image Quality | Good | Excellent |
| Dynamic Range | Limited | HDR capable |
| Low-light | Poor-Fair | Excellent (IMX477) |

## ESP32-P4 Key Features

### CPU & Memory
- **CPU:** Dual-core RISC-V @ 400MHz
- **RAM:** 768 KB SRAM
- **PSRAM:** Up to 32MB external
- **Flash:** Up to 16MB external

### Video Processing
- **H.264/H.265 Encoder:** Hardware-accelerated
  - Up to 1920x1080 @ 60fps
  - Bitrate: 100Kbps - 20Mbps
  - I/P/B frames
  - Rate control
- **JPEG Encoder/Decoder:** Hardware-accelerated
- **2D Graphics Accelerator**
- **Camera Interface:** MIPI CSI-2
  - Up to 4 data lanes
  - Up to 2.5 Gbps per lane
  - RAW8/RAW10/RAW12/YUV422

### Peripherals
- **SPI:** Multiple, up to 80MHz
- **SDIO:** Host/slave, up to 50MHz
- **UART:** Multiple high-speed
- **I2C:** Multiple
- **USB 2.0 OTG**
- **SD Card:** SDMMC host

## ESP32-C5 Key Features

### Wireless
- **WiFi 6 (802.11ax):**
  - 2.4GHz: up to 86Mbps
  - 5GHz: up to 430Mbps
- **Bluetooth 5.3**
- **Features:**
  - OFDMA
  - MU-MIMO
  - TWT (Target Wake Time)
  - Packet injection support
  - Monitor mode

### CPU & Memory
- **CPU:** RISC-V @ 240MHz
- **RAM:** 400 KB SRAM
- **Flash:** Up to 4MB

## Inter-Processor Communication

### SPI Interface (Primary)

**Configuration:**
- **Mode:** Master (ESP32-P4) / Slave (ESP32-C5)
- **Speed:** 40-50MHz
- **DMA:** Both sides
- **Protocol:** Custom packet-based

**Physical Connections:**
```
ESP32-P4          ESP32-C5
--------          --------
GPIO_MOSI   -->   GPIO_MOSI
GPIO_MISO   <--   GPIO_MISO
GPIO_CLK    -->   GPIO_CLK
GPIO_CS     -->   GPIO_CS
GPIO_HANDSHAKE <-> GPIO_HANDSHAKE
GND         ---   GND
```

**Data Flow:**

```
ESP32-P4                              ESP32-C5
   │                                     │
   │  ┌─────────────────┐               │
   │  │ H.264 Encoder   │               │
   │  │ Output Buffer   │               │
   │  └────────┬────────┘               │
   │           │                         │
   │  ┌────────▼────────┐               │
   │  │ Packet Framer   │               │
   │  │ - Add headers   │               │
   │  │ - Fragment NALUs│               │
   │  └────────┬────────┘               │
   │           │                         │
   │  ┌────────▼────────┐               │
   │  │  SPI TX DMA     │               │
   │  └────────┬────────┘               │
   │           │                         │
   │           │ SPI Transfer            │
   ├───────────┴─────────────────────────┤
   │                                     │
   │                         ┌───────────▼─────┐
   │                         │  SPI RX DMA     │
   │                         └───────────┬─────┘
   │                                     │
   │                         ┌───────────▼─────┐
   │                         │  FEC Encoder    │
   │                         │  (6/12)         │
   │                         └───────────┬─────┘
   │                                     │
   │                         ┌───────────▼─────┐
   │                         │  WiFi Injection │
   │                         │  (802.11ax)     │
   │                         └─────────────────┘
   │                                     │
   └─────────────────────────────────────┘
```

### SDIO Interface (Alternative)

**Advantages:**
- Higher throughput (up to 200Mbps)
- Lower CPU overhead
- Native DMA support

**Configuration:**
- **Mode:** Host (ESP32-P4) / Device (ESP32-C5)
- **Speed:** 50MHz (4-bit mode)
- **Protocol:** SDIO standard

## Implementation Components

### 1. ESP32-P4 MIPI Camera Driver

**File:** `esp32-p4-mipi-fpv/components/mipi_camera/mipi_camera.c`

```c
/**
 * @brief MIPI CSI-2 camera driver for ESP32-P4
 *
 * Supports:
 * - IMX219 (1920x1080 @ 60fps)
 * - IMX477 (1920x1080 @ 60fps, 4056x3040 @ 10fps)
 * - OV5647 (1920x1080 @ 30fps)
 */

typedef struct {
    mipi_camera_sensor_t sensor;     // IMX219, IMX477, OV5647
    mipi_camera_resolution_t resolution;
    uint8_t fps;
    mipi_camera_format_t format;     // RAW10, YUV422, etc.
    bool hdr_enabled;
    bool auto_exposure;
    bool auto_white_balance;
    uint16_t exposure_time_us;
    uint8_t gain;
} mipi_camera_config_t;

esp_err_t mipi_camera_init(mipi_camera_config_t *config);
esp_err_t mipi_camera_start();
esp_err_t mipi_camera_stop();

// Frame callback
typedef void (*mipi_camera_frame_cb_t)(
    const uint8_t *frame_data,
    size_t frame_size,
    uint32_t width,
    uint32_t height,
    mipi_camera_format_t format,
    uint64_t timestamp_us
);

esp_err_t mipi_camera_register_frame_callback(mipi_camera_frame_cb_t cb);
```

### 2. ESP32-P4 H.264 Encoder

**File:** `esp32-p4-mipi-fpv/components/h264_encoder/h264_encoder.c`

```c
/**
 * @brief Hardware H.264 encoder for ESP32-P4
 *
 * Utilizes ESP32-P4's built-in H.264 encoding engine for
 * efficient video compression.
 */

typedef enum {
    H264_PROFILE_BASELINE,
    H264_PROFILE_MAIN,
    H264_PROFILE_HIGH
} h264_profile_t;

typedef enum {
    H264_LEVEL_3_0,  // Up to 720p @ 30fps
    H264_LEVEL_3_1,  // Up to 720p @ 60fps
    H264_LEVEL_4_0   // Up to 1080p @ 30fps
} h264_level_t;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint8_t fps;
    uint32_t bitrate;              // bps
    uint8_t gop_size;              // I-frame interval
    h264_profile_t profile;
    h264_level_t level;
    bool enable_b_frames;
    uint8_t qp_min;                // 0-51
    uint8_t qp_max;                // 0-51
} h264_encoder_config_t;

esp_err_t h264_encoder_init(h264_encoder_config_t *config);
esp_err_t h264_encoder_start();

// Encode frame (YUV422 input)
esp_err_t h264_encoder_encode_frame(
    const uint8_t *yuv_data,
    size_t yuv_size,
    uint64_t pts_us
);

// NALU callback
typedef void (*h264_nalu_cb_t)(
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint8_t nal_type,         // SPS, PPS, I, P, B
    uint64_t pts_us,
    uint64_t dts_us
);

esp_err_t h264_encoder_register_nalu_callback(h264_nalu_cb_t cb);
```

### 3. Inter-Processor Communication (IPC)

**File:** `esp32-p4-mipi-fpv/components/ipc/esp32_ipc.c`

```c
/**
 * @brief Inter-processor communication between ESP32-P4 and ESP32-C5
 *
 * Uses SPI for low-latency, high-throughput data transfer.
 */

typedef enum {
    IPC_PACKET_TYPE_VIDEO = 0x01,
    IPC_PACKET_TYPE_CONFIG = 0x02,
    IPC_PACKET_TYPE_TELEMETRY = 0x03,
    IPC_PACKET_TYPE_STATS = 0x04
} ipc_packet_type_t;

#pragma pack(push, 1)
typedef struct {
    uint8_t type;                  // IPC_PACKET_TYPE_*
    uint8_t flags;
    uint16_t sequence;
    uint32_t payload_size;
    uint16_t crc16;
} ipc_packet_header_t;

typedef struct {
    ipc_packet_header_t header;
    uint8_t nal_type;              // H.264 NAL type
    uint32_t frame_index;
    uint64_t pts_us;
    uint64_t dts_us;
    // NAL data follows
} ipc_video_packet_t;
#pragma pack(pop)

// ESP32-P4 (Master) API
esp_err_t ipc_master_init(uint8_t mosi_pin, uint8_t miso_pin,
                         uint8_t clk_pin, uint8_t cs_pin);
esp_err_t ipc_master_send(const void *data, size_t size);

// ESP32-C5 (Slave) API
esp_err_t ipc_slave_init(uint8_t mosi_pin, uint8_t miso_pin,
                        uint8_t clk_pin, uint8_t cs_pin);

typedef void (*ipc_receive_cb_t)(const void *data, size_t size);
esp_err_t ipc_slave_register_callback(ipc_receive_cb_t cb);
```

### 4. ESP32-C5 WiFi Transmission

**File:** `esp32-p4-mipi-fpv/components/wifi_c5/wifi_transmitter.c`

```c
/**
 * @brief WiFi 6 packet injection for ESP32-C5
 *
 * Transmits H.264-encoded video over WiFi using packet injection.
 */

typedef enum {
    WIFI_BAND_2_4GHZ,
    WIFI_BAND_5GHZ
} wifi_band_t;

typedef struct {
    wifi_band_t band;
    uint8_t channel;               // 1-14 (2.4GHz) or 36-165 (5GHz)
    int8_t tx_power_dbm;           // Max output power
    uint8_t mcs_index;             // 0-11 (WiFi 6)
    bool enable_fec;               // FEC encoding
    uint8_t fec_k;                 // Data blocks
    uint8_t fec_n;                 // Total blocks
} wifi_tx_config_t;

esp_err_t wifi_transmitter_init(wifi_tx_config_t *config);
esp_err_t wifi_transmitter_start();

// Send video packet
esp_err_t wifi_transmitter_send_video(
    const uint8_t *nalu_data,
    size_t nalu_size,
    uint32_t frame_index,
    uint64_t pts_us
);

// Get statistics
typedef struct {
    uint32_t packets_sent;
    uint32_t bytes_sent;
    uint32_t packets_dropped;
    int8_t rssi_dbm;               // From ground station ACKs (if available)
} wifi_tx_stats_t;

esp_err_t wifi_transmitter_get_stats(wifi_tx_stats_t *stats);
```

## Video Encoding Pipeline

### End-to-End Flow

```
┌───────────────────────────────────────────────────────────┐
│                ESP32-P4 Processing                        │
├───────────────────────────────────────────────────────────┤
│                                                           │
│  ┌──────────┐     ┌──────────┐     ┌──────────┐        │
│  │  MIPI    │────▶│  ISP     │────▶│  Format  │        │
│  │ Camera   │     │(Debayer, │     │ Converter│        │
│  │          │     │ HDR, AWB)│     │(YUV422)  │        │
│  └──────────┘     └──────────┘     └────┬─────┘        │
│                                          │               │
│                                  ┌───────▼──────┐       │
│                                  │ H.264 Encoder│       │
│                                  │  (Hardware)  │       │
│                                  │              │       │
│                                  │ - NAL units  │       │
│                                  │ - SPS/PPS    │       │
│                                  │ - I/P frames │       │
│                                  └───────┬──────┘       │
│                                          │               │
│                                  ┌───────▼──────┐       │
│                                  │ IPC Framing  │       │
│                                  │              │       │
│                                  │ - Add headers│       │
│                                  │ - Fragment   │       │
│                                  └───────┬──────┘       │
│                                          │               │
│                                  ┌───────▼──────┐       │
│                                  │  SPI Master  │       │
│                                  │     TX       │       │
│                                  └───────┬──────┘       │
└──────────────────────────────────────────┼──────────────┘
                                           │
                                           │ SPI (50MHz)
                                           │
┌──────────────────────────────────────────▼──────────────┐
│                ESP32-C5 Processing                      │
├─────────────────────────────────────────────────────────┤
│                                                         │
│                                  ┌──────────┐          │
│                                  │ SPI Slave│          │
│                                  │    RX    │          │
│                                  └────┬─────┘          │
│                                       │                 │
│                                  ┌────▼─────┐          │
│                                  │   FEC    │          │
│                                  │ Encoder  │          │
│                                  │  (6/12)  │          │
│                                  └────┬─────┘          │
│                                       │                 │
│                                  ┌────▼─────┐          │
│                                  │  WiFi 6  │          │
│                                  │Injection │          │
│                                  │          │          │
│                                  │ 802.11ax │          │
│                                  │  MCS7-11 │          │
│                                  └────┬─────┘          │
│                                       │                 │
└───────────────────────────────────────┼─────────────────┘
                                        │
                                        │ 2.4GHz / 5GHz
                                        ▼
                                Ground Station
```

## Ground Station Modifications

### H.264 Decoder Integration

```c
/**
 * @brief H.264 decoder for ground station
 *
 * Uses FFmpeg/libavcodec for H.264 decoding.
 */

#include <libavcodec/avcodec.h>

typedef struct {
    AVCodecContext *codec_ctx;
    AVFrame *frame;
    AVPacket *packet;
    struct SwsContext *sws_ctx;
} h264_decoder_t;

bool h264_decoder_init(h264_decoder_t *decoder) {
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    decoder->codec_ctx = avcodec_alloc_context3(codec);
    avcodec_open2(decoder->codec_ctx, codec, NULL);
    decoder->frame = av_frame_alloc();
    decoder->packet = av_packet_alloc();
    return true;
}

bool h264_decoder_decode_nalu(h264_decoder_t *decoder,
                              const uint8_t *nalu, size_t size,
                              uint8_t **rgb_out, int *width, int *height) {
    decoder->packet->data = (uint8_t*)nalu;
    decoder->packet->size = size;

    if (avcodec_send_packet(decoder->codec_ctx, decoder->packet) < 0) {
        return false;
    }

    if (avcodec_receive_frame(decoder->codec_ctx, decoder->frame) < 0) {
        return false;  // Need more data
    }

    *width = decoder->frame->width;
    *height = decoder->frame->height;

    // Convert YUV to RGB
    *rgb_out = malloc(*width * *height * 3);
    sws_scale(decoder->sws_ctx,
             (const uint8_t *const *)decoder->frame->data,
             decoder->frame->linesize,
             0, decoder->frame->height,
             rgb_out, rgb_stride);

    return true;
}
```

## Bandwidth and Bitrate Analysis

### H.264 @ 1920x1080 60fps

**Bitrate Estimates:**

| Quality | Bitrate | Description |
|---------|---------|-------------|
| Low | 2-3 Mbps | Visible compression, smooth motion |
| Medium | 4-6 Mbps | Good quality, minor artifacts |
| High | 8-10 Mbps | Excellent quality |
| Ultra | 12-15 Mbps | Near-lossless |

**FEC Overhead (6/12):**
- Raw: 6 Mbps
- With FEC: 12 Mbps

**WiFi 6 Capacity:**
- 2.4GHz MCS9: ~86 Mbps (theoretical)
- 2.4GHz MCS9: ~50 Mbps (practical, with FEC)
- **Headroom:** 50 - 12 = 38 Mbps ✓

- 5GHz MCS11: ~430 Mbps (theoretical)
- 5GHz MCS11: ~250 Mbps (practical, with FEC)
- **Headroom:** 250 - 12 = 238 Mbps ✓✓

**Conclusion:** System can support 1920x1080 @ 60fps with good quality on both 2.4GHz and 5GHz.

## Power Consumption

### Component Power Budget

| Component | Current @ 3.3V | Power |
|-----------|---------------|-------|
| ESP32-P4 (active) | ~300mA | ~1W |
| ESP32-C5 (WiFi TX) | ~350mA | ~1.2W |
| IMX219 camera | ~200mA @ 1.8V | ~0.4W |
| Voltage regulators | ~50mA | ~0.2W |
| **Total** | | **~2.8W** |

**Battery Life (1000mAh LiPo @ 3.7V):**
- Capacity: 1000mAh × 3.7V = 3.7Wh
- Runtime: 3.7Wh / 2.8W = **~1.3 hours**

**With 2200mAh battery:** ~2.9 hours

## Latency Budget (Target)

```
┌──────────────────────────┬──────────┐
│ Camera capture (16.7ms/frame @ 60fps)│  17ms│
├──────────────────────────┼──────────┤
│ H.264 encoding           │   8ms    │
├──────────────────────────┼──────────┤
│ IPC transfer (SPI)       │   2ms    │
├──────────────────────────┼──────────┤
│ FEC encoding             │   5ms    │
├──────────────────────────┼──────────┤
│ WiFi transmission        │   3ms    │
├──────────────────────────┼──────────┤
│ WiFi reception           │   2ms    │
├──────────────────────────┼──────────┤
│ FEC decoding             │   5ms    │
├──────────────────────────┼──────────┤
│ H.264 decoding           │  10ms    │
├──────────────────────────┼──────────┤
│ Rendering (60Hz)         │  17ms    │
├──────────────────────────┼──────────┤
│ Buffering/jitter         │  10ms    │
└──────────────────────────┴──────────┘
Total:                       79ms
```

**Target:** 60-80ms (improvement from 90-110ms)

## Development Phases

### Phase 1: ESP32-P4 MIPI Camera Driver (Week 1-2)
- [ ] MIPI CSI-2 interface initialization
- [ ] IMX219 sensor driver
- [ ] Frame capture and DMA
- [ ] ISP configuration
- [ ] Unit tests

### Phase 2: ESP32-P4 H.264 Encoder (Week 2-3)
- [ ] H.264 hardware encoder initialization
- [ ] NAL unit generation
- [ ] Rate control implementation
- [ ] Performance optimization
- [ ] Unit tests

### Phase 3: Inter-Processor Communication (Week 3-4)
- [ ] SPI master implementation (ESP32-P4)
- [ ] SPI slave implementation (ESP32-C5)
- [ ] Packet framing protocol
- [ ] DMA transfers
- [ ] Integration tests

### Phase 4: ESP32-C5 WiFi Transmission (Week 4-5)
- [ ] WiFi 6 initialization
- [ ] Packet injection
- [ ] FEC encoding integration
- [ ] Channel management
- [ ] Unit tests

### Phase 5: Ground Station Integration (Week 5-6)
- [ ] H.264 decoder (FFmpeg)
- [ ] WiFi reception modifications
- [ ] OpenGL rendering updates
- [ ] OSD integration
- [ ] End-to-end testing

### Phase 6: Testing and Optimization (Week 6-8)
- [ ] Latency measurements
- [ ] Bandwidth optimization
- [ ] Power consumption tuning
- [ ] Range testing
- [ ] Stress testing

## Testing Strategy

### Unit Tests

1. **MIPI Camera:**
   - Sensor initialization
   - Frame capture
   - Format conversion
   - FPS verification

2. **H.264 Encoder:**
   - Encoding quality
   - Bitrate control
   - NAL generation
   - Performance benchmarks

3. **IPC:**
   - Data integrity
   - Throughput
   - Latency
   - Error handling

4. **WiFi Transmission:**
   - Packet injection
   - FEC encoding
   - Bandwidth utilization

### Integration Tests

1. **End-to-End Latency**
2. **Video Quality Assessment (PSNR/SSIM)**
3. **Packet Loss Recovery**
4. **Range Testing**
5. **Power Consumption**

## Bill of Materials (BOM)

### Development Kit

| Item | Qty | Unit Price | Total |
|------|-----|------------|-------|
| ESP32-P4-Function-EV-Board | 1 | $50 | $50 |
| ESP32-C5-DevKitC | 1 | $15 | $15 |
| IMX219 Camera Module | 1 | $25 | $25 |
| MicroSD Card (32GB) | 1 | $10 | $10 |
| Breadboard + Jumpers | 1 | $10 | $10 |
| Power Supply (5V 3A) | 1 | $10 | $10 |
| **Total** | | | **$120** |

## Conclusion

The ESP32-P4 + ESP32-C5 platform enables a significant upgrade:
- **4x resolution** (1080p vs 720p)
- **2x frame rate** (60fps vs 30fps)
- **Better compression** (H.264 vs MJPEG)
- **Lower latency** (60-80ms vs 90-110ms)
- **Better image quality** (IMX219/IMX477 vs OV5640)

This implementation plan provides a complete roadmap for development, from hardware setup through testing and deployment.

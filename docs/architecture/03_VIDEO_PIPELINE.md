# Video Pipeline Architecture

## Overview

The video pipeline is the heart of the FPV system, responsible for capturing, encoding, transmitting, receiving, and rendering video with minimal latency. This document details the complete flow from camera sensor to display.

## Air Unit Video Pipeline

### Camera Sensor to I2S

```
┌──────────────────────────────────────────────────────────────┐
│                    OV2640 / OV5640 Sensor                    │
│                                                              │
│  ┌────────┐   ┌────────┐   ┌────────┐   ┌────────┐        │
│  │ Image  │──▶│  JPEG  │──▶│  Row   │──▶│  I2S   │        │
│  │ Sensor │   │Encoder │   │ by Row │   │  TX    │        │
│  │ Array  │   │(HW)    │   │ Output │   │        │        │
│  └────────┘   └────────┘   └────────┘   └────────┘        │
│                                                              │
└─────────────────────────────┬────────────────────────────────┘
                              │
                              │ I2S Protocol
                              │ 10MHz (OV2640)
                              │ 20MHz (OV5640 on ESP32-S3)
                              ▼
                    ┌─────────────────┐
                    │   ESP32 I2S     │
                    │   DMA Buffer    │
                    └─────────────────┘
```

### I2S DMA Configuration

**OV2640 (ESP32/ESP32-S3):**
```c
I2S Clock: 10 MHz
DMA Buffer Size: 2048 bytes
Number of DMA Buffers: 4
Transfer Mode: Streaming
```

**OV5640 (ESP32-S3 only):**
```c
I2S Clock: 20 MHz (2x faster)
DMA Buffer Size: 4096 bytes
Number of DMA Buffers: 4
Transfer Mode: Streaming
```

### JPEG Frame Structure

```
JPEG Frame Format:
┌────────┬─────────────────┬────────┐
│  SOI   │  Compressed     │  EOI   │
│ 0xFFD8 │  Image Data     │ 0xFFD9 │
└────────┴─────────────────┴────────┘

Detailed Structure:
┌────────┬────────┬─────────┬──────────┬─────────┬────────┐
│  SOI   │  APP0  │   DQT   │   Frame  │  Scan   │  EOI   │
│ 0xFFD8 │ (JFIF) │(Quant.) │  Header  │  Data   │ 0xFFD9 │
└────────┴────────┴─────────┴──────────┴─────────┴────────┘
```

### Modified ESP32-Camera Component

**Key Modification:** Streaming instead of buffering

**Original Behavior:**
```c
// Original esp32-camera
1. Wait for complete frame in DMA
2. Copy entire frame to PSRAM
3. Return frame pointer
4. Wait for application to process
5. Release frame buffer
```

**Modified Behavior:**
```c
// Modified for low-latency streaming
1. DMA interrupt on each buffer (2KB/4KB)
2. Immediately pass buffer to application
3. Application processes while DMA continues
4. No waiting for complete frame
5. Parallel capture and transmission
```

### Frame Parser Task

```
DMA Interrupt Handler
       │
       ▼
┌──────────────────────────────────┐
│   Frame Parser Task              │
│   (IRAM for performance)         │
├──────────────────────────────────┤
│                                  │
│  while (dma_data_available) {    │
│    ┌─────────────────────┐      │
│    │ Read DMA Buffer     │      │
│    └──────────┬──────────┘      │
│               │                  │
│    ┌──────────▼──────────┐      │
│    │ Parse JPEG Markers  │      │
│    │ - Detect SOI        │      │
│    │ - Track frame size  │      │
│    │ - Detect EOI        │      │
│    └──────────┬──────────┘      │
│               │                  │
│    ┌──────────▼──────────┐      │
│    │ Adaptive Quality    │      │
│    │ Control             │      │
│    └──────────┬──────────┘      │
│               │                  │
│    ┌──────────▼──────────┐      │
│    │ Create Video Packet │      │
│    └──────────┬──────────┘      │
│               │                  │
│    ┌──────────▼──────────┐      │
│    │ Send to FEC Encoder │      │
│    └─────────────────────┘      │
│  }                               │
└──────────────────────────────────┘
```

### Adaptive Quality Control

**Quality Control Algorithm:**

```c
static int s_quality = 20;  // Initial quality
static float K1 = 0;  // Bandwidth coefficient
static float K2 = 1;  // WiFi queue coefficient
static float K3 = 1;  // SD write coefficient

void update_quality() {
    // Factor 1: Target frame size based on WiFi rate
    float wifi_rate_mbps = get_wifi_rate_mbps();
    float fec_overhead = (float)fec_n / (float)fec_k;
    float target_fps = get_target_fps();

    int target_frame_size = (wifi_rate_mbps * 0.5 * 1000000)
                           / (8 * fec_overhead * target_fps);

    if (last_frame_size > target_frame_size * 1.2) {
        K1 = 1.5;  // Need more compression
    } else if (last_frame_size < target_frame_size * 0.8) {
        K1 = 0.7;  // Can afford better quality
    } else {
        K1 = 1.0;
    }

    // Factor 2: WiFi queue usage
    uint8_t queue_usage = get_wlan_queue_usage();
    if (queue_usage > 70) {
        K2 = 2.0;  // Critical - reduce quality
    } else if (queue_usage > 50) {
        K2 = 1.3;
    } else {
        K2 = 1.0;
    }

    // Factor 3: SD card write speed (if recording)
    if (sd_recording_enabled) {
        float sd_max_rate = is_esp32s3() ? 1.8 : 0.8;  // MB/s
        float current_rate = get_sd_write_rate();

        if (current_rate > sd_max_rate * 0.9) {
            K3 = 1.5;  // SD card struggling
        } else {
            K3 = 1.0;
        }
    }

    // Compute final quality
    float quality_f = s_quality * K1 * K2 * K3;
    s_quality = clamp((int)quality_f, 8, 63);

    // Apply to camera
    sensor_set_quality(s_quality);
}
```

**Quality Update Frequency:** Every frame

### Frame Fragmentation

**Large frames must be split into multiple packets:**

```c
Frame Size: 12,000 bytes
Packet MTU: 1470 bytes (AIR2GROUND_MTU - header)

Fragmentation:
┌───────────────────────────────────────────────┐
│ Frame 100 (12000 bytes)                       │
├───────────────────────────────────────────────┤
│                                               │
│  Packet 1:  part=0, last=0, data[0:1470]     │
│  Packet 2:  part=1, last=0, data[1470:2940]  │
│  Packet 3:  part=2, last=0, data[2940:4410]  │
│  Packet 4:  part=3, last=0, data[4410:5880]  │
│  Packet 5:  part=4, last=0, data[5880:7350]  │
│  Packet 6:  part=5, last=0, data[7350:8820]  │
│  Packet 7:  part=6, last=0, data[8820:10290] │
│  Packet 8:  part=7, last=1, data[10290:12000]│
│                                               │
└───────────────────────────────────────────────┘
```

**Packet Assembly:**

```c
struct Air2Ground_Video_Packet {
    // ... header fields ...
    uint32_t frame_index;      // Monotonic counter
    uint8_t part_index : 7;    // 0-127
    uint8_t last_part : 1;     // 1 = final part
};

// Sender
for (size_t offset = 0; offset < frame_size; ) {
    size_t chunk = min(MTU, frame_size - offset);
    packet.part_index = offset / MTU;
    packet.last_part = (offset + chunk >= frame_size);
    memcpy(packet.data, frame_data + offset, chunk);
    send_to_fec_encoder(&packet);
    offset += chunk;
}
```

### SD Card DVR

**Parallel Recording:**

```
Video Data Stream
       │
       ├──────────────┬──────────────┐
       │              │              │
       ▼              ▼              ▼
  WiFi Queue    SD Card Queue   Display
  (Realtime)    (Buffered 3MB)  (Status LED)
```

**SD Card Write Strategy:**

```c
// 3MB circular buffer for SD writes
#define SD_BUFFER_SIZE (3 * 1024 * 1024)
uint8_t sd_buffer[SD_BUFFER_SIZE];
size_t sd_write_offset = 0;

void sd_write_task() {
    while (recording) {
        // Wait for data in buffer
        if (sd_data_available() > SD_WRITE_THRESHOLD) {
            // Write chunk to SD card
            size_t write_size = min(SD_CHUNK_SIZE, sd_data_available());
            sd_write(sd_buffer, write_size);

            // Track write performance
            if (write_too_slow()) {
                set_sd_slow_flag();  // Display warning
                reduce_quality();    // Adapt
            }
        }
        vTaskDelay(10);
    }
}
```

**AVI Format:**

```c
AVI File Structure:
┌────────────────┐
│  RIFF Header   │  "AVI " format
├────────────────┤
│  hdrl List     │  Stream headers (MJPEG)
├────────────────┤
│  movi List     │
│  ├─ Frame 1    │  JPEG data
│  ├─ Frame 2    │
│  ├─ Frame 3    │
│  └─ ...        │
├────────────────┤
│  idx1 Chunk    │  Frame index (for seeking)
└────────────────┘
```

### Resolution Modes

**Supported Resolutions (from `packets.h`):**

| Resolution | Size | OV2640 FPS | OV5640 FPS | OV5640 HiFPS | Aspect |
|------------|------|------------|------------|--------------|--------|
| QVGA | 320x240 | 60 | 60 | 60 | 4:3 |
| CIF | 400x296 | 50 | 50 | 50 | 4:3 |
| HVGA | 480x320 | 50 | 50 | 50 | 3:2 |
| VGA | 640x480 | 30 | 30 | 40 | 4:3 |
| VGA16 | 640x360 | 30 | 30 | 50 | 16:9 |
| SVGA | 800x600 | 30 | 30 | 30 | 4:3 |
| SVGA16 | 800x456 | 30 | 30 | 50 | 16:9 |
| XGA | 1024x768 | 15 | 30 | 30 | 4:3 |
| XGA16 | 1024x576 | 13 | 30 | 30 | 16:9 |
| HD | 1280x720 | 13 | 30 | 30 | 16:9 |

**Note:** OV5640 supports binning for better quality at same FPS

### HQ DVR Mode

**Special mode for high-quality recording:**

```
Camera captures: 1280x720 @ 30fps (best quality)
       │
       ├───────────────┬──────────────┐
       │               │              │
       ▼               ▼              ▼
  WiFi: 5-10fps   SD Card: 30fps   OSD: Updated
  (Bandwidth       (Full quality)
   limited)
```

**Implementation:**

```c
if (hq_dvr_mode) {
    // Capture at full res
    set_resolution(HD);  // 1280x720
    set_quality(8);      // Best quality

    // Send every Nth frame over WiFi
    if (frame_index % frame_skip == 0) {
        send_to_wifi(frame);
    }

    // Record all frames to SD
    write_to_sd(frame);
}
```

## Ground Station Video Pipeline

### WiFi Monitor Mode Reception

```
┌──────────────────────────────────────┐
│     RTL8812AU WiFi Card              │
│     (Monitor Mode)                   │
└──────────────┬───────────────────────┘
               │
               │ Raw 802.11 frames
               ▼
┌──────────────────────────────────────┐
│     PCAP (libpcap)                   │
│     - Packet capture                 │
│     - Radiotap header parsing        │
│     - RSSI extraction                │
└──────────────┬───────────────────────┘
               │
               │ Filtered packets
               ▼
┌──────────────────────────────────────┐
│     Packet Filter                    │
│     - Check signature (56)           │
│     - Check version (2)              │
│     - Validate device IDs            │
│     - Extract RSSI                   │
└──────────────┬───────────────────────┘
               │
               │ Valid packets
               ▼
        FEC Decoder
```

### Diversity Reception

**Dual WiFi cards for improved reception:**

```
┌─────────────┐       ┌─────────────┐
│  RTL8812AU  │       │  RTL8812AU  │
│   Card #1   │       │   Card #2   │
└──────┬──────┘       └──────┬──────┘
       │                     │
       │ Thread #1           │ Thread #2
       ▼                     ▼
┌──────────────────────────────────────┐
│     Packet Merger                    │
│     - Deduplicate packets            │
│     - Select best RSSI               │
│     - Combine diversity              │
└──────────────┬───────────────────────┘
               │
               ▼
        FEC Decoder
```

**Deduplication:**

```c
struct ReceivedPacket {
    uint32_t block_index;
    uint8_t packet_index;
    int8_t rssi;
    uint8_t source_card;
};

bool is_duplicate(ReceivedPacket& new_pkt) {
    for (auto& existing : packet_cache) {
        if (existing.block_index == new_pkt.block_index &&
            existing.packet_index == new_pkt.packet_index) {
            // Duplicate - keep better RSSI
            if (new_pkt.rssi > existing.rssi) {
                existing = new_pkt;
            }
            return true;
        }
    }
    return false;
}
```

### FEC Decoding

```
┌────────────────────────────────────────────────┐
│          FEC Decoder Thread                    │
├────────────────────────────────────────────────┤
│                                                │
│  Received packets: [0,1,_,3,_,5,6,7,_,9,10,11] │
│                                                │
│  if (received >= K) {                          │
│    ┌────────────────────────────────┐         │
│    │  Reed-Solomon Decode           │         │
│    │  - Reconstruct missing packets │         │
│    │  - Packets 2, 4, 8 recovered   │         │
│    └──────────────┬─────────────────┘         │
│                   │                            │
│                   ▼                            │
│    ┌────────────────────────────────┐         │
│    │  Reassemble Application Data   │         │
│    │  - Extract payload              │         │
│    │  - Pass to decoder              │         │
│    └────────────────────────────────┘         │
│  } else {                                      │
│    // Block lost (< K packets)                 │
│    discard_block();                            │
│  }                                             │
│                                                │
└────────────────────────────────────────────────┘
```

### Frame Reassembly

```c
struct FrameAssembler {
    uint32_t current_frame_index = 0;
    std::vector<uint8_t> frame_buffer;
    std::map<uint8_t, bool> parts_received;

    bool process_video_packet(Air2Ground_Video_Packet& pkt) {
        if (pkt.frame_index != current_frame_index) {
            // New frame - reset
            current_frame_index = pkt.frame_index;
            frame_buffer.clear();
            parts_received.clear();
        }

        // Add part to buffer
        size_t offset = pkt.part_index * MTU;
        frame_buffer.insert(frame_buffer.begin() + offset,
                           pkt.data,
                           pkt.data + pkt.size);
        parts_received[pkt.part_index] = true;

        // Check if complete
        if (pkt.last_part) {
            if (all_parts_received()) {
                decode_jpeg_frame(frame_buffer);
                return true;
            } else {
                // Missing parts - drop frame
                return false;
            }
        }
        return false;
    }
};
```

### MJPEG Decoding

**TurboJPEG Library:**

```c
#include <turbojpeg.h>

tjhandle jpeg_decoder = tjInitDecompress();

void decode_frame(uint8_t* jpeg_data, size_t jpeg_size) {
    int width, height, subsample, colorspace;

    // Get image info
    tjDecompressHeader3(jpeg_decoder, jpeg_data, jpeg_size,
                       &width, &height, &subsample, &colorspace);

    // Allocate RGB buffer
    uint8_t* rgb_buffer = new uint8_t[width * height * 3];

    // Decompress
    auto start = high_resolution_clock::now();

    tjDecompress2(jpeg_decoder,
                 jpeg_data, jpeg_size,
                 rgb_buffer,
                 width, 0, height,
                 TJPF_RGB, TJFLAG_FASTDCT);

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    // Typical: 1-7ms depending on resolution

    upload_to_gpu(rgb_buffer, width, height);
    delete[] rgb_buffer;
}
```

### OpenGL Rendering

**Texture Upload:**

```c
GLuint video_texture;

void upload_to_gpu(uint8_t* rgb_data, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, video_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                width, height, 0,
                GL_RGB, GL_UNSIGNED_BYTE, rgb_data);
}
```

**Rendering:**

```c
void render_video() {
    // Full-screen quad
    glBindTexture(GL_TEXTURE_2D, video_texture);
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(-1, -1);
        glTexCoord2f(1, 0); glVertex2f( 1, -1);
        glTexCoord2f(1, 1); glVertex2f( 1,  1);
        glTexCoord2f(0, 1); glVertex2f(-1,  1);
    glEnd();

    // Overlay OSD
    render_osd();

    // Swap buffers
    eglSwapBuffers(display, surface);
}
```

### Ground Station DVR

**Recording Pipeline:**

```
Decoded JPEG Frame
       │
       ▼
┌──────────────────┐
│  Frame Queue     │
│  (Ring buffer)   │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  AVI Writer      │
│  (Separate thread│
└────────┬─────────┘
         │
         ▼
    SD Card / Disk
```

**AVI Writing:**

```c
void write_frame_to_avi(uint8_t* jpeg_data, size_t size) {
    // Write movi chunk
    write_fourcc("00dc");  // Compressed video
    write_uint32(size);     // Chunk size
    write_data(jpeg_data, size);

    // Add to index
    index_entries.push_back({
        .chunk_id = fourcc("00dc"),
        .flags = 0x10,  // Keyframe
        .offset = current_offset,
        .size = size
    });
}
```

## Performance Optimization

### IRAM Placement

**Critical functions in IRAM for speed:**

```c
IRAM_ATTR void camera_dma_handler();
IRAM_ATTR void fec_encode_block();
IRAM_ATTR void packet_injection_handler();
```

### Task Priorities

```c
// ESP32 FreeRTOS
Camera DMA Task:    Priority 24 (highest)
FEC Encoder Task:   Priority 23
WiFi TX Task:       Priority 22
WiFi RX Task:       Priority 21
Config Task:        Priority 10
LED Blink Task:     Priority 1 (lowest)
```

### Memory Management

**PSRAM vs DRAM:**

```c
// Fast access - DRAM
uint8_t packet_buffer[2048] __attribute__((section(".dram")));

// Large buffers - PSRAM
uint8_t sd_buffer[3*1024*1024] __attribute__((section(".psram")));
```

## Latency Optimization Techniques

1. **Zero-copy architecture** - DMA directly to WiFi queue
2. **Pipeline parallelism** - Capture while transmitting previous frame
3. **IRAM placement** - Critical code in fast memory
4. **Lock-free queues** - Minimize synchronization overhead
5. **Adaptive quality** - Prevent queue buildup
6. **Direct streaming** - No buffering between stages

## See Also

- [02_PROTOCOL_SPECIFICATION.md](02_PROTOCOL_SPECIFICATION.md) - Protocol details
- [05_FEC_IMPLEMENTATION.md](05_FEC_IMPLEMENTATION.md) - FEC algorithms
- [06_GROUND_STATION.md](06_GROUND_STATION.md) - GS architecture

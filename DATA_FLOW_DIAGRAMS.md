# Data Flow Diagrams - Visual Architecture Guide

**Project:** HX-ESP32-CAM-FPV
**Companion to:** SYSTEM_ARCHITECTURE.md
**Date:** 2025-11-22

---

## ESP32-P4 to ESP32-C5/C6 - SPI Communication

### Hardware Connection Diagram

```
┌─────────────────────────────────────────────────────────────────┐
│                    ESP32-P4 Evaluation Board                    │
│                                                                 │
│  ┌──────────────────┐              ┌──────────────────┐        │
│  │   ESP32-P4       │              │   ESP32-C6       │        │
│  │   Main SoC       │              │   Integrated     │        │
│  │                  │              │   on eval board  │        │
│  │  GPIO Pins:      │    SPI Bus   │                  │        │
│  │  ┌─────────────┐ │   ════════>  │  ┌─────────────┐ │        │
│  │  │ SPI Master  │ │              │  │ SPI Slave   │ │        │
│  │  │             │ │              │  │             │ │        │
│  │  │ CLK  (GPIO) ├─┼──────────────┼─>│ CLK         │ │        │
│  │  │ MOSI (GPIO) ├─┼──────────────┼─>│ MOSI        │ │        │
│  │  │ MISO (GPIO) ├─┼──────────────┼─<│ MISO        │ │        │
│  │  │ CS   (GPIO) ├─┼──────────────┼─>│ CS          │ │        │
│  │  └─────────────┘ │              │  └─────────────┘ │        │
│  │                  │              │                  │        │
│  └──────────────────┘              └──────────────────┘        │
│                                                                 │
│  Ground connection: Common GND between both chips              │
│  Clock speed: 80 MHz                                           │
│  Mode: SPI Mode 0 (CPOL=0, CPHA=0)                            │
│  DMA: Enabled on both sides                                    │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘

Note: ESP32-P4 eval board has ESP32-C6 integrated for WiFi
      Alternative: External ESP32-C5 via SPI on separate board
```

### SPI Transfer Sequence Diagram

```
ESP32-P4                                ESP32-C6
(Master)                                (Slave)
    │                                      │
    │  1. H.264 encoder produces NAL      │
    │     unit (6KB typical)               │
    │                                      │
    │  2. Prepare SPI transaction         │
    │     - Allocate DMA buffer            │
    │     - Build packet header            │
    │     - Calculate CRC16                │
    │                                      │
    │  3. Assert CS (chip select)         │
    ├─────── CS LOW ────────────────────>│
    │                                      │
    │  4. Transfer header (11 bytes)      │
    ├─────── [SYNC|TYPE|SIZE|SEQ] ──────>│
    │                                      │  Parse header
    │                                      │  Validate sync
    │                                      │  Prepare RX buffer
    │                                      │
    │  5. Transfer payload (6144 bytes)   │
    ├─────── H.264 NAL Unit ─────────────>│
    │        [DMA transfer]                │  DMA receive
    │        ~77 µs @ 80 MHz               │  to PSRAM
    │                                      │
    │  6. Transfer CRC (2 bytes)          │
    ├─────── [CRC16] ────────────────────>│
    │                                      │  Validate CRC
    │                                      │  Queue for FEC
    │                                      │
    │  7. Deassert CS                     │
    ├─────── CS HIGH ───────────────────>│
    │                                      │
    │  8. Wait for next NAL unit          │
    │                                      │
    │                                      │  Forward to FEC encoder
    │                                      │  in separate task
    │                                      │
    ▼                                      ▼

Total SPI transaction time: ~0.8 ms
  - Header: 11 bytes = 1.1 µs
  - Payload: 6144 bytes = 768 µs
  - CRC: 2 bytes = 0.25 µs
  - CS overhead: ~10 µs
  ─────────────────────────────
  Total: ~779 µs ≈ 0.8 ms
```

### SPI Packet Format (Detailed)

```
┌────────────────────────────────────────────────────────────────┐
│  SPI Packet Structure (ESP32-P4 → ESP32-C6)                   │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  Byte Offset  Field        Size    Value/Description          │
│  ══════════════════════════════════════════════════════════    │
│                                                                │
│  0-1          SYNC         2B      0xA5 0x5A                  │
│                                    (Frame synchronization)     │
│                                                                │
│  2            TYPE         1B      0x01 = H.264 NAL unit      │
│                                    0x02 = Telemetry data      │
│                                    0x03 = Control command     │
│                                                                │
│  3-6          SIZE         4B      Payload size (little-endian)│
│                                    Does NOT include header/CRC │
│                                                                │
│  7-8          SEQUENCE     2B      Packet sequence (0-65535)  │
│                                    Wraps around                │
│                                                                │
│  9-10         FLAGS        2B      Bit flags:                 │
│                                    [15:8] = NAL unit type      │
│                                    [7]    = Keyframe (IDR)     │
│                                    [6]    = Priority           │
│                                    [5:0]  = Reserved           │
│                                                                │
│  11...        PAYLOAD      N       H.264 NAL unit data        │
│               (variable)           Typical: 1KB - 8KB          │
│                                    Max: 16KB per packet        │
│                                                                │
│  11+N...      CRC16        2B      CCITT-FALSE checksum       │
│               11+N+1                Covers: TYPE to PAYLOAD    │
│                                    (excludes SYNC and CRC)     │
│                                                                │
└────────────────────────────────────────────────────────────────┘

Total packet size: 13 + N bytes
Maximum packet: 13 + 16384 = 16397 bytes
Typical packet: 13 + 6144 = 6157 bytes

CRC Calculation:
  Polynomial: 0x1021 (CRC-16-CCITT)
  Init value: 0xFFFF
  XOR out: 0x0000
  Covers: Bytes 2 through 10+N (TYPE through end of PAYLOAD)
```

---

## FEC Encoding on ESP32-C5/C6

### Reed-Solomon Encoding Process

```
┌────────────────────────────────────────────────────────────────────┐
│         Reed-Solomon (6,12) Encoding Pipeline                      │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  STEP 1: Receive H.264 NAL unit from SPI                          │
│  ─────────────────────────────────────────────────                │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────┐     │
│  │  H.264 NAL Unit: 6144 bytes                              │     │
│  │  (Example: P-frame slice from 1080p60 stream)            │     │
│  └──────────────────────────────────────────────────────────┘     │
│                           │                                        │
│                           │                                        │
│  STEP 2: Divide into K=6 equal data blocks                        │
│  ────────────────────────────────────────────                     │
│                           │                                        │
│                           ▼                                        │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┬────────┐│
│  │   D0     │   D1     │   D2     │   D3     │   D4     │   D5   ││
│  │ 1024 B   │ 1024 B   │ 1024 B   │ 1024 B   │ 1024 B   │ 1024 B ││
│  └──────────┴──────────┴──────────┴──────────┴──────────┴────────┘│
│       │          │          │          │          │          │     │
│       └──────────┴──────────┴──────────┴──────────┴──────────┘     │
│                           │                                        │
│                           │                                        │
│  STEP 3: Reed-Solomon encoding (Galois Field GF(256))             │
│  ───────────────────────────────────────────────────────          │
│                           │                                        │
│                           ▼                                        │
│                                                                    │
│  Generate N-K=6 parity blocks using generator polynomial:         │
│                                                                    │
│     g(x) = (x-α⁰)(x-α¹)(x-α²)(x-α³)(x-α⁴)(x-α⁵)                   │
│                                                                    │
│  Where α is primitive element in GF(256)                          │
│                                                                    │
│  For each byte position i (0 to 1023):                            │
│    - Take bytes d[0][i], d[1][i], ..., d[5][i]                    │
│    - Compute parity bytes p[0][i], ..., p[5][i]                   │
│    - Using systematic RS encoding                                 │
│                           │                                        │
│                           ▼                                        │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┬────────┐│
│  │   P0     │   P1     │   P2     │   P3     │   P4     │   P5   ││
│  │ 1024 B   │ 1024 B   │ 1024 B   │ 1024 B   │ 1024 B   │ 1024 B ││
│  └──────────┴──────────┴──────────┴──────────┴──────────┴────────┘│
│                                                                    │
│  STEP 4: Output all 12 blocks (data + parity)                     │
│  ──────────────────────────────────────────────                   │
│                                                                    │
│  Data blocks (sent first):                                        │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┬────────┐│
│  │   D0     │   D1     │   D2     │   D3     │   D4     │   D5   ││
│  └──────────┴──────────┴──────────┴──────────┴──────────┴────────┘│
│                                                                    │
│  Parity blocks (sent after):                                      │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┬────────┐│
│  │   P0     │   P1     │   P2     │   P3     │   P4     │   P5   ││
│  └──────────┴──────────┴──────────┴──────────┴──────────┴────────┘│
│                                                                    │
│  Each block transmitted as separate WiFi packet                   │
│  Total: 12 WiFi packets per original NAL unit                     │
│                                                                    │
│  PROPERTIES:                                                       │
│  ───────────                                                       │
│  • Can recover original data with ANY 6 out of 12 blocks          │
│  • 100% overhead (doubles data size)                              │
│  • Can tolerate 50% packet loss                                   │
│  • Encoding time: ~0.5ms (optimized GF arithmetic)                │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### FEC Block WiFi Encapsulation

```
┌────────────────────────────────────────────────────────────────────┐
│  WiFi Packet Structure (per FEC block)                            │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  802.11 MAC Frame:                                                 │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │ Frame Control │ Duration │ Addr1 │ Addr2 │ Addr3 │ Seq Ctl  │ │
│  │     2B        │    2B    │  6B   │  6B   │  6B   │   2B     │ │
│  └──────────────────────────────────────────────────────────────┘ │
│  │                                                                │ │
│  │ QoS Control │ HT Control │ LLC/SNAP Header                    │ │
│  │     2B      │     4B     │        8B                          │ │
│  └────────────────────────────────────────────────────────────── │ │
│                                                                    │
│  Custom Payload (FEC block):                                      │
│  ┌──────────────────────────────────────────────────────────────┐ │
│  │                                                              │ │
│  │  Byte 0-1:   Magic: 0xFEC0                                   │ │
│  │  Byte 2:     Block index (0-11)                              │ │
│  │  Byte 3:     Block type:                                     │ │
│  │              0x00 = Data block (D0-D5)                       │ │
│  │              0x01 = Parity block (P0-P5)                     │ │
│  │  Byte 4-5:   NAL sequence number                             │ │
│  │  Byte 6-7:   Total NAL size (before FEC)                     │ │
│  │  Byte 8-9:   Timestamp (low 16 bits of µs counter)           │ │
│  │  Byte 10-11: Reserved                                        │ │
│  │                                                              │ │
│  │  Byte 12-1035: FEC block data (1024 bytes)                   │ │
│  │                                                              │ │
│  │  Byte 1036-1039: CRC32 (covers bytes 0-1035)                 │ │
│  │                                                              │ │
│  └──────────────────────────────────────────────────────────────┘ │
│                                                                    │
│  Total payload: 1040 bytes                                         │
│  802.11 frame size: ~1082 bytes (with headers)                    │
│  On-air time @ MCS7 (65 Mbps): ~133 µs per packet                 │
│  Total for 12 packets: ~1.6 ms                                    │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

## ESP32-S3 WiFi Monitor Mode Reception

### WiFi Packet Capture Flow

```
┌────────────────────────────────────────────────────────────────────┐
│           ESP32-S3 WiFi Reception & Processing                     │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  LAYER 1: WiFi Hardware (PHY/MAC)                                  │
│  ══════════════════════════════════                                │
│                                                                    │
│     WiFi antenna receives 802.11 frames                            │
│            │                                                       │
│            ▼                                                       │
│     ┌──────────────────────────────┐                              │
│     │  WiFi PHY                    │                              │
│     │  - Demodulate signal         │                              │
│     │  - Extract frame             │                              │
│     │  - Check FCS (frame check)   │                              │
│     └──────────┬───────────────────┘                              │
│                │                                                   │
│                │ Valid frame                                       │
│                ▼                                                   │
│     ┌──────────────────────────────┐                              │
│     │  WiFi MAC                    │                              │
│     │  - Parse 802.11 header       │                              │
│     │  - Check destination addr    │                              │
│     │  - In monitor mode: accept   │                              │
│     │    all frames on channel     │                              │
│     └──────────┬───────────────────┘                              │
│                │                                                   │
│                │ Frame accepted                                    │
│                ▼                                                   │
│                                                                    │
│  LAYER 2: WiFi Driver ISR                                          │
│  ═══════════════════════════                                       │
│                │                                                   │
│                ▼                                                   │
│     ┌──────────────────────────────────────┐                      │
│     │  wifi_promiscuous_rx_cb()            │  ISR Context         │
│     │  (Interrupt Service Routine)         │  Must be fast!       │
│     │                                      │                      │
│     │  1. Check frame type                 │                      │
│     │     - Only data frames              │                      │
│     │                                      │                      │
│     │  2. Quick MAC filter                │                      │
│     │     - Check transmitter MAC         │                      │
│     │     - Match against ESP32-C6 MAC    │                      │
│     │                                      │                      │
│     │  3. Extract RSSI                    │                      │
│     │     - Signal strength               │                      │
│     │     - Noise floor                   │                      │
│     │                                      │                      │
│     │  4. Copy to ring buffer             │                      │
│     │     - ISR-safe lock-free queue      │                      │
│     │     - No malloc in ISR!             │                      │
│     │     - Pre-allocated buffers         │                      │
│     │                                      │                      │
│     └──────────┬───────────────────────────┘                      │
│                │                                                   │
│                │ Queued packet                                     │
│                ▼                                                   │
│                                                                    │
│  LAYER 3: Packet Handler Task                                      │
│  ════════════════════════════════                                  │
│                │                                                   │
│                ▼                                                   │
│     ┌──────────────────────────────────────┐                      │
│     │  packet_handler_task()               │  FreeRTOS Task       │
│     │  (Separate task, not ISR)            │  Priority: 5         │
│     │                                      │                      │
│     │  Loop:                               │                      │
│     │    1. Wait for queue item            │                      │
│     │       (blocks on semaphore)          │                      │
│     │                                      │                      │
│     │    2. Parse 802.11 frame             │                      │
│     │       - Skip MAC header (24B)        │                      │
│     │       - Skip LLC/SNAP (8B)           │                      │
│     │       - Extract custom payload       │                      │
│     │                                      │                      │
│     │    3. Validate FEC header            │                      │
│     │       - Check magic (0xFEC0)         │                      │
│     │       - Verify CRC32                 │                      │
│     │       - Extract block index          │                      │
│     │                                      │                      │
│     │    4. Forward to FEC decoder         │                      │
│     │       - Queue FEC block              │                      │
│     │       - Update statistics            │                      │
│     │                                      │                      │
│     │    5. Free buffer                    │                      │
│     │       - Return to pool               │                      │
│     │                                      │                      │
│     └──────────┬───────────────────────────┘                      │
│                │                                                   │
│                │ FEC blocks queued                                 │
│                ▼                                                   │
│                                                                    │
│  LAYER 4: FEC Decoder Task                                         │
│  ═══════════════════════════                                       │
│                │                                                   │
│                ▼                                                   │
│     ┌──────────────────────────────────────┐                      │
│     │  fec_decoder_task()                  │  FreeRTOS Task       │
│     │                                      │  Priority: 6         │
│     │  (See FEC Decoding section)          │  (Higher priority)   │
│     │                                      │                      │
│     └──────────┬───────────────────────────┘                      │
│                │                                                   │
│                │ Decoded NAL units                                 │
│                ▼                                                   │
│                                                                    │
│  LAYER 5: USB Streamer Task                                        │
│  ══════════════════════════════                                    │
│                │                                                   │
│                ▼                                                   │
│     ┌──────────────────────────────────────┐                      │
│     │  usb_streamer_task()                 │  FreeRTOS Task       │
│     │  (See USB Streaming section)         │  Priority: 7         │
│     │                                      │  (Highest)           │
│     └──────────────────────────────────────┘                      │
│                │                                                   │
│                │ USB bulk transfers                                │
│                ▼                                                   │
│            Android device                                          │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

### Monitor Mode Configuration

```
ESP32-S3 WiFi Monitor Mode Setup:

┌────────────────────────────────────────────────────────────┐
│  wifi_rx_init() Configuration                             │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  wifi_init_config_t wifi_config = {                       │
│      .nvs_enable = 0,              // No NVS needed       │
│      .nano_enable = 0,              // Full              │
│  };                                                        │
│                                                            │
│  wifi_mode_set(WIFI_MODE_STA);     // Station mode        │
│                                                            │
│  wifi_promiscuous_enable(true);    // Enable monitor      │
│                                                            │
│  wifi_set_promiscuous_rx_cb(       // Set callback        │
│      wifi_rx_callback              // ISR function        │
│  );                                                        │
│                                                            │
│  wifi_set_channel(                 // Set channel         │
│      channel,                      // e.g., 149 (5GHz)    │
│      WIFI_SECOND_CHAN_ABOVE        // HT40+ for 5GHz      │
│  );                                                        │
│                                                            │
│  Filter configuration:                                     │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ wifi_promiscuous_filter_t filter = {                │ │
│  │     .filter_mask =                                  │ │
│  │         WIFI_PROMIS_FILTER_MASK_DATA |              │ │
│  │         WIFI_PROMIS_FILTER_MASK_MISC,               │ │
│  │ };                                                  │ │
│  │ wifi_set_promiscuous_filter(&filter);               │ │
│  └──────────────────────────────────────────────────────┘ │
│                                                            │
│  Channel selection:                                        │
│  • 2.4 GHz: Channels 1-14 (HT20/HT40)                     │
│  • 5 GHz: Channels 36-165 (HT20/HT40/HT80)                │
│  • Must match transmitter exactly                         │
│  • Can implement channel hopping for search               │
│                                                            │
│  MAC filtering:                                            │
│  • Hardware filtering not available in monitor mode       │
│  • Software filtering in ISR callback                     │
│  • Compare against known transmitter MAC                  │
│  • Accept only matching packets                           │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## FEC Decoding on ESP32-S3

### Block Assembly and Decoding

```
┌────────────────────────────────────────────────────────────────────┐
│        FEC Decoder State Machine (per NAL unit)                    │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  STATE 1: WAITING FOR BLOCKS                                       │
│  ════════════════════════════                                      │
│                                                                    │
│  Block bitmap (12 blocks):                                         │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐               │
│  │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │  (empty)       │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘               │
│    0   1   2   3   4   5   6   7   8   9  10  11                  │
│                                                                    │
│  Receive block 0 (data) ───────>                                   │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐               │
│  │ 1 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │  (1/12)        │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘               │
│                                                                    │
│  Receive block 1 (data) ───────>                                   │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐               │
│  │ 1 │ 1 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │  (2/12)        │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘               │
│                                                                    │
│  [Block 2 LOST - packet not received]                             │
│                                                                    │
│  Receive block 3 (data) ───────>                                   │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐               │
│  │ 1 │ 1 │ 0 │ 1 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │ 0 │  (3/12)        │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘               │
│                                                                    │
│  [Block 4 LOST]                                                    │
│                                                                    │
│  Receive block 5, 6, 7 ───────>                                    │
│  ┌───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┐               │
│  │ 1 │ 1 │ 0 │ 1 │ 0 │ 1 │ 1 │ 1 │ 0 │ 0 │ 0 │ 0 │  (6/12) ✓      │
│  └───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┘               │
│                                                                    │
│  THRESHOLD REACHED! Have 6 blocks (minimum required)              │
│                                                                    │
│  ────────────────────────────────────────────────────────────     │
│                                                                    │
│  STATE 2: DECODING                                                 │
│  ═══════════════════                                               │
│                                                                    │
│  Received blocks:                                                  │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┬────────┐│
│  │    D0    │    D1    │          │    D3    │          │   D5   ││
│  │  (1024B) │  (1024B) │   LOST   │  (1024B) │   LOST   │(1024B) ││
│  └──────────┴──────────┴──────────┴──────────┴──────────┴────────┘│
│  ┌──────────┬──────────┐                                          │
│  │    P0    │    P1    │                                          │
│  │  (1024B) │  (1024B) │                                          │
│  └──────────┴──────────┘                                          │
│                                                                    │
│  Setup decoding matrix:                                            │
│  ┌────────────────────────────────────────────────────────┐       │
│  │  Decoding matrix (6×6) in GF(256):                    │       │
│  │                                                        │       │
│  │  Each received block gives one equation:              │       │
│  │    Block 0 (D0): [1, 0, 0, 0, 0, 0]                   │       │
│  │    Block 1 (D1): [0, 1, 0, 0, 0, 0]                   │       │
│  │    Block 3 (D3): [0, 0, 0, 1, 0, 0]                   │       │
│  │    Block 5 (D5): [0, 0, 0, 0, 0, 1]                   │       │
│  │    Block 6 (P0): [α⁰, α¹, α², α³, α⁴, α⁵]             │       │
│  │    Block 7 (P1): [β⁰, β¹, β², β³, β⁴, β⁵]             │       │
│  │                                                        │       │
│  │  Solve for missing blocks D2 and D4                   │       │
│  │  using Gaussian elimination in GF(256)                │       │
│  │                                                        │       │
│  └────────────────────────────────────────────────────────┘       │
│                           │                                        │
│                           │ Matrix inversion                       │
│                           │ Galois field multiply/add              │
│                           ▼                                        │
│  Recovered blocks:                                                 │
│  ┌──────────┬──────────┐                                          │
│  │    D2    │    D4    │                                          │
│  │  (1024B) │  (1024B) │  ← Recovered from parity!                │
│  └──────────┴──────────┘                                          │
│                                                                    │
│  ────────────────────────────────────────────────────────────     │
│                                                                    │
│  STATE 3: REASSEMBLY                                               │
│  ══════════════════════                                            │
│                                                                    │
│  Concatenate all 6 data blocks in order:                          │
│  ┌──────────┬──────────┬──────────┬──────────┬──────────┬────────┐│
│  │    D0    │    D1    │    D2    │    D3    │    D4    │   D5   ││
│  │  (1024B) │  (1024B) │  (1024B) │  (1024B) │  (1024B) │(1024B) ││
│  └──────────┴──────────┴──────────┴──────────┴──────────┴────────┘│
│                           │                                        │
│                           │ memcpy to output buffer                │
│                           ▼                                        │
│  ┌─────────────────────────────────────────────────────────┐      │
│  │  Complete H.264 NAL Unit: 6144 bytes                    │      │
│  │  Ready for USB transmission                             │      │
│  └─────────────────────────────────────────────────────────┘      │
│                           │                                        │
│                           │ Callback to usb_streamer               │
│                           ▼                                        │
│                   Queue for USB transmission                       │
│                                                                    │
│  STATISTICS:                                                       │
│  • Blocks received: 6/12 (50%)                                     │
│  • Packet loss: 6/12 (50%)                                         │
│  • Decoding time: ~0.8 ms                                          │
│  • Success: ✓                                                      │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

## USB OTG Data Transfer

### ESP32-S3 USB Bulk Transfer Flow

```
┌────────────────────────────────────────────────────────────────────┐
│              USB Bulk Transfer Timeline                            │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  ESP32-S3                                  Android Host            │
│  (Device)                                  (Host)                  │
│      │                                         │                   │
│      │  1. FEC decoder produces NAL unit       │                   │
│      │     (6144 bytes)                        │                   │
│      │                                         │                   │
│      │  2. Build USB packet header             │                   │
│      │     Add SYNC, TYPE, SIZE, SEQ, TS       │                   │
│      │     Calculate CRC16                     │                   │
│      │                                         │                   │
│      │  3. Fragment into USB packets           │                   │
│      │     Max payload per packet: 492 bytes   │                   │
│      │     Total packets: 13                   │                   │
│      │                                         │                   │
│      │  4. Queue USB bulk transfer #1          │                   │
│      ├────── IN Token ──────────────────────> │                   │
│      │                                         │  USB host polls   │
│      ├────── DATA [512B] ──────────────────> │  endpoint         │
│      │       [HDR + 492B payload + CRC]        │                   │
│      │                                         │  Receive & ACK    │
│      │ <────── ACK ──────────────────────────┤                   │
│      │                                         │                   │
│      │  ~10 µs for 512B @ 480 Mbps             │                   │
│      │                                         │                   │
│      │  5. Queue USB bulk transfer #2          │                   │
│      ├────── IN Token ──────────────────────> │                   │
│      ├────── DATA [512B] ──────────────────> │                   │
│      │ <────── ACK ──────────────────────────┤                   │
│      │                                         │                   │
│      │  ... (repeat for packets 3-12)          │                   │
│      │                                         │                   │
│      │  17. Queue USB bulk transfer #13        │                   │
│      │      (last packet, partial)             │                   │
│      ├────── IN Token ──────────────────────> │                   │
│      ├────── DATA [258B] ──────────────────> │                   │
│      │       [HDR + 236B payload + CRC]        │                   │
│      │ <────── ACK ──────────────────────────┤                   │
│      │                                         │                   │
│      │  Total transfer time: ~0.7 ms           │                   │
│      │  (13 packets × ~54 µs each)             │                   │
│      │                                         │                   │
│      │                                         │  18. USB driver   │
│      │                                         │      delivers     │
│      │                                         │      to app       │
│      │                                         │      ↓            │
│      │                                         │  bulkTransfer()   │
│      │                                         │  returns          │
│      │                                         │  bytesRead = 6656 │
│      │                                         │                   │
│      ▼                                         ▼                   │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘

USB Bandwidth Utilization:
  Data transferred: 6656 bytes (6144 + 512 overhead)
  Time taken: 0.7 ms
  Effective rate: 6656 bytes ÷ 0.0007 s = 9.5 MB/s = 76 Mbps

  At 60 FPS:
    Per second: 6656 bytes × 60 = 399 KB/s = 3.2 Mbps
    USB utilization: 3.2 Mbps ÷ 480 Mbps = 0.67%

  Very low utilization! Plenty of headroom.
```

---

## Android Processing Pipeline

### Complete Android Data Flow

```
┌────────────────────────────────────────────────────────────────────┐
│          Android Application Data Flow (Detailed)                  │
├────────────────────────────────────────────────────────────────────┤
│                                                                    │
│  THREAD 1: USB Read Thread                                         │
│  ══════════════════════════                                        │
│                                                                    │
│  ┌──────────────────────────────────────────────────────┐         │
│  │  UsbCommunicationManager.readLoop()                  │         │
│  │                                                      │         │
│  │  while (isConnected) {                               │         │
│  │      buffer = ByteArray(16384)                       │         │
│  │                                                      │         │
│  │      bytesRead = connection.bulkTransfer(            │         │
│  │          endpoint = 0x81,  // IN endpoint            │         │
│  │          buffer,                                     │         │
│  │          length = 16384,                             │         │
│  │          timeout = 100ms                             │         │
│  │      )                                               │         │
│  │                                                      │         │
│  │      if (bytesRead > 0) {                            │         │
│  │          parser.parse(buffer, bytesRead)             │         │
│  │      }                                               │         │
│  │  }                                                   │         │
│  └─────────────────────┬────────────────────────────────┘         │
│                        │                                           │
│                        │ Raw USB data                              │
│                        ▼                                           │
│                                                                    │
│  COMPONENT: Protocol Parser                                        │
│  ══════════════════════════════                                    │
│                                                                    │
│  ┌──────────────────────────────────────────────────────┐         │
│  │  UsbProtocolParser                                   │         │
│  │                                                      │         │
│  │  parse(data: ByteArray, length: Int) {               │         │
│  │      var offset = 0                                  │         │
│  │                                                      │         │
│  │      while (offset < length) {                       │         │
│  │          // 1. Find sync marker                      │         │
│  │          while (offset < length - 1) {               │         │
│  │              if (data[offset] == 0xA5 &&             │         │
│  │                  data[offset+1] == 0x5A) {           │         │
│  │                  break                               │         │
│  │              }                                       │         │
│  │              offset++                                │         │
│  │          }                                           │         │
│  │                                                      │         │
│  │          // 2. Extract header                        │         │
│  │          if (offset + 20 > length) {                 │         │
│  │              savePartial()  // Incomplete            │         │
│  │              break                                   │         │
│  │          }                                           │         │
│  │                                                      │         │
│  │          type = data[offset + 2]                     │         │
│  │          size = readUInt32(data, offset + 4)         │         │
│  │          seq = readUInt16(data, offset + 8)          │         │
│  │          timestamp = readUInt64(data, offset + 10)   │         │
│  │                                                      │         │
│  │          // 3. Extract payload                       │         │
│  │          payloadStart = offset + 20                  │         │
│  │          payloadEnd = payloadStart + size - 2        │         │
│  │                                                      │         │
│  │          if (payloadEnd > length) {                  │         │
│  │              savePartial()                           │         │
│  │              break                                   │         │
│  │          }                                           │         │
│  │                                                      │         │
│  │          payload = data.copyOfRange(                 │         │
│  │              payloadStart, payloadEnd                │         │
│  │          )                                           │         │
│  │                                                      │         │
│  │          // 4. Validate CRC                          │         │
│  │          crcReceived = readUInt16(data, payloadEnd)  │         │
│  │          crcCalculated = calculateCRC16(             │         │
│  │              data, offset + 2, payloadEnd            │         │
│  │          )                                           │         │
│  │                                                      │         │
│  │          if (crcReceived != crcCalculated) {         │         │
│  │              dropPacket()                            │         │
│  │              offset = payloadEnd + 2                 │         │
│  │              continue                                │         │
│  │          }                                           │         │
│  │                                                      │         │
│  │          // 5. Route by type                         │         │
│  │          when (type) {                               │         │
│  │              0x01 -> videoCallback(                  │         │
│  │                  payload, seq, timestamp             │         │
│  │              )                                       │         │
│  │              0x02 -> telemetryCallback(payload)      │         │
│  │              0x03 -> osdCallback(payload)            │         │
│  │          }                                           │         │
│  │                                                      │         │
│  │          offset = payloadEnd + 2                     │         │
│  │      }                                               │         │
│  │  }                                                   │         │
│  └─────────────────────┬────────────────────────────────┘         │
│                        │                                           │
│                        │ Video packets (type 0x01)                 │
│                        ▼                                           │
│                                                                    │
│  COMPONENT: Frame Assembler                                        │
│  ═══════════════════════════                                       │
│                                                                    │
│  ┌──────────────────────────────────────────────────────┐         │
│  │  UsbFrameAssembler                                   │         │
│  │                                                      │         │
│  │  // State: Map<sequence, FrameBuilder>               │         │
│  │  private val frames = ConcurrentHashMap<Int,         │         │
│  │                                         FrameBuilder>│         │
│  │                                                      │         │
│  │  addFragment(payload, seq, timestamp) {              │         │
│  │      frameBuilder = frames.getOrPut(seq) {           │         │
│  │          FrameBuilder(seq, timestamp)                │         │
│  │      }                                               │         │
│  │                                                      │         │
│  │      frameBuilder.addFragment(payload)               │         │
│  │                                                      │         │
│  │      if (frameBuilder.isComplete()) {                │         │
│  │          completeNalUnit = frameBuilder.assemble()   │         │
│  │          frames.remove(seq)                          │         │
│  │          decoderCallback(completeNalUnit, timestamp) │         │
│  │      }                                               │         │
│  │  }                                                   │         │
│  │                                                      │         │
│  │  // Timeout cleanup (separate coroutine)             │         │
│  │  launch {                                            │         │
│  │      while (true) {                                  │         │
│  │          delay(100)                                  │         │
│  │          now = System.currentTimeMillis()            │         │
│  │          frames.entries.removeIf {                   │         │
│  │              (now - it.value.startTime) > 100        │         │
│  │          }                                           │         │
│  │      }                                               │         │
│  │  }                                                   │         │
│  └─────────────────────┬────────────────────────────────┘         │
│                        │                                           │
│                        │ Complete NAL units                        │
│                        ▼                                           │
│                                                                    │
│  THREAD 2: Decoder Thread                                          │
│  ═══════════════════════                                           │
│                                                                    │
│  ┌──────────────────────────────────────────────────────┐         │
│  │  H264Decoder                                         │         │
│  │                                                      │         │
│  │  feedNalUnit(nalData, timestampUs) {                 │         │
│  │      // 1. Get input buffer                          │         │
│  │      val inputIndex = decoder.dequeueInputBuffer(    │         │
│  │          timeoutUs = 10000                           │         │
│  │      )                                               │         │
│  │                                                      │         │
│  │      if (inputIndex >= 0) {                          │         │
│  │          // 2. Fill input buffer                     │         │
│  │          val inputBuffer = decoder.getInputBuffer(   │         │
│  │              inputIndex                              │         │
│  │          )                                           │         │
│  │          inputBuffer.clear()                         │         │
│  │          inputBuffer.put(nalData)                    │         │
│  │                                                      │         │
│  │          // 3. Queue for decoding                    │         │
│  │          decoder.queueInputBuffer(                   │         │
│  │              index = inputIndex,                     │         │
│  │              offset = 0,                             │         │
│  │              size = nalData.size,                    │         │
│  │              presentationTimeUs = timestampUs,       │         │
│  │              flags = 0                               │         │
│  │          )                                           │         │
│  │      }                                               │         │
│  │                                                      │         │
│  │      // 4. Immediately try to get output             │         │
│  │      //    (low-latency mode)                        │         │
│  │      val bufferInfo = MediaCodec.BufferInfo()        │         │
│  │      val outputIndex = decoder.dequeueOutputBuffer(  │         │
│  │          bufferInfo,                                 │         │
│  │          timeoutUs = 0  // Non-blocking              │         │
│  │      )                                               │         │
│  │                                                      │         │
│  │      if (outputIndex >= 0) {                         │         │
│  │          // 5. Release to surface (renders)          │         │
│  │          decoder.releaseOutputBuffer(                │         │
│  │              outputIndex,                            │         │
│  │              render = true  // Render to surface     │         │
│  │          )                                           │         │
│  │                                                      │         │
│  │          // 6. Update statistics                     │         │
│  │          updateStats(bufferInfo)                     │         │
│  │      }                                               │         │
│  │  }                                                   │         │
│  └─────────────────────┬────────────────────────────────┘         │
│                        │                                           │
│                        │ Decoded frames rendered to Surface        │
│                        ▼                                           │
│                                                                    │
│  UI THREAD: Rendering                                              │
│  ═══════════════════                                               │
│                                                                    │
│  ┌──────────────────────────────────────────────────────┐         │
│  │  SurfaceView (GPU rendering)                         │         │
│  │                                                      │         │
│  │  ┌──────────────────────────────────────────┐       │         │
│  │  │  Video Surface                           │       │         │
│  │  │  - Receives frames from MediaCodec       │       │         │
│  │  │  - GPU compositing                       │       │         │
│  │  │  - Hardware overlay                      │       │         │
│  │  └──────────────────────────────────────────┘       │         │
│  │              ↓                                       │         │
│  │  Composited with OSD overlay (Canvas layer)         │         │
│  │              ↓                                       │         │
│  │  ┌──────────────────────────────────────────┐       │         │
│  │  │  Display on screen                       │       │         │
│  │  │  - 60 Hz refresh (or device max)         │       │         │
│  │  └──────────────────────────────────────────┘       │         │
│  └──────────────────────────────────────────────────────┘         │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

*End of Data Flow Diagrams*
*See SYSTEM_ARCHITECTURE.md for detailed specifications*

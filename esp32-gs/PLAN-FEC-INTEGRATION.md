# FEC Integration Plan for ESP32-GS

## Overview

Integrate Reed-Solomon FEC decoding from `components/common/` into `esp32-gs` for robust packet recovery. FEC will be:
- **Enabled** for USB NCM access (robust, low-latency wired connection)
- **Disabled** for WiFi AP access (simpler, optional wireless fallback)

## Current State Analysis

### Existing FEC Implementation (`components/common/`)
- **Algorithm**: Reed-Solomon over GF(2^8) with Vandermonde matrix
- **Default Config**: k=6 (data packets), n=12 (total packets), 50% redundancy
- **Key Files**:
  - `fec.h/cpp` - Core GF arithmetic and RS encode/decode
  - `fec_codec.h/cpp` - High-level encoder/decoder with threading
  - `packets.h` - Packet header structures

### Current esp32-gs Implementation
- **packet_rx.c**: Receives packets, skips FEC packets (index >= k)
- **frame_buffer.c**: Assembles video frame parts
- **fpv_gs.h**: Header structures matching air unit protocol

### Gap Analysis
1. No FEC decoding - lost packets = lost frame parts
2. No block-level packet buffering
3. No GF arithmetic code ported
4. No runtime FEC enable/disable based on access method

---

## Implementation Plan

### Phase 1: Port Core FEC Library (C version)

#### 1.1 Create `components/fec/` ESP-IDF Component

```
esp32-gs/
└── components/
    └── fec/
        ├── CMakeLists.txt
        ├── Kconfig
        ├── include/
        │   └── fec.h
        └── src/
            └── fec.c
```

**Files to create:**

1. **`components/fec/CMakeLists.txt`**
```cmake
idf_component_register(
    SRCS "src/fec.c"
    INCLUDE_DIRS "include"
    REQUIRES "esp_common"
)
```

2. **`components/fec/Kconfig`**
```kconfig
menu "FEC Configuration"
    config FEC_MAX_K
        int "Maximum K value (data packets)"
        default 16
        range 1 32

    config FEC_MAX_N
        int "Maximum N value (total packets)"
        default 32
        range 2 64

    config FEC_USE_PSRAM
        bool "Use PSRAM for GF multiplication table"
        default y
        help
            Store 64KB GF multiplication table in PSRAM if available
endmenu
```

3. **`components/fec/include/fec.h`** - Port from C++ to C:
   - Remove C++ features (classes, templates)
   - Use static allocation where possible
   - Key structures:
     ```c
     typedef struct {
         uint8_t k;           // Data packets needed
         uint8_t n;           // Total packets
         uint8_t *enc_matrix; // k*n encoding matrix
     } fec_t;

     // Core functions
     fec_t *fec_new(uint8_t k, uint8_t n);
     void fec_free(fec_t *fec);
     void fec_encode_block(const fec_t *fec, const uint8_t **src,
                           uint8_t *dst, size_t sz, uint8_t fec_idx);
     int fec_decode(const fec_t *fec, uint8_t **pkts,
                    const uint8_t *indices, size_t sz);
     ```

4. **`components/fec/src/fec.c`** - Core implementation:
   - GF(2^8) arithmetic with primitive polynomial "101110001"
   - Precomputed tables: `gf_exp[510]`, `gf_log[256]`, `gf_mul_table[256][256]`
   - Vandermonde matrix generation
   - `addmul()` - GF multiply-accumulate (core operation)
   - `invert_mat()` - Matrix inversion for decoding

**Memory Considerations:**
- GF multiplication table: 64KB (store in PSRAM)
- Encoding matrix: k*n bytes (~192 bytes for 6x12)
- Per-block buffers: n * MTU bytes (~18KB for 12 * 1500)

---

### Phase 2: Create FEC Decoder Module

#### 2.1 Create `main/fec_decoder.h`

```c
#ifndef FEC_DECODER_H
#define FEC_DECODER_H

#include "esp_err.h"
#include "fpv_gs.h"

// Decoder configuration
typedef struct {
    uint8_t coding_k;        // Data packets needed (default: 6)
    uint8_t coding_n;        // Total packets (default: 12)
    uint16_t mtu;            // Max packet payload size
    bool enabled;            // FEC decoding enabled
} fec_decoder_config_t;

// Decoded packet callback
typedef void (*fec_decoded_cb_t)(const uint8_t *data, size_t len, void *ctx);

// Initialize FEC decoder
esp_err_t fec_decoder_init(const fec_decoder_config_t *config);

// Set decoded packet callback
void fec_decoder_set_callback(fec_decoded_cb_t cb, void *ctx);

// Enable/disable FEC decoding at runtime
void fec_decoder_set_enabled(bool enabled);
bool fec_decoder_is_enabled(void);

// Process incoming packet (call from packet_rx)
// Returns true if packet was consumed by FEC decoder
bool fec_decoder_process_packet(const fpv_fec_header_t *header,
                                 const uint8_t *payload, size_t len);

// Get decoder statistics
typedef struct {
    uint32_t blocks_received;
    uint32_t blocks_recovered;    // Blocks with FEC recovery
    uint32_t blocks_failed;       // Blocks with too many losses
    uint32_t packets_recovered;   // Individual packets recovered
} fec_decoder_stats_t;

void fec_decoder_get_stats(fec_decoder_stats_t *stats);

// Cleanup
void fec_decoder_deinit(void);

#endif
```

#### 2.2 Create `main/fec_decoder.c`

**Block Buffer Structure:**
```c
typedef struct {
    uint32_t block_index;
    uint8_t *packets[MAX_N];      // Packet data pointers
    uint16_t sizes[MAX_N];        // Packet sizes
    uint8_t received_mask;        // Bitmask of received packets
    uint8_t primary_count;        // Count of packets 0..k-1
    uint8_t fec_count;            // Count of packets k..n-1
    int64_t timestamp;            // First packet arrival time
} fec_block_t;
```

**Key Implementation Details:**

1. **Block Management:**
   - Maintain sliding window of 4 blocks (configurable)
   - Auto-expire blocks older than 100ms
   - Handle out-of-order and duplicate packets

2. **Decoding Logic:**
   ```c
   void try_decode_block(fec_block_t *block) {
       if (block->primary_count >= k) {
           // All primary packets received - output directly
           for (int i = 0; i < k; i++) {
               if (block->packets[i]) {
                   callback(block->packets[i], block->sizes[i]);
               }
           }
       } else if (block->primary_count + block->fec_count >= k) {
           // Need FEC recovery
           fec_decode(...);
           // Output recovered packets
       }
       // else: block incomplete, wait for more packets
   }
   ```

3. **Immediate Output Optimization:**
   - Output primary packets (index < k) immediately as they arrive
   - Only buffer for FEC recovery when packets are missing
   - Track which packets have been output to avoid duplicates

---

### Phase 3: Integrate with packet_rx.c

#### 3.1 Modify `packet_rx.c`

```c
// In process_fpv_packet():

// Check if FEC decoder is enabled
if (fec_decoder_is_enabled()) {
    // Let FEC decoder handle the packet
    if (fec_decoder_process_packet(fec, payload, payload_len)) {
        return true;  // Packet consumed by FEC decoder
    }
}

// Fall through to existing non-FEC processing
// (for WiFi AP mode or if FEC disabled)
if (packet_index >= g_config.fec_k) {
    return true;  // Skip FEC packets in non-FEC mode
}
// ... existing video processing ...
```

#### 3.2 FEC Decoder Callback

```c
// Callback from FEC decoder when packet is ready
static void on_fec_decoded(const uint8_t *data, size_t len, void *ctx) {
    // Process video/telemetry/OSD packet
    // Same logic as current process_fpv_packet payload handling
    process_decoded_payload(data, len);
}
```

---

### Phase 4: Access-Method Based FEC Control

#### 4.1 Modify `usb_network.c`

```c
// When USB NCM client connects
void on_usb_client_connected(void) {
    ESP_LOGI(TAG, "USB client connected - enabling FEC");
    fec_decoder_set_enabled(true);
}

// When USB NCM client disconnects
void on_usb_client_disconnected(void) {
    ESP_LOGI(TAG, "USB client disconnected - disabling FEC");
    fec_decoder_set_enabled(false);
}
```

#### 4.2 Modify `wifi_manager.c` (WiFi AP mode)

```c
// WiFi AP client connect handler
static void wifi_event_handler(...) {
    case WIFI_EVENT_AP_STACONNECTED:
        // WiFi client connected - FEC stays disabled for AP
        ESP_LOGI(TAG, "WiFi client connected - FEC disabled");
        break;
}
```

#### 4.3 Web Server Mode Detection

```c
// In web_server.c - detect access method per WebSocket client
typedef struct {
    bool via_usb;  // true = USB NCM, false = WiFi AP
} ws_client_info_t;

// Determine access method by source IP
bool is_usb_client(const char *client_ip) {
    // USB NCM subnet: 192.168.7.x
    return strncmp(client_ip, "192.168.7.", 10) == 0;
}
```

---

### Phase 5: Configuration & Kconfig

#### 5.1 Add to `main/Kconfig.projbuild`

```kconfig
menu "FPV Ground Station FEC"
    config FPV_GS_FEC_ENABLED
        bool "Enable FEC decoding support"
        default y
        help
            Enable Forward Error Correction decoding for packet recovery

    config FPV_GS_FEC_K
        int "FEC K value (data packets per block)"
        default 6
        range 2 16
        depends on FPV_GS_FEC_ENABLED

    config FPV_GS_FEC_N
        int "FEC N value (total packets per block)"
        default 12
        range 4 32
        depends on FPV_GS_FEC_ENABLED

    config FPV_GS_FEC_BLOCK_WINDOW
        int "FEC block buffer window size"
        default 4
        range 2 8
        depends on FPV_GS_FEC_ENABLED
        help
            Number of concurrent blocks to buffer for FEC recovery

    config FPV_GS_FEC_BLOCK_TIMEOUT_MS
        int "FEC block timeout (ms)"
        default 100
        range 50 500
        depends on FPV_GS_FEC_ENABLED
        help
            Time to wait for missing packets before giving up on block

    config FPV_GS_FEC_USB_ONLY
        bool "Enable FEC only for USB NCM access"
        default y
        depends on FPV_GS_FEC_ENABLED
        help
            When enabled, FEC decoding is only active for USB NCM clients.
            WiFi AP clients receive packets without FEC processing.
endmenu
```

---

### Phase 6: Memory Budget

| Component | Size | Location |
|-----------|------|----------|
| GF mul table | 64 KB | PSRAM |
| Encoding matrix | 192 B | Internal |
| Block buffers (4 blocks) | 72 KB | PSRAM |
| FEC decoder state | 1 KB | Internal |
| **Total** | **~137 KB** | Mostly PSRAM |

**PSRAM Requirement:** ESP32-S3 with PSRAM is required for FEC

---

### Phase 7: Testing Plan

1. **Unit Tests:**
   - GF arithmetic correctness
   - Encode/decode round-trip with various k/n
   - Matrix inversion edge cases

2. **Integration Tests:**
   - Packet loss simulation (drop X% of packets)
   - Out-of-order packet delivery
   - Block timeout handling
   - USB vs WiFi access method switching

3. **Performance Tests:**
   - Decode latency measurement
   - CPU usage during FEC recovery
   - Memory allocation stability

---

## File Changes Summary

### New Files
```
esp32-gs/
├── components/
│   └── fec/
│       ├── CMakeLists.txt
│       ├── Kconfig
│       ├── include/
│       │   └── fec.h
│       └── src/
│           └── fec.c
└── main/
    ├── fec_decoder.h
    └── fec_decoder.c
```

### Modified Files
```
esp32-gs/main/
├── packet_rx.c      # Add FEC decoder integration
├── usb_network.c    # Add USB connect/disconnect FEC control
├── wifi_manager.c   # Ensure FEC disabled for WiFi AP
├── fpv_gs.h         # Add FEC stats to fpv_gs_stats_t
├── config_manager.c # Add FEC config persistence
├── main.c           # Initialize FEC decoder
├── CMakeLists.txt   # Add fec component dependency
└── Kconfig.projbuild # Add FEC configuration options
```

---

## Implementation Order

1. **Week 1: Core FEC Library**
   - Port `fec.c` to pure C
   - Implement GF arithmetic
   - Unit test encode/decode

2. **Week 2: FEC Decoder Module**
   - Create `fec_decoder.c`
   - Block buffering logic
   - Immediate output optimization

3. **Week 3: Integration**
   - Integrate with `packet_rx.c`
   - Add USB/WiFi mode detection
   - Runtime enable/disable

4. **Week 4: Testing & Optimization**
   - Integration testing
   - Performance optimization
   - Memory profiling

---

## API Usage Example

```c
// In main.c
void app_main(void) {
    // ... existing init ...

    #ifdef CONFIG_FPV_GS_FEC_ENABLED
    // Initialize FEC decoder
    fec_decoder_config_t fec_cfg = {
        .coding_k = CONFIG_FPV_GS_FEC_K,
        .coding_n = CONFIG_FPV_GS_FEC_N,
        .mtu = 1474,  // Max payload size
        .enabled = false,  // Start disabled, USB connect enables
    };
    ESP_ERROR_CHECK(fec_decoder_init(&fec_cfg));
    fec_decoder_set_callback(on_fec_decoded, NULL);
    #endif

    // ... rest of init ...
}

// USB client connection callback
void on_usb_dhcp_lease(const char *client_ip) {
    ESP_LOGI(TAG, "USB client %s connected", client_ip);
    #ifdef CONFIG_FPV_GS_FEC_ENABLED
    fec_decoder_set_enabled(true);
    #endif
}
```

---

## Risks & Mitigations

| Risk | Mitigation |
|------|------------|
| PSRAM not available | Fallback to reduced block window, fail gracefully |
| High CPU usage | Use lookup tables, avoid divisions |
| Block buffer overflow | Aggressive timeout, drop oldest block |
| Mismatched k/n with air unit | Auto-detect from packet stream |

---

## Success Criteria

1. FEC decoder recovers packets when up to (n-k) packets lost per block
2. No packet loss on USB NCM when FEC enabled
3. WiFi AP mode works unchanged (FEC disabled)
4. CPU usage < 10% during FEC recovery
5. Decode latency < 5ms per block

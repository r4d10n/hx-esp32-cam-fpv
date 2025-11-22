# Component Dependencies Documentation

## Overview

This document provides a comprehensive analysis of all component dependencies across the ESP32-CAM FPV system, including the ESP32-P4 MIPI FPV system, ESP32-C5 WiFi Transmitter, and ESP32-C6 WiFi Transmitter projects.

**Last Updated:** 2025-11-22
**ESP-IDF Version Required:** v5.3 or later
**CMake Version Required:** 3.16+

---

## 1. Project Structure

```
hx-esp32-cam-fpv/
├── esp32-p4-mipi-fpv/           # Main air unit (ESP32-P4 + MIPI camera)
│   ├── components/
│   │   ├── mipi_camera/         # MIPI CSI-2 camera driver
│   │   ├── h264_encoder/        # H.264 hardware encoder
│   │   ├── esp32_ipc/           # Inter-processor communication (IPC)
│   │   └── wifi_c5_transmitter/ # WiFi 6 transmitter (for ESP32-C5)
│   ├── main/                    # Main application
│   └── tests/
├── esp32-c5-wifi-transmitter/   # ESP32-C5 WiFi transmitter firmware
│   ├── components/              # EMPTY - should contain esp32_ipc, wifi_c5_transmitter
│   ├── main/                    # Main application
│   └── README.md
├── esp32-c6-wifi-transmitter/   # ESP32-C6 WiFi transmitter firmware (alternative)
│   ├── components/
│   │   └── wifi_c6_transmitter/ # WiFi 6 transmitter (C6-specific)
│   ├── main/                    # Main application
│   ├── sdkconfig.defaults
│   └── README.md
└── components/                  # Shared components (legacy)
    ├── common/                  # Common utilities (FEC codec, etc.)
    ├── air/
    └── esp32-camera/            # OV2640/OV3660/OV5640 camera driver
```

---

## 2. Dependency Trees by Project

### 2.1 ESP32-P4 MIPI FPV System

```
esp32-p4-mipi-fpv (Project)
└── main (component)
    ├── nvs_flash                        [ESP-IDF]
    ├── esp_timer                        [ESP-IDF]
    ├── mipi_camera                      (component)
    │   ├── driver                       [ESP-IDF]
    │   └── esp_timer                    [ESP-IDF]
    ├── h264_encoder                     (component)
    │   ├── driver                       [ESP-IDF]
    │   └── esp_timer                    [ESP-IDF]
    └── esp32_ipc                        (component)
        ├── driver                       [ESP-IDF]
        └── esp_timer                    [ESP-IDF]

Legend: [ESP-IDF] = Built-in ESP-IDF component
        (component) = Custom component in this project
```

**Component Details:**

#### mipi_camera
- **Type:** Hardware Driver
- **Target:** ESP32-P4
- **Sources:**
  - mipi_camera.c (main driver)
  - sensors/imx219.c (Sony IMX219 support)
  - sensors/imx477.c (Sony IMX477 support)
  - sensors/ov5647.c (OmniVision OV5647 support)
- **Dependencies:**
  - `driver` (GPIO, I2C interfaces)
  - `esp_timer` (timing control)
- **Headers:** include/mipi_camera.h
- **Notes:** Supports multiple MIPI camera sensors

#### h264_encoder
- **Type:** Hardware Encoder
- **Target:** ESP32-P4
- **Sources:**
  - h264_encoder.c
- **Dependencies:**
  - `driver` (hardware access)
  - `esp_timer` (timing)
- **Headers:** include/h264_encoder.h
- **Notes:** Uses ESP32-P4's hardware H.264 encoder

#### esp32_ipc
- **Type:** Inter-Processor Communication
- **Target:** Can be used on both ESP32-P4 and ESP32-C5/C6
- **Sources:**
  - esp32_ipc_master.c (master side - ESP32-P4)
  - esp32_ipc_slave.c (slave side - ESP32-C5/C6)
  - esp32_ipc_common.c (shared utilities)
- **Dependencies:**
  - `driver` (SPI interfaces)
  - `esp_timer` (timing)
- **Headers:** include/esp32_ipc.h
- **Notes:** Uses SPI for high-speed communication between P4 and C5/C6

#### wifi_c5_transmitter
- **Type:** WiFi 6 Transmitter (for ESP32-C5)
- **Target:** ESP32-C5 (controlled via IPC from ESP32-P4)
- **Sources:**
  - wifi_c5_transmitter.c
  - fec_encoder.c (FEC encoding wrapper)
- **Dependencies:**
  - `esp_wifi` (WiFi support)
  - `esp_timer` (timing)
  - `nvs_flash` (NVS configuration)
- **Headers:** include/wifi_c5_transmitter.h, fec_encoder_wifi.h
- **Notes:** Contains FEC encoding implementation for error correction

### 2.2 ESP32-C5 WiFi Transmitter System

```
esp32-c5-wifi-transmitter (Project)
└── main (component)
    ├── nvs_flash                        [ESP-IDF]
    ├── esp_wifi                         [ESP-IDF]
    ├── esp_timer                        [ESP-IDF]
    ├── esp32_ipc ⚠️ MISSING             ❌ CRITICAL ISSUE
    └── wifi_c5_transmitter ⚠️ MISSING   ❌ CRITICAL ISSUE

⚠️ = Component should be present but is missing from this project
```

**Critical Issues Found:**

1. **Missing Components:** The ESP32-C5 project's `main` component declares dependencies on:
   - `esp32_ipc` (not in esp32-c5-wifi-transmitter/components/)
   - `wifi_c5_transmitter` (not in esp32-c5-wifi-transmitter/components/)

2. **Missing Configuration:** The project root CMakeLists.txt does NOT include:
   - `set(EXTRA_COMPONENT_DIRS ...)` to reference components from other locations
   - `set(COMPONENT_REQUIRES ...)` to require specific components

3. **Resolution Required:**
   - Option A: Add symlinks to components in esp32-p4-mipi-fpv/components/
   - Option B: Copy components to esp32-c5-wifi-transmitter/components/
   - Option C: Configure EXTRA_COMPONENT_DIRS in CMakeLists.txt (Recommended)

**Expected Configuration:**
```cmake
# esp32-c5-wifi-transmitter/CMakeLists.txt should include:
set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")
```

### 2.3 ESP32-C6 WiFi Transmitter System

```
esp32-c6-wifi-transmitter (Project)
└── main (component)
    ├── nvs_flash                        [ESP-IDF]
    ├── esp_wifi                         [ESP-IDF]
    ├── esp_timer                        [ESP-IDF]
    ├── esp32_ipc ⚠️ MISSING             ❌ CRITICAL ISSUE
    └── wifi_c6_transmitter              (component)
        ├── esp_wifi                     [ESP-IDF]
        ├── esp_event                    [ESP-IDF]
        ├── esp_timer                    [ESP-IDF]
        ├── nvs_flash                    [ESP-IDF]
        └── fec_encoder ⚠️ MISSING       ❌ CRITICAL ISSUE

⚠️ = Component should be present but is missing from this project
```

**Critical Issues Found:**

1. **Missing esp32_ipc Component:** The main component requires `esp32_ipc` but:
   - Not defined in esp32-c6-wifi-transmitter/components/
   - Not referenced via EXTRA_COMPONENT_DIRS

2. **Missing fec_encoder Component:** The wifi_c6_transmitter component requires `fec_encoder` but:
   - No fec_encoder directory exists anywhere in the project
   - FEC encoder implementation exists in wifi_c5_transmitter/fec_encoder.c (P4 project)
   - Likely needs to be created as a shared component

3. **Missing Configuration:** Like ESP32-C5, missing EXTRA_COMPONENT_DIRS and COMPONENT_REQUIRES

**Expected Configuration:**
```cmake
# esp32-c6-wifi-transmitter/CMakeLists.txt should include:
set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")
```

---

## 3. ESP-IDF Component Requirements

### All Projects Use:

| Component | Purpose | Min Version | Required For |
|-----------|---------|-------------|--------------|
| `driver` | GPIO, I2C, SPI drivers | 5.3 | Camera control, IPC |
| `esp_timer` | Timer utilities | 5.3 | Frame timing, scheduling |
| `nvs_flash` | Non-volatile storage | 5.3 | Configuration storage |
| `esp_wifi` | WiFi stack | 5.3 | WiFi transmission |
| `esp_event` | Event system | 5.3 | WiFi events (C6 only) |
| `freertos` | RTOS kernel | 5.3 | Task scheduling |

### Project-Specific Components:

#### ESP32-P4 (esp32-p4-mipi-fpv)
- Requires MIPI CSI-2 camera interface support
- Requires H.264 encoder hardware
- Requires SPI Master capability for IPC

#### ESP32-C5 (esp32-c5-wifi-transmitter)
- Requires SPI Slave capability for IPC
- Requires WiFi 6 (802.11ax) support
- Requires high-speed data transfer

#### ESP32-C6 (esp32-c6-wifi-transmitter)
- Requires SPI Slave capability for IPC
- Requires WiFi 6 (802.11ax) support
- Integrated on ESP32-P4 evaluation board

---

## 4. External Library Requirements

### FEC (Forward Error Correction)

**Location:** `/components/common/fec.*` and `/esp32-p4-mipi-fpv/components/wifi_c5_transmitter/fec_encoder.*`

**Dependencies:**
- None (self-contained C/C++ implementation)

**Status:** ⚠️ **NOT PROPERLY COMPONENTIZED**
- FEC codec exists in `components/common/` (legacy)
- FEC encoder wrapper exists in `wifi_c5_transmitter/`
- C6 project references missing `fec_encoder` component

**Recommendation:**
Create a dedicated `fec_encoder` component that:
1. Can be shared by both C5 and C6 transmitter implementations
2. Properly exposes FEC encoder API
3. Located in: `/esp32-p4-mipi-fpv/components/fec_encoder/`

### FFmpeg (Ground Station)

**Location:** Used in ground station (`gs/` directory)

**Status:** Not a component dependency, external binary

---

## 5. Circular Dependency Analysis

### Current Status: ✅ NO CIRCULAR DEPENDENCIES DETECTED

**Dependency Flow Analysis:**

```
Hardware Layer (ESP-IDF):
    └─→ driver, esp_timer, esp_wifi, nvs_flash
        └─→ FreeRTOS

Custom Components:
    mipi_camera ──┐
    h264_encoder │
    esp32_ipc ───┼──→ main (ESP32-P4)
    wifi_c5_transmitter ──→ USED BY ESP32-C5 project
    wifi_c6_transmitter ──→ USED BY ESP32-C6 project

Linear dependency chain confirmed: No circular dependencies
```

**Verification:**
- ✅ No component includes its own headers
- ✅ No bidirectional dependencies between components
- ✅ All dependencies flow downward (toward ESP-IDF)

---

## 6. Component-to-Component Dependency Matrix

| Component | mipi_camera | h264_encoder | esp32_ipc | wifi_c5_tx | wifi_c6_tx | main |
|-----------|:-:|:-:|:-:|:-:|:-:|:-:|
| **mipi_camera** | - | ✅ | ✅ | - | - | ✅ |
| **h264_encoder** | - | - | ✅ | - | - | ✅ |
| **esp32_ipc** | - | - | - | ✅ | ✅ | ✅ |
| **wifi_c5_tx** | - | - | ✅ | - | - | ✅ |
| **wifi_c6_tx** | - | - | ✅* | ❌ | - | ✅ |
| **main (P4)** | - | - | - | - | - | - |

**Legend:**
- ✅ = Dependency present and correct
- ❌ = Missing dependency
- \* = Should exist via EXTRA_COMPONENT_DIRS
- **\*** = fec_encoder missing

---

## 7. Build Order Requirements

### ESP32-P4 MIPI FPV

**Build Order (Dependencies resolved bottom-to-top):**

```
1. ESP-IDF Components
   ├── driver
   ├── esp_timer
   ├── nvs_flash
   └── freertos

2. Custom Components (no inter-dependencies)
   ├── mipi_camera
   ├── h264_encoder
   ├── esp32_ipc
   └── wifi_c5_transmitter

3. Main Application
   └── main (depends on all above)
```

**Build Command:**
```bash
cd esp32-p4-mipi-fpv
idf.py set-target esp32p4
idf.py build
```

### ESP32-C5 WiFi Transmitter

⚠️ **Currently Cannot Build Due to Missing Components**

**After Fixes (Add EXTRA_COMPONENT_DIRS):**

```
1. ESP-IDF Components
   ├── driver
   ├── esp_wifi
   ├── esp_timer
   ├── nvs_flash
   └── freertos

2. Custom Components (from ../esp32-p4-mipi-fpv/components/)
   ├── esp32_ipc
   └── wifi_c5_transmitter

3. Main Application
   └── main
```

**Build Command (After Fix):**
```bash
cd esp32-c5-wifi-transmitter
idf.py set-target esp32c5
idf.py build
```

### ESP32-C6 WiFi Transmitter

⚠️ **Currently Cannot Build Due to Missing Components**

**After Fixes (Create fec_encoder component, Add EXTRA_COMPONENT_DIRS):**

```
1. ESP-IDF Components
   ├── driver
   ├── esp_wifi
   ├── esp_event
   ├── esp_timer
   ├── nvs_flash
   └── freertos

2. Shared Custom Components (from ../esp32-p4-mipi-fpv/components/)
   ├── esp32_ipc
   └── fec_encoder (NEW)

3. Local Custom Components
   └── wifi_c6_transmitter

4. Main Application
   └── main
```

**Build Command (After Fix):**
```bash
cd esp32-c6-wifi-transmitter
idf.py set-target esp32c6
idf.py build
```

---

## 8. ASCII Dependency Diagrams

### System Architecture Overview

```
                    ┌─────────────────────┐
                    │  Ground Station     │
                    │  (FFmpeg decoder)   │
                    └──────────┬──────────┘
                               │
                        WiFi 6 (802.11ax)
                               │
                ┌──────────────┴──────────────┐
                │                             │
        ┌───────▼────────┐           ┌────────▼────────┐
        │   ESP32-C5     │           │   ESP32-C6      │
        │ WiFi Receiver  │           │ WiFi Receiver   │
        │ (Alt: SPI Slave)           │ (Alt: SPI Slave)│
        └────────▲────────┘           └────────▲────────┘
                 │                              │
         High-Speed SPI (50MHz, both directions)
                 │                              │
                 └──────────────┬───────────────┘
                                │
                        ┌───────▼────────┐
                        │   ESP32-P4     │
                        │   MIPI CSI-2   │
                        │  (SPI Master)  │
                        └────────┬────────┘
                                 │
                            MIPI CSI-2
                                 │
                        ┌────────▼────────┐
                        │  MIPI Camera    │
                        │ (IMX219/IMX477) │
                        └─────────────────┘
```

### Component Dependency Graph

```
        ┌─────────────────┐
        │   ESP-IDF v5.3+ │
        │  Core Libraries │
        │  (driver, timer)│
        └────────┬────────┘
                 │
    ┌────────────┼────────────┐
    │            │            │
┌───▼──────┐ ┌──▼─────┐ ┌────▼───┐
│  MIPI    │ │ H.264  │ │ IPC    │
│ Camera   │ │Encoder │ │(SPI)   │
└────┬─────┘ └────────┘ └────┬───┘
     │                       │
     └───────────┬───────────┘
                 │
            ┌────▼────┐
            │   main   │
            │(ESP32-P4)│
            └────┬─────┘
                 │
        ┌────────┴─────────┐
        │                  │
    ┌───▼────┐      ┌─────▼────┐
    │ WiFi   │      │ WiFi C6  │
    │ C5 TX  │      │ TX*      │
    │(via IPC)      │(via IPC) │
    └────────┘      └──────────┘

* C6 transmitter: NEW - requires shared fec_encoder
```

### Project-Specific Dependency Trees

**ESP32-P4 Firmware:**
```
                    esp32-p4-mipi-fpv
                           │
                           ▼
                    ┌─────────────┐
                    │  main (P4)  │
                    └─────┬───────┘
                  ┌────────┼────────┐
                  │        │        │
            ┌─────▼─┐  ┌───▼───┐  ┌┴─────────┐
            │MIPI   │  │H.264  │  │ ESP32_IPC│
            │Camera │  │Encoder│  │ (Master) │
            └───────┘  └───────┘  └──────────┘
```

**ESP32-C5 Firmware (Current - Broken):**
```
            esp32-c5-wifi-transmitter
                      │
                      ▼
               ┌─────────────┐
               │  main (C5)  │
               └──────┬──────┘
          ┌────────────┼──────────────┐
          │            │              │
      ❌  │       ❌    │         ✅   │
    ┌─────▼────┐  ┌────▼──┐  ┌──────▼────┐
    │ ESP32_IPC│  │WiFi C5│  │ ESP-IDF   │
    │(Missing!)│  │TX(Miss)  │ WiFi, NVS │
    └──────────┘  └────────┘  └───────────┘
```

**ESP32-C6 Firmware (Current - Broken):**
```
            esp32-c6-wifi-transmitter
                      │
                      ▼
               ┌─────────────┐
               │  main (C6)  │
               └──────┬──────┘
          ┌────────────┼──────────────┐
          │            │              │
      ❌  │         ┌───▼──────┐  ✅  │
    ┌─────▼────┐   │ WiFi C6  │  ┌──▼────────┐
    │ ESP32_IPC│   │ TX       │  │ ESP-IDF   │
    │(Missing!)│   └────┬─────┘  │ WiFi, NVS │
    └──────────┘        │        │ Event     │
                        │        └───────────┘
                        │
                   ❌  │
                   ┌───▼──────┐
                   │FEC Encoder│
                   │(Missing!) │
                   └───────────┘
```

---

## 9. Validation Results

### Critical Issues Found: 5

#### 1. ❌ ESP32-C5: Missing esp32_ipc Component
- **Severity:** CRITICAL
- **Type:** Build Blocker
- **Location:** `esp32-c5-wifi-transmitter/main/CMakeLists.txt`
- **Current State:** Component declares dependency on `esp32_ipc`
- **Problem:** No `esp32_ipc` component in project directory
- **Solution:** Add to CMakeLists.txt:
  ```cmake
  set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")
  ```

#### 2. ❌ ESP32-C5: Missing wifi_c5_transmitter Component
- **Severity:** CRITICAL
- **Type:** Build Blocker
- **Location:** `esp32-c5-wifi-transmitter/main/CMakeLists.txt`
- **Current State:** Component declares dependency on `wifi_c5_transmitter`
- **Problem:** No `wifi_c5_transmitter` component in project directory
- **Solution:** Same as #1 above

#### 3. ❌ ESP32-C6: Missing esp32_ipc Component
- **Severity:** CRITICAL
- **Type:** Build Blocker
- **Location:** `esp32-c6-wifi-transmitter/main/CMakeLists.txt`
- **Current State:** Component declares dependency on `esp32_ipc`
- **Problem:** No `esp32_ipc` component in project directory
- **Solution:** Same as #1 above

#### 4. ❌ ESP32-C6: Missing fec_encoder Component
- **Severity:** CRITICAL
- **Type:** Build Blocker
- **Location:** `esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/CMakeLists.txt`
- **Current State:** Component declares dependency on `fec_encoder`
- **Problem:** No `fec_encoder` component exists anywhere
- **Includes:** `#include "fec_encoder.h"` in source code
- **Solution Options:**
  - Option A: Create dedicated `fec_encoder` component in `esp32-p4-mipi-fpv/components/`
  - Option B: Restructure `wifi_c5_transmitter` to expose FEC encoder as separate component

#### 5. ⚠️ Missing CMake Configuration in C5 and C6
- **Severity:** HIGH
- **Type:** Build Configuration Issue
- **Location:** `esp32-c5-wifi-transmitter/CMakeLists.txt` and `esp32-c6-wifi-transmitter/CMakeLists.txt`
- **Current State:** Minimal configuration
- **Problem:** No EXTRA_COMPONENT_DIRS or COMPONENT_REQUIRES settings
- **Reference:** Compare with legacy projects (`air_firmware_esp32cam/CMakeLists.txt`)
- **Solution:**
  ```cmake
  set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")
  ```

### Warnings: 3

#### W1: FEC Encoder Not Properly Componentized
- **Status:** ⚠️ Code Quality Issue
- **Locations:**
  - `/components/common/fec.*` (legacy FEC codec)
  - `/esp32-p4-mipi-fpv/components/wifi_c5_transmitter/fec_encoder.*` (WiFi-specific wrapper)
- **Impact:** Code duplication, hard to maintain
- **Recommendation:** Create unified `fec_encoder` component

#### W2: esp32_ipc Component Not Clear on Dual-Role
- **Status:** ⚠️ Design Clarity Issue
- **Issue:** Component works as both SPI Master (P4) and Slave (C5/C6)
- **Recommendation:** Document dual-role clearly, consider separation if code diverges

#### W3: Missing sdkconfig.defaults for P4 and C5
- **Status:** ⚠️ Build Configuration Issue
- **Missing from:**
  - `/esp32-p4-mipi-fpv/sdkconfig.defaults`
  - `/esp32-c5-wifi-transmitter/sdkconfig.defaults`
- **Note:** C6 has proper `sdkconfig.defaults`
- **Recommendation:** Create and commit defaults files for consistency

---

## 10. REQUIRES Clause Validation

### All Components - Detailed Analysis

#### ESP32-P4 Components

**mipi_camera/CMakeLists.txt:**
```cmake
REQUIRES
    driver          ✅ Correct (I2C, GPIO needed)
    esp_timer       ✅ Correct (timing control)
```

**h264_encoder/CMakeLists.txt:**
```cmake
REQUIRES
    driver          ✅ Correct (hardware access)
    esp_timer       ✅ Correct (performance measurement)
```

**esp32_ipc/CMakeLists.txt:**
```cmake
REQUIRES
    driver          ✅ Correct (SPI master/slave)
    esp_timer       ✅ Correct (IPC timing)
```

**wifi_c5_transmitter/CMakeLists.txt:**
```cmake
REQUIRES
    esp_wifi        ✅ Correct (WiFi stack)
    esp_timer       ✅ Correct (frame timing)
    nvs_flash       ✅ Correct (config storage)
```

**Note:** FEC encoder is internal, not exposed as separate component ⚠️

#### ESP32-P4 Main Component

**main/CMakeLists.txt:**
```cmake
REQUIRES
    nvs_flash           ✅ Correct (config storage)
    esp_timer           ✅ Correct (frame timing)
    mipi_camera         ✅ Correct (camera driver)
    h264_encoder        ✅ Correct (H.264 encoding)
    esp32_ipc           ✅ Correct (IPC to C5/C6)
```

#### ESP32-C5 Main Component

**main/CMakeLists.txt:**
```cmake
REQUIRES
    nvs_flash           ✅ Correct (config storage)
    esp_wifi            ✅ Correct (WiFi stack)
    esp_timer           ✅ Correct (timing)
    esp32_ipc           ❌ MISSING (component not found)
    wifi_c5_transmitter ❌ MISSING (component not found)
```

#### ESP32-C6 Main Component

**main/CMakeLists.txt:**
```cmake
REQUIRES
    nvs_flash           ✅ Correct (config storage)
    esp_wifi            ✅ Correct (WiFi stack)
    esp_timer           ✅ Correct (timing)
    esp32_ipc           ❌ MISSING (component not found)
    wifi_c6_transmitter ✅ Found (local component)
```

#### ESP32-C6 WiFi Transmitter Component

**components/wifi_c6_transmitter/CMakeLists.txt:**
```cmake
REQUIRES
    esp_wifi            ✅ Correct (WiFi stack)
    esp_event           ✅ Correct (WiFi events)
    esp_timer           ✅ Correct (timing)
    nvs_flash           ✅ Correct (config)
    fec_encoder         ❌ MISSING (component not found)
```

---

## 11. Recommendations for Dependency Optimization

### Priority 1: Critical Fixes (Required for Build)

1. **Add EXTRA_COMPONENT_DIRS to C5 and C6 Projects**
   ```cmake
   # esp32-c5-wifi-transmitter/CMakeLists.txt
   # esp32-c6-wifi-transmitter/CMakeLists.txt

   set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")
   ```

2. **Create fec_encoder Component**
   - **Location:** `esp32-p4-mipi-fpv/components/fec_encoder/`
   - **Source:** Extract and refactor from `wifi_c5_transmitter/fec_encoder.c`
   - **Headers:** `include/fec_encoder.h` (with public API)
   - **CMakeLists.txt:**
     ```cmake
     idf_component_register(
         SRCS "fec_encoder.c"
         INCLUDE_DIRS "include"
         REQUIRES esp_timer
     )
     ```

### Priority 2: Build Configuration

3. **Add sdkconfig.defaults Files**
   - Create `esp32-p4-mipi-fpv/sdkconfig.defaults`
   - Create `esp32-c5-wifi-transmitter/sdkconfig.defaults`
   - Document required configuration options

4. **Document Component Requirements**
   - Add COMPONENT_REQUIRES clause to project CMakeLists.txt
   - Reference: Legacy projects use this pattern

### Priority 3: Code Quality

5. **Refactor FEC Codec**
   - Consolidate FEC implementations
   - Remove duplication between `components/common/fec_codec` and `wifi_c5_transmitter/fec_encoder`
   - Create unified component with versioning

6. **Document IPC Component**
   - Clearly document master (P4) vs slave (C5/C6) modes
   - Add configuration examples for both modes
   - Consider creating separate ipc_master and ipc_slave components if code diverges

7. **Add Component Version Information**
   - Update CMakeLists.txt with version strings
   - Consider using idf_component.yml for metadata

---

## 12. Dependency Resolution Strategy

### For Development/Testing

**Immediate Fix (Allows builds):**
```bash
# Update both C5 and C6 CMakeLists.txt files
sed -i 's/set(PROJECT_NAME/set(EXTRA_COMPONENT_DIRS ..\/esp32-p4-mipi-fpv\/components")\nset(PROJECT_NAME/' esp32-c5-wifi-transmitter/CMakeLists.txt
sed -i 's/set(PROJECT_NAME/set(EXTRA_COMPONENT_DIRS ..\/esp32-p4-mipi-fpv\/components")\nset(PROJECT_NAME/' esp32-c6-wifi-transmitter/CMakeLists.txt
```

### For Production Release

1. Create proper `fec_encoder` component
2. Add sdkconfig.defaults to all projects
3. Add CI/CD verification for component paths
4. Create build guide documentation

---

## 13. Build Verification Checklist

### Pre-Build Validation

- [ ] All referenced components exist in their declared locations
- [ ] No circular dependencies detected
- [ ] All REQUIRES clauses reference valid components
- [ ] ESP-IDF version 5.3+ installed
- [ ] CMake version 3.16+ installed
- [ ] All EXTRA_COMPONENT_DIRS paths are correct

### Post-Build Validation

- [ ] All components compiled successfully
- [ ] No linker errors or missing symbols
- [ ] Binary size within device memory limits
- [ ] All sections properly placed in flash/RAM

### Runtime Validation

- [ ] IPC initialization completes successfully
- [ ] WiFi stack initializes without errors
- [ ] Camera initialization succeeds
- [ ] H.264 encoder produces valid output
- [ ] FEC encoding/decoding functional

---

## 14. External Dependencies Summary

### FFmpeg (Ground Station Only)

**Location:** `/gs/` (not an embedded dependency)

**Usage:** H.264 video decoding on ground station PC/Linux

**Notes:** Not required for ESP32 firmware builds

### ESP-IDF Dependencies

**Version:** 5.3 or later

**Core Components Used:**
- `driver` (v5.3+) - GPIO, I2C, SPI interfaces
- `esp_wifi` (v5.3+) - 802.11 WiFi stack
- `esp_timer` (v5.3+) - Timing and scheduling
- `nvs_flash` (v5.3+) - Non-volatile storage
- `esp_event` (v5.3+) - Event handling (C6 only)
- `freertos` (v5.3+) - Real-time OS kernel

### No External C/C++ Libraries

**Status:** ✅ All FEC encoding is self-contained in C code

---

## 15. Testing Strategy for Dependencies

### Unit Tests

```
esp32-p4-mipi-fpv/tests/
├── test_mipi_camera.c       - Camera driver tests
├── test_h264_encoder.c      - Encoder functionality tests
├── test_esp32_ipc.c         - IPC communication tests
└── test_wifi_c5_tx.c        - WiFi transmitter tests

esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/test/
└── test_wifi_c6_transmitter.c - C6-specific tests
```

### Integration Tests

1. **IPC Communication Test**
   - Verify P4 ↔ C5/C6 SPI data transfer
   - Test packet integrity with FEC

2. **WiFi Transmission Test**
   - Verify H.264 NAL unit transmission
   - Monitor packet loss and recovery

3. **End-to-End Test**
   - Camera → Encoder → IPC → WiFi → Ground Station

---

## Appendix A: Component Metadata

### ESP-IDF Target Support

| Component | ESP32-P4 | ESP32-C5 | ESP32-C6 | Notes |
|-----------|:--------:|:--------:|:--------:|-------|
| mipi_camera | ✅ | ❌ | ❌ | P4 only (has MIPI interface) |
| h264_encoder | ✅ | ❌ | ❌ | P4 only (has H.264 encoder) |
| esp32_ipc | ✅ (M) | ✅ (S) | ✅ (S) | Master on P4, Slave on C5/C6 |
| wifi_c5_transmitter | ✅ | ✅ | ❌ | C5-specific |
| wifi_c6_transmitter | ❌ | ❌ | ✅ | C6-specific (NEW) |

**M = Master, S = Slave**

### Component Size Estimates

| Component | Source Files | Headers | Est. Size |
|-----------|:---:|:---:|:---:|
| mipi_camera | 4 | 1 | ~15 KB |
| h264_encoder | 1 | 1 | ~10 KB |
| esp32_ipc | 3 | 1 | ~20 KB |
| wifi_c5_transmitter | 2 | 2 | ~25 KB |
| wifi_c6_transmitter | 1 | 1 | ~30 KB |
| **Total** | **11** | **6** | **~100 KB** |

---

## Appendix B: Detailed Component Information

### mipi_camera Component

**File:** `/esp32-p4-mipi-fpv/components/mipi_camera/`

**Public API:**
```c
// Initialize MIPI camera
esp_err_t mipi_camera_init(const mipi_camera_config_t *config);

// Get frame data
esp_err_t mipi_camera_get_frame(uint8_t **data, size_t *size);

// Release frame
void mipi_camera_release_frame(uint8_t *data);
```

**Supported Sensors:**
- Sony IMX219 (Recommended)
- Sony IMX477 (High quality)
- OmniVision OV5647 (Budget)

---

### h264_encoder Component

**File:** `/esp32-p4-mipi-fpv/components/h264_encoder/`

**Public API:**
```c
// Initialize H.264 encoder
esp_err_t h264_encoder_init(const h264_encoder_config_t *config);

// Encode frame
esp_err_t h264_encoder_encode(uint8_t *input, size_t size,
                              uint8_t **output, size_t *out_size);
```

**Features:**
- Hardware H.264 encoding (H.264 Main Profile)
- Frame rate up to 60 fps
- Resolution from QVGA to FHD

---

### esp32_ipc Component

**File:** `/esp32-p4-mipi-fpv/components/esp32_ipc/`

**Public API - Master (P4):**
```c
// Initialize IPC master
esp_err_t esp32_ipc_master_init(const esp32_ipc_config_t *config);

// Send data to slave
esp_err_t esp32_ipc_master_send(uint8_t *data, size_t size);

// Receive data from slave
esp_err_t esp32_ipc_master_recv(uint8_t **data, size_t *size);
```

**Public API - Slave (C5/C6):**
```c
// Initialize IPC slave
esp_err_t esp32_ipc_slave_init(const esp32_ipc_config_t *config);

// Receive data from master
esp_err_t esp32_ipc_slave_recv(uint8_t **data, size_t *size);

// Send data to master
esp_err_t esp32_ipc_slave_send(uint8_t *data, size_t size);
```

**Protocol:**
- High-speed SPI (50 MHz)
- Master-initiated transactions
- Full-duplex communication

---

### wifi_c5_transmitter Component

**File:** `/esp32-p4-mipi-fpv/components/wifi_c5_transmitter/`

**Public API:**
```c
// Initialize WiFi transmitter
esp_err_t wifi_c5_tx_init(const wifi_c5_tx_config_t *config);

// Send packet
esp_err_t wifi_c5_tx_send(uint8_t *data, size_t size);

// Get statistics
void wifi_c5_tx_get_stats(wifi_c5_tx_stats_t *stats);
```

**Features:**
- WiFi 6 (802.11ax) support
- Dynamic MCS selection (0-11)
- FEC encoding with configurable redundancy

---

### wifi_c6_transmitter Component

**File:** `/esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/`

**Public API:**
```c
// Initialize WiFi transmitter
esp_err_t wifi_c6_tx_init(const wifi_c6_tx_config_t *config);

// Send packet with priority
esp_err_t wifi_c6_tx_send(uint8_t *data, size_t size, wifi_c6_priority_t priority);

// Get statistics
void wifi_c6_tx_get_stats(wifi_c6_tx_stats_t *stats);

// Dynamic configuration
esp_err_t wifi_c6_tx_set_channel(uint8_t channel);
esp_err_t wifi_c6_tx_set_mcs(wifi_c6_mcs_t mcs);
esp_err_t wifi_c6_tx_set_power(int8_t power_dbm);
```

**Features:**
- WiFi 6 (802.11ax) support
- Priority-based transmission queues
- Dynamic channel/MCS/power adjustment
- FEC encoding (requires fec_encoder component)
- Integrated on ESP32-P4 evaluation board

---

## Final Summary

**Total Components:** 5 (P4) + 2 (C5/C6 transmitters)

**Build Status:**
- ✅ ESP32-P4: Can build (all dependencies present)
- ❌ ESP32-C5: Cannot build (missing 2 components)
- ❌ ESP32-C6: Cannot build (missing 2 components + fec_encoder)

**Critical Issues:** 5 (2 per C5/C6 project + 1 shared FEC encoder)

**Required Actions:**
1. Add EXTRA_COMPONENT_DIRS to C5/C6 CMakeLists.txt
2. Create fec_encoder component
3. Add sdkconfig.defaults files
4. Validate builds complete successfully

**Estimated Fix Time:** 30-45 minutes (adding paths + creating fec_encoder component)

---

*Documentation generated: 2025-11-22*
*Analyzed by: Claude Code - Component Dependency Analyzer*
*ESP-IDF Version: 5.3+*
*CMake Version: 3.16+*

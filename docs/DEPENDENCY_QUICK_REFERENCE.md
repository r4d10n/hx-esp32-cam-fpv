# Component Dependencies - Quick Reference

## At a Glance

### Project Status

| Project | Status | Build | Issues | Fix Time |
|---------|:------:|:-----:|:------:|:--------:|
| **ESP32-P4 MIPI FPV** | ✅ Ready | ✅ Yes | 0 | - |
| **ESP32-C5 Transmitter** | ❌ Broken | ❌ No | 2 | 2 min |
| **ESP32-C6 Transmitter** | ❌ Broken | ❌ No | 3 | 12 min |

---

## Dependency Tree (Simplified)

### ESP32-P4
```
main
├── mipi_camera → driver, esp_timer
├── h264_encoder → driver, esp_timer
└── esp32_ipc → driver, esp_timer
```

### ESP32-C5
```
main
├── esp32_ipc ❌ (NOT FOUND)
└── wifi_c5_transmitter ❌ (NOT FOUND)
```

### ESP32-C6
```
main
├── esp32_ipc ❌ (NOT FOUND)
└── wifi_c6_transmitter
    └── fec_encoder ❌ (NOT FOUND)
```

---

## Critical Issues (5)

| # | Issue | Fix |
|---|-------|-----|
| 1 | C5: Missing esp32_ipc | Add EXTRA_COMPONENT_DIRS |
| 2 | C5: Missing wifi_c5_transmitter | Add EXTRA_COMPONENT_DIRS |
| 3 | C6: Missing esp32_ipc | Add EXTRA_COMPONENT_DIRS |
| 4 | C6: Missing fec_encoder | Create component |
| 5 | Missing sdkconfig.defaults | Create files (optional) |

---

## Quick Fixes

### Fix #1 & #2 & #3: Add EXTRA_COMPONENT_DIRS

**File:** `esp32-c5-wifi-transmitter/CMakeLists.txt`
```cmake
# Add this line after "set(PROJECT_NAME...":
set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")
```

**File:** `esp32-c6-wifi-transmitter/CMakeLists.txt`
```cmake
# Add this line after "set(PROJECT_NAME...":
set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")
```

**Test:**
```bash
cd esp32-c5-wifi-transmitter && idf.py reconfigure
cd esp32-c6-wifi-transmitter && idf.py reconfigure
```

---

### Fix #4: Create fec_encoder Component

```bash
# 1. Create directories
mkdir -p esp32-p4-mipi-fpv/components/fec_encoder/include

# 2. Copy files
cp esp32-p4-mipi-fpv/components/wifi_c5_transmitter/fec_encoder.c \
   esp32-p4-mipi-fpv/components/fec_encoder/

cp esp32-p4-mipi-fpv/components/wifi_c5_transmitter/fec_encoder_wifi.h \
   esp32-p4-mipi-fpv/components/fec_encoder/include/fec_encoder.h

# 3. Create CMakeLists.txt in fec_encoder/
# (See detailed docs)
```

---

## ESP-IDF Component Map

| Component | Type | Status |
|-----------|------|:------:|
| `driver` | ESP-IDF Built-in | ✅ |
| `esp_wifi` | ESP-IDF Built-in | ✅ |
| `esp_timer` | ESP-IDF Built-in | ✅ |
| `nvs_flash` | ESP-IDF Built-in | ✅ |
| `esp_event` | ESP-IDF Built-in | ✅ |
| `freertos` | ESP-IDF Built-in | ✅ |
| `mipi_camera` | Custom (P4) | ✅ |
| `h264_encoder` | Custom (P4) | ✅ |
| `esp32_ipc` | Custom (P4) | ✅ |
| `wifi_c5_transmitter` | Custom (P4) | ✅ |
| `wifi_c6_transmitter` | Custom (C6) | ✅ |
| `fec_encoder` | Custom (P4) | ❌ Missing |

---

## Build Commands

### After Fixes

```bash
# ESP32-P4
cd esp32-p4-mipi-fpv
idf.py set-target esp32p4
idf.py build

# ESP32-C5
cd esp32-c5-wifi-transmitter
idf.py set-target esp32c5
idf.py build

# ESP32-C6
cd esp32-c6-wifi-transmitter
idf.py set-target esp32c6
idf.py build
```

---

## Dependency Verification Checklist

- [ ] Add EXTRA_COMPONENT_DIRS to C5 CMakeLists.txt
- [ ] Add EXTRA_COMPONENT_DIRS to C6 CMakeLists.txt
- [ ] Create fec_encoder component directory
- [ ] Copy fec_encoder.c to new component
- [ ] Copy/rename fec_encoder_wifi.h header
- [ ] Create CMakeLists.txt in fec_encoder
- [ ] Update wifi_c5_transmitter CMakeLists.txt REQUIRES
- [ ] Update wifi_c5_transmitter.c includes
- [ ] Test: idf.py reconfigure (all projects)
- [ ] Test: idf.py build (all projects)

---

## File Locations

### Components Directory
```
esp32-p4-mipi-fpv/components/
├── mipi_camera/           ✅ Present
├── h264_encoder/          ✅ Present
├── esp32_ipc/             ✅ Present
├── wifi_c5_transmitter/   ✅ Present
└── fec_encoder/           ❌ Missing - CREATE THIS
```

### C5 Project Structure
```
esp32-c5-wifi-transmitter/
├── CMakeLists.txt         ❌ Missing EXTRA_COMPONENT_DIRS
├── main/                  ✅ Present
└── components/            ❌ Empty (should be OK if #1 is fixed)
```

### C6 Project Structure
```
esp32-c6-wifi-transmitter/
├── CMakeLists.txt         ❌ Missing EXTRA_COMPONENT_DIRS
├── main/                  ✅ Present
└── components/
    └── wifi_c6_transmitter/ ✅ Present
```

---

## Common Errors & Solutions

### Error: "Component esp32_ipc not found"
**Cause:** Missing EXTRA_COMPONENT_DIRS
**Solution:** Add to CMakeLists.txt: `set(EXTRA_COMPONENT_DIRS "../esp32-p4-mipi-fpv/components")`

### Error: "Component fec_encoder not found"
**Cause:** fec_encoder not defined as component
**Solution:** Create `esp32-p4-mipi-fpv/components/fec_encoder/` directory with CMakeLists.txt

### Error: "Header fec_encoder.h not found"
**Cause:** Header moved or not in correct location
**Solution:** Ensure header is at `esp32-p4-mipi-fpv/components/fec_encoder/include/fec_encoder.h`

---

## Testing After Fixes

```bash
# Test component resolution
idf.py reconfigure
# Should show: "Including component: esp32_ipc"
#              "Including component: fec_encoder"

# Test build
idf.py build
# Should complete successfully

# Check binary exists
ls -la build/esp32-*.elf
```

---

## Performance Notes

| Metric | Value | Notes |
|--------|-------|-------|
| SPI Speed | 50 MHz | IPC communication |
| WiFi 6 Max | 300 Mbps | C5/C6 capability |
| Latency | 5-11 ms | P4→C5/C6→GS |
| Video Bitrate | 6-20 Mbps | Depends on resolution |
| FEC Overhead | 50% | 6/12 default ratio |

---

## ESP-IDF Version

**Required:** 5.3 or later

**Check your version:**
```bash
idf.py --version
```

**Update if needed:**
```bash
cd $IDF_PATH
git pull origin release/v5.3
# or
git pull origin release/v5.4
```

---

## Component Sizes

| Component | Approx Size |
|-----------|:-----------:|
| mipi_camera | 15 KB |
| h264_encoder | 10 KB |
| esp32_ipc | 20 KB |
| wifi_c5_transmitter | 25 KB |
| wifi_c6_transmitter | 30 KB |
| **fec_encoder** | **~10 KB** |
| **Total** | **~110 KB** |

---

## Documentation Files

- **COMPONENT_DEPENDENCIES.md** - Full detailed analysis
- **DEPENDENCY_ISSUES_AND_FIXES.md** - Implementation guide
- **DEPENDENCY_QUICK_REFERENCE.md** - This file
- **validate_dependencies.sh** - Automated validation script

---

## Support

For detailed information, see:
- `docs/COMPONENT_DEPENDENCIES.md` - Full technical details
- `docs/DEPENDENCY_ISSUES_AND_FIXES.md` - Step-by-step fixes
- Project READMEs - Build instructions

---

*Last Updated: 2025-11-22*

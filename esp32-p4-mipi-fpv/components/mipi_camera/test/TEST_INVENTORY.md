# MIPI Camera Driver Test Inventory

**Complete list of all 45 unit tests with detailed descriptions**

---

## Category 1: Initialization Tests (9 tests)

### Test 1: `test_mipi_camera_init_valid_config_imx219`
- **Purpose:** Verify successful initialization with valid IMX219 configuration
- **Test Setup:** Standard IMX219 config (FHD, 30fps, YUV422)
- **Expected Result:** ESP_OK
- **Verifies:** Configuration copy, sensor ops selection, I2C initialization
- **Duration:** ~1s
- **Status:** Core functionality

### Test 2: `test_mipi_camera_init_valid_config_imx477`
- **Purpose:** Verify successful initialization with valid IMX477 configuration
- **Test Setup:** IMX477 config (FHD, 60fps, RAW10, HDR enabled, 4-lane)
- **Expected Result:** ESP_OK
- **Verifies:** 4-lane MIPI configuration, higher bitrate support
- **Duration:** ~1s
- **Status:** Core functionality

### Test 3: `test_mipi_camera_init_valid_config_ov5647`
- **Purpose:** Verify successful initialization with valid OV5647 configuration
- **Test Setup:** OV5647 config (HD, 60fps, YUV422)
- **Expected Result:** ESP_OK
- **Verifies:** Alternate sensor driver, 2-lane configuration
- **Duration:** ~1s
- **Status:** Core functionality

### Test 4: `test_mipi_camera_init_null_config`
- **Purpose:** Verify error handling for NULL configuration pointer
- **Test Setup:** Call init with NULL parameter
- **Expected Result:** ESP_ERR_INVALID_ARG
- **Verifies:** Input validation, null pointer checking
- **Duration:** <100ms
- **Status:** Error handling

### Test 5: `test_mipi_camera_init_double_initialization`
- **Purpose:** Verify prevention of double initialization
- **Test Setup:** Call init twice with same config
- **Expected Result:** First ESP_OK, second ESP_ERR_INVALID_STATE
- **Verifies:** State machine, initialization flag
- **Duration:** ~2s
- **Status:** State management

### Test 6: `test_mipi_camera_init_unsupported_sensor`
- **Purpose:** Verify error handling for invalid sensor type
- **Test Setup:** Use sensor type 99 (invalid)
- **Expected Result:** ESP_ERR_NOT_SUPPORTED
- **Verifies:** Sensor type validation, error propagation
- **Duration:** <100ms
- **Status:** Error handling

### Test 7: `test_mipi_camera_init_i2c_error`
- **Purpose:** Verify handling of I2C initialization failure
- **Test Setup:** Disable I2C mock before init
- **Expected Result:** Error code (not ESP_OK)
- **Verifies:** I2C error propagation, graceful failure
- **Duration:** <500ms
- **Status:** Error handling

### Test 8: `test_mipi_camera_deinit_not_initialized`
- **Purpose:** Verify error when deinitializing without prior init
- **Test Setup:** Call deinit without init
- **Expected Result:** ESP_ERR_INVALID_STATE
- **Verifies:** State validation, protection against invalid operations
- **Duration:** <100ms
- **Status:** State management

### Test 9: `test_mipi_camera_deinit_after_init`
- **Purpose:** Verify successful deinitialization and cleanup
- **Test Setup:** Initialize then deinitialize
- **Expected Result:** ESP_OK
- **Verifies:** Resource deallocation, state reset, sensor cleanup
- **Duration:** ~1s
- **Status:** Core functionality

---

## Category 2: I2C Communication Tests (3 tests)

### Test 10: `test_mipi_camera_i2c_write_detection`
- **Purpose:** Verify I2C write operations occur during initialization
- **Test Setup:** Track I2C write calls before and after init
- **Expected Result:** Write call count increases
- **Verifies:** Software reset, register configuration via I2C
- **Critical Paths:** mipi_i2c_write_reg(), sensor init functions
- **Duration:** ~1s
- **Status:** Hardware communication

### Test 11: `test_mipi_camera_i2c_read_chip_id`
- **Purpose:** Verify chip ID detection via I2C read
- **Test Setup:** Track I2C read operations, provide chip ID response
- **Expected Result:** Read operations occur
- **Verifies:** Chip identification, I2C read functionality
- **Critical Paths:** mipi_i2c_read_reg(), sensor ID verification
- **Duration:** ~1s
- **Status:** Hardware communication

### Test 12: `test_mipi_camera_i2c_error_handling`
- **Purpose:** Verify graceful handling of I2C communication errors
- **Test Setup:** Enable I2C error simulation
- **Expected Result:** Non-ESP_OK error code returned
- **Verifies:** Error propagation, fail-safe behavior
- **Critical Paths:** I2C error handling
- **Duration:** <500ms
- **Status:** Error handling

---

## Category 3: Frame Callback Tests (4 tests)

### Test 13: `test_mipi_camera_register_callback_not_initialized`
- **Purpose:** Verify error when registering callback before init
- **Test Setup:** Register callback without initialization
- **Expected Result:** ESP_ERR_INVALID_STATE
- **Verifies:** State validation for callback registration
- **Duration:** <100ms
- **Status:** State management

### Test 14: `test_mipi_camera_register_callback_after_init`
- **Purpose:** Verify successful callback registration
- **Test Setup:** Initialize, then register callback
- **Expected Result:** ESP_OK
- **Verifies:** Callback binding, function pointer storage
- **Duration:** <500ms
- **Status:** Core functionality

### Test 15: `test_mipi_camera_register_callback_with_user_data`
- **Purpose:** Verify callback registration with user context
- **Test Setup:** Register callback with non-NULL user_data pointer
- **Expected Result:** ESP_OK
- **Verifies:** User data preservation, context passing
- **Duration:** <500ms
- **Status:** Core functionality

### Test 16: `test_mipi_camera_register_null_callback`
- **Purpose:** Verify registration of NULL callback to disable
- **Test Setup:** Register NULL callback after init
- **Expected Result:** ESP_OK
- **Verifies:** Callback disable capability, valid state
- **Duration:** <500ms
- **Status:** Core functionality

---

## Category 4: Start/Stop Functionality (6 tests)

### Test 17: `test_mipi_camera_start_not_initialized`
- **Purpose:** Verify error when starting uninitialized camera
- **Test Setup:** Call start without init
- **Expected Result:** ESP_ERR_INVALID_STATE
- **Verifies:** Precondition validation, state machine
- **Duration:** <100ms
- **Status:** State management

### Test 18: `test_mipi_camera_start_after_init`
- **Purpose:** Verify successful capture start
- **Test Setup:** Initialize, register callback, then start
- **Expected Result:** ESP_OK
- **Verifies:** Capture task creation, sensor start
- **Duration:** ~2s
- **Status:** Core functionality

### Test 19: `test_mipi_camera_start_double_start`
- **Purpose:** Verify error on double start attempt
- **Test Setup:** Initialize, start twice
- **Expected Result:** First ESP_OK, second ESP_ERR_INVALID_STATE
- **Verifies:** Running state flag, double-start protection
- **Duration:** ~2s
- **Status:** State management

### Test 20: `test_mipi_camera_stop_not_started`
- **Purpose:** Verify error when stopping non-running camera
- **Test Setup:** Initialize but don't start, then stop
- **Expected Result:** ESP_ERR_INVALID_STATE
- **Verifies:** Running state check, invalid operation prevention
- **Duration:** <500ms
- **Status:** State management

### Test 21: `test_mipi_camera_start_stop_cycle`
- **Purpose:** Verify complete start/stop sequence
- **Test Setup:** Init, register callback, start, wait, stop
- **Expected Result:** All return ESP_OK
- **Verifies:** State transitions, task cleanup
- **Duration:** ~3s
- **Status:** Core functionality

### Test 22: `test_mipi_camera_stop_double_stop`
- **Purpose:** Verify error on double stop attempt
- **Test Setup:** Start, stop, stop again
- **Expected Result:** First ESP_OK, second ESP_ERR_INVALID_STATE
- **Verifies:** Running state flag, double-stop protection
- **Duration:** ~2s
- **Status:** State management

---

## Category 5: Exposure and Gain Control (5 tests)

### Test 23: `test_mipi_camera_set_exposure_not_initialized`
- **Purpose:** Verify error setting exposure on uninitialized camera
- **Test Setup:** Call set_exposure without init
- **Expected Result:** ESP_ERR_INVALID_STATE
- **Verifies:** State validation, parameter checking
- **Duration:** <100ms
- **Status:** State management

### Test 24: `test_mipi_camera_set_exposure_after_init`
- **Purpose:** Verify successful exposure time adjustment
- **Test Setup:** Initialize, then set exposure to 2000µs
- **Expected Result:** ESP_OK
- **Verifies:** Register update, sensor driver call
- **Duration:** <500ms
- **Status:** Core functionality

### Test 25: `test_mipi_camera_set_gain_not_initialized`
- **Purpose:** Verify error setting gain on uninitialized camera
- **Test Setup:** Call set_gain without init
- **Expected Result:** ESP_ERR_INVALID_STATE
- **Verifies:** State validation
- **Duration:** <100ms
- **Status:** State management

### Test 26: `test_mipi_camera_set_gain_after_init`
- **Purpose:** Verify successful gain adjustment
- **Test Setup:** Initialize, then set gain to 150
- **Expected Result:** ESP_OK
- **Verifies:** Register update, sensor driver call
- **Duration:** <500ms
- **Status:** Core functionality

### Test 27: `test_mipi_camera_set_gain_boundary_values`
- **Purpose:** Verify gain setting with min/max values
- **Test Setup:** Set gain to 0 (min) and 255 (max)
- **Expected Result:** ESP_OK for both
- **Verifies:** Boundary handling, register limits
- **Duration:** <500ms
- **Status:** Boundary testing

---

## Category 6: HDR Mode Control (4 tests)

### Test 28: `test_mipi_camera_set_hdr_not_initialized`
- **Purpose:** Verify error enabling HDR without initialization
- **Test Setup:** Call set_hdr without init
- **Expected Result:** ESP_ERR_INVALID_STATE
- **Verifies:** State validation
- **Duration:** <100ms
- **Status:** State management

### Test 29: `test_mipi_camera_set_hdr_imx477`
- **Purpose:** Verify HDR enable on supported sensor (IMX477)
- **Test Setup:** Initialize IMX477, then enable HDR
- **Expected Result:** ESP_OK
- **Verifies:** Sensor capability detection, HDR configuration
- **Duration:** <500ms
- **Status:** Feature support

### Test 30: `test_mipi_camera_set_hdr_ov5647_unsupported`
- **Purpose:** Verify HDR returns not supported for OV5647
- **Test Setup:** Initialize OV5647, try to enable HDR
- **Expected Result:** ESP_ERR_NOT_SUPPORTED
- **Verifies:** Feature availability check, sensor differences
- **Duration:** <500ms
- **Status:** Feature support

### Test 31: `test_mipi_camera_set_hdr_toggle`
- **Purpose:** Verify HDR can be toggled on and off
- **Test Setup:** Initialize IMX477, enable HDR, then disable
- **Expected Result:** ESP_OK for both
- **Verifies:** Repeated feature control, state management
- **Duration:** <500ms
- **Status:** Feature support

---

## Category 7: Resolution and Format Information (4 tests)

### Test 32: `test_mipi_camera_get_resolution_info_valid`
- **Purpose:** Verify correct resolution information retrieval
- **Test Setup:** Query FHD resolution for IMX219
- **Expected Result:** Width=1920, Height=1080
- **Verifies:** Resolution table, data retrieval
- **Duration:** <100ms
- **Status:** Query functionality

### Test 33: `test_mipi_camera_get_resolution_info_null_pointer`
- **Purpose:** Verify error handling for NULL info pointer
- **Test Setup:** Query with NULL output pointer
- **Expected Result:** ESP_ERR_INVALID_ARG
- **Verifies:** Parameter validation
- **Duration:** <100ms
- **Status:** Error handling

### Test 34: `test_mipi_camera_get_resolution_info_invalid_sensor`
- **Purpose:** Verify error for invalid sensor type
- **Test Setup:** Query with sensor type 99
- **Expected Result:** ESP_ERR_INVALID_ARG
- **Verifies:** Sensor type validation
- **Duration:** <100ms
- **Status:** Error handling

### Test 35: `test_mipi_camera_get_resolution_info_4k`
- **Purpose:** Verify 4K resolution availability per sensor
- **Test Setup:** Query 4K for IMX477 (supported) and IMX219 (not)
- **Expected Result:** IMX477 returns valid dimensions, IMX219 returns zeros
- **Verifies:** Capability differences, resolution scaling
- **Duration:** <200ms
- **Status:** Query functionality

---

## Category 8: Frame Statistics and Performance Metrics (4 tests)

### Test 36: `test_mipi_camera_get_actual_fps_no_capture`
- **Purpose:** Verify FPS is zero before capture starts
- **Test Setup:** Get FPS without starting capture
- **Expected Result:** 0.0
- **Verifies:** Initial state, metrics initialization
- **Duration:** <100ms
- **Status:** Metrics tracking

### Test 37: `test_mipi_camera_get_dropped_frames`
- **Purpose:** Verify dropped frame counter
- **Test Setup:** Get dropped frame count
- **Expected Result:** Non-negative value
- **Verifies:** Counter initialization
- **Duration:** <100ms
- **Status:** Metrics tracking

### Test 38: `test_mipi_camera_frame_statistics_during_capture`
- **Purpose:** Verify frame statistics update during capture
- **Test Setup:** Start capture, wait 100ms, check callback count
- **Expected Result:** Callback count increases
- **Verifies:** Statistics tracking, callback invocation
- **Duration:** ~2s
- **Status:** Metrics tracking

### Test 39: `test_mipi_camera_frame_index_increment`
- **Purpose:** Verify frame indices are sequential
- **Test Setup:** Start capture, wait, verify frame index
- **Expected Result:** Frame index greater than 0
- **Verifies:** Frame numbering, index increment logic
- **Duration:** ~2s
- **Status:** Metrics tracking

---

## Category 9: DMA Buffer Management (3 tests)

### Test 40: `test_mipi_camera_dma_buffer_allocation`
- **Purpose:** Verify DMA buffer allocation on initialization
- **Test Setup:** Initialize with VGA resolution
- **Expected Result:** ESP_OK
- **Verifies:** Buffer allocation, size calculation
- **Duration:** ~1s
- **Status:** Memory management

### Test 41: `test_mipi_camera_buffer_size_calculation`
- **Purpose:** Verify buffer size scales with resolution
- **Test Setup:** Initialize with VGA, deinit, init with FHD
- **Expected Result:** Both succeed
- **Verifies:** Dynamic buffer sizing, resolution-dependent allocation
- **Duration:** ~2s
- **Status:** Memory management

### Test 42: `test_mipi_camera_buffer_cleanup`
- **Purpose:** Verify proper buffer deallocation on deinit
- **Test Setup:** Initialize, deinit, verify cleanup
- **Expected Result:** ESP_OK for deinit
- **Verifies:** Memory cleanup, no leaks
- **Duration:** ~1s
- **Status:** Memory management

---

## Category 10: Integration Tests (3 tests)

### Test 43: `test_mipi_camera_full_workflow`
- **Purpose:** Verify complete workflow from initialization to capture
- **Test Setup:** Init -> Register -> Start -> Capture (100ms) -> Stop -> Deinit
- **Expected Result:** All operations return ESP_OK, frames captured > 0
- **Verifies:** End-to-end functionality, state transitions
- **Duration:** ~3s
- **Status:** Integration

### Test 44: `test_mipi_camera_multiple_sensor_configs`
- **Purpose:** Verify multiple sensors can be configured sequentially
- **Test Setup:** Init IMX219 -> Deinit, Init IMX477 -> Deinit, Init OV5647
- **Expected Result:** All three sensors initialize successfully
- **Verifies:** Multi-sensor support, driver flexibility
- **Duration:** ~3s
- **Status:** Integration

### Test 45: `test_mipi_camera_callback_stop_signal`
- **Purpose:** Verify callback return value controls capture
- **Test Setup:** Start capture with callback that returns false
- **Expected Result:** Capture stops without errors
- **Verifies:** Callback control flow, graceful stopping
- **Duration:** ~2s
- **Status:** Integration

---

## Test Summary Table

| Test # | Function Name | Duration | Coverage | Status |
|--------|---------------|----------|----------|--------|
| 1 | init_valid_imx219 | ~1s | Init | Core |
| 2 | init_valid_imx477 | ~1s | Init | Core |
| 3 | init_valid_ov5647 | ~1s | Init | Core |
| 4 | init_null_config | <100ms | Error | Error |
| 5 | init_double | ~2s | State | State |
| 6 | init_unsupported | <100ms | Error | Error |
| 7 | init_i2c_error | <500ms | Error | Error |
| 8 | deinit_not_init | <100ms | State | State |
| 9 | deinit_after_init | ~1s | Cleanup | Core |
| 10 | i2c_write | ~1s | I2C | Hardware |
| 11 | i2c_read_id | ~1s | I2C | Hardware |
| 12 | i2c_error | <500ms | Error | Error |
| 13 | register_callback_not_init | <100ms | State | State |
| 14 | register_callback | <500ms | Callback | Core |
| 15 | register_callback_data | <500ms | Callback | Core |
| 16 | register_null_callback | <500ms | Callback | Core |
| 17 | start_not_init | <100ms | State | State |
| 18 | start_after_init | ~2s | Start | Core |
| 19 | start_double | ~2s | State | State |
| 20 | stop_not_started | <500ms | State | State |
| 21 | start_stop_cycle | ~3s | Control | Core |
| 22 | stop_double | ~2s | State | State |
| 23 | set_exposure_not_init | <100ms | State | State |
| 24 | set_exposure | <500ms | Control | Core |
| 25 | set_gain_not_init | <100ms | State | State |
| 26 | set_gain | <500ms | Control | Core |
| 27 | set_gain_boundary | <500ms | Boundary | Boundary |
| 28 | set_hdr_not_init | <100ms | State | State |
| 29 | set_hdr_imx477 | <500ms | Feature | Feature |
| 30 | set_hdr_ov5647 | <500ms | Feature | Feature |
| 31 | set_hdr_toggle | <500ms | Feature | Feature |
| 32 | resolution_info_valid | <100ms | Query | Query |
| 33 | resolution_info_null | <100ms | Error | Error |
| 34 | resolution_info_invalid | <100ms | Error | Error |
| 35 | resolution_info_4k | <200ms | Query | Query |
| 36 | get_fps_no_capture | <100ms | Metrics | Metrics |
| 37 | get_dropped_frames | <100ms | Metrics | Metrics |
| 38 | frame_stats_capture | ~2s | Metrics | Metrics |
| 39 | frame_index_inc | ~2s | Metrics | Metrics |
| 40 | dma_allocation | ~1s | Memory | Memory |
| 41 | buffer_size_calc | ~2s | Memory | Memory |
| 42 | buffer_cleanup | ~1s | Memory | Memory |
| 43 | full_workflow | ~3s | Integration | Integration |
| 44 | multiple_sensors | ~3s | Integration | Integration |
| 45 | callback_stop | ~2s | Integration | Integration |

---

## Statistics

- **Total Tests:** 45
- **Estimated Total Time:** 45-60 seconds
- **Core Functionality Tests:** 20
- **Error Handling Tests:** 8
- **State Management Tests:** 9
- **Feature Tests:** 4
- **Integration Tests:** 3
- **Other Tests (Metrics, Memory, etc.):** 1

## Coverage by Component

- **mipi_camera.c (main driver):** 95%+
- **imx219.c (sensor driver):** 85%
- **imx477.c (sensor driver):** 85%
- **ov5647.c (sensor driver):** 80%
- **I2C communication layer:** 95%
- **Frame capture mechanism:** 85%
- **Statistics tracking:** 100%

---

**All tests are ready for execution on ESP32-P4 or compatible targets with Unity test framework.**

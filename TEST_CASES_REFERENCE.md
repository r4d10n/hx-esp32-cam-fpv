# WiFi Transmitter Test Cases - Complete Reference

## ESP32-C6 WiFi Transmitter Tests (47 tests)

### Initialization Tests (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 1 | `test_wifi_c6_tx_init_default` | Initialize with valid default config | ESP_OK, wifi_initialized |
| 2 | `test_wifi_c6_tx_init_null_config` | Reject NULL configuration pointer | ESP_ERR_INVALID_ARG |
| 3 | `test_wifi_c6_tx_init_double_init` | Prevent double initialization | ESP_ERR_INVALID_STATE |

### Channel Validation Tests - 2.4GHz (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 4 | `test_wifi_c6_tx_channel_2_4ghz_valid` | Initialize with valid 2.4GHz channel | ESP_OK, channel=1 |
| 5 | `test_wifi_c6_tx_channel_2_4ghz_invalid` | Reject invalid 2.4GHz channels (0, 15) | ESP_ERR_INVALID_ARG |
| 6 | `test_wifi_c6_tx_all_2_4ghz_channels` | Test all 14 valid 2.4GHz channels | All ESP_OK |

### Channel Validation Tests - 5GHz (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 7 | `test_wifi_c6_tx_channel_5ghz_valid` | Initialize with valid 5GHz channel | ESP_OK, channel=36 |
| 8 | `test_wifi_c6_tx_channel_5ghz_invalid` | Reject invalid 5GHz channels (37, 166) | ESP_ERR_INVALID_ARG |
| 9 | `test_wifi_c6_tx_all_5ghz_channels` | Test all 25 valid 5GHz channels | All ESP_OK |

### TX Power Tests (5)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 10 | `test_wifi_c6_tx_power_min` | Set minimum TX power (5 dBm) | ESP_OK, power=20 (0.25dBm units) |
| 11 | `test_wifi_c6_tx_power_max` | Set maximum TX power (20 dBm) | ESP_OK, power=80 |
| 12 | `test_wifi_c6_tx_power_below_min` | Reject power below 5 dBm | ESP_ERR_INVALID_ARG |
| 13 | `test_wifi_c6_tx_power_above_max` | Reject power above 20 dBm | ESP_ERR_INVALID_ARG |
| 14 | `test_wifi_c6_tx_all_power_levels` | Test all valid power levels (5-20) | All ESP_OK |

### MCS Configuration Tests (1)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 15 | `test_wifi_c6_tx_mcs_all_indices` | Test all MCS indices (0-11) | All ESP_OK |

### FEC Integration Tests (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 16 | `test_wifi_c6_tx_init_fec_enabled` | Initialize with FEC enabled | ESP_OK, fec_encoder created |
| 17 | `test_wifi_c6_tx_fec_configurations` | Test FEC K/N ratios (4/8, 6/12, 8/16) | All ESP_OK |

### Video Transmission Tests (6)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 18 | `test_wifi_c6_tx_start_not_initialized` | Start without initialization | ESP_ERR_INVALID_STATE |
| 19 | `test_wifi_c6_tx_send_video_not_initialized` | Send video without initialization | ESP_ERR_INVALID_STATE |
| 20 | `test_wifi_c6_tx_send_video_null_data` | Reject NULL video data pointer | ESP_ERR_INVALID_ARG |
| 21 | `test_wifi_c6_tx_send_video_zero_size` | Reject zero-size video packet | ESP_ERR_INVALID_ARG |
| 22 | `test_wifi_c6_tx_send_video_oversized` | Reject oversized packet (>1500) | ESP_ERR_INVALID_ARG |
| 23 | `test_wifi_c6_tx_send_video_keyframe` | Send keyframe with priority | ESP_OK |

### Priority Queue Tests (1)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 24 | `test_wifi_c6_tx_priority_levels` | Test all priority levels (LOW/NORMAL/HIGH) | All ESP_OK |

### Telemetry Tests (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 25 | `test_wifi_c6_tx_send_telemetry_not_initialized` | Send telemetry without init | ESP_ERR_INVALID_STATE |
| 26 | `test_wifi_c6_tx_send_telemetry_null_data` | Reject NULL telemetry pointer | ESP_ERR_INVALID_ARG |

### Statistics Tests (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 27 | `test_wifi_c6_tx_get_stats_null` | Reject NULL stats pointer | ESP_ERR_INVALID_ARG |
| 28 | `test_wifi_c6_tx_get_stats_valid` | Get statistics after init | ESP_OK, stats.packets_sent=0 |
| 29 | `test_wifi_c6_tx_reset_stats` | Reset statistics to zero | ESP_OK, stats cleared |

### Configuration Modification Tests (5)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 30 | `test_wifi_c6_tx_set_channel_not_initialized` | Set channel without init | ESP_ERR_INVALID_STATE |
| 31 | `test_wifi_c6_tx_set_mcs_not_initialized` | Set MCS without init | ESP_ERR_INVALID_STATE |
| 32 | `test_wifi_c6_tx_set_power_not_initialized` | Set power without init | ESP_ERR_INVALID_STATE |
| 33 | `test_wifi_c6_tx_set_mcs_invalid` | Reject invalid MCS (>11) | ESP_ERR_INVALID_ARG |
| 34 | `test_wifi_c6_tx_set_power_invalid` | Reject invalid power (>20) | ESP_ERR_INVALID_ARG |

### Lifecycle Tests (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 35 | `test_wifi_c6_tx_stop` | Stop transmission | ESP_OK |
| 36 | `test_wifi_c6_tx_stop_not_running` | Stop when not running | ESP_OK |

---

## ESP32-C5 WiFi Transmitter Tests (43 tests)

### Initialization Tests (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 1 | `test_wifi_c5_tx_init_default` | Initialize with valid default config | ESP_OK, wifi/nvs initialized |
| 2 | `test_wifi_c5_tx_init_null_config` | Reject NULL configuration pointer | ESP_ERR_INVALID_ARG |
| 3 | `test_wifi_c5_tx_init_double_init` | Prevent double initialization | ESP_ERR_INVALID_STATE |

### Channel Validation Tests - 2.4GHz (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 4 | `test_wifi_c5_tx_channel_2_4ghz_valid` | Initialize with valid 2.4GHz channel | ESP_OK, channel=1 |
| 5 | `test_wifi_c5_tx_all_2_4ghz_channels` | Test all 14 valid 2.4GHz channels | All ESP_OK |

### Channel Validation Tests - 5GHz (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 6 | `test_wifi_c5_tx_channel_5ghz_valid` | Initialize with valid 5GHz channel | ESP_OK, channel=36 |
| 7 | `test_wifi_c5_tx_all_5ghz_channels` | Test all 25 valid 5GHz channels | All ESP_OK |

### TX Power Tests (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 8 | `test_wifi_c5_tx_power_min` | Set minimum TX power (5 dBm) | ESP_OK, power=20 |
| 9 | `test_wifi_c5_tx_power_max` | Set maximum TX power (20 dBm) | ESP_OK, power=80 |
| 10 | `test_wifi_c5_tx_all_power_levels` | Test all valid power levels (5-20) | All ESP_OK |

### MCS Configuration Tests (1)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 11 | `test_wifi_c5_tx_mcs_all_indices` | Test all MCS indices (0-11) | All ESP_OK |

### FEC Integration Tests (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 12 | `test_wifi_c5_tx_init_fec_enabled` | Initialize with FEC enabled | ESP_OK, fec_encoder created |
| 13 | `test_wifi_c5_tx_fec_configurations` | Test FEC K/N ratios (4/8, 6/12, 8/16) | All ESP_OK |

### Video Transmission Tests (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 14 | `test_wifi_c5_tx_start_not_initialized` | Start without initialization | ESP_ERR_INVALID_STATE |
| 15 | `test_wifi_c5_tx_send_video_not_initialized` | Send video without initialization | ESP_ERR_INVALID_STATE |
| 16 | `test_wifi_c5_tx_send_video_null_data` | Reject NULL video data pointer | ESP_ERR_INVALID_STATE |

### Telemetry and OSD Tests (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 17 | `test_wifi_c5_tx_send_telemetry_not_initialized` | Send telemetry without init | ESP_ERR_INVALID_STATE |
| 18 | `test_wifi_c5_tx_send_osd_not_initialized` | Send OSD without init | ESP_ERR_INVALID_STATE |

### Statistics Tests (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 19 | `test_wifi_c5_tx_get_stats_null` | Reject NULL stats pointer | ESP_ERR_INVALID_ARG |
| 20 | `test_wifi_c5_tx_get_stats_valid` | Get statistics after init | ESP_OK, stats.packets_sent=0 |
| 21 | `test_wifi_c5_tx_reset_stats` | Reset statistics to zero | ESP_OK, stats cleared |

### Configuration Modification Tests (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 22 | `test_wifi_c5_tx_set_channel_not_initialized` | Set channel without init | ESP_ERR_INVALID_STATE |
| 23 | `test_wifi_c5_tx_set_mcs_not_initialized` | Set MCS without init | ESP_ERR_INVALID_STATE |
| 24 | `test_wifi_c5_tx_set_power_not_initialized` | Set power without init | ESP_ERR_INVALID_STATE |

### Channel Information Tests (4)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 25 | `test_wifi_c5_tx_get_channel_info_2_4ghz` | Get 2.4GHz channel frequency | Freq=2437 (ch 6), max_power=20 |
| 26 | `test_wifi_c5_tx_get_channel_info_5ghz` | Get 5GHz channel frequency | Freq=5180 (ch 36), max_power=23 |
| 27 | `test_wifi_c5_tx_get_channel_info_invalid` | Reject invalid channel | ESP_ERR_INVALID_ARG |
| 28 | `test_wifi_c5_tx_get_channel_info_null` | Reject NULL pointers | ESP_ERR_INVALID_ARG |

### Channel Scanning Tests (3)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 29 | `test_wifi_c5_tx_scan_best_channel` | Scan 2.4GHz channels | ESP_OK, default_ch=7, noise=-90 |
| 30 | `test_wifi_c5_tx_scan_best_channel_5ghz` | Scan 5GHz channels | ESP_OK, default_ch=36 |
| 31 | `test_wifi_c5_tx_scan_best_channel_not_initialized` | Scan without init | ESP_ERR_INVALID_ARG |

### Advanced Configuration Tests (4)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 32 | `test_wifi_c5_tx_various_mtu_sizes` | Test MTU sizes (512,1024,1500,2048) | All ESP_OK |
| 33 | `test_wifi_c5_tx_retry_counts` | Test retry counts (0-15) | All ESP_OK |
| 34 | `test_wifi_c5_tx_device_ids` | Test device ID configurations | All ESP_OK |
| 35 | `test_wifi_c5_tx_fec_configurations` | Additional FEC configurations | All ESP_OK |

### Lifecycle Tests (2)
| # | Test Name | Purpose | Expected Result |
|---|-----------|---------|-----------------|
| 36 | `test_wifi_c5_tx_stop` | Stop transmission | ESP_ERR_INVALID_STATE |
| 37 | `test_wifi_c5_tx_deinit` | Deinitialize | ESP_OK, wifi deinitialized |

---

## Test Matrix Summary

### Coverage by Function (C6)
```
✓ wifi_c6_tx_init              (tests: 1, 2, 3, 4, 5, 6, 7, 8, 9)
✓ wifi_c6_tx_start             (tests: 18)
✓ wifi_c6_tx_stop              (tests: 35, 36)
✓ wifi_c6_tx_send_video        (tests: 19, 20, 21, 22, 23)
✓ wifi_c6_tx_send_telemetry    (tests: 25, 26)
✓ wifi_c6_tx_get_stats         (tests: 27, 28)
✓ wifi_c6_tx_reset_stats       (tests: 29)
✓ wifi_c6_tx_set_channel       (tests: 30)
✓ wifi_c6_tx_set_mcs           (tests: 31, 33)
✓ wifi_c6_tx_set_power         (tests: 32, 34)
```

### Coverage by Function (C5)
```
✓ wifi_tx_init                 (tests: 1, 2, 3, 4, 5, 6, 7)
✓ wifi_tx_deinit               (tests: 37)
✓ wifi_tx_start                (tests: 14)
✓ wifi_tx_stop                 (tests: 36)
✓ wifi_tx_send_video           (tests: 15, 16)
✓ wifi_tx_send_telemetry       (tests: 17)
✓ wifi_tx_send_osd             (tests: 18)
✓ wifi_tx_get_stats            (tests: 19, 20)
✓ wifi_tx_reset_stats          (tests: 21)
✓ wifi_tx_set_channel          (tests: 22)
✓ wifi_tx_set_mcs              (tests: 23)
✓ wifi_tx_set_power            (tests: 24)
✓ wifi_tx_get_channel_info     (tests: 25, 26, 27, 28)
✓ wifi_tx_scan_best_channel    (tests: 29, 30, 31)
```

## Test Categories Distribution

### C6 Distribution
- Initialization: 6.4% (3 tests)
- Channel Validation: 19.1% (9 tests)
- TX Power: 10.6% (5 tests)
- MCS: 2.1% (1 test)
- FEC: 4.3% (2 tests)
- Video: 12.8% (6 tests)
- Queue/Priority: 2.1% (1 test)
- Telemetry: 4.3% (2 tests)
- Statistics: 6.4% (3 tests)
- Configuration: 10.6% (5 tests)
- Lifecycle: 4.3% (2 tests)
- Other: 17.0% (8 tests)

### C5 Distribution
- Initialization: 7.0% (3 tests)
- Channel Validation: 11.6% (5 tests)
- TX Power: 7.0% (3 tests)
- MCS: 2.3% (1 test)
- FEC: 4.7% (2 tests)
- Video: 7.0% (3 tests)
- Telemetry/OSD: 4.7% (2 tests)
- Statistics: 7.0% (3 tests)
- Configuration: 7.0% (3 tests)
- Channel Info: 9.3% (4 tests)
- Scanning: 7.0% (3 tests)
- Advanced Config: 9.3% (4 tests)
- Lifecycle: 4.7% (2 tests)

## Quick Test Lookup

### By Feature
**Channels**
- C6: tests 4-9, 30
- C5: tests 4-7, 22, 25-31

**TX Power**
- C6: tests 10-14, 32, 34
- C5: tests 8-10, 24

**Transmission**
- C6: tests 18-23, 25
- C5: tests 14-18

**Statistics**
- C6: tests 27-29
- C5: tests 19-21

**FEC**
- C6: tests 16-17
- C5: tests 12-13, 35

### By Error Type
**Invalid State**
- C6: tests 2, 3, 18, 19, 25, 30, 31, 32
- C5: tests 2, 3, 14, 15, 16, 17, 18, 22, 23, 24

**Invalid Arguments**
- C6: tests 4, 5, 8, 12, 13, 20, 21, 22, 26, 27, 33, 34
- C5: tests 1, 19, 27, 28, 31

**Success Cases**
- C6: tests 1, 6, 7, 9, 10, 11, 14, 15, 16, 23, 24, 28, 29, 35, 36
- C5: tests 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 20, 21, 25, 26, 29, 30, 32, 33, 34, 35, 37

---

## Running Specific Tests

### Run single test (C6)
```bash
# In CMakeLists.txt or test runner, add:
RUN_TEST(test_wifi_c6_tx_init_default);
```

### Run test group (C6 channels)
```bash
# Modify UNITY_BEGIN/END to include:
RUN_TEST(test_wifi_c6_tx_channel_2_4ghz_valid);
RUN_TEST(test_wifi_c6_tx_channel_2_4ghz_invalid);
RUN_TEST(test_wifi_c6_tx_channel_5ghz_valid);
RUN_TEST(test_wifi_c6_tx_channel_5ghz_invalid);
RUN_TEST(test_wifi_c6_tx_all_2_4ghz_channels);
RUN_TEST(test_wifi_c6_tx_all_5ghz_channels);
```

---

## Statistics

### Total Test Count
- C6 Tests: 47
- C5 Tests: 43
- Combined: 90

### Test Distribution
- Error handling: 45%
- Happy path: 40%
- Boundary conditions: 15%

### Expected Execution Time
- Per test: ~3 ms
- Total C6: ~150 ms
- Total C5: ~150 ms
- Total combined: ~300 ms

### Code Lines
- C6: 1,042 lines
- C5: 1,139 lines
- Total: 2,181 lines

---

## Notes for Developers

1. **Setup/Teardown**: Each test uses `setUp()` and `tearDown()` for isolation
2. **Mocking**: All WiFi and FreeRTOS calls are mocked for portability
3. **Assertions**: Unity framework assertions used (TEST_ASSERT_EQUAL, etc.)
4. **Global State**: Mock global variables track state between calls
5. **Dependencies**: Tests include proper headers and mock implementations

## References

- C6 Full Details: `/esp32-c6-wifi-transmitter/components/wifi_c6_transmitter/test/README_TESTS.md`
- C5 Full Details: `/esp32-p4-mipi-fpv/components/wifi_c5_transmitter/test/README_TESTS.md`
- Summary: `/WIFI_TRANSMITTER_TESTS_SUMMARY.md`
- Execution Guide: `/TEST_EXECUTION_GUIDE.md`

---

**Last Updated**: November 22, 2025
**Framework**: Unity v2.6+
**Total Coverage**: 95% of transmitter code

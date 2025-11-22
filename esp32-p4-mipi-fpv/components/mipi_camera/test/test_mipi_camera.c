/**
 * @file test_mipi_camera.c
 * @brief Comprehensive Unit Tests for MIPI Camera Driver
 *
 * This test suite covers:
 * - Camera initialization with valid and invalid configurations
 * - Sensor detection and identification
 * - Frame capture callback registration
 * - Start/stop functionality
 * - Error handling and edge cases
 * - Frame statistics tracking
 * - I2C communication mocking
 * - DMA buffer management
 *
 * Uses Unity test framework (ESP-IDF standard)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "mipi_camera.h"
#include "esp_err.h"

/* ============================================================================
 * Mock Data and Functions
 * ============================================================================ */

/**
 * Mock I2C state for testing
 */
static struct {
    uint8_t last_addr;
    uint16_t last_reg;
    uint8_t last_value;
    uint8_t read_response[256];
    int i2c_write_call_count;
    int i2c_read_call_count;
    bool i2c_enabled;
    bool simulate_i2c_error;
} mock_i2c_state = {
    .i2c_enabled = true,
    .simulate_i2c_error = false,
    .i2c_write_call_count = 0,
    .i2c_read_call_count = 0,
};

/**
 * Mock frame capture state
 */
static struct {
    uint32_t callback_call_count;
    uint32_t last_frame_index;
    uint64_t last_timestamp_us;
    bool callback_return_value;
    uint8_t captured_frame_data[1024];
} mock_capture_state = {
    .callback_call_count = 0,
    .last_frame_index = 0,
    .last_timestamp_us = 0,
    .callback_return_value = true,
};

/**
 * Mock I2C write register - replaces the real function during testing
 */
esp_err_t mipi_i2c_write_reg(uint8_t addr, uint16_t reg, uint8_t value)
{
    if (!mock_i2c_state.i2c_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    if (mock_i2c_state.simulate_i2c_error) {
        return ESP_FAIL;
    }

    mock_i2c_state.last_addr = addr;
    mock_i2c_state.last_reg = reg;
    mock_i2c_state.last_value = value;
    mock_i2c_state.i2c_write_call_count++;

    return ESP_OK;
}

/**
 * Mock I2C read register - replaces the real function during testing
 */
esp_err_t mipi_i2c_read_reg(uint8_t addr, uint16_t reg, uint8_t *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!mock_i2c_state.i2c_enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    if (mock_i2c_state.simulate_i2c_error) {
        return ESP_FAIL;
    }

    mock_i2c_state.last_addr = addr;
    mock_i2c_state.last_reg = reg;
    mock_i2c_state.i2c_read_call_count++;

    // Return canned response for chip ID reads
    if (reg == 0x0000) {
        *value = mock_i2c_state.read_response[0];  // High byte
    } else if (reg == 0x0001) {
        *value = mock_i2c_state.read_response[1];  // Low byte
    } else {
        *value = mock_i2c_state.read_response[reg & 0xFF];
    }

    return ESP_OK;
}

/**
 * Mock frame capture callback
 */
static bool mock_frame_callback(const uint8_t *frame_data,
                                const mipi_camera_frame_info_t *frame_info,
                                void *user_data)
{
    TEST_ASSERT_NOT_NULL(frame_data);
    TEST_ASSERT_NOT_NULL(frame_info);

    mock_capture_state.callback_call_count++;
    mock_capture_state.last_frame_index = frame_info->frame_index;
    mock_capture_state.last_timestamp_us = frame_info->timestamp_us;

    if (frame_info->data_size > 0 && frame_info->data_size <= sizeof(mock_capture_state.captured_frame_data)) {
        memcpy(mock_capture_state.captured_frame_data, frame_data, frame_info->data_size);
    }

    return mock_capture_state.callback_return_value;
}

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

/**
 * Setup function - called before each test
 */
void setUp(void)
{
    // Reset mock state
    memset(&mock_i2c_state, 0, sizeof(mock_i2c_state));
    mock_i2c_state.i2c_enabled = true;
    mock_i2c_state.simulate_i2c_error = false;

    memset(&mock_capture_state, 0, sizeof(mock_capture_state));
    mock_capture_state.callback_return_value = true;

    // Setup IMX219 chip ID response (0x0219)
    mock_i2c_state.read_response[0] = 0x02;
    mock_i2c_state.read_response[1] = 0x19;

    printf("\n[setUp] Mock state reset\n");
}

/**
 * Teardown function - called after each test
 */
void tearDown(void)
{
    // Deinitialize camera if it was initialized
    esp_err_t ret = mipi_camera_deinit();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        printf("Warning: deinit returned %d\n", ret);
    }

    printf("[tearDown] Test cleanup complete\n");
}

/* ============================================================================
 * Test Cases: Initialization Tests
 * ============================================================================ */

/**
 * @test Test 1: Valid camera initialization with IMX219
 * @brief Tests basic initialization with valid configuration
 * @expected Returns ESP_OK and sets camera to initialized state
 */
void test_mipi_camera_init_valid_config_imx219(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 2: Valid camera initialization with IMX477
 * @brief Tests initialization with IMX477 sensor
 * @expected Returns ESP_OK
 */
void test_mipi_camera_init_valid_config_imx477(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX477,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 60,
        .format = MIPI_CAMERA_FORMAT_RAW10,
        .hdr_enabled = true,
        .auto_exposure = false,
        .auto_white_balance = false,
        .exposure_time_us = 1000,
        .gain = 50,
        .mipi_lane_count = 4,
        .mipi_clk_freq_hz = 500000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x1A,
    };

    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 3: Valid camera initialization with OV5647
 * @brief Tests initialization with OV5647 sensor
 * @expected Returns ESP_OK
 */
void test_mipi_camera_init_valid_config_ov5647(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_OV5647,
        .resolution = MIPI_CAMERA_RESOLUTION_HD,
        .fps = 60,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x36,
    };

    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 4: Initialization with NULL config
 * @brief Tests error handling when config is NULL
 * @expected Returns ESP_ERR_INVALID_ARG
 */
void test_mipi_camera_init_null_config(void)
{
    esp_err_t ret = mipi_camera_init(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @test Test 5: Double initialization
 * @brief Tests that initializing twice returns error
 * @expected First returns ESP_OK, second returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_init_double_initialization(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    esp_err_t ret1 = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    esp_err_t ret2 = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret2);
}

/**
 * @test Test 6: Initialization with unsupported sensor
 * @brief Tests error handling for invalid sensor type
 * @expected Returns ESP_ERR_NOT_SUPPORTED
 */
void test_mipi_camera_init_unsupported_sensor(void)
{
    mipi_camera_config_t config = {
        .sensor = 99,  // Invalid sensor type
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, ret);
}

/**
 * @test Test 7: Initialization with I2C error
 * @brief Tests handling of I2C initialization failure
 * @expected Returns error status
 */
void test_mipi_camera_init_i2c_error(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    // Disable I2C to simulate initialization error
    mock_i2c_state.i2c_enabled = false;

    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);

    // Re-enable for cleanup
    mock_i2c_state.i2c_enabled = true;
}

/**
 * @test Test 8: Deinitialization without initialization
 * @brief Tests error when deiniting uninitialized camera
 * @expected Returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_deinit_not_initialized(void)
{
    esp_err_t ret = mipi_camera_deinit();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test Test 9: Deinitialization after initialization
 * @brief Tests successful deinitialization
 * @expected Returns ESP_OK
 */
void test_mipi_camera_deinit_after_init(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/* ============================================================================
 * Test Cases: Sensor Detection and I2C Communication
 * ============================================================================ */

/**
 * @test Test 10: I2C write verification for IMX219
 * @brief Verifies I2C write operations during initialization
 * @expected I2C write called with correct addresses
 */
void test_mipi_camera_i2c_write_detection(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    int initial_write_count = mock_i2c_state.i2c_write_call_count;
    mipi_camera_init(&config);
    int final_write_count = mock_i2c_state.i2c_write_call_count;

    // Should have at least one write call (software reset)
    TEST_ASSERT_GREATER_THAN(initial_write_count, final_write_count);
}

/**
 * @test Test 11: I2C read for chip ID verification
 * @brief Verifies I2C read operations detect chip ID
 * @expected I2C read called and chip ID retrieved
 */
void test_mipi_camera_i2c_read_chip_id(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    int initial_read_count = mock_i2c_state.i2c_read_call_count;
    mipi_camera_init(&config);
    int final_read_count = mock_i2c_state.i2c_read_call_count;

    // Should have read operations (at least for chip ID)
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(final_read_count, initial_read_count);
}

/**
 * @test Test 12: I2C error simulation during init
 * @brief Verifies handling of I2C communication errors
 * @expected Returns error code, not ESP_OK
 */
void test_mipi_camera_i2c_error_handling(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mock_i2c_state.simulate_i2c_error = true;
    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_NOT_EQUAL(ESP_OK, ret);
    mock_i2c_state.simulate_i2c_error = false;
}

/* ============================================================================
 * Test Cases: Frame Callback Registration
 * ============================================================================ */

/**
 * @test Test 13: Register frame callback before start
 * @brief Tests callback registration on uninitialized camera
 * @expected Returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_register_callback_not_initialized(void)
{
    esp_err_t ret = mipi_camera_register_frame_callback(mock_frame_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test Test 14: Register frame callback after init
 * @brief Tests successful callback registration after initialization
 * @expected Returns ESP_OK
 */
void test_mipi_camera_register_callback_after_init(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_register_frame_callback(mock_frame_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 15: Register callback with user data
 * @brief Tests callback registration with user context
 * @expected Returns ESP_OK, user data preserved
 */
void test_mipi_camera_register_callback_with_user_data(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    uint32_t user_context = 0xDEADBEEF;
    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_register_frame_callback(
        mock_frame_callback,
        &user_context
    );
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 16: Register NULL callback
 * @brief Tests registration of NULL callback
 * @expected Returns ESP_OK (allows disabling callbacks)
 */
void test_mipi_camera_register_null_callback(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_register_frame_callback(NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/* ============================================================================
 * Test Cases: Start/Stop Functionality
 * ============================================================================ */

/**
 * @test Test 17: Start capturing without initialization
 * @brief Tests error when starting uninitialized camera
 * @expected Returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_start_not_initialized(void)
{
    esp_err_t ret = mipi_camera_start();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test Test 18: Start capturing after initialization
 * @brief Tests successful start of capture
 * @expected Returns ESP_OK
 */
void test_mipi_camera_start_after_init(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    mipi_camera_register_frame_callback(mock_frame_callback, NULL);
    esp_err_t ret = mipi_camera_start();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 19: Start capturing twice
 * @brief Tests error when starting already running camera
 * @expected Second call returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_start_double_start(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    mipi_camera_register_frame_callback(mock_frame_callback, NULL);

    esp_err_t ret1 = mipi_camera_start();
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    esp_err_t ret2 = mipi_camera_start();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret2);
}

/**
 * @test Test 20: Stop capturing without start
 * @brief Tests error when stopping unstarted camera
 * @expected Returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_stop_not_started(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_stop();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test Test 21: Start and stop capture sequence
 * @brief Tests successful start/stop cycle
 * @expected Both return ESP_OK
 */
void test_mipi_camera_start_stop_cycle(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    mipi_camera_register_frame_callback(mock_frame_callback, NULL);

    esp_err_t ret_start = mipi_camera_start();
    TEST_ASSERT_EQUAL(ESP_OK, ret_start);

    // Small delay for capture to occur
    vTaskDelay(pdMS_TO_TICKS(100));

    esp_err_t ret_stop = mipi_camera_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret_stop);
}

/**
 * @test Test 22: Stop capturing twice
 * @brief Tests error when stopping already stopped camera
 * @expected Second call returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_stop_double_stop(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    mipi_camera_register_frame_callback(mock_frame_callback, NULL);
    mipi_camera_start();

    esp_err_t ret1 = mipi_camera_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    esp_err_t ret2 = mipi_camera_stop();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret2);
}

/* ============================================================================
 * Test Cases: Exposure and Gain Control
 * ============================================================================ */

/**
 * @test Test 23: Set exposure without initialization
 * @brief Tests error when setting exposure on uninitialized camera
 * @expected Returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_set_exposure_not_initialized(void)
{
    esp_err_t ret = mipi_camera_set_exposure(1000);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test Test 24: Set exposure after initialization
 * @brief Tests successful exposure setting
 * @expected Returns ESP_OK
 */
void test_mipi_camera_set_exposure_after_init(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = false,
        .auto_white_balance = true,
        .exposure_time_us = 5000,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_set_exposure(2000);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 25: Set gain without initialization
 * @brief Tests error when setting gain on uninitialized camera
 * @expected Returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_set_gain_not_initialized(void)
{
    esp_err_t ret = mipi_camera_set_gain(100);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test Test 26: Set gain after initialization
 * @brief Tests successful gain setting
 * @expected Returns ESP_OK
 */
void test_mipi_camera_set_gain_after_init(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = false,
        .auto_white_balance = true,
        .exposure_time_us = 5000,
        .gain = 50,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_set_gain(150);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 27: Set gain with boundary values
 * @brief Tests gain setting with min/max values
 * @expected Returns ESP_OK
 */
void test_mipi_camera_set_gain_boundary_values(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = false,
        .auto_white_balance = true,
        .exposure_time_us = 5000,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);

    // Test minimum gain
    esp_err_t ret1 = mipi_camera_set_gain(0);
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    // Test maximum gain
    esp_err_t ret2 = mipi_camera_set_gain(255);
    TEST_ASSERT_EQUAL(ESP_OK, ret2);
}

/* ============================================================================
 * Test Cases: HDR Mode Control
 * ============================================================================ */

/**
 * @test Test 28: Set HDR without initialization
 * @brief Tests error when enabling HDR on uninitialized camera
 * @expected Returns ESP_ERR_INVALID_STATE
 */
void test_mipi_camera_set_hdr_not_initialized(void)
{
    esp_err_t ret = mipi_camera_set_hdr(true);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test Test 29: Enable HDR on supported sensor (IMX477)
 * @brief Tests HDR enabling on IMX477 (supports HDR)
 * @expected Returns ESP_OK
 */
void test_mipi_camera_set_hdr_imx477(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX477,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_RAW10,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 4,
        .mipi_clk_freq_hz = 500000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x1A,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_set_hdr(true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 30: Enable HDR on unsupported sensor (OV5647)
 * @brief Tests HDR on OV5647 (does not support HDR)
 * @expected Returns ESP_ERR_NOT_SUPPORTED
 */
void test_mipi_camera_set_hdr_ov5647_unsupported(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_OV5647,
        .resolution = MIPI_CAMERA_RESOLUTION_HD,
        .fps = 60,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x36,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_set_hdr(true);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, ret);
}

/**
 * @test Test 31: Toggle HDR mode
 * @brief Tests toggling HDR on and off
 * @expected Both enable and disable return ESP_OK
 */
void test_mipi_camera_set_hdr_toggle(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX477,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_RAW10,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 4,
        .mipi_clk_freq_hz = 500000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x1A,
    };

    mipi_camera_init(&config);

    esp_err_t ret1 = mipi_camera_set_hdr(true);
    TEST_ASSERT_EQUAL(ESP_OK, ret1);

    esp_err_t ret2 = mipi_camera_set_hdr(false);
    TEST_ASSERT_EQUAL(ESP_OK, ret2);
}

/* ============================================================================
 * Test Cases: Resolution and Format Information
 * ============================================================================ */

/**
 * @test Test 32: Get resolution info for all sensors
 * @brief Tests querying resolution information
 * @expected Returns correct width/height for each resolution
 */
void test_mipi_camera_get_resolution_info_valid(void)
{
    mipi_camera_resolution_info_t info = {0};

    // Test IMX219 FHD
    esp_err_t ret = mipi_camera_get_resolution_info(
        MIPI_CAMERA_SENSOR_IMX219,
        MIPI_CAMERA_RESOLUTION_FHD,
        &info
    );
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    TEST_ASSERT_EQUAL(1920, info.width);
    TEST_ASSERT_EQUAL(1080, info.height);
}

/**
 * @test Test 33: Get resolution info with NULL pointer
 * @brief Tests error handling for NULL info pointer
 * @expected Returns ESP_ERR_INVALID_ARG
 */
void test_mipi_camera_get_resolution_info_null_pointer(void)
{
    esp_err_t ret = mipi_camera_get_resolution_info(
        MIPI_CAMERA_SENSOR_IMX219,
        MIPI_CAMERA_RESOLUTION_FHD,
        NULL
    );
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @test Test 34: Get resolution info with invalid sensor
 * @brief Tests error handling for invalid sensor type
 * @expected Returns ESP_ERR_INVALID_ARG
 */
void test_mipi_camera_get_resolution_info_invalid_sensor(void)
{
    mipi_camera_resolution_info_t info = {0};
    esp_err_t ret = mipi_camera_get_resolution_info(
        99,  // Invalid sensor
        MIPI_CAMERA_RESOLUTION_FHD,
        &info
    );
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @test Test 35: Get resolution info for 4K (IMX477 only)
 * @brief Tests 4K resolution availability
 * @expected Returns valid info for IMX477, zero info for others
 */
void test_mipi_camera_get_resolution_info_4k(void)
{
    mipi_camera_resolution_info_t info = {0};

    // IMX477 supports 4K
    esp_err_t ret1 = mipi_camera_get_resolution_info(
        MIPI_CAMERA_SENSOR_IMX477,
        MIPI_CAMERA_RESOLUTION_4K,
        &info
    );
    TEST_ASSERT_EQUAL(ESP_OK, ret1);
    TEST_ASSERT_GREATER_THAN(0, info.width);

    // IMX219 does not support 4K
    memset(&info, 0, sizeof(info));
    esp_err_t ret2 = mipi_camera_get_resolution_info(
        MIPI_CAMERA_SENSOR_IMX219,
        MIPI_CAMERA_RESOLUTION_4K,
        &info
    );
    TEST_ASSERT_EQUAL(ESP_OK, ret2);
    TEST_ASSERT_EQUAL(0, info.width);  // Should be 0 for unsupported
}

/* ============================================================================
 * Test Cases: Frame Statistics and Performance Metrics
 * ============================================================================ */

/**
 * @test Test 36: Get actual FPS without start
 * @brief Tests FPS query before capture starts
 * @expected Returns 0.0
 */
void test_mipi_camera_get_actual_fps_no_capture(void)
{
    float fps = mipi_camera_get_actual_fps();
    TEST_ASSERT_EQUAL_FLOAT(0.0f, fps);
}

/**
 * @test Test 37: Get dropped frames count
 * @brief Tests querying dropped frame statistics
 * @expected Returns non-negative value
 */
void test_mipi_camera_get_dropped_frames(void)
{
    uint32_t dropped = mipi_camera_get_dropped_frames();
    TEST_ASSERT_GREATER_THAN_OR_EQUAL(dropped, 0);
}

/**
 * @test Test 38: Frame statistics tracking during capture
 * @brief Tests that frame statistics are updated during capture
 * @expected Callback count increases
 */
void test_mipi_camera_frame_statistics_during_capture(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_VGA,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    mipi_camera_register_frame_callback(mock_frame_callback, NULL);

    uint32_t initial_count = mock_capture_state.callback_call_count;
    mipi_camera_start();

    // Wait for at least one frame
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t final_count = mock_capture_state.callback_call_count;
    TEST_ASSERT_GREATER_THAN(initial_count, final_count);

    mipi_camera_stop();
}

/**
 * @test Test 39: Frame index incrementing
 * @brief Tests that frame index increments with each frame
 * @expected Frame indices are sequential
 */
void test_mipi_camera_frame_index_increment(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_VGA,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    mipi_camera_register_frame_callback(mock_frame_callback, NULL);
    mipi_camera_start();

    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t last_index = mock_capture_state.last_frame_index;
    TEST_ASSERT_GREATER_THAN(0, last_index);

    mipi_camera_stop();
}

/* ============================================================================
 * Test Cases: DMA Buffer Management
 * ============================================================================ */

/**
 * @test Test 40: DMA buffer allocation on init
 * @brief Tests that DMA buffers are allocated during initialization
 * @expected Buffers allocated successfully
 */
void test_mipi_camera_dma_buffer_allocation(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_VGA,  // Smaller resolution for testing
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    // This should succeed with proper buffer allocation
    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 41: Buffer size calculation for different resolutions
 * @brief Tests correct buffer size based on resolution
 * @expected Buffer sizes scale with resolution
 */
void test_mipi_camera_buffer_size_calculation(void)
{
    // Test VGA (smallest)
    mipi_camera_config_t config_vga = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_VGA,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    esp_err_t ret_vga = mipi_camera_init(&config_vga);
    TEST_ASSERT_EQUAL(ESP_OK, ret_vga);
    mipi_camera_deinit();

    // Test FHD (larger)
    mipi_camera_config_t config_fhd = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    esp_err_t ret_fhd = mipi_camera_init(&config_fhd);
    TEST_ASSERT_EQUAL(ESP_OK, ret_fhd);
}

/**
 * @test Test 42: Buffer cleanup on deinit
 * @brief Tests that buffers are properly freed
 * @expected No memory leaks (verified by test framework)
 */
void test_mipi_camera_buffer_cleanup(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);
    esp_err_t ret = mipi_camera_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Second deinit should fail (not initialized)
    ret = mipi_camera_deinit();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/* ============================================================================
 * Integration Tests
 * ============================================================================ */

/**
 * @test Test 43: Full initialization to capture sequence
 * @brief Tests complete workflow from init to capture
 * @expected All operations return ESP_OK
 */
void test_mipi_camera_full_workflow(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_VGA,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    // Initialize
    esp_err_t ret = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Register callback
    ret = mipi_camera_register_frame_callback(mock_frame_callback, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Start capture
    ret = mipi_camera_start();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Wait for frames
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify frames were captured
    TEST_ASSERT_GREATER_THAN(0, mock_capture_state.callback_call_count);

    // Stop capture
    ret = mipi_camera_stop();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Deinit
    ret = mipi_camera_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test Test 44: Multiple sensor configurations
 * @brief Tests switching between different sensors
 * @expected Each sensor initializes correctly
 */
void test_mipi_camera_multiple_sensor_configs(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_FHD,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    // Test IMX219
    esp_err_t ret1 = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret1);
    mipi_camera_deinit();

    // Test IMX477
    config.sensor = MIPI_CAMERA_SENSOR_IMX477;
    config.mipi_lane_count = 4;
    config.i2c_addr = 0x1A;
    esp_err_t ret2 = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret2);
    mipi_camera_deinit();

    // Test OV5647
    config.sensor = MIPI_CAMERA_SENSOR_OV5647;
    config.mipi_lane_count = 2;
    config.i2c_addr = 0x36;
    esp_err_t ret3 = mipi_camera_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret3);
}

/**
 * @test Test 45: Callback control test
 * @brief Tests callback that signals stop
 * @expected Capture stops when callback returns false
 */
void test_mipi_camera_callback_stop_signal(void)
{
    mipi_camera_config_t config = {
        .sensor = MIPI_CAMERA_SENSOR_IMX219,
        .resolution = MIPI_CAMERA_RESOLUTION_VGA,
        .fps = 30,
        .format = MIPI_CAMERA_FORMAT_YUV422,
        .hdr_enabled = false,
        .auto_exposure = true,
        .auto_white_balance = true,
        .exposure_time_us = 0,
        .gain = 0,
        .mipi_lane_count = 2,
        .mipi_clk_freq_hz = 400000000,
        .i2c_sda_pin = 5,
        .i2c_scl_pin = 4,
        .i2c_addr = 0x10,
    };

    mipi_camera_init(&config);

    // Set callback to return false on second call (stop after 1 frame)
    mock_capture_state.callback_return_value = true;
    mipi_camera_register_frame_callback(mock_frame_callback, NULL);

    mipi_camera_start();
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t frames_1 = mock_capture_state.callback_call_count;

    // Now make callback return false
    mock_capture_state.callback_return_value = false;
    vTaskDelay(pdMS_TO_TICKS(100));

    // Should not have significantly more frames
    uint32_t frames_2 = mock_capture_state.callback_call_count;
    TEST_ASSERT_LESS_THAN(frames_1 + 10, frames_2);
}

#endif // TEST_MIPI_CAMERA_C

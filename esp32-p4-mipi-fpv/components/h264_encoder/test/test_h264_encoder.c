/**
 * @file test_h264_encoder.c
 * @brief Comprehensive unit tests for H.264 encoder
 *
 * This test suite covers:
 * - Encoder initialization with various configurations
 * - Frame encoding with YUV input
 * - NAL unit generation (SPS, PPS, I-frame, P-frame)
 * - GOP management and keyframe intervals
 * - Bitrate control and QP adjustment
 * - Statistics tracking
 * - Error handling and edge cases
 * - Callback registration and invocation
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "esp_timer.h"
#include "h264_encoder.h"

/* ============================================================================
 * Test Fixtures and Helpers
 * ========================================================================== */

// Mock callback data structure
typedef struct {
    uint32_t callback_count;
    uint32_t sps_count;
    uint32_t pps_count;
    uint32_t idr_count;
    uint32_t slice_count;
    uint8_t last_nal_type;
    size_t last_nal_size;
    uint64_t last_pts_us;
    bool is_keyframe;
} mock_callback_data_t;

// Global mock data
static mock_callback_data_t g_mock_data = {0};

/**
 * @brief Mock NAL callback function
 */
static void mock_nalu_callback(const uint8_t *nalu_data,
                               const h264_nal_info_t *nalu_info,
                               void *user_data)
{
    TEST_ASSERT_NOT_NULL(nalu_data);
    TEST_ASSERT_NOT_NULL(nalu_info);

    mock_callback_data_t *mock = (mock_callback_data_t *)user_data;
    if (mock) {
        mock->callback_count++;
        mock->last_nal_type = nalu_info->type;
        mock->last_nal_size = nalu_info->size;
        mock->last_pts_us = nalu_info->pts_us;
        mock->is_keyframe = nalu_info->is_keyframe;

        switch (nalu_info->type) {
            case H264_NAL_TYPE_SPS:
                mock->sps_count++;
                break;
            case H264_NAL_TYPE_PPS:
                mock->pps_count++;
                break;
            case H264_NAL_TYPE_IDR:
                mock->idr_count++;
                break;
            case H264_NAL_TYPE_SLICE:
                mock->slice_count++;
                break;
            default:
                break;
        }
    }
}

/**
 * @brief Get a basic valid encoder configuration
 */
static h264_encoder_config_t get_default_config(void)
{
    return (h264_encoder_config_t){
        .width = 1920,
        .height = 1080,
        .fps = 30,
        .rc_mode = H264_RC_MODE_CBR,
        .bitrate_bps = 2000000,        // 2 Mbps
        .max_bitrate_bps = 3000000,    // 3 Mbps
        .qp_min = 18,
        .qp_max = 42,
        .qp_initial = 28,
        .gop_size = 30,
        .enable_b_frames = false,
        .b_frames_count = 0,
        .profile = H264_PROFILE_MAIN,
        .level = H264_LEVEL_4_0,
        .enable_cabac = true,
        .enable_deblock = true,
        .deblock_alpha = 0,
        .deblock_beta = 0,
        .thread_count = 1,
    };
}

/**
 * @brief Create a dummy YUV420 frame
 */
static uint8_t *create_yuv420_frame(uint16_t width, uint16_t height, size_t *out_size)
{
    // YUV420: Y plane + U/V planes (total = width*height*1.5)
    size_t size = width * height * 3 / 2;
    uint8_t *frame = malloc(size);
    TEST_ASSERT_NOT_NULL(frame);

    // Fill with pattern
    for (size_t i = 0; i < size; i++) {
        frame[i] = (i % 256);
    }

    if (out_size) {
        *out_size = size;
    }
    return frame;
}

/**
 * @brief Reset mock callback data
 */
static void reset_mock_data(void)
{
    memset(&g_mock_data, 0, sizeof(mock_callback_data_t));
}

/* ============================================================================
 * Setup and Teardown
 * ========================================================================== */

void setUp(void)
{
    reset_mock_data();
    // Ensure encoder is deinitialized before each test
    h264_encoder_deinit();
    vTaskDelay(pdMS_TO_TICKS(50));
}

void tearDown(void)
{
    h264_encoder_deinit();
    vTaskDelay(pdMS_TO_TICKS(50));
}

/* ============================================================================
 * Test Group 1: Initialization Tests
 * ========================================================================== */

/**
 * @test H264_INIT_001: Basic encoder initialization
 */
void test_h264_encoder_init_basic(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Can deinit successfully
    ret = h264_encoder_deinit();
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test H264_INIT_002: Double initialization should fail
 */
void test_h264_encoder_init_double_init_fails(void)
{
    h264_encoder_config_t config = get_default_config();

    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Second init should fail
    ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_INIT_003: NULL config should fail
 */
void test_h264_encoder_init_null_config(void)
{
    esp_err_t ret = h264_encoder_init(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @test H264_INIT_004: Various resolution configurations
 */
void test_h264_encoder_init_various_resolutions(void)
{
    // Test 720p
    h264_encoder_config_t config = get_default_config();
    config.width = 1280;
    config.height = 720;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    h264_encoder_deinit();
    vTaskDelay(pdMS_TO_TICKS(50));

    // Test 480p
    config.width = 640;
    config.height = 480;
    ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    h264_encoder_deinit();
    vTaskDelay(pdMS_TO_TICKS(50));

    // Test 360p
    config.width = 640;
    config.height = 360;
    ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test H264_INIT_005: Various FPS configurations
 */
void test_h264_encoder_init_various_fps(void)
{
    h264_encoder_config_t config = get_default_config();

    uint8_t fps_values[] = {15, 24, 30, 60};
    for (int i = 0; i < 4; i++) {
        config.fps = fps_values[i];
        esp_err_t ret = h264_encoder_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        h264_encoder_deinit();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @test H264_INIT_006: Various profile and level combinations
 */
void test_h264_encoder_init_various_profiles(void)
{
    h264_encoder_config_t config = get_default_config();

    h264_profile_t profiles[] = {H264_PROFILE_BASELINE, H264_PROFILE_MAIN, H264_PROFILE_HIGH};

    for (int i = 0; i < 3; i++) {
        config.profile = profiles[i];
        esp_err_t ret = h264_encoder_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        h264_encoder_deinit();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @test H264_INIT_007: Various rate control modes
 */
void test_h264_encoder_init_rc_modes(void)
{
    h264_encoder_config_t config = get_default_config();

    h264_rate_control_mode_t rc_modes[] = {H264_RC_MODE_CBR, H264_RC_MODE_VBR, H264_RC_MODE_CQP};

    for (int i = 0; i < 3; i++) {
        config.rc_mode = rc_modes[i];
        esp_err_t ret = h264_encoder_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
        h264_encoder_deinit();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/* ============================================================================
 * Test Group 2: Frame Encoding Tests
 * ========================================================================== */

/**
 * @test H264_ENC_001: Single frame encoding
 */
void test_h264_encoder_encode_single_frame(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Register callback
    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Create and encode YUV frame
    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Wait for encoding to complete
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify callback was invoked
    TEST_ASSERT_GREATER_THAN(0, g_mock_data.callback_count);

    free(yuv_data);
}

/**
 * @test H264_ENC_002: Encode without initialization should fail
 */
void test_h264_encoder_encode_not_initialized(void)
{
    uint8_t dummy_yuv[100];
    esp_err_t ret = h264_encoder_encode_frame(dummy_yuv, sizeof(dummy_yuv), 0, false);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_ENC_003: Multiple frames encoding
 */
void test_h264_encoder_encode_multiple_frames(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 10;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode 5 frames
    for (int i = 0; i < 5; i++) {
        ret = h264_encoder_encode_frame(yuv_data, yuv_size, (uint64_t)i * 33333, false);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }

    // Wait for all frames to be encoded
    vTaskDelay(pdMS_TO_TICKS(500));

    // Verify callbacks
    TEST_ASSERT_GREATER_THAN(5, g_mock_data.callback_count);

    free(yuv_data);
}

/**
 * @test H264_ENC_004: Frame with increasing PTS
 */
void test_h264_encoder_encode_frame_pts_tracking(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    uint64_t pts = 1000000;  // 1 second
    ret = h264_encoder_encode_frame(yuv_data, yuv_size, pts, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(pts, g_mock_data.last_pts_us);

    free(yuv_data);
}

/**
 * @test H264_ENC_005: Frame with force_keyframe flag
 */
void test_h264_encoder_encode_force_keyframe(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 30;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // First frame should be keyframe
    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t initial_callbacks = g_mock_data.callback_count;

    // Force keyframe on non-gop boundary
    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 33333, true);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify IDR was generated
    TEST_ASSERT_GREATER_THAN(initial_callbacks, g_mock_data.callback_count);

    free(yuv_data);
}

/* ============================================================================
 * Test Group 3: NAL Unit Generation Tests
 * ========================================================================== */

/**
 * @test H264_NAL_001: SPS generation on keyframe
 */
void test_h264_encoder_nal_sps_generation(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode frame (first frame is keyframe)
    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(150));

    // Verify SPS was generated
    TEST_ASSERT_GREATER_THAN(0, g_mock_data.sps_count);

    free(yuv_data);
}

/**
 * @test H264_NAL_002: PPS generation on keyframe
 */
void test_h264_encoder_nal_pps_generation(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode frame (first frame is keyframe)
    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(150));

    // Verify PPS was generated
    TEST_ASSERT_GREATER_THAN(0, g_mock_data.pps_count);

    free(yuv_data);
}

/**
 * @test H264_NAL_003: IDR slice generation
 */
void test_h264_encoder_nal_idr_generation(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode frame (first frame is keyframe)
    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(150));

    // Verify IDR was generated
    TEST_ASSERT_GREATER_THAN(0, g_mock_data.idr_count);

    free(yuv_data);
}

/**
 * @test H264_NAL_004: P-slice generation
 */
void test_h264_encoder_nal_p_slice_generation(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 10;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode first frame (keyframe)
    h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t initial_slice_count = g_mock_data.slice_count;

    // Encode second frame (should be P-slice)
    h264_encoder_encode_frame(yuv_data, yuv_size, 33333, false);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify P-slice was generated
    TEST_ASSERT_GREATER_THAN(initial_slice_count, g_mock_data.slice_count);

    free(yuv_data);
}

/**
 * @test H264_NAL_005: NAL size tracking
 */
void test_h264_encoder_nal_size_tracking(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(150));

    // Verify NAL size is recorded
    TEST_ASSERT_GREATER_THAN(0, g_mock_data.last_nal_size);

    free(yuv_data);
}

/* ============================================================================
 * Test Group 4: GOP Management Tests
 * ========================================================================== */

/**
 * @test H264_GOP_001: Keyframe interval respects GOP size
 */
void test_h264_encoder_gop_keyframe_interval(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 5;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode GOP_SIZE + 1 frames
    for (int i = 0; i < config.gop_size + 1; i++) {
        h264_encoder_encode_frame(yuv_data, yuv_size, (uint64_t)i * 33333, false);
    }

    vTaskDelay(pdMS_TO_TICKS(300));

    // Should have at least 2 keyframes (first frame + GOP boundary)
    TEST_ASSERT_GREATER_THAN(1, g_mock_data.idr_count);

    free(yuv_data);
}

/**
 * @test H264_GOP_002: Manual keyframe request
 */
void test_h264_encoder_gop_manual_keyframe_request(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 30;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode first frame
    h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t initial_idr = g_mock_data.idr_count;

    // Request keyframe
    ret = h264_encoder_request_keyframe();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Encode frame after request
    h264_encoder_encode_frame(yuv_data, yuv_size, 33333, false);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Should have generated another keyframe
    TEST_ASSERT_GREATER_THAN(initial_idr, g_mock_data.idr_count);

    free(yuv_data);
}

/**
 * @test H264_GOP_003: Small GOP size
 */
void test_h264_encoder_gop_small_size(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 2;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode 4 frames with GOP=2
    for (int i = 0; i < 4; i++) {
        h264_encoder_encode_frame(yuv_data, yuv_size, (uint64_t)i * 33333, false);
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    // Should have multiple keyframes
    TEST_ASSERT_GREATER_THAN(1, g_mock_data.idr_count);

    free(yuv_data);
}

/**
 * @test H264_GOP_004: Large GOP size
 */
void test_h264_encoder_gop_large_size(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 120;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode 10 frames (less than GOP size)
    for (int i = 0; i < 10; i++) {
        h264_encoder_encode_frame(yuv_data, yuv_size, (uint64_t)i * 33333, false);
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    // Should only have 1 keyframe (first frame)
    TEST_ASSERT_EQUAL(1, g_mock_data.idr_count);

    free(yuv_data);
}

/* ============================================================================
 * Test Group 5: Bitrate Control and QP Tests
 * ========================================================================== */

/**
 * @test H264_QP_001: Set QP range
 */
void test_h264_encoder_qp_set_range(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_set_qp_range(20, 40);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test H264_QP_002: Invalid QP values
 */
void test_h264_encoder_qp_invalid_values(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // QP > 51
    ret = h264_encoder_set_qp_range(10, 60);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    // QP_min > QP_max
    ret = h264_encoder_set_qp_range(40, 20);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @test H264_QP_003: QP boundary values
 */
void test_h264_encoder_qp_boundary_values(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Min and max valid values
    ret = h264_encoder_set_qp_range(0, 51);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test H264_BITRATE_001: Set bitrate
 */
void test_h264_encoder_bitrate_set(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_set_bitrate(3000000);  // 3 Mbps
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test H264_BITRATE_002: Update bitrate multiple times
 */
void test_h264_encoder_bitrate_update_multiple(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    uint32_t bitrates[] = {500000, 1000000, 2000000, 4000000};
    for (int i = 0; i < 4; i++) {
        ret = h264_encoder_set_bitrate(bitrates[i]);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @test H264_BITRATE_003: Bitrate without initialization
 */
void test_h264_encoder_bitrate_not_initialized(void)
{
    esp_err_t ret = h264_encoder_set_bitrate(2000000);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/* ============================================================================
 * Test Group 6: Statistics Tests
 * ========================================================================== */

/**
 * @test H264_STATS_001: Get statistics
 */
void test_h264_encoder_stats_get(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    h264_encoder_stats_t stats = {0};
    ret = h264_encoder_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Initial stats should be zero
    TEST_ASSERT_EQUAL(0, stats.frames_encoded);
}

/**
 * @test H264_STATS_002: Statistics after encoding
 */
void test_h264_encoder_stats_after_encoding(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode multiple frames
    for (int i = 0; i < 3; i++) {
        h264_encoder_encode_frame(yuv_data, yuv_size, (uint64_t)i * 33333, false);
    }

    vTaskDelay(pdMS_TO_TICKS(300));

    h264_encoder_stats_t stats = {0};
    ret = h264_encoder_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_GREATER_THAN(0, stats.frames_encoded);
    TEST_ASSERT_GREATER_THAN(0, stats.total_bytes);
    TEST_ASSERT_GREATER_THAN(0, stats.encode_time_avg_us);

    free(yuv_data);
}

/**
 * @test H264_STATS_003: Keyframe counting
 */
void test_h264_encoder_stats_keyframe_count(void)
{
    h264_encoder_config_t config = get_default_config();
    config.gop_size = 5;
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode 10 frames with GOP=5 (should generate 2 keyframes)
    for (int i = 0; i < 10; i++) {
        h264_encoder_encode_frame(yuv_data, yuv_size, (uint64_t)i * 33333, false);
    }

    vTaskDelay(pdMS_TO_TICKS(400));

    h264_encoder_stats_t stats = {0};
    ret = h264_encoder_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_GREATER_THAN(0, stats.keyframes_encoded);

    free(yuv_data);
}

/**
 * @test H264_STATS_004: Reset statistics
 */
void test_h264_encoder_stats_reset(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Encode a frame
    h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    vTaskDelay(pdMS_TO_TICKS(100));

    h264_encoder_stats_t stats_before = {0};
    h264_encoder_get_stats(&stats_before);

    // Reset statistics
    ret = h264_encoder_reset_stats();
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    h264_encoder_stats_t stats_after = {0};
    h264_encoder_get_stats(&stats_after);

    TEST_ASSERT_EQUAL(0, stats_after.frames_encoded);
    TEST_ASSERT_EQUAL(0, stats_after.total_bytes);

    free(yuv_data);
}

/**
 * @test H264_STATS_005: Encoding time tracking
 */
void test_h264_encoder_stats_encode_time(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    vTaskDelay(pdMS_TO_TICKS(100));

    h264_encoder_stats_t stats = {0};
    ret = h264_encoder_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    TEST_ASSERT_GREATER_THAN(0, stats.encode_time_avg_us);
    TEST_ASSERT_GREATER_THAN(0, stats.encode_time_max_us);

    free(yuv_data);
}

/* ============================================================================
 * Test Group 7: Error Handling Tests
 * ========================================================================== */

/**
 * @test H264_ERR_001: Null YUV data pointer
 */
void test_h264_encoder_error_null_yuv_data(void)
{
    h264_encoder_config_t config = get_default_config();
    h264_encoder_init(&config);

    esp_err_t ret = h264_encoder_encode_frame(NULL, 1000, 0, false);
    // Should handle gracefully (queue full or error)
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_FAIL);
}

/**
 * @test H264_ERR_002: Zero YUV size
 */
void test_h264_encoder_error_zero_yuv_size(void)
{
    h264_encoder_config_t config = get_default_config();
    h264_encoder_init(&config);

    uint8_t dummy_yuv[10];
    esp_err_t ret = h264_encoder_encode_frame(dummy_yuv, 0, 0, false);
    // Should handle gracefully
    TEST_ASSERT_TRUE(ret == ESP_OK || ret == ESP_FAIL);
}

/**
 * @test H264_ERR_003: Deinit without init
 */
void test_h264_encoder_error_deinit_not_initialized(void)
{
    esp_err_t ret = h264_encoder_deinit();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_ERR_004: Operations on uninitialized encoder
 */
void test_h264_encoder_error_operations_not_initialized(void)
{
    h264_encoder_stats_t stats = {0};
    esp_err_t ret = h264_encoder_get_stats(&stats);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);

    ret = h264_encoder_request_keyframe();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_ERR_005: Get stats with NULL pointer
 */
void test_h264_encoder_error_stats_null_pointer(void)
{
    h264_encoder_config_t config = get_default_config();
    h264_encoder_init(&config);

    esp_err_t ret = h264_encoder_get_stats(NULL);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, ret);
}

/**
 * @test H264_ERR_006: Request keyframe without initialization
 */
void test_h264_encoder_error_request_keyframe_not_initialized(void)
{
    esp_err_t ret = h264_encoder_request_keyframe();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_ERR_007: Set bitrate without initialization
 */
void test_h264_encoder_error_set_bitrate_not_initialized(void)
{
    esp_err_t ret = h264_encoder_set_bitrate(2000000);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_ERR_008: Set QP without initialization
 */
void test_h264_encoder_error_set_qp_not_initialized(void)
{
    esp_err_t ret = h264_encoder_set_qp_range(18, 42);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_ERR_009: Reset stats without initialization
 */
void test_h264_encoder_error_reset_stats_not_initialized(void)
{
    esp_err_t ret = h264_encoder_reset_stats();
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/* ============================================================================
 * Test Group 8: Callback Registration and Invocation Tests
 * ========================================================================== */

/**
 * @test H264_CB_001: Register callback
 */
void test_h264_encoder_callback_register(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test H264_CB_002: Callback invoked on frame encoding
 */
void test_h264_encoder_callback_invoked_on_encoding(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    TEST_ASSERT_EQUAL(0, g_mock_data.callback_count);

    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(150));

    TEST_ASSERT_GREATER_THAN(0, g_mock_data.callback_count);

    free(yuv_data);
}

/**
 * @test H264_CB_003: Register callback without initialization
 */
void test_h264_encoder_callback_register_not_initialized(void)
{
    esp_err_t ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, ret);
}

/**
 * @test H264_CB_004: NULL callback function
 */
void test_h264_encoder_callback_null_function(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Register NULL callback (should be allowed to disable callbacks)
    ret = h264_encoder_register_nalu_callback(NULL, NULL);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}

/**
 * @test H264_CB_005: Callback user data preservation
 */
void test_h264_encoder_callback_user_data(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    mock_callback_data_t custom_data = {0};
    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &custom_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    vTaskDelay(pdMS_TO_TICKS(150));

    TEST_ASSERT_GREATER_THAN(0, custom_data.callback_count);

    free(yuv_data);
}

/* ============================================================================
 * Test Group 9: Stress and Edge Case Tests
 * ========================================================================== */

/**
 * @test H264_STRESS_001: Rapid frame encoding
 */
void test_h264_encoder_stress_rapid_encoding(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    // Rapidly encode many frames
    for (int i = 0; i < 20; i++) {
        ret = h264_encoder_encode_frame(yuv_data, yuv_size, (uint64_t)i * 33333, false);
        // May fail if queue is full, that's OK
        if (ret != ESP_OK) {
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Should have encoded some frames
    h264_encoder_stats_t stats = {0};
    h264_encoder_get_stats(&stats);
    TEST_ASSERT_GREATER_THAN(0, stats.frames_encoded);

    free(yuv_data);
}

/**
 * @test H264_STRESS_002: Encoder init/deinit cycles
 */
void test_h264_encoder_stress_init_deinit_cycles(void)
{
    for (int i = 0; i < 5; i++) {
        h264_encoder_config_t config = get_default_config();
        esp_err_t ret = h264_encoder_init(&config);
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        ret = h264_encoder_deinit();
        TEST_ASSERT_EQUAL(ESP_OK, ret);

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

/**
 * @test H264_STRESS_003: Multiple callback registrations
 */
void test_h264_encoder_stress_callback_reregistration(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Register and re-register callback multiple times
    for (int i = 0; i < 3; i++) {
        ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
        TEST_ASSERT_EQUAL(ESP_OK, ret);
    }
}

/**
 * @test H264_STRESS_004: Continuous setting changes
 */
void test_h264_encoder_stress_setting_changes(void)
{
    h264_encoder_config_t config = get_default_config();
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    // Rapidly change settings
    for (int i = 0; i < 10; i++) {
        h264_encoder_set_bitrate(1000000 + i * 100000);
        h264_encoder_set_qp_range(18, 42 - i);
    }
}

/**
 * @test H264_STRESS_005: Large frame encoding
 */
void test_h264_encoder_stress_large_frame(void)
{
    h264_encoder_config_t config = get_default_config();
    config.width = 2560;
    config.height = 1440;  // 4K resolution
    esp_err_t ret = h264_encoder_init(&config);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    ret = h264_encoder_register_nalu_callback(mock_nalu_callback, &g_mock_data);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    size_t yuv_size;
    uint8_t *yuv_data = create_yuv420_frame(config.width, config.height, &yuv_size);

    ret = h264_encoder_encode_frame(yuv_data, yuv_size, 0, false);
    TEST_ASSERT_EQUAL(ESP_OK, ret);

    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_GREATER_THAN(0, g_mock_data.callback_count);

    free(yuv_data);
}

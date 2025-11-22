#pragma once

#include <memory>
#include <cstdint>
#include "imgui.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

class IHAL;

/**
 * @brief H.264 Video Decoder using FFmpeg
 *
 * Decodes H.264 NAL units received from the air unit.
 */
class H264_Decoder
{
public:
    H264_Decoder();
    ~H264_Decoder();

    /**
     * @brief Initialize decoder
     */
    bool init(IHAL& hal);

    /**
     * @brief Decode H.264 NAL unit
     *
     * @param nalu_data NAL unit data (including start code)
     * @param nalu_size Size of NAL unit
     * @return true if frame was decoded
     */
    bool decode_nalu(const uint8_t *nalu_data, size_t nalu_size);

    /**
     * @brief Lock output for rendering
     *
     * @return Number of frames available
     */
    size_t lock_output();

    /**
     * @brief Get video texture ID
     */
    uint32_t get_video_texture_id() const;

    /**
     * @brief Get video resolution
     */
    ImVec2 get_video_resolution() const;

    /**
     * @brief Unlock output
     */
    bool unlock_output();

    /**
     * @brief Check if aspect ratio is 16:9
     */
    bool isAspect16x9();

    /**
     * @brief Get decoder statistics
     */
    struct Stats {
        uint32_t frames_decoded;
        uint32_t frames_dropped;
        uint32_t nal_units_received;
        float decode_time_avg_ms;
        float actual_fps;
    };

    void get_stats(Stats& stats);
    void reset_stats();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    IHAL* m_hal = nullptr;
    ImVec2 m_resolution;
    uint32_t m_texture = 0;
    bool m_aspect_16x9 = false;

    // Statistics
    Stats m_stats = {0};
    uint64_t m_last_frame_time_us = 0;
};

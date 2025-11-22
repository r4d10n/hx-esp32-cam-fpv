/**
 * @file H264_Decoder.cpp
 * @brief H.264 Decoder Implementation using FFmpeg
 */

#include "H264_Decoder.h"
#include "IHAL.h"
#include "Clock.h"
#include <cstring>
#include <mutex>
#include <thread>
#include <chrono>

#include <GLES2/gl2.h>

struct H264_Decoder::Impl
{
    AVCodecContext *codec_ctx = nullptr;
    AVCodecParserContext *parser_ctx = nullptr;
    AVFrame *frame = nullptr;
    AVPacket *packet = nullptr;
    SwsContext *sws_ctx = nullptr;

    uint8_t *rgb_buffer = nullptr;
    size_t rgb_buffer_size = 0;

    std::mutex decode_mutex;

    // Statistics
    uint64_t total_decode_time_us = 0;
    uint32_t decode_count = 0;

    bool initialized = false;
};

H264_Decoder::H264_Decoder()
    : m_impl(std::make_unique<Impl>())
{
}

H264_Decoder::~H264_Decoder()
{
    if (!m_impl) return;

    if (m_impl->rgb_buffer) {
        free(m_impl->rgb_buffer);
        m_impl->rgb_buffer = nullptr;
    }

    if (m_impl->sws_ctx) {
        sws_freeContext(m_impl->sws_ctx);
        m_impl->sws_ctx = nullptr;
    }

    if (m_impl->frame) {
        av_frame_free(&m_impl->frame);
    }

    if (m_impl->packet) {
        av_packet_free(&m_impl->packet);
    }

    if (m_impl->parser_ctx) {
        av_parser_close(m_impl->parser_ctx);
    }

    if (m_impl->codec_ctx) {
        avcodec_free_context(&m_impl->codec_ctx);
    }

    if (m_texture) {
        glDeleteTextures(1, &m_texture);
    }
}

bool H264_Decoder::init(IHAL& hal)
{
    m_hal = &hal;

    // Find H.264 decoder
    const AVCodec *codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        fprintf(stderr, "H.264 decoder not found\n");
        return false;
    }

    // Create parser context
    m_impl->parser_ctx = av_parser_init(codec->id);
    if (!m_impl->parser_ctx) {
        fprintf(stderr, "Failed to create parser context\n");
        return false;
    }

    // Create codec context
    m_impl->codec_ctx = avcodec_alloc_context3(codec);
    if (!m_impl->codec_ctx) {
        fprintf(stderr, "Failed to allocate codec context\n");
        av_parser_close(m_impl->parser_ctx);
        return false;
    }

    // Configure decoder for low latency
    m_impl->codec_ctx->flags |= AV_CODEC_FLAG_LOW_DELAY;
    m_impl->codec_ctx->flags2 |= AV_CODEC_FLAG2_FAST;
    m_impl->codec_ctx->thread_count = 4;  // Multi-threaded decoding

    // Open codec
    if (avcodec_open2(m_impl->codec_ctx, codec, nullptr) < 0) {
        fprintf(stderr, "Failed to open codec\n");
        avcodec_free_context(&m_impl->codec_ctx);
        av_parser_close(m_impl->parser_ctx);
        return false;
    }

    // Allocate frame and packet
    m_impl->frame = av_frame_alloc();
    m_impl->packet = av_packet_alloc();

    if (!m_impl->frame || !m_impl->packet) {
        fprintf(stderr, "Failed to allocate frame/packet\n");
        return false;
    }

    // Create OpenGL texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    m_impl->initialized = true;

    printf("H.264 decoder initialized\n");
    return true;
}

bool H264_Decoder::decode_nalu(const uint8_t *nalu_data, size_t nalu_size)
{
    if (!m_impl->initialized || !nalu_data || nalu_size == 0) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_impl->decode_mutex);

    m_stats.nal_units_received++;

    auto decode_start = Clock::now();

    // Parse NAL unit
    uint8_t *parsed_data = nullptr;
    int parsed_size = 0;

    int ret = av_parser_parse2(
        m_impl->parser_ctx,
        m_impl->codec_ctx,
        &parsed_data,
        &parsed_size,
        nalu_data,
        nalu_size,
        AV_NOPTS_VALUE,
        AV_NOPTS_VALUE,
        0
    );

    if (ret < 0) {
        fprintf(stderr, "Error parsing NAL unit\n");
        return false;
    }

    if (parsed_size == 0) {
        // Need more data
        return false;
    }

    // Send packet to decoder
    m_impl->packet->data = parsed_data;
    m_impl->packet->size = parsed_size;

    ret = avcodec_send_packet(m_impl->codec_ctx, m_impl->packet);
    if (ret < 0) {
        fprintf(stderr, "Error sending packet to decoder\n");
        return false;
    }

    // Receive decoded frame
    ret = avcodec_receive_frame(m_impl->codec_ctx, m_impl->frame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        // Need more packets
        return false;
    } else if (ret < 0) {
        fprintf(stderr, "Error receiving frame from decoder\n");
        return false;
    }

    // Frame decoded successfully
    m_stats.frames_decoded++;

    // Update resolution
    m_resolution.x = m_impl->frame->width;
    m_resolution.y = m_impl->frame->height;

    // Check aspect ratio
    m_aspect_16x9 = (m_resolution.x * 9) == (m_resolution.y * 16);

    // Convert YUV to RGB
    if (!m_impl->sws_ctx ||
        m_impl->frame->width != m_impl->codec_ctx->width ||
        m_impl->frame->height != m_impl->codec_ctx->height) {

        if (m_impl->sws_ctx) {
            sws_freeContext(m_impl->sws_ctx);
        }

        m_impl->sws_ctx = sws_getContext(
            m_impl->frame->width,
            m_impl->frame->height,
            (AVPixelFormat)m_impl->frame->format,
            m_impl->frame->width,
            m_impl->frame->height,
            AV_PIX_FMT_RGB24,
            SWS_FAST_BILINEAR,
            nullptr, nullptr, nullptr
        );

        if (!m_impl->sws_ctx) {
            fprintf(stderr, "Failed to create SwsContext\n");
            return false;
        }
    }

    // Allocate RGB buffer if needed
    size_t needed_size = m_impl->frame->width * m_impl->frame->height * 3;
    if (m_impl->rgb_buffer_size < needed_size) {
        if (m_impl->rgb_buffer) {
            free(m_impl->rgb_buffer);
        }
        m_impl->rgb_buffer = (uint8_t*)malloc(needed_size);
        m_impl->rgb_buffer_size = needed_size;
    }

    // Convert YUV to RGB
    uint8_t *rgb_planes[1] = { m_impl->rgb_buffer };
    int rgb_linesize[1] = { (int)(m_impl->frame->width * 3) };

    sws_scale(
        m_impl->sws_ctx,
        m_impl->frame->data,
        m_impl->frame->linesize,
        0,
        m_impl->frame->height,
        rgb_planes,
        rgb_linesize
    );

    // Upload to OpenGL texture
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        m_impl->frame->width,
        m_impl->frame->height,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        m_impl->rgb_buffer
    );

    // Update statistics
    auto decode_end = Clock::now();
    auto decode_duration = std::chrono::duration_cast<std::chrono::microseconds>(
        decode_end - decode_start);

    m_impl->total_decode_time_us += decode_duration.count();
    m_impl->decode_count++;

    m_stats.decode_time_avg_ms = (float)m_impl->total_decode_time_us /
                                 m_impl->decode_count / 1000.0f;

    // Calculate FPS
    uint64_t now = std::chrono::duration_cast<std::chrono::microseconds>(
        Clock::now().time_since_epoch()).count();

    if (m_last_frame_time_us > 0) {
        uint64_t delta = now - m_last_frame_time_us;
        if (delta > 0) {
            m_stats.actual_fps = 1000000.0f / delta;
        }
    }
    m_last_frame_time_us = now;

    return true;
}

size_t H264_Decoder::lock_output()
{
    m_impl->decode_mutex.lock();
    return m_stats.frames_decoded;
}

uint32_t H264_Decoder::get_video_texture_id() const
{
    return m_texture;
}

ImVec2 H264_Decoder::get_video_resolution() const
{
    return m_resolution;
}

bool H264_Decoder::unlock_output()
{
    m_impl->decode_mutex.unlock();
    return true;
}

bool H264_Decoder::isAspect16x9()
{
    return m_aspect_16x9;
}

void H264_Decoder::get_stats(Stats& stats)
{
    std::lock_guard<std::mutex> lock(m_impl->decode_mutex);
    stats = m_stats;
}

void H264_Decoder::reset_stats()
{
    std::lock_guard<std::mutex> lock(m_impl->decode_mutex);
    m_stats = {0};
    m_impl->total_decode_time_us = 0;
    m_impl->decode_count = 0;
}

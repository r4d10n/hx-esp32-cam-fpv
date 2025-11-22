package com.hxesp32.fpvgs.data.model

/**
 * Represents the video streaming state
 */
sealed class VideoState {
    object Idle : VideoState()
    object Initializing : VideoState()
    data class Streaming(val streamInfo: VideoStreamInfo) : VideoState()
    data class Error(val message: String, val throwable: Throwable? = null) : VideoState()
}

/**
 * Information about the current video stream
 */
data class VideoStreamInfo(
    val width: Int,
    val height: Int,
    val fps: Float,
    val bitrate: Long,
    val codec: String = "H.264"
)

/**
 * Video frame statistics
 */
data class VideoStatistics(
    val fps: Float = 0f,
    val bitrate: Long = 0L,
    val latency: Long = 0L, // milliseconds
    val droppedFrames: Int = 0,
    val totalFrames: Long = 0L,
    val decoderQueueSize: Int = 0
)

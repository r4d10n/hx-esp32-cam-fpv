package com.hxesp32.fpvgs.data.model

import java.io.File

/**
 * Represents the recording state
 */
sealed class RecordingState {
    object Idle : RecordingState()
    object Starting : RecordingState()
    data class Recording(val info: RecordingInfo) : RecordingState()
    object Stopping : RecordingState()
    data class Error(val message: String, val throwable: Throwable? = null) : RecordingState()
}

/**
 * Information about the current recording
 */
data class RecordingInfo(
    val file: File,
    val startTime: Long,
    val duration: Long = 0L, // milliseconds
    val fileSize: Long = 0L, // bytes
    val frameCount: Int = 0
)

/**
 * Recording configuration
 */
data class RecordingConfig(
    val outputDirectory: File,
    val fileNamePrefix: String = "fpv_recording",
    val videoWidth: Int,
    val videoHeight: Int,
    val videoBitrate: Int = 5_000_000, // 5 Mbps
    val videoFrameRate: Int = 30,
    val audioEnabled: Boolean = false,
    val outputFormat: OutputFormat = OutputFormat.MP4
)

enum class OutputFormat(val extension: String, val mimeType: String) {
    MP4("mp4", "video/mp4"),
    WEBM("webm", "video/webm"),
    TS("ts", "video/mp2ts")
}

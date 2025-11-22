package com.hxesp32.fpvgs.service

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaFormat
import android.media.MediaMuxer
import android.os.Binder
import android.os.Build
import android.os.Environment
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.hxesp32.fpvgs.FpvApplication
import com.hxesp32.fpvgs.R
import com.hxesp32.fpvgs.data.model.RecordingInfo
import com.hxesp32.fpvgs.data.model.RecordingState
import com.hxesp32.fpvgs.ui.MainActivity
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import timber.log.Timber
import java.io.File
import java.text.SimpleDateFormat
import java.util.*

/**
 * Foreground service for video recording
 */
class RecordingService : Service() {

    private val binder = LocalBinder()
    private val serviceScope = CoroutineScope(Dispatchers.IO + SupervisorJob())

    private var mediaCodec: MediaCodec? = null
    private var mediaMuxer: MediaMuxer? = null
    private var videoTrackIndex = -1
    private var muxerStarted = false

    private val _recordingState = MutableStateFlow<RecordingState>(RecordingState.Idle)
    val recordingState: StateFlow<RecordingState> = _recordingState.asStateFlow()

    private var currentRecordingInfo: RecordingInfo? = null
    private var recordingStartTime = 0L
    private var frameCount = 0

    override fun onCreate() {
        super.onCreate()
        Timber.d("RecordingService created")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(NOTIFICATION_ID, createNotification())
        return START_STICKY
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    /**
     * Start recording
     */
    fun startRecording(width: Int, height: Int, bitrate: Int = 5_000_000, frameRate: Int = 30) {
        if (_recordingState.value !is RecordingState.Idle) {
            Timber.w("Recording already in progress")
            return
        }

        _recordingState.value = RecordingState.Starting

        serviceScope.launch {
            try {
                val outputFile = createOutputFile()
                recordingStartTime = System.currentTimeMillis()
                frameCount = 0

                // Create encoder
                mediaCodec = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_VIDEO_AVC).apply {
                    val format = MediaFormat.createVideoFormat(
                        MediaFormat.MIMETYPE_VIDEO_AVC,
                        width,
                        height
                    ).apply {
                        setInteger(MediaFormat.KEY_BIT_RATE, bitrate)
                        setInteger(MediaFormat.KEY_FRAME_RATE, frameRate)
                        setInteger(MediaFormat.KEY_COLOR_FORMAT,
                            MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)
                        setInteger(MediaFormat.KEY_I_FRAME_INTERVAL, 1)
                    }

                    configure(format, null, null, MediaCodec.CONFIGURE_FLAG_ENCODE)
                    start()
                }

                // Create muxer
                mediaMuxer = MediaMuxer(
                    outputFile.absolutePath,
                    MediaMuxer.OutputFormat.MUXER_OUTPUT_MPEG_4
                )

                currentRecordingInfo = RecordingInfo(
                    file = outputFile,
                    startTime = recordingStartTime
                )

                _recordingState.value = RecordingState.Recording(currentRecordingInfo!!)
                Timber.i("Recording started: ${outputFile.absolutePath}")

            } catch (e: Exception) {
                Timber.e(e, "Failed to start recording")
                _recordingState.value = RecordingState.Error("Failed to start recording: ${e.message}", e)
            }
        }
    }

    /**
     * Stop recording
     */
    fun stopRecording() {
        if (_recordingState.value !is RecordingState.Recording) {
            Timber.w("No recording in progress")
            return
        }

        _recordingState.value = RecordingState.Stopping

        serviceScope.launch {
            try {
                mediaCodec?.stop()
                mediaCodec?.release()
                mediaCodec = null

                if (muxerStarted) {
                    mediaMuxer?.stop()
                }
                mediaMuxer?.release()
                mediaMuxer = null

                muxerStarted = false
                videoTrackIndex = -1

                _recordingState.value = RecordingState.Idle
                Timber.i("Recording stopped")

            } catch (e: Exception) {
                Timber.e(e, "Error stopping recording")
                _recordingState.value = RecordingState.Error("Failed to stop recording: ${e.message}", e)
            }
        }
    }

    /**
     * Write encoded frame data
     */
    fun writeFrame(data: ByteArray, presentationTimeUs: Long, isKeyFrame: Boolean) {
        if (_recordingState.value !is RecordingState.Recording) return

        serviceScope.launch {
            try {
                val bufferInfo = MediaCodec.BufferInfo().apply {
                    offset = 0
                    size = data.size
                    presentationTimeUs = presentationTimeUs
                    flags = if (isKeyFrame) MediaCodec.BUFFER_FLAG_KEY_FRAME else 0
                }

                // Add track if not added yet
                if (videoTrackIndex < 0 && isKeyFrame) {
                    val format = MediaFormat.createVideoFormat(
                        MediaFormat.MIMETYPE_VIDEO_AVC,
                        640, // Will be updated from actual stream
                        480
                    )
                    videoTrackIndex = mediaMuxer?.addTrack(format) ?: -1
                    mediaMuxer?.start()
                    muxerStarted = true
                }

                if (muxerStarted && videoTrackIndex >= 0) {
                    val buffer = java.nio.ByteBuffer.wrap(data)
                    mediaMuxer?.writeSampleData(videoTrackIndex, buffer, bufferInfo)
                    frameCount++

                    // Update recording info
                    val info = currentRecordingInfo?.copy(
                        duration = System.currentTimeMillis() - recordingStartTime,
                        frameCount = frameCount,
                        fileSize = currentRecordingInfo?.file?.length() ?: 0L
                    )
                    if (info != null) {
                        currentRecordingInfo = info
                        _recordingState.value = RecordingState.Recording(info)
                    }
                }

            } catch (e: Exception) {
                Timber.e(e, "Error writing frame")
            }
        }
    }

    private fun createOutputFile(): File {
        val recordingsDir = File(
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MOVIES),
            "HX-ESP32-FPV"
        )
        recordingsDir.mkdirs()

        val timestamp = SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(Date())
        val fileName = "fpv_recording_$timestamp.mp4"

        return File(recordingsDir, fileName)
    }

    private fun createNotification(): Notification {
        val intent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this, 0, intent,
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, FpvApplication.CHANNEL_RECORDING)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(getString(R.string.notification_recording_active))
            .setSmallIcon(R.drawable.ic_launcher_foreground)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

    override fun onDestroy() {
        super.onDestroy()
        serviceScope.cancel()
        stopRecording()
        Timber.d("RecordingService destroyed")
    }

    inner class LocalBinder : Binder() {
        fun getService(): RecordingService = this@RecordingService
    }

    companion object {
        private const val NOTIFICATION_ID = 1003
    }
}

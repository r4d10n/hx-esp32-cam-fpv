package com.hxesp32.fpvgs.service

import android.app.Notification
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import android.view.Surface
import androidx.core.app.NotificationCompat
import com.hxesp32.fpvgs.FpvApplication
import com.hxesp32.fpvgs.R
import com.hxesp32.fpvgs.data.model.VideoState
import com.hxesp32.fpvgs.data.model.VideoStatistics
import com.hxesp32.fpvgs.ui.MainActivity
import com.hxesp32.fpvgs.video.H264Decoder
import com.hxesp32.fpvgs.video.VideoStreamParser
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.StateFlow
import timber.log.Timber

/**
 * Foreground service for H.264 video decoding
 */
class VideoDecoderService : Service() {

    private val binder = LocalBinder()
    private val decoder = H264Decoder()
    private val parser = VideoStreamParser()
    private val serviceScope = CoroutineScope(Dispatchers.Default + SupervisorJob())

    val videoState: StateFlow<VideoState>
        get() = decoder.videoState

    val statistics: StateFlow<VideoStatistics>
        get() = decoder.statistics

    override fun onCreate() {
        super.onCreate()
        Timber.d("VideoDecoderService created")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(NOTIFICATION_ID, createNotification())
        return START_STICKY
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    /**
     * Initialize the decoder
     */
    fun initializeDecoder(width: Int, height: Int, surface: Surface) {
        serviceScope.launch {
            try {
                decoder.initialize(width, height, surface)
                Timber.i("Video decoder initialized: ${width}x${height}")
            } catch (e: Exception) {
                Timber.e(e, "Failed to initialize decoder")
            }
        }
    }

    /**
     * Process incoming video data
     */
    fun processVideoData(data: ByteArray) {
        serviceScope.launch {
            try {
                // Parse NAL units from the stream
                parser.parse(data) { nalUnit ->
                    // Decode each NAL unit
                    decoder.decodeFrame(nalUnit)
                }
            } catch (e: Exception) {
                Timber.e(e, "Error processing video data")
            }
        }
    }

    /**
     * Flush the decoder
     */
    fun flush() {
        decoder.flush()
        parser.flush { nalUnit ->
            decoder.decodeFrame(nalUnit)
        }
    }

    /**
     * Release the decoder
     */
    fun releaseDecoder() {
        decoder.release()
        parser.reset()
    }

    private fun createNotification(): Notification {
        val intent = Intent(this, MainActivity::class.java)
        val pendingIntent = PendingIntent.getActivity(
            this, 0, intent,
            PendingIntent.FLAG_IMMUTABLE
        )

        return NotificationCompat.Builder(this, FpvApplication.CHANNEL_VIDEO)
            .setContentTitle(getString(R.string.app_name))
            .setContentText(getString(R.string.notification_video_streaming))
            .setSmallIcon(R.drawable.ic_launcher_foreground)
            .setContentIntent(pendingIntent)
            .setOngoing(true)
            .build()
    }

    override fun onDestroy() {
        super.onDestroy()
        serviceScope.cancel()
        decoder.release()
        Timber.d("VideoDecoderService destroyed")
    }

    inner class LocalBinder : Binder() {
        fun getService(): VideoDecoderService = this@VideoDecoderService
    }

    companion object {
        private const val NOTIFICATION_ID = 1002
    }
}

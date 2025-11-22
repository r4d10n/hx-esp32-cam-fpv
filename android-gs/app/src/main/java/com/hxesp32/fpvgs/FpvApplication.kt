package com.hxesp32.fpvgs

import android.app.Application
import android.app.NotificationChannel
import android.app.NotificationManager
import android.os.Build
import timber.log.Timber

/**
 * Application class for HX-ESP32 FPV Ground Station
 */
class FpvApplication : Application() {

    override fun onCreate() {
        super.onCreate()

        // Initialize Timber for logging
        if (BuildConfig.DEBUG) {
            Timber.plant(Timber.DebugTree())
        }

        // Create notification channels
        createNotificationChannels()

        Timber.d("FpvApplication initialized")
    }

    private fun createNotificationChannels() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            val notificationManager = getSystemService(NotificationManager::class.java)

            // Video service channel
            val videoChannel = NotificationChannel(
                CHANNEL_VIDEO,
                getString(R.string.notification_channel_video),
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Video streaming service notifications"
                setShowBadge(false)
            }

            // Recording channel
            val recordingChannel = NotificationChannel(
                CHANNEL_RECORDING,
                getString(R.string.notification_channel_recording),
                NotificationManager.IMPORTANCE_LOW
            ).apply {
                description = "Video recording notifications"
                setShowBadge(false)
            }

            notificationManager?.createNotificationChannel(videoChannel)
            notificationManager?.createNotificationChannel(recordingChannel)
        }
    }

    companion object {
        const val CHANNEL_VIDEO = "video_service"
        const val CHANNEL_RECORDING = "recording_service"
    }
}

package com.hxesp32.fpvgs.service

import android.app.Notification
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Intent
import android.os.Binder
import android.os.IBinder
import androidx.core.app.NotificationCompat
import com.hxesp32.fpvgs.FpvApplication
import com.hxesp32.fpvgs.R
import com.hxesp32.fpvgs.data.model.ConnectionState
import com.hxesp32.fpvgs.ui.MainActivity
import com.hxesp32.fpvgs.usb.UsbDeviceManager
import kotlinx.coroutines.*
import kotlinx.coroutines.flow.StateFlow
import timber.log.Timber

/**
 * Foreground service for USB communication with ESP32-S3
 */
class UsbCommunicationService : Service() {

    private val binder = LocalBinder()
    private lateinit var usbManager: UsbDeviceManager
    private val serviceScope = CoroutineScope(Dispatchers.IO + SupervisorJob())
    private var readJob: Job? = null

    private var dataCallback: ((ByteArray) -> Unit)? = null

    val connectionState: StateFlow<ConnectionState>
        get() = usbManager.connectionState

    override fun onCreate() {
        super.onCreate()
        usbManager = UsbDeviceManager(applicationContext)
        Timber.d("UsbCommunicationService created")
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        startForeground(NOTIFICATION_ID, createNotification())
        return START_STICKY
    }

    override fun onBind(intent: Intent): IBinder {
        return binder
    }

    /**
     * Connect to USB device
     */
    fun connect() {
        serviceScope.launch {
            usbManager.findAndConnect()

            // Start reading data if connected
            usbManager.connectionState.collect { state ->
                if (state is ConnectionState.Connected) {
                    startReading()
                } else {
                    stopReading()
                }
            }
        }
    }

    /**
     * Disconnect from USB device
     */
    fun disconnect() {
        stopReading()
        usbManager.disconnect()
    }

    /**
     * Set callback for received data
     */
    fun setDataCallback(callback: (ByteArray) -> Unit) {
        dataCallback = callback
    }

    /**
     * Send data to USB device
     */
    fun sendData(data: ByteArray) {
        serviceScope.launch {
            usbManager.write(data)
        }
    }

    private fun startReading() {
        if (readJob?.isActive == true) return

        readJob = serviceScope.launch {
            val buffer = ByteArray(READ_BUFFER_SIZE)

            while (isActive) {
                try {
                    val bytesRead = usbManager.read(buffer)
                    if (bytesRead > 0) {
                        val data = buffer.copyOfRange(0, bytesRead)
                        dataCallback?.invoke(data)
                    }
                } catch (e: Exception) {
                    Timber.e(e, "Error reading from USB")
                    delay(100)
                }
            }
        }
    }

    private fun stopReading() {
        readJob?.cancel()
        readJob = null
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
        usbManager.release()
        Timber.d("UsbCommunicationService destroyed")
    }

    inner class LocalBinder : Binder() {
        fun getService(): UsbCommunicationService = this@UsbCommunicationService
    }

    companion object {
        private const val NOTIFICATION_ID = 1001
        private const val READ_BUFFER_SIZE = 64 * 1024 // 64KB
    }
}

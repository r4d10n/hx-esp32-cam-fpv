package com.hxesp32.fpvgs

import android.graphics.BitmapFactory
import android.hardware.usb.UsbDevice
import android.os.Bundle
import android.util.Log
import android.widget.*
import androidx.appcompat.app.AppCompatActivity
import com.hxesp32.fpvgs.protocol.*
import com.hxesp32.fpvgs.usb.UsbCommunicationManager
import com.hxesp32.fpvgs.usb.UsbError
import java.util.concurrent.Executors

/**
 * Example Activity demonstrating USB communication with ESP32-S3
 * This is a complete working example showing how to use the USB communication layer
 */
class UsbExampleActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "UsbExampleActivity"
    }

    // USB Communication
    private lateinit var usbManager: UsbCommunicationManager

    // Video decoding
    private val videoDecoder = Executors.newSingleThreadExecutor()

    // Statistics
    private var frameCount = 0
    private var lastStatsTime = System.currentTimeMillis()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // Simple layout would be defined in XML, but showing concept here
        initializeUI()

        // Initialize USB communication
        initializeUsbCommunication()

        // Auto-discover and connect
        discoverAndConnect()
    }

    private fun initializeUI() {
        // In a real app, this would be in XML layout
        // For demo purposes, showing the concept
    }

    private fun initializeUsbCommunication() {
        val listener = object : UsbCommunicationManager.UsbCommunicationListener {
            override fun onDeviceConnected(device: UsbDevice) {
                Log.i(TAG, "Device connected: ${device.deviceName}")
                runOnUiThread {
                    showToast("Connected to ${device.deviceName}")
                    updateConnectionStatus(true)
                }
            }

            override fun onDeviceDisconnected() {
                Log.i(TAG, "Device disconnected")
                runOnUiThread {
                    showToast("Device disconnected")
                    updateConnectionStatus(false)
                    clearVideoDisplay()
                }
            }

            override fun onPermissionGranted(device: UsbDevice) {
                Log.i(TAG, "Permission granted for ${device.deviceName}")
            }

            override fun onPermissionDenied(device: UsbDevice) {
                Log.w(TAG, "Permission denied for ${device.deviceName}")
                runOnUiThread {
                    showToast("USB permission denied. Please grant permission.")
                }
            }

            override fun onVideoFrameReceived(frame: AssembledVideoFrame) {
                handleVideoFrame(frame)
            }

            override fun onTelemetryReceived(data: ByteArray) {
                handleTelemetry(data)
            }

            override fun onOsdDataReceived(stats: AirStats, osdBuffer: ByteArray) {
                handleOsdData(stats, osdBuffer)
            }

            override fun onConfigReceived(config: UsbFrame.ConfigFrame) {
                handleConfig(config)
            }

            override fun onError(error: UsbError) {
                handleUsbError(error)
            }

            override fun onStatsUpdated(stats: UsbStats) {
                updateStatisticsDisplay(stats)
            }
        }

        usbManager = UsbCommunicationManager(this, listener)
    }

    private fun discoverAndConnect() {
        // Discover ESP32-S3 devices
        val devices = usbManager.discoverDevices()

        when {
            devices.isEmpty() -> {
                Log.w(TAG, "No ESP32-S3 devices found")
                showToast("No ESP32-S3 devices found. Connect device via USB.")
            }
            devices.size == 1 -> {
                // Single device, auto-connect
                Log.i(TAG, "Found device: ${devices[0].deviceName}")
                usbManager.requestPermission(devices[0])
            }
            else -> {
                // Multiple devices, show selection dialog
                showDeviceSelectionDialog(devices)
            }
        }
    }

    private fun showDeviceSelectionDialog(devices: List<UsbDevice>) {
        val deviceNames = devices.map { it.deviceName }.toTypedArray()

        android.app.AlertDialog.Builder(this)
            .setTitle("Select ESP32-S3 Device")
            .setItems(deviceNames) { _, which ->
                usbManager.requestPermission(devices[which])
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun handleVideoFrame(frame: AssembledVideoFrame) {
        // Update frame counter
        frameCount++

        // Calculate FPS every second
        val now = System.currentTimeMillis()
        if (now - lastStatsTime >= 1000) {
            val fps = frameCount.toFloat()
            Log.d(TAG, "Video FPS: $fps")
            runOnUiThread {
                updateFpsDisplay(fps)
            }
            frameCount = 0
            lastStatsTime = now
        }

        // Decode JPEG on background thread
        videoDecoder.execute {
            try {
                val bitmap = BitmapFactory.decodeByteArray(frame.data, 0, frame.data.size)

                if (bitmap != null) {
                    runOnUiThread {
                        displayVideoFrame(bitmap, frame)
                    }
                } else {
                    Log.e(TAG, "Failed to decode JPEG frame ${frame.frameIndex}")
                }
            } catch (e: Exception) {
                Log.e(TAG, "Error decoding video frame", e)
            }
        }
    }

    private fun displayVideoFrame(bitmap: android.graphics.Bitmap, frame: AssembledVideoFrame) {
        // In real app, update ImageView
        // imageView.setImageBitmap(bitmap)

        // Update frame info
        Log.d(TAG, "Frame ${frame.frameIndex}: ${frame.resolution.width}x${frame.resolution.height}, " +
                "${frame.data.size} bytes")
    }

    private fun handleTelemetry(data: ByteArray) {
        // Parse and process MAVLink telemetry
        Log.d(TAG, "Received telemetry: ${data.size} bytes")

        // In a real app, parse MAVLink messages here
        // Example: parseMavlinkMessage(data)
    }

    private fun handleOsdData(stats: AirStats, osdBuffer: ByteArray) {
        runOnUiThread {
            updateOsdDisplay(stats)
        }
    }

    private fun updateOsdDisplay(stats: AirStats) {
        Log.d(TAG, buildString {
            append("OSD Stats:\n")
            append("RSSI: ${stats.getRssiDbmSigned()} dBm\n")
            append("FPS: ${stats.captureFPS}\n")
            append("Resolution: ${stats.resolution.width}x${stats.resolution.height}\n")
            append("Recording: ${stats.airRecordState}\n")
            append("Temperature: ${stats.temperature}°C")
            if (stats.overheatThrottling) append(" (THROTTLING)")
            append("\n")
            if (stats.sdDetected) {
                append("SD: ${"%.1f".format(stats.getSdFreeSpaceGB())} / ${"%.1f".format(stats.getSdTotalSpaceGB())} GB")
                if (stats.sdSlow) append(" (SLOW)")
            }
        })

        // In real app, update UI elements
        // rssiTextView.text = "${stats.getRssiDbmSigned()} dBm"
        // fpsTextView.text = "${stats.captureFPS} FPS"
        // etc.
    }

    private fun handleConfig(config: UsbFrame.ConfigFrame) {
        Log.i(TAG, "Received config from Air Device ${config.airDeviceId}")
    }

    private fun handleUsbError(error: UsbError) {
        val message = when (error) {
            is UsbError.NoSuitableInterface ->
                "Device doesn't have required USB interface"
            is UsbError.ConnectionFailed ->
                "Failed to open USB connection"
            is UsbError.InterfaceClaimFailed ->
                "Failed to claim USB interface"
            is UsbError.NoEndpoints ->
                "Device doesn't have required endpoints"
            is UsbError.ConnectionException ->
                "Connection error: ${error.exception.message}"
            is UsbError.ReadError ->
                "USB read error: ${error.message}"
            is UsbError.WriteError ->
                "USB write error: ${error.message}"
        }

        Log.e(TAG, "USB Error: $message")
        runOnUiThread {
            showToast("Error: $message")
        }
    }

    private fun updateStatisticsDisplay(stats: UsbStats) {
        val uptimeMs = usbManager.getUptimeMs()
        val throughputMbps = stats.getThroughputBps(uptimeMs) / 1_000_000.0
        val frameRate = stats.getFrameRate(uptimeMs)

        Log.v(TAG, buildString {
            append("Stats: ")
            append("${stats.bytesReceived / 1024 / 1024} MB received, ")
            append("${"%.1f".format(throughputMbps)} Mbps, ")
            append("${"%.1f".format(frameRate)} frames/s, ")
            append("${stats.crcErrors} CRC errors")
        })
    }

    private fun updateConnectionStatus(connected: Boolean) {
        // Update UI connection indicator
        Log.i(TAG, "Connection status: ${if (connected) "Connected" else "Disconnected"}")
    }

    private fun clearVideoDisplay() {
        // Clear video display when disconnected
        // imageView.setImageBitmap(null)
    }

    private fun updateFpsDisplay(fps: Float) {
        // Update FPS display
        // fpsTextView.text = "${"%.1f".format(fps)} FPS"
    }

    private fun showToast(message: String) {
        Toast.makeText(this, message, Toast.LENGTH_SHORT).show()
    }

    // Button handlers (would be connected to UI buttons)

    fun onStartRecordingClicked() {
        // Send command to start recording on air unit
        val config = DataChannelConfig(
            airRecordBtn = 1u  // Increment to toggle recording
        )

        // In a full implementation, send complete config
        Log.i(TAG, "Start recording command sent")
    }

    fun onChangeResolutionClicked(resolution: Resolution) {
        val cameraConfig = CameraConfig(
            resolution = resolution
        )

        val dataChannelConfig = DataChannelConfig()

        usbManager.sendConfig(
            airDeviceId = 0u,  // Would get from config
            gsDeviceId = 0u,   // Would get from config
            camera = cameraConfig,
            dataChannel = dataChannelConfig
        )

        Log.i(TAG, "Resolution change to $resolution sent")
    }

    fun onSendTelemetryClicked(mavlinkData: ByteArray) {
        usbManager.sendTelemetry(mavlinkData)
        Log.i(TAG, "Telemetry data sent: ${mavlinkData.size} bytes")
    }

    fun onRefreshDevicesClicked() {
        discoverAndConnect()
    }

    fun onDisconnectClicked() {
        usbManager.disconnect()
        Log.i(TAG, "Disconnected from device")
    }

    override fun onDestroy() {
        super.onDestroy()

        // Cleanup
        videoDecoder.shutdown()
        usbManager.cleanup()

        Log.i(TAG, "Activity destroyed, USB manager cleaned up")
    }

    override fun onResume() {
        super.onResume()

        // Check if device is still connected
        if (!usbManager.isConnected()) {
            Log.i(TAG, "Device not connected, attempting to discover")
            discoverAndConnect()
        }
    }
}

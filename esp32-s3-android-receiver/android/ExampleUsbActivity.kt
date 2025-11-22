package com.example.esp32usb

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbManager
import android.os.Bundle
import android.util.Log
import androidx.appcompat.app.AppCompatActivity
import com.felhr.usbserial.UsbSerialDevice
import com.felhr.usbserial.UsbSerialInterface
import kotlinx.coroutines.*
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Example Android Activity demonstrating USB OTG integration with ESP32-S3
 *
 * This example shows how to:
 * - Connect to ESP32-S3 via USB OTG
 * - Read video stream data
 * - Parse protocol packets
 * - Handle H.264 NAL units
 * - Display statistics
 *
 * Dependencies:
 * - UsbSerial library: implementation 'com.github.felHR85:UsbSerial:6.1.0'
 * - Kotlin Coroutines: implementation 'org.jetbrains.kotlinx:kotlinx-coroutines-android:1.6.4'
 *
 * @author ESP32-S3 USB Streamer Team
 */
class ExampleUsbActivity : AppCompatActivity() {

    companion object {
        private const val TAG = "ExampleUsbActivity"
        private const val ACTION_USB_PERMISSION = "com.example.esp32usb.USB_PERMISSION"
        private const val ESP32_VID = 0x303A // Espressif VID
        private const val READ_BUFFER_SIZE = 16384
    }

    private lateinit var usbManager: UsbManager
    private var usbDevice: UsbDevice? = null
    private var usbConnection: UsbDeviceConnection? = null
    private var serialPort: UsbSerialDevice? = null
    private var parser: UsbProtocolParser? = null

    private val isRunning = AtomicBoolean(false)
    private var readJob: Job? = null

    // Statistics
    private var videoFramesReceived = 0
    private var metadataPacketsReceived = 0
    private var heartbeatsReceived = 0
    private var lastRssi = 0
    private var lastFps = 0.0f
    private var lastBitrate = 0L

    /**
     * USB permission broadcast receiver
     */
    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                ACTION_USB_PERMISSION -> {
                    synchronized(this) {
                        val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            device?.let {
                                Log.i(TAG, "USB permission granted for device: ${it.deviceName}")
                                connectToDevice(it)
                            }
                        } else {
                            Log.w(TAG, "USB permission denied")
                        }
                    }
                }
                UsbManager.ACTION_USB_DEVICE_ATTACHED -> {
                    val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                    device?.let {
                        Log.i(TAG, "USB device attached: ${it.deviceName}")
                        requestPermissionAndConnect(it)
                    }
                }
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    val device = intent.getParcelableExtra<UsbDevice>(UsbManager.EXTRA_DEVICE)
                    device?.let {
                        Log.i(TAG, "USB device detached: ${it.deviceName}")
                        disconnect()
                    }
                }
            }
        }
    }

    /**
     * Protocol parser callback
     */
    private val parserCallback = object : UsbProtocolParser.PacketCallback {
        override fun onVideoPacket(video: UsbProtocolParser.VideoPacket) {
            videoFramesReceived++
            Log.d(TAG, "Video NAL: type=0x${video.nalType.toString(16)}, size=${video.nalSize}, " +
                      "keyframe=${video.isKeyFrame()}")

            // TODO: Send NAL unit to MediaCodec for decoding
            // Example:
            // if (video.isKeyFrame()) {
            //     decoder.queueInputBuffer(...)
            // }
        }

        override fun onMetadataPacket(metadata: UsbProtocolParser.MetadataPacket) {
            metadataPacketsReceived++
            lastRssi = metadata.rssi.toInt()
            lastFps = metadata.fps / 10.0f
            lastBitrate = metadata.bitrate

            Log.i(TAG, "Metadata: RSSI=${metadata.rssi}dBm, FPS=$lastFps, " +
                      "Bitrate=${metadata.bitrate}kbps, Buffer=${metadata.bufferUsage}%")

            // TODO: Update UI with statistics
            runOnUiThread {
                updateStatisticsUI()
            }
        }

        override fun onHeartbeat(uptime: Long) {
            heartbeatsReceived++
            Log.d(TAG, "Heartbeat: uptime=${uptime}s")
        }

        override fun onDebugMessage(message: String) {
            Log.d(TAG, "ESP32 Debug: $message")
        }

        override fun onParseError(error: String) {
            Log.e(TAG, "Parse error: $error")
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // setContentView(R.layout.activity_usb_example)

        usbManager = getSystemService(Context.USB_SERVICE) as UsbManager

        // Initialize protocol parser
        parser = UsbProtocolParser(parserCallback)

        // Register USB broadcast receiver
        val filter = IntentFilter().apply {
            addAction(ACTION_USB_PERMISSION)
            addAction(UsbManager.ACTION_USB_DEVICE_ATTACHED)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        registerReceiver(usbReceiver, filter)

        // Check for already connected devices
        findAndConnectDevice()
    }

    override fun onDestroy() {
        super.onDestroy()
        disconnect()
        unregisterReceiver(usbReceiver)
    }

    /**
     * Find and connect to ESP32-S3 device
     */
    private fun findAndConnectDevice() {
        val deviceList = usbManager.deviceList
        Log.i(TAG, "Found ${deviceList.size} USB devices")

        for ((_, device) in deviceList) {
            Log.d(TAG, "Device: VID=0x${device.vendorId.toString(16)}, " +
                      "PID=0x${device.productId.toString(16)}, Name=${device.deviceName}")

            // Look for ESP32-S3 (Espressif VID)
            if (device.vendorId == ESP32_VID) {
                Log.i(TAG, "Found ESP32 device: ${device.deviceName}")
                requestPermissionAndConnect(device)
                return
            }
        }

        Log.w(TAG, "No ESP32-S3 device found")
    }

    /**
     * Request USB permission and connect
     */
    private fun requestPermissionAndConnect(device: UsbDevice) {
        if (usbManager.hasPermission(device)) {
            connectToDevice(device)
        } else {
            val permissionIntent = PendingIntent.getBroadcast(
                this,
                0,
                Intent(ACTION_USB_PERMISSION),
                PendingIntent.FLAG_IMMUTABLE
            )
            usbManager.requestPermission(device, permissionIntent)
        }
    }

    /**
     * Connect to USB device
     */
    private fun connectToDevice(device: UsbDevice) {
        try {
            usbConnection = usbManager.openDevice(device)
            if (usbConnection == null) {
                Log.e(TAG, "Failed to open USB connection")
                return
            }

            serialPort = UsbSerialDevice.createUsbSerialDevice(device, usbConnection)
            if (serialPort == null) {
                Log.e(TAG, "Failed to create serial port")
                usbConnection?.close()
                usbConnection = null
                return
            }

            if (!serialPort!!.open()) {
                Log.e(TAG, "Failed to open serial port")
                usbConnection?.close()
                usbConnection = null
                serialPort = null
                return
            }

            // Configure serial port (115200 baud, 8N1)
            serialPort!!.apply {
                setBaudRate(115200)
                setDataBits(UsbSerialInterface.DATA_BITS_8)
                setStopBits(UsbSerialInterface.STOP_BITS_1)
                setParity(UsbSerialInterface.PARITY_NONE)
                setFlowControl(UsbSerialInterface.FLOW_CONTROL_OFF)
            }

            usbDevice = device
            isRunning.set(true)

            Log.i(TAG, "USB connection established")

            // Start reading data
            startReading()

            // Send initial control command (GET_STATUS)
            sendControlCommand(0x06, 0, 0)

        } catch (e: Exception) {
            Log.e(TAG, "Error connecting to device: ${e.message}")
            disconnect()
        }
    }

    /**
     * Disconnect from USB device
     */
    private fun disconnect() {
        isRunning.set(false)

        readJob?.cancel()
        readJob = null

        serialPort?.close()
        serialPort = null

        usbConnection?.close()
        usbConnection = null

        usbDevice = null

        parser?.reset()

        Log.i(TAG, "USB disconnected")
    }

    /**
     * Start reading data from USB
     */
    private fun startReading() {
        readJob = CoroutineScope(Dispatchers.IO).launch {
            val buffer = ByteArray(READ_BUFFER_SIZE)

            while (isRunning.get() && serialPort != null) {
                try {
                    // Read data from USB serial port
                    val bytesRead = serialPort!!.read(buffer, 1000) // 1 second timeout

                    if (bytesRead > 0) {
                        // Parse received data
                        parser?.parse(buffer, bytesRead)
                    }

                } catch (e: Exception) {
                    if (isRunning.get()) {
                        Log.e(TAG, "Error reading USB data: ${e.message}")
                    }
                    break
                }
            }

            Log.i(TAG, "USB read loop stopped")
        }
    }

    /**
     * Send control command to ESP32-S3
     */
    private fun sendControlCommand(command: Byte, param1: Short, param2: Int) {
        // TODO: Implement control command packet construction and sending
        // Format: [SYNC][TYPE=0x03][FLAGS][SIZE=6][SEQ][TIMESTAMP][CMD][PARAM1][PARAM2][CRC]

        Log.d(TAG, "Sending control command: cmd=$command, param1=$param1, param2=$param2")

        // Example:
        // val packet = buildControlPacket(command, param1, param2)
        // serialPort?.write(packet)
    }

    /**
     * Update statistics UI
     */
    private fun updateStatisticsUI() {
        // TODO: Update TextView or other UI elements
        val stats = parser?.statistics

        Log.i(TAG, "=== Statistics ===")
        Log.i(TAG, "Video frames: $videoFramesReceived")
        Log.i(TAG, "Metadata packets: $metadataPacketsReceived")
        Log.i(TAG, "Heartbeats: $heartbeatsReceived")
        Log.i(TAG, "RSSI: ${lastRssi}dBm")
        Log.i(TAG, "FPS: $lastFps")
        Log.i(TAG, "Bitrate: ${lastBitrate}kbps")
        stats?.let {
            Log.i(TAG, "Packets received: ${it.packetsReceived}")
            Log.i(TAG, "CRC errors: ${it.crcErrors}")
            Log.i(TAG, "Sequence gaps: ${it.sequenceGaps}")
        }
    }
}

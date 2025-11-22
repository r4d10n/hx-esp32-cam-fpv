package com.hxesp32.fpvgs.usb

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbDeviceConnection
import android.hardware.usb.UsbEndpoint
import android.hardware.usb.UsbInterface
import android.hardware.usb.UsbManager
import android.os.Build
import android.util.Log
import com.hxesp32.fpvgs.protocol.*
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.util.concurrent.ConcurrentLinkedQueue
import java.util.concurrent.atomic.AtomicBoolean
import java.util.zip.CRC32
import kotlin.concurrent.thread

/**
 * Manages USB communication with ESP32-S3
 * Handles device detection, permissions, data transfer, and protocol parsing
 */
class UsbCommunicationManager(
    private val context: Context,
    private val listener: UsbCommunicationListener
) {
    companion object {
        private const val TAG = "UsbCommunicationMgr"
        private const val ACTION_USB_PERMISSION = "com.hxesp32.fpvgs.USB_PERMISSION"

        // ESP32-S3 USB VID/PID (Espressif)
        private const val ESP32_VENDOR_ID = 0x303A
        private const val ESP32_PRODUCT_ID = 0x1001  // Typical ESP32-S3 PID

        // USB constants
        private const val USB_TIMEOUT_MS = 1000
        private const val READ_BUFFER_SIZE = ProtocolConstants.USB_BULK_TRANSFER_SIZE
        private const val WRITE_BUFFER_SIZE = 512
    }

    /**
     * Listener for USB communication events
     */
    interface UsbCommunicationListener {
        fun onDeviceConnected(device: UsbDevice)
        fun onDeviceDisconnected()
        fun onPermissionGranted(device: UsbDevice)
        fun onPermissionDenied(device: UsbDevice)
        fun onVideoFrameReceived(frame: AssembledVideoFrame)
        fun onTelemetryReceived(data: ByteArray)
        fun onOsdDataReceived(stats: AirStats, osdBuffer: ByteArray)
        fun onConfigReceived(config: UsbFrame.ConfigFrame)
        fun onError(error: UsbError)
        fun onStatsUpdated(stats: UsbStats)
    }

    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    private val parser = UsbProtocolParser()
    private val assembler = UsbFrameAssembler()
    private val stats = UsbStats()

    private var usbDevice: UsbDevice? = null
    private var usbConnection: UsbDeviceConnection? = null
    private var usbInterface: UsbInterface? = null
    private var readEndpoint: UsbEndpoint? = null
    private var writeEndpoint: UsbEndpoint? = null

    private val isRunning = AtomicBoolean(false)
    private val isConnected = AtomicBoolean(false)
    private var readerThread: Thread? = null
    private var writerThread: Thread? = null

    private val writeQueue = ConcurrentLinkedQueue<ByteArray>()
    private val startTime = System.currentTimeMillis()

    /**
     * USB permission receiver
     */
    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                ACTION_USB_PERMISSION -> {
                    synchronized(this) {
                        val device = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                            intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                        } else {
                            @Suppress("DEPRECATION")
                            intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                        }

                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            device?.let {
                                Log.i(TAG, "Permission granted for device ${it.deviceName}")
                                listener.onPermissionGranted(it)
                                connectToDevice(it)
                            }
                        } else {
                            device?.let {
                                Log.w(TAG, "Permission denied for device ${it.deviceName}")
                                listener.onPermissionDenied(it)
                            }
                        }
                    }
                }
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    val device = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE, UsbDevice::class.java)
                    } else {
                        @Suppress("DEPRECATION")
                        intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                    }

                    if (device == usbDevice) {
                        Log.i(TAG, "Device detached: ${device?.deviceName}")
                        disconnect()
                        listener.onDeviceDisconnected()
                    }
                }
            }
        }
    }

    init {
        // Register USB receivers
        val filter = IntentFilter().apply {
            addAction(ACTION_USB_PERMISSION)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            context.registerReceiver(usbReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            context.registerReceiver(usbReceiver, filter)
        }
    }

    /**
     * Discover ESP32-S3 USB devices
     */
    fun discoverDevices(): List<UsbDevice> {
        val deviceList = usbManager.deviceList
        return deviceList.values.filter { device ->
            device.vendorId == ESP32_VENDOR_ID ||
                    device.productId == ESP32_PRODUCT_ID ||
                    isEsp32Device(device)
        }
    }

    /**
     * Check if device is ESP32-S3
     */
    private fun isEsp32Device(device: UsbDevice): Boolean {
        // Check for CDC ACM or vendor-specific interface
        for (i in 0 until device.interfaceCount) {
            val iface = device.getInterface(i)
            // CDC ACM class or vendor-specific
            if (iface.interfaceClass == 2 || iface.interfaceClass == 0xFF) {
                return true
            }
        }
        return false
    }

    /**
     * Request permission for USB device
     */
    fun requestPermission(device: UsbDevice) {
        if (usbManager.hasPermission(device)) {
            Log.i(TAG, "Already have permission for ${device.deviceName}")
            listener.onPermissionGranted(device)
            connectToDevice(device)
        } else {
            Log.i(TAG, "Requesting permission for ${device.deviceName}")
            val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                PendingIntent.FLAG_MUTABLE
            } else {
                0
            }
            val permissionIntent = PendingIntent.getBroadcast(
                context,
                0,
                Intent(ACTION_USB_PERMISSION),
                flags
            )
            usbManager.requestPermission(device, permissionIntent)
        }
    }

    /**
     * Connect to USB device
     */
    fun connectToDevice(device: UsbDevice): Boolean {
        synchronized(this) {
            try {
                Log.i(TAG, "Connecting to device ${device.deviceName}")

                // Find appropriate interface
                var selectedInterface: UsbInterface? = null
                for (i in 0 until device.interfaceCount) {
                    val iface = device.getInterface(i)
                    // Look for CDC or vendor-specific interface with bulk endpoints
                    if (iface.interfaceClass == 2 || iface.interfaceClass == 0xFF) {
                        if (hasRequiredEndpoints(iface)) {
                            selectedInterface = iface
                            break
                        }
                    }
                }

                if (selectedInterface == null) {
                    listener.onError(UsbError.NoSuitableInterface(device))
                    return false
                }

                // Open connection
                val connection = usbManager.openDevice(device)
                if (connection == null) {
                    listener.onError(UsbError.ConnectionFailed(device))
                    return false
                }

                // Claim interface
                if (!connection.claimInterface(selectedInterface, true)) {
                    connection.close()
                    listener.onError(UsbError.InterfaceClaimFailed(device))
                    return false
                }

                // Find endpoints
                var readEp: UsbEndpoint? = null
                var writeEp: UsbEndpoint? = null

                for (i in 0 until selectedInterface.endpointCount) {
                    val endpoint = selectedInterface.getEndpoint(i)
                    if (endpoint.type == android.hardware.usb.UsbConstants.USB_ENDPOINT_XFER_BULK) {
                        if (endpoint.direction == android.hardware.usb.UsbConstants.USB_DIR_IN) {
                            readEp = endpoint
                        } else {
                            writeEp = endpoint
                        }
                    }
                }

                if (readEp == null || writeEp == null) {
                    connection.releaseInterface(selectedInterface)
                    connection.close()
                    listener.onError(UsbError.NoEndpoints(device))
                    return false
                }

                // Store connection info
                usbDevice = device
                usbConnection = connection
                usbInterface = selectedInterface
                readEndpoint = readEp
                writeEndpoint = writeEp

                // Start communication threads
                startCommunication()

                isConnected.set(true)
                listener.onDeviceConnected(device)

                Log.i(TAG, "Connected successfully to ${device.deviceName}")
                return true

            } catch (e: Exception) {
                Log.e(TAG, "Error connecting to device", e)
                listener.onError(UsbError.ConnectionException(device, e))
                return false
            }
        }
    }

    /**
     * Check if interface has required endpoints
     */
    private fun hasRequiredEndpoints(iface: UsbInterface): Boolean {
        var hasIn = false
        var hasOut = false

        for (i in 0 until iface.endpointCount) {
            val endpoint = iface.getEndpoint(i)
            if (endpoint.type == android.hardware.usb.UsbConstants.USB_ENDPOINT_XFER_BULK) {
                if (endpoint.direction == android.hardware.usb.UsbConstants.USB_DIR_IN) {
                    hasIn = true
                } else {
                    hasOut = true
                }
            }
        }

        return hasIn && hasOut
    }

    /**
     * Start communication threads
     */
    private fun startCommunication() {
        isRunning.set(true)

        // Start reader thread
        readerThread = thread(name = "USB-Reader") {
            runReader()
        }

        // Start writer thread
        writerThread = thread(name = "USB-Writer") {
            runWriter()
        }

        Log.i(TAG, "Communication threads started")
    }

    /**
     * Reader thread main loop
     */
    private fun runReader() {
        val buffer = ByteArray(READ_BUFFER_SIZE)
        var consecutiveErrors = 0

        while (isRunning.get()) {
            try {
                val connection = usbConnection ?: break
                val endpoint = readEndpoint ?: break

                val bytesRead = connection.bulkTransfer(
                    endpoint,
                    buffer,
                    buffer.size,
                    USB_TIMEOUT_MS
                )

                if (bytesRead > 0) {
                    consecutiveErrors = 0
                    stats.bytesReceived += bytesRead

                    // Parse data
                    val data = buffer.copyOf(bytesRead)
                    parseIncomingData(data)

                } else if (bytesRead < 0) {
                    consecutiveErrors++
                    if (consecutiveErrors > 10) {
                        Log.e(TAG, "Too many consecutive read errors, disconnecting")
                        listener.onError(UsbError.ReadError("Too many consecutive errors"))
                        disconnect()
                        break
                    }
                    Thread.sleep(10)
                }

            } catch (e: Exception) {
                Log.e(TAG, "Error in reader thread", e)
                stats.recordError("Read error: ${e.message}")
                consecutiveErrors++
                if (consecutiveErrors > 10) {
                    listener.onError(UsbError.ReadError(e.message ?: "Unknown error"))
                    disconnect()
                    break
                }
                Thread.sleep(100)
            }
        }

        Log.i(TAG, "Reader thread stopped")
    }

    /**
     * Parse incoming data
     */
    private fun parseIncomingData(data: ByteArray) {
        try {
            val frames = parser.parseData(data)

            frames.forEach { frame ->
                stats.framesReceived++

                when (frame) {
                    is UsbFrame.VideoFrame -> {
                        stats.videoFramesReceived++
                        val assembled = assembler.addFramePart(frame)
                        assembled?.let {
                            listener.onVideoFrameReceived(it)
                        }
                    }

                    is UsbFrame.TelemetryFrame -> {
                        stats.telemetryFramesReceived++
                        listener.onTelemetryReceived(frame.data)
                    }

                    is UsbFrame.OsdFrame -> {
                        stats.osdFramesReceived++
                        listener.onOsdDataReceived(frame.stats, frame.osdBuffer)
                    }

                    is UsbFrame.ConfigFrame -> {
                        stats.configFramesReceived++
                        listener.onConfigReceived(frame)
                    }
                }
            }

            // Update stats periodically
            if (stats.framesReceived % 100 == 0L) {
                listener.onStatsUpdated(stats)
            }

        } catch (e: Exception) {
            Log.e(TAG, "Error parsing data", e)
            stats.framingErrors++
            stats.recordError("Parse error: ${e.message}")
        }
    }

    /**
     * Writer thread main loop
     */
    private fun runWriter() {
        while (isRunning.get()) {
            try {
                val data = writeQueue.poll()
                if (data == null) {
                    Thread.sleep(10)
                    continue
                }

                val connection = usbConnection ?: break
                val endpoint = writeEndpoint ?: break

                val bytesWritten = connection.bulkTransfer(
                    endpoint,
                    data,
                    data.size,
                    USB_TIMEOUT_MS
                )

                if (bytesWritten >= 0) {
                    stats.bytesSent += bytesWritten
                    stats.framesSent++
                } else {
                    Log.w(TAG, "Write failed: $bytesWritten")
                    stats.recordError("Write failed")
                }

            } catch (e: Exception) {
                Log.e(TAG, "Error in writer thread", e)
                stats.recordError("Write error: ${e.message}")
                Thread.sleep(100)
            }
        }

        Log.i(TAG, "Writer thread stopped")
    }

    /**
     * Send control command to ESP32-S3
     */
    fun sendControlCommand(
        type: Ground2AirPacketType,
        payload: ByteArray
    ): Boolean {
        try {
            val frame = buildFrame(type.value, payload)
            writeQueue.offer(frame)
            return true
        } catch (e: Exception) {
            Log.e(TAG, "Error sending control command", e)
            stats.recordError("Send error: ${e.message}")
            return false
        }
    }

    /**
     * Send configuration to ESP32-S3
     */
    fun sendConfig(
        airDeviceId: UShort,
        gsDeviceId: UShort,
        camera: CameraConfig,
        dataChannel: DataChannelConfig
    ): Boolean {
        // Build config packet (simplified - implement full serialization as needed)
        val payload = ByteArray(64)
        return sendControlCommand(Ground2AirPacketType.CONFIG, payload)
    }

    /**
     * Send telemetry data to ESP32-S3
     */
    fun sendTelemetry(data: ByteArray): Boolean {
        return sendControlCommand(Ground2AirPacketType.TELEMETRY, data)
    }

    /**
     * Build USB frame with header and CRC
     */
    private fun buildFrame(type: Byte, payload: ByteArray): ByteArray {
        val frameSize = ProtocolConstants.FRAME_HEADER_SIZE + payload.size + 1  // +1 for CRC
        val buffer = ByteBuffer.allocate(frameSize).order(ByteOrder.LITTLE_ENDIAN)

        // Header
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE1)
        buffer.put(ProtocolConstants.FRAME_SYNC_BYTE2)
        buffer.put(type)
        buffer.putInt(payload.size + 1)  // Size includes CRC

        // Payload
        buffer.put(payload)

        // Calculate CRC (excluding sync bytes, including everything else)
        val crc = CRC32()
        crc.update(buffer.array(), 2, buffer.position() - 2)
        buffer.put((crc.value and 0xFF).toByte())

        return buffer.array()
    }

    /**
     * Disconnect from USB device
     */
    fun disconnect() {
        synchronized(this) {
            Log.i(TAG, "Disconnecting...")

            isRunning.set(false)
            isConnected.set(false)

            // Stop threads
            readerThread?.interrupt()
            writerThread?.interrupt()

            try {
                readerThread?.join(1000)
                writerThread?.join(1000)
            } catch (e: InterruptedException) {
                Log.w(TAG, "Interrupted while waiting for threads")
            }

            // Close USB connection
            usbConnection?.let { connection ->
                usbInterface?.let { iface ->
                    connection.releaseInterface(iface)
                }
                connection.close()
            }

            // Clear references
            usbDevice = null
            usbConnection = null
            usbInterface = null
            readEndpoint = null
            writeEndpoint = null

            // Clear queues
            writeQueue.clear()

            // Reset parsers
            parser.reset()
            assembler.reset()

            Log.i(TAG, "Disconnected")
        }
    }

    /**
     * Check if connected
     */
    fun isConnected(): Boolean = isConnected.get()

    /**
     * Get current statistics
     */
    fun getStats(): UsbStats = stats

    /**
     * Get frame assembly statistics
     */
    fun getAssemblyStats(): FrameAssemblyStats = assembler.getStats()

    /**
     * Get uptime in milliseconds
     */
    fun getUptimeMs(): Long = System.currentTimeMillis() - startTime

    /**
     * Cleanup resources
     */
    fun cleanup() {
        disconnect()
        try {
            context.unregisterReceiver(usbReceiver)
        } catch (e: Exception) {
            Log.w(TAG, "Error unregistering receiver", e)
        }
    }
}

/**
 * USB error types
 */
sealed class UsbError {
    data class NoSuitableInterface(val device: UsbDevice) : UsbError()
    data class ConnectionFailed(val device: UsbDevice) : UsbError()
    data class InterfaceClaimFailed(val device: UsbDevice) : UsbError()
    data class NoEndpoints(val device: UsbDevice) : UsbError()
    data class ConnectionException(val device: UsbDevice, val exception: Exception) : UsbError()
    data class ReadError(val message: String) : UsbError()
    data class WriteError(val message: String) : UsbError()
}

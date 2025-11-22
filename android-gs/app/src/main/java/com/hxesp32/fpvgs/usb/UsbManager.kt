package com.hxesp32.fpvgs.usb

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.hardware.usb.UsbDevice
import android.hardware.usb.UsbManager
import com.hoho.android.usbserial.driver.UsbSerialDriver
import com.hoho.android.usbserial.driver.UsbSerialPort
import com.hoho.android.usbserial.driver.UsbSerialProber
import com.hxesp32.fpvgs.data.model.ConnectionState
import com.hxesp32.fpvgs.data.model.UsbDeviceInfo
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import timber.log.Timber

/**
 * Manages USB device connection and communication
 */
class UsbDeviceManager(private val context: Context) {

    private val usbManager = context.getSystemService(Context.USB_SERVICE) as UsbManager
    private var serialPort: UsbSerialPort? = null
    private var currentDriver: UsbSerialDriver? = null

    private val _connectionState = MutableStateFlow<ConnectionState>(ConnectionState.Disconnected)
    val connectionState: StateFlow<ConnectionState> = _connectionState.asStateFlow()

    private val usbReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            when (intent.action) {
                ACTION_USB_PERMISSION -> {
                    synchronized(this) {
                        val device: UsbDevice? = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                        if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                            device?.let { connectToDevice(it) }
                        } else {
                            Timber.w("USB permission denied for device: ${device?.deviceName}")
                            _connectionState.value = ConnectionState.Error("USB permission denied")
                        }
                    }
                }
                UsbManager.ACTION_USB_DEVICE_DETACHED -> {
                    val device: UsbDevice? = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE)
                    if (device == currentDriver?.device) {
                        disconnect()
                    }
                }
            }
        }
    }

    init {
        val filter = IntentFilter().apply {
            addAction(ACTION_USB_PERMISSION)
            addAction(UsbManager.ACTION_USB_DEVICE_DETACHED)
        }
        context.registerReceiver(usbReceiver, filter)
    }

    /**
     * Find and connect to ESP32-S3 device
     */
    fun findAndConnect() {
        _connectionState.value = ConnectionState.Connecting

        val availableDrivers = UsbSerialProber.getDefaultProber().findAllDrivers(usbManager)

        if (availableDrivers.isEmpty()) {
            Timber.w("No USB devices found")
            _connectionState.value = ConnectionState.Error("No USB device found")
            return
        }

        // Try to find ESP32-S3 device (Espressif VID: 0x303A)
        val esp32Driver = availableDrivers.firstOrNull { driver ->
            driver.device.vendorId == ESPRESSIF_VID
        } ?: availableDrivers.first()

        val device = esp32Driver.device

        if (!usbManager.hasPermission(device)) {
            val permissionIntent = PendingIntent.getBroadcast(
                context,
                0,
                Intent(ACTION_USB_PERMISSION),
                PendingIntent.FLAG_MUTABLE
            )
            usbManager.requestPermission(device, permissionIntent)
        } else {
            connectToDevice(device)
        }
    }

    private fun connectToDevice(device: UsbDevice) {
        try {
            val driver = UsbSerialProber.getDefaultProber().probeDevice(device)
            if (driver == null) {
                _connectionState.value = ConnectionState.Error("No driver for device")
                return
            }

            val connection = usbManager.openDevice(driver.device)
            if (connection == null) {
                _connectionState.value = ConnectionState.Error("Failed to open device")
                return
            }

            val port = driver.ports[0]
            port.open(connection)

            // Configure serial port parameters
            port.setParameters(
                BAUD_RATE,
                UsbSerialPort.DATABITS_8,
                UsbSerialPort.STOPBITS_1,
                UsbSerialPort.PARITY_NONE
            )

            serialPort = port
            currentDriver = driver

            val deviceInfo = UsbDeviceInfo(
                deviceName = device.deviceName,
                vendorId = device.vendorId,
                productId = device.productId,
                serialNumber = device.serialNumber,
                manufacturer = device.manufacturerName,
                product = device.productName
            )

            _connectionState.value = ConnectionState.Connected(deviceInfo)
            Timber.i("Connected to USB device: ${deviceInfo.deviceName}")

        } catch (e: Exception) {
            Timber.e(e, "Error connecting to USB device")
            _connectionState.value = ConnectionState.Error("Connection failed: ${e.message}", e)
        }
    }

    /**
     * Disconnect from USB device
     */
    fun disconnect() {
        try {
            serialPort?.close()
            serialPort = null
            currentDriver = null
            _connectionState.value = ConnectionState.Disconnected
            Timber.i("Disconnected from USB device")
        } catch (e: Exception) {
            Timber.e(e, "Error disconnecting from USB device")
        }
    }

    /**
     * Read data from USB device
     */
    fun read(buffer: ByteArray, timeout: Int = READ_TIMEOUT): Int {
        return try {
            serialPort?.read(buffer, timeout) ?: 0
        } catch (e: Exception) {
            Timber.e(e, "Error reading from USB device")
            0
        }
    }

    /**
     * Write data to USB device
     */
    fun write(data: ByteArray, timeout: Int = WRITE_TIMEOUT): Int {
        return try {
            serialPort?.write(data, timeout) ?: 0
        } catch (e: Exception) {
            Timber.e(e, "Error writing to USB device")
            0
        }
    }

    /**
     * Release resources
     */
    fun release() {
        disconnect()
        try {
            context.unregisterReceiver(usbReceiver)
        } catch (e: Exception) {
            Timber.e(e, "Error unregistering USB receiver")
        }
    }

    companion object {
        private const val ACTION_USB_PERMISSION = "com.hxesp32.fpvgs.USB_PERMISSION"
        private const val ESPRESSIF_VID = 0x303A
        private const val BAUD_RATE = 2_000_000 // 2 Mbps
        private const val READ_TIMEOUT = 1000
        private const val WRITE_TIMEOUT = 1000
    }
}

package com.hxesp32.fpvgs.data.model

/**
 * Represents the USB connection state
 */
sealed class ConnectionState {
    object Disconnected : ConnectionState()
    object Connecting : ConnectionState()
    data class Connected(val deviceInfo: UsbDeviceInfo) : ConnectionState()
    data class Error(val message: String, val throwable: Throwable? = null) : ConnectionState()
}

/**
 * Information about the connected USB device
 */
data class UsbDeviceInfo(
    val deviceName: String,
    val vendorId: Int,
    val productId: Int,
    val serialNumber: String?,
    val manufacturer: String?,
    val product: String?
)

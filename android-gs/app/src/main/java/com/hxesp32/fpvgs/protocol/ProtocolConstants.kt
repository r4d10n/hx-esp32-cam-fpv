package com.hxesp32.fpvgs.protocol

/**
 * Protocol constants for ESP32-S3 USB communication
 * Based on components/common/packets.h
 */
object ProtocolConstants {
    const val DEFAULT_WIFI_CHANNEL: Byte = 7
    const val FW_VERSION = "0.3"
    const val FEC_K = 6
    const val FEC_N = 12
    const val PACKET_VERSION: Byte = 1

    // USB Frame protocol constants
    const val FRAME_SYNC_BYTE1: Byte = 0xA5.toByte()
    const val FRAME_SYNC_BYTE2: Byte = 0x5A.toByte()
    const val FRAME_HEADER_SIZE = 8  // SYNC(2) + TYPE(1) + SIZE(4) + CRC(1)
    const val MAX_FRAME_SIZE = 65536
    const val USB_BULK_TRANSFER_SIZE = 16384

    // OSD constants
    const val OSD_COLS = 53
    const val OSD_COLS_H = 7
    const val OSD_ROWS = 20
    const val OSD_BUFFER_SIZE = (OSD_ROWS * OSD_COLS + OSD_ROWS * OSD_COLS_H)
}

/**
 * WiFi rates supported by ESP32
 */
enum class WifiRate(val value: Byte) {
    RATE_B_2M_CCK(0),
    RATE_B_2M_CCK_S(1),
    RATE_B_5_5M_CCK(2),
    RATE_B_5_5M_CCK_S(3),
    RATE_B_11M_CCK(4),
    RATE_B_11M_CCK_S(5),

    RATE_G_6M_ODFM(6),
    RATE_G_9M_ODFM(7),
    RATE_G_12M_ODFM(8),
    RATE_G_18M_ODFM(9),
    RATE_G_24M_ODFM(10),
    RATE_G_36M_ODFM(11),
    RATE_G_48M_ODFM(12),
    RATE_G_54M_ODFM(13),

    RATE_N_6_5M_MCS0(14),
    RATE_N_7_2M_MCS0_S(15),
    RATE_N_13M_MCS1(16),
    RATE_N_14_4M_MCS1_S(17),
    RATE_N_19_5M_MCS2(18),
    RATE_N_21_7M_MCS2_S(19),
    RATE_N_26M_MCS3(20),
    RATE_N_28_9M_MCS3_S(21),
    RATE_N_39M_MCS4(22),
    RATE_N_43_3M_MCS4_S(23),
    RATE_N_52M_MCS5(24),
    RATE_N_57_8M_MCS5_S(25),
    RATE_N_58M_MCS6(26),
    RATE_N_65M_MCS6_S(27),
    RATE_N_65M_MCS7(28),
    RATE_N_72M_MCS7_S(29);

    companion object {
        fun fromValue(value: Byte): WifiRate? = values().find { it.value == value }
    }
}

/**
 * Video resolutions supported
 */
enum class Resolution(val value: Byte, val width: Int, val height: Int) {
    QVGA(0, 320, 240),
    CIF(1, 400, 296),
    HVGA(2, 480, 320),
    VGA(3, 640, 480),
    VGA16(4, 640, 360),
    SVGA(5, 800, 600),
    SVGA16(6, 800, 456),
    XGA(7, 1024, 768),
    XGA16(8, 1024, 576),
    SXGA(9, 1280, 960),
    HD(10, 1280, 720),
    UXGA(11, 1600, 1200);

    companion object {
        fun fromValue(value: Byte): Resolution? = values().find { it.value == value }
    }
}

/**
 * Packet types for Air2Ground communication
 */
enum class Air2GroundPacketType(val value: Byte) {
    VIDEO(0),
    TELEMETRY(1),
    OSD(2),
    CONFIG(3);

    companion object {
        fun fromValue(value: Byte): Air2GroundPacketType? = values().find { it.value == value }
    }
}

/**
 * Packet types for Ground2Air communication
 */
enum class Ground2AirPacketType(val value: Byte) {
    TELEMETRY(0),
    CONFIG(1),
    CONNECT(2);

    companion object {
        fun fromValue(value: Byte): Ground2AirPacketType? = values().find { it.value == value }
    }
}

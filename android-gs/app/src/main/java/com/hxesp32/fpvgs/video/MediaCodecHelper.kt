package com.hxesp32.fpvgs.video

import android.media.MediaCodec
import android.media.MediaCodecInfo
import android.media.MediaCodecList
import android.media.MediaFormat
import android.os.Build
import android.util.Log
import android.view.Surface

/**
 * Helper class for MediaCodec configuration and capability checking
 *
 * Provides utilities for:
 * - Finding optimal hardware decoder
 * - Checking codec capabilities
 * - Creating optimized MediaFormat configurations
 * - Low-latency decoder setup
 */
object MediaCodecHelper {
    private const val TAG = "MediaCodecHelper"
    private const val MIME_TYPE_H264 = MediaFormat.MIMETYPE_VIDEO_AVC

    /**
     * Codec capability information
     */
    data class CodecCapability(
        val name: String,
        val isHardware: Boolean,
        val maxWidth: Int,
        val maxHeight: Int,
        val maxFrameRate: Int,
        val supportsLowLatency: Boolean,
        val supportedColorFormats: List<Int>,
        val supportedProfiles: List<Int>,
        val maxInstances: Int
    ) {
        override fun toString(): String {
            val hwType = if (isHardware) "HW" else "SW"
            return "[$hwType] $name: ${maxWidth}x${maxHeight}@${maxFrameRate}fps, " +
                   "lowLatency=$supportsLowLatency"
        }
    }

    /**
     * Find the best H.264 decoder for FPV use
     *
     * Prioritizes:
     * 1. Hardware decoders
     * 2. Low-latency support
     * 3. Higher performance capabilities
     */
    fun findBestH264Decoder(): CodecCapability? {
        val decoders = findAllH264Decoders()
        if (decoders.isEmpty()) {
            Log.e(TAG, "No H.264 decoders found!")
            return null
        }

        // Sort by priority: hardware first, then low-latency support
        val sorted = decoders.sortedWith(
            compareByDescending<CodecCapability> { it.isHardware }
                .thenByDescending { it.supportsLowLatency }
                .thenByDescending { it.maxWidth * it.maxHeight * it.maxFrameRate }
        )

        val best = sorted.first()
        Log.i(TAG, "Selected decoder: $best")

        return best
    }

    /**
     * Find all available H.264 decoders
     */
    fun findAllH264Decoders(): List<CodecCapability> {
        val codecs = mutableListOf<CodecCapability>()
        val codecList = MediaCodecList(MediaCodecList.ALL_CODECS)

        for (codecInfo in codecList.codecInfos) {
            if (codecInfo.isEncoder) continue

            val supportedTypes = codecInfo.supportedTypes
            if (!supportedTypes.contains(MIME_TYPE_H264)) continue

            try {
                val capability = getCodecCapability(codecInfo)
                codecs.add(capability)
                Log.d(TAG, "Found: $capability")
            } catch (e: Exception) {
                Log.w(TAG, "Failed to query codec ${codecInfo.name}", e)
            }
        }

        return codecs
    }

    /**
     * Get detailed capability information for a codec
     */
    private fun getCodecCapability(codecInfo: MediaCodecInfo): CodecCapability {
        val capabilities = codecInfo.getCapabilitiesForType(MIME_TYPE_H264)
        val videoCapabilities = capabilities.videoCapabilities

        // Check if hardware accelerated
        val isHardware = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            codecInfo.isHardwareAccelerated
        } else {
            // Heuristic for older Android versions
            !codecInfo.name.startsWith("OMX.google")
        }

        // Low-latency support
        val supportsLowLatency = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            capabilities.isFeatureSupported(MediaCodecInfo.CodecCapabilities.FEATURE_LowLatency)
        } else {
            false // Not available on older versions
        }

        // Color formats
        val colorFormats = capabilities.colorFormats.toList()

        // Supported profiles
        val profiles = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            capabilities.profileLevels.map { it.profile }.distinct()
        } else {
            emptyList()
        }

        // Max instances
        val maxInstances = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            capabilities.maxSupportedInstances
        } else {
            1
        }

        return CodecCapability(
            name = codecInfo.name,
            isHardware = isHardware,
            maxWidth = videoCapabilities.supportedWidths.upper,
            maxHeight = videoCapabilities.supportedHeights.upper,
            maxFrameRate = videoCapabilities.supportedFrameRates.upper.toInt(),
            supportsLowLatency = supportsLowLatency,
            supportedColorFormats = colorFormats,
            supportedProfiles = profiles,
            maxInstances = maxInstances
        )
    }

    /**
     * Create a low-latency MediaFormat for H.264 decoding
     *
     * Optimized for FPV with minimal buffering and latency
     */
    fun createLowLatencyFormat(
        width: Int,
        height: Int,
        fps: Int = 30,
        surface: Surface? = null
    ): MediaFormat {
        return MediaFormat.createVideoFormat(MIME_TYPE_H264, width, height).apply {
            // Priority and latency
            setInteger(MediaFormat.KEY_PRIORITY, 0) // Realtime
            setInteger(MediaFormat.KEY_LOW_LATENCY, 1)
            setInteger(MediaFormat.KEY_LATENCY, 0)

            // Operating rate - use maximum for lowest latency
            setInteger(MediaFormat.KEY_OPERATING_RATE, Int.MAX_VALUE)

            // Frame rate hint
            setInteger(MediaFormat.KEY_FRAME_RATE, fps)

            // Max input size (0 = dynamic)
            setInteger(MediaFormat.KEY_MAX_INPUT_SIZE, 0)

            // Color format for surface rendering
            if (surface != null) {
                setInteger(
                    MediaFormat.KEY_COLOR_FORMAT,
                    MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface
                )
            }

            // Additional optimizations for Android 10+
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                // Disable frame dropping
                setInteger(MediaFormat.KEY_ALLOW_FRAME_DROP, 0)
            }

            Log.d(TAG, "Created low-latency format: $this")
        }
    }

    /**
     * Create MediaCodec instance with optimal configuration
     */
    fun createOptimizedDecoder(
        width: Int,
        height: Int,
        surface: Surface?,
        fps: Int = 30,
        preferredCodecName: String? = null
    ): MediaCodec? {
        try {
            // Find or use preferred decoder
            val codecName = preferredCodecName ?: findBestH264Decoder()?.name
            if (codecName == null) {
                Log.e(TAG, "No suitable H.264 decoder found")
                return null
            }

            Log.i(TAG, "Creating decoder: $codecName")

            // Create codec
            val codec = MediaCodec.createByCodecName(codecName)

            // Configure with low-latency format
            val format = createLowLatencyFormat(width, height, fps, surface)
            codec.configure(format, surface, null, 0)

            return codec

        } catch (e: Exception) {
            Log.e(TAG, "Failed to create decoder", e)
            return null
        }
    }

    /**
     * Check if device supports low-latency decoding
     */
    fun supportsLowLatency(): Boolean {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return false
        }

        val decoders = findAllH264Decoders()
        return decoders.any { it.supportsLowLatency }
    }

    /**
     * Get recommended buffer size for given resolution
     */
    fun getRecommendedBufferSize(width: Int, height: Int): Int {
        val pixels = width * height
        // Rough estimate: 1.5 bytes per pixel for compressed H.264
        return (pixels * 1.5).toInt()
    }

    /**
     * Check if resolution is supported
     */
    fun isResolutionSupported(width: Int, height: Int): Boolean {
        val decoder = findBestH264Decoder() ?: return false
        return width <= decoder.maxWidth && height <= decoder.maxHeight
    }

    /**
     * Get codec performance information
     */
    fun getPerformanceInfo(): String {
        val sb = StringBuilder()
        sb.appendLine("=== MediaCodec Performance Info ===")
        sb.appendLine("Android Version: ${Build.VERSION.SDK_INT} (${Build.VERSION.RELEASE})")
        sb.appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL}")
        sb.appendLine("Low-latency support: ${supportsLowLatency()}")
        sb.appendLine()

        val decoders = findAllH264Decoders()
        sb.appendLine("Available H.264 Decoders: ${decoders.size}")
        sb.appendLine()

        decoders.forEach { codec ->
            sb.appendLine(codec.toString())
        }

        return sb.toString()
    }

    /**
     * Create CSD buffer (Codec Specific Data) from SPS and PPS
     */
    fun createCsdBuffer(sps: ByteArray, pps: ByteArray): ByteArray {
        val startCode = byteArrayOf(0x00, 0x00, 0x00, 0x01)
        val csd = ByteArray(sps.size + pps.size + startCode.size * 2)

        var offset = 0
        System.arraycopy(startCode, 0, csd, offset, startCode.size)
        offset += startCode.size
        System.arraycopy(sps, 0, csd, offset, sps.size)
        offset += sps.size
        System.arraycopy(startCode, 0, csd, offset, startCode.size)
        offset += startCode.size
        System.arraycopy(pps, 0, csd, offset, pps.size)

        return csd
    }

    /**
     * Parse SPS to extract video parameters
     */
    fun parseSpsParameters(sps: ByteArray): SpsParameters? {
        try {
            // Strip start code if present
            val data = when {
                sps.size >= 4 && sps[0] == 0x00.toByte() &&
                sps[1] == 0x00.toByte() && sps[2] == 0x00.toByte() &&
                sps[3] == 0x01.toByte() -> sps.copyOfRange(4, sps.size)
                sps.size >= 3 && sps[0] == 0x00.toByte() &&
                sps[1] == 0x00.toByte() && sps[2] == 0x01.toByte() ->
                    sps.copyOfRange(3, sps.size)
                else -> sps
            }

            if (data.isEmpty()) return null

            val nalType = (data[0].toInt() and 0x1F)
            if (nalType != 7) { // SPS NAL type
                Log.w(TAG, "Not an SPS NAL unit: type=$nalType")
                return null
            }

            // Profile and level
            val profile = data[1].toInt() and 0xFF
            val level = data[3].toInt() and 0xFF

            // Note: Full SPS parsing requires bitstream parsing
            // This is a simplified version
            Log.d(TAG, "SPS parsed: profile=$profile, level=$level")

            return SpsParameters(profile, level)

        } catch (e: Exception) {
            Log.e(TAG, "Failed to parse SPS", e)
            return null
        }
    }

    data class SpsParameters(
        val profile: Int,
        val level: Int
    )

    /**
     * Detect NAL unit type
     */
    fun detectNalType(data: ByteArray): NalUnitType {
        val offset = when {
            data.size >= 4 && data[0] == 0x00.toByte() &&
            data[1] == 0x00.toByte() && data[2] == 0x00.toByte() &&
            data[3] == 0x01.toByte() -> 4
            data.size >= 3 && data[0] == 0x00.toByte() &&
            data[1] == 0x00.toByte() && data[2] == 0x01.toByte() -> 3
            else -> 0
        }

        if (offset >= data.size) return NalUnitType.UNKNOWN

        val nalType = (data[offset].toInt() and 0x1F)

        return when (nalType) {
            1 -> NalUnitType.NON_IDR
            5 -> NalUnitType.IDR
            7 -> NalUnitType.SPS
            8 -> NalUnitType.PPS
            6 -> NalUnitType.SEI
            9 -> NalUnitType.AUD
            else -> NalUnitType.UNKNOWN
        }
    }

    enum class NalUnitType {
        NON_IDR,  // Non-IDR slice
        IDR,      // IDR slice (keyframe)
        SPS,      // Sequence Parameter Set
        PPS,      // Picture Parameter Set
        SEI,      // Supplemental Enhancement Information
        AUD,      // Access Unit Delimiter
        UNKNOWN
    }
}

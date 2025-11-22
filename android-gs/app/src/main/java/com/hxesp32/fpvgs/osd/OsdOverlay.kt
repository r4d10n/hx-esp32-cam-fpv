package com.hxesp32.fpvgs.osd

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.View
import com.hxesp32.fpvgs.data.*
import kotlin.math.cos
import kotlin.math.sin

/**
 * OSD (On-Screen Display) overlay for displaying telemetry and video information.
 * Draws over the video surface using Canvas.
 */
class OsdOverlay @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    // Telemetry state
    private var telemetryState = TelemetryState()
    private var config = OsdConfig()

    // Paint objects for different elements
    private val textPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        textSize = 24f
        typeface = Typeface.MONOSPACE
        setShadowLayer(3f, 1f, 1f, Color.BLACK)
    }

    private val strokePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        style = Paint.Style.STROKE
        strokeWidth = 2f
        setShadowLayer(2f, 1f, 1f, Color.BLACK)
    }

    private val fillPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        style = Paint.Style.FILL
    }

    private val backgroundPaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.argb(128, 0, 0, 0)
        style = Paint.Style.FILL
    }

    // Layout constants
    private val margin = 20f
    private val lineHeight = 30f
    private val elementSpacing = 10f

    init {
        setWillNotDraw(false)
        setLayerType(LAYER_TYPE_HARDWARE, null) // Use GPU acceleration
    }

    /**
     * Update telemetry data
     */
    fun updateTelemetry(state: TelemetryState) {
        telemetryState = state
        invalidate()
    }

    /**
     * Update OSD configuration
     */
    fun updateConfig(newConfig: OsdConfig) {
        config = newConfig
        textPaint.textSize = config.fontSize
        textPaint.color = config.textColor
        strokePaint.color = config.strokeColor
        invalidate()
    }

    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        if (!config.enabled) return

        // Draw each OSD element if enabled
        var yOffset = margin

        // Top-left elements
        if (config.showRssi) {
            yOffset = drawRssiIndicator(canvas, margin, yOffset)
        }

        if (config.showLinkQuality) {
            yOffset = drawLinkQuality(canvas, margin, yOffset)
        }

        if (config.showVideoStats) {
            yOffset = drawVideoStats(canvas, margin, yOffset)
        }

        if (config.showLatency) {
            yOffset = drawLatency(canvas, margin, yOffset)
        }

        // Top-right elements
        var yOffsetRight = margin

        if (config.showBattery && telemetryState.batteryData.available) {
            yOffsetRight = drawBattery(canvas, width - margin, yOffsetRight)
        }

        if (config.showGps && telemetryState.gpsData.fix) {
            yOffsetRight = drawGps(canvas, width - margin, yOffsetRight)
        }

        if (config.showRecording && telemetryState.airStats.airRecordState) {
            yOffsetRight = drawRecordingIndicator(canvas, width - margin, yOffsetRight)
        }

        // Center elements
        if (config.showArtificialHorizon && telemetryState.imuData.available) {
            drawArtificialHorizon(canvas, width / 2f, height / 2f)
        }

        if (config.showCrosshair) {
            drawCrosshair(canvas, width / 2f, height / 2f)
        }

        // Bottom elements
        if (config.showCustomText && config.customText.isNotEmpty()) {
            drawCustomText(canvas, margin, height - margin)
        }

        // Additional stats (bottom right)
        if (config.showDetailedStats) {
            drawDetailedStats(canvas, width - margin, height - margin)
        }
    }

    /**
     * Draw RSSI signal strength indicator
     */
    private fun drawRssiIndicator(canvas: Canvas, x: Float, y: Float): Float {
        val rssi = telemetryState.airStats.rssiDbm
        val snr = telemetryState.airStats.snr

        // Determine signal quality color
        val color = when {
            rssi < 30 -> Color.GREEN
            rssi < 60 -> Color.YELLOW
            rssi < 80 -> Color.rgb(255, 165, 0) // Orange
            else -> Color.RED
        }

        // Draw RSSI value
        textPaint.color = color
        val text = "RSSI: -%d dBm  SNR: %d dB".format(rssi, snr)
        canvas.drawText(text, x, y + textPaint.textSize, textPaint)

        // Draw signal bars
        val barWidth = 10f
        val barHeight = 30f
        val barSpacing = 5f
        val numBars = 5
        val filledBars = when {
            rssi < 30 -> 5
            rssi < 50 -> 4
            rssi < 70 -> 3
            rssi < 85 -> 2
            else -> 1
        }

        var barX = x
        val barY = y + textPaint.textSize + elementSpacing

        for (i in 0 until numBars) {
            val height = barHeight * (i + 1) / numBars
            val barTop = barY + barHeight - height

            fillPaint.color = if (i < filledBars) color else Color.GRAY
            canvas.drawRect(barX, barTop, barX + barWidth, barY + barHeight, fillPaint)

            strokePaint.color = Color.WHITE
            canvas.drawRect(barX, barTop, barX + barWidth, barY + barHeight, strokePaint)

            barX += barWidth + barSpacing
        }

        return barY + barHeight + lineHeight
    }

    /**
     * Draw link quality indicator
     */
    private fun drawLinkQuality(canvas: Canvas, x: Float, y: Float): Float {
        val quality = telemetryState.calculateLinkQuality()
        val packetLoss = telemetryState.groundStats.packetLoss

        val color = when {
            quality > 80 -> Color.GREEN
            quality > 60 -> Color.YELLOW
            quality > 40 -> Color.rgb(255, 165, 0)
            else -> Color.RED
        }

        textPaint.color = color
        val text = "Link: %.0f%%  Loss: %.1f%%".format(quality, packetLoss)
        canvas.drawText(text, x, y + textPaint.textSize, textPaint)

        return y + lineHeight
    }

    /**
     * Draw video statistics (FPS, bitrate)
     */
    private fun drawVideoStats(canvas: Canvas, x: Float, y: Float): Float {
        val fps = telemetryState.videoStats.fps
        val bitrate = telemetryState.videoStats.bitrateMbps
        val resolution = telemetryState.airStats.resolution

        textPaint.color = config.textColor
        val text = "%s @ %d FPS  %.2f Mbps".format(
            resolution.displayName,
            fps,
            bitrate
        )
        canvas.drawText(text, x, y + textPaint.textSize, textPaint)

        return y + lineHeight
    }

    /**
     * Draw latency display
     */
    private fun drawLatency(canvas: Canvas, x: Float, y: Float): Float {
        val latency = telemetryState.latencyMs
        val pingMin = telemetryState.groundStats.pingMinMS
        val pingMax = telemetryState.groundStats.pingMaxMS

        val color = when {
            latency < 100 -> Color.GREEN
            latency < 150 -> Color.YELLOW
            else -> Color.RED
        }

        textPaint.color = color
        val text = "Latency: %d ms (%d-%d)".format(latency, pingMin, pingMax)
        canvas.drawText(text, x, y + textPaint.textSize, textPaint)

        return y + lineHeight
    }

    /**
     * Draw battery status
     */
    private fun drawBattery(canvas: Canvas, x: Float, y: Float): Float {
        val battery = telemetryState.batteryData
        val voltage = battery.voltage
        val current = battery.current
        val percentage = battery.percentage
        val cells = battery.cells

        val color = when {
            battery.isLow -> Color.RED
            percentage < 30 -> Color.YELLOW
            else -> Color.GREEN
        }

        textPaint.color = color
        textPaint.textAlign = Paint.Align.RIGHT

        val text = "%.2fV (%dS) %.1fA %d%%".format(voltage, cells, current, percentage)
        canvas.drawText(text, x, y + textPaint.textSize, textPaint)

        // Draw battery icon
        val iconWidth = 60f
        val iconHeight = 25f
        val iconX = x - textPaint.measureText(text) - elementSpacing - iconWidth
        val iconY = y

        // Battery outline
        strokePaint.color = Color.WHITE
        canvas.drawRect(iconX, iconY, iconX + iconWidth, iconY + iconHeight, strokePaint)

        // Battery terminal
        canvas.drawRect(
            iconX + iconWidth,
            iconY + iconHeight * 0.3f,
            iconX + iconWidth + 5f,
            iconY + iconHeight * 0.7f,
            fillPaint
        )

        // Battery fill
        val fillWidth = (iconWidth - 4) * (percentage / 100f)
        fillPaint.color = color
        canvas.drawRect(
            iconX + 2,
            iconY + 2,
            iconX + 2 + fillWidth,
            iconY + iconHeight - 2,
            fillPaint
        )

        textPaint.textAlign = Paint.Align.LEFT

        return y + lineHeight
    }

    /**
     * Draw GPS coordinates
     */
    private fun drawGps(canvas: Canvas, x: Float, y: Float): Float {
        val gps = telemetryState.gpsData

        textPaint.color = if (gps.fix) Color.GREEN else Color.GRAY
        textPaint.textAlign = Paint.Align.RIGHT

        val text = gps.toFormattedString()
        canvas.drawText(text, x, y + textPaint.textSize, textPaint)

        textPaint.textAlign = Paint.Align.LEFT

        return y + lineHeight
    }

    /**
     * Draw recording indicator (blinking red dot)
     */
    private fun drawRecordingIndicator(canvas: Canvas, x: Float, y: Float): Float {
        val blink = (System.currentTimeMillis() / 500) % 2 == 0L

        if (blink) {
            fillPaint.color = Color.RED
            val radius = 10f
            canvas.drawCircle(x - 20, y + radius, radius, fillPaint)

            textPaint.color = Color.RED
            textPaint.textAlign = Paint.Align.RIGHT
            canvas.drawText("REC", x - 40, y + textPaint.textSize, textPaint)
            textPaint.textAlign = Paint.Align.LEFT
        }

        return y + lineHeight
    }

    /**
     * Draw artificial horizon for flight attitude
     */
    private fun drawArtificialHorizon(canvas: Canvas, centerX: Float, centerY: Float) {
        val imu = telemetryState.imuData
        if (!imu.available) return

        val size = 150f
        val pitch = imu.pitch
        val roll = imu.roll

        canvas.save()
        canvas.translate(centerX, centerY)
        canvas.rotate(-roll)

        // Draw horizon line
        val pitchOffset = pitch * 2f
        strokePaint.color = Color.WHITE
        strokePaint.strokeWidth = 3f
        canvas.drawLine(-size, pitchOffset, size, pitchOffset, strokePaint)

        // Draw sky (blue)
        fillPaint.color = Color.argb(100, 135, 206, 235)
        canvas.drawRect(-size, -size, size, pitchOffset, fillPaint)

        // Draw ground (brown)
        fillPaint.color = Color.argb(100, 139, 69, 19)
        canvas.drawRect(-size, pitchOffset, size, size, fillPaint)

        // Draw pitch ladder
        strokePaint.strokeWidth = 2f
        for (i in -30..30 step 10) {
            if (i == 0) continue
            val y = pitchOffset + i * 2f
            val lineWidth = if (i % 20 == 0) 40f else 20f
            canvas.drawLine(-lineWidth, y, lineWidth, y, strokePaint)
        }

        canvas.restore()

        // Draw center reference (aircraft symbol)
        strokePaint.color = Color.YELLOW
        strokePaint.strokeWidth = 3f
        canvas.drawLine(centerX - 40, centerY, centerX - 10, centerY, strokePaint)
        canvas.drawLine(centerX + 40, centerY, centerX + 10, centerY, strokePaint)
        canvas.drawCircle(centerX, centerY, 5f, strokePaint)
    }

    /**
     * Draw crosshair
     */
    private fun drawCrosshair(canvas: Canvas, centerX: Float, centerY: Float) {
        strokePaint.color = Color.WHITE
        strokePaint.strokeWidth = 1f

        val size = 20f
        val gap = 5f

        // Horizontal line
        canvas.drawLine(centerX - size, centerY, centerX - gap, centerY, strokePaint)
        canvas.drawLine(centerX + gap, centerY, centerX + size, centerY, strokePaint)

        // Vertical line
        canvas.drawLine(centerX, centerY - size, centerX, centerY - gap, strokePaint)
        canvas.drawLine(centerX, centerY + gap, centerX, centerY + size, strokePaint)

        // Center dot
        fillPaint.color = Color.WHITE
        canvas.drawCircle(centerX, centerY, 2f, fillPaint)
    }

    /**
     * Draw custom text overlay
     */
    private fun drawCustomText(canvas: Canvas, x: Float, y: Float): Float {
        textPaint.color = config.textColor
        textPaint.textAlign = Paint.Align.LEFT

        // Draw background
        val textWidth = textPaint.measureText(config.customText)
        val textHeight = textPaint.textSize
        canvas.drawRect(
            x - 5,
            y - textHeight - 5,
            x + textWidth + 5,
            y + 5,
            backgroundPaint
        )

        canvas.drawText(config.customText, x, y, textPaint)

        return y - lineHeight
    }

    /**
     * Draw detailed statistics panel
     */
    private fun drawDetailedStats(canvas: Canvas, x: Float, y: Float): Float {
        val stats = buildList {
            add("Air Pkt: ${telemetryState.airStats.outPacketRate}/s")
            add("GS Pkt: ${telemetryState.groundStats.totalInPackets}/s")
            add("FEC: ${telemetryState.groundStats.fecSuccessRate}%")
            add("WiFi: Ch${telemetryState.airStats.wifiChannel}")
            add("Rate: ${telemetryState.airStats.currentWifiRate.displayName}")
            if (telemetryState.airStats.temperature > 0) {
                add("Temp: ${telemetryState.airStats.temperature}°C")
            }
            if (telemetryState.airStats.sdDetected) {
                add("SD: %.1fGB/%.1fGB".format(
                    telemetryState.airStats.sdFreeSpaceGB,
                    telemetryState.airStats.sdTotalSpaceGB
                ))
            }
        }

        textPaint.textAlign = Paint.Align.RIGHT
        textPaint.color = config.textColor
        textPaint.textSize = config.fontSize * 0.8f

        var currentY = y
        stats.forEach { stat ->
            canvas.drawText(stat, x, currentY, textPaint)
            currentY -= lineHeight * 0.8f
        }

        textPaint.textAlign = Paint.Align.LEFT
        textPaint.textSize = config.fontSize

        return currentY
    }
}

/**
 * OSD configuration
 */
data class OsdConfig(
    val enabled: Boolean = true,
    val showRssi: Boolean = true,
    val showLinkQuality: Boolean = true,
    val showVideoStats: Boolean = true,
    val showLatency: Boolean = true,
    val showBattery: Boolean = true,
    val showGps: Boolean = true,
    val showRecording: Boolean = true,
    val showArtificialHorizon: Boolean = false,
    val showCrosshair: Boolean = true,
    val showCustomText: Boolean = false,
    val showDetailedStats: Boolean = true,
    val customText: String = "",
    val fontSize: Float = 24f,
    val textColor: Int = Color.WHITE,
    val strokeColor: Int = Color.WHITE,
    val backgroundColor: Int = Color.argb(128, 0, 0, 0),
    val transparency: Float = 1.0f
)

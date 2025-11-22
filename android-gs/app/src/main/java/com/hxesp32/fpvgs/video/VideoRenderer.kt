package com.hxesp32.fpvgs.video

import android.content.Context
import android.graphics.*
import android.util.AttributeSet
import android.view.SurfaceHolder
import android.view.SurfaceView
import com.hxesp32.fpvgs.data.Resolution
import java.io.ByteArrayInputStream
import kotlin.math.min

/**
 * Video renderer using SurfaceView for efficient video playback.
 * Handles MJPEG stream decoding and display with proper aspect ratio.
 */
class VideoRenderer @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null,
    defStyleAttr: Int = 0
) : SurfaceView(context, attrs, defStyleAttr), SurfaceHolder.Callback {

    private var videoWidth = 0
    private var videoHeight = 0
    private var surfaceWidth = 0
    private var surfaceHeight = 0

    // Scaling and positioning
    private val drawRect = Rect()
    private val srcRect = Rect()
    private var scaleMode = ScaleMode.FIT
    private var rotation = 0f

    // Video frame buffer
    private var currentBitmap: Bitmap? = null
    private val paint = Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG)

    // Performance tracking
    private var lastFrameTime = 0L
    private var frameCount = 0
    private var fps = 0

    // Callbacks
    var onFrameDecoded: ((fps: Int, frameSize: Int) -> Unit)? = null
    var onDecodingError: ((error: String) -> Unit)? = null

    init {
        holder.addCallback(this)
        setZOrderMediaOverlay(false) // Video layer behind OSD
    }

    /**
     * Scaling modes for video display
     */
    enum class ScaleMode {
        FIT,           // Fit video with letterboxing
        FILL,          // Fill screen, may crop
        STRETCH,       // Stretch to fill (distorts aspect ratio)
        ORIGINAL       // Original size (1:1)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        // Surface is ready
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        surfaceWidth = width
        surfaceHeight = height
        calculateDrawRect()
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        clearFrame()
    }

    /**
     * Set video resolution
     */
    fun setVideoResolution(resolution: Resolution) {
        setVideoResolution(resolution.width, resolution.height)
    }

    /**
     * Set video resolution by width and height
     */
    fun setVideoResolution(width: Int, height: Int) {
        if (videoWidth != width || videoHeight != height) {
            videoWidth = width
            videoHeight = height
            calculateDrawRect()
            post { requestLayout() }
        }
    }

    /**
     * Set scale mode
     */
    fun setScaleMode(mode: ScaleMode) {
        if (scaleMode != mode) {
            scaleMode = mode
            calculateDrawRect()
            invalidate()
        }
    }

    /**
     * Set rotation angle (0, 90, 180, 270)
     */
    fun setRotation(degrees: Float) {
        if (rotation != degrees) {
            rotation = degrees
            calculateDrawRect()
            invalidate()
        }
    }

    /**
     * Render a JPEG frame
     */
    fun renderFrame(jpegData: ByteArray) {
        try {
            // Decode JPEG
            val options = BitmapFactory.Options().apply {
                inPreferredConfig = Bitmap.Config.RGB_565 // Faster decoding
                inMutable = false
            }

            val bitmap = BitmapFactory.decodeByteArray(jpegData, 0, jpegData.size, options)
                ?: throw IllegalArgumentException("Failed to decode JPEG")

            // Update video dimensions if needed
            if (videoWidth != bitmap.width || videoHeight != bitmap.height) {
                setVideoResolution(bitmap.width, bitmap.height)
            }

            // Update frame
            synchronized(this) {
                currentBitmap?.recycle()
                currentBitmap = bitmap
            }

            // Draw frame
            drawFrame()

            // Update stats
            updateStats(jpegData.size)

            onFrameDecoded?.invoke(fps, jpegData.size)

        } catch (e: Exception) {
            onDecodingError?.invoke("Frame decode error: ${e.message}")
        }
    }

    /**
     * Render a pre-decoded bitmap
     */
    fun renderBitmap(bitmap: Bitmap) {
        synchronized(this) {
            currentBitmap?.recycle()
            currentBitmap = bitmap.copy(Bitmap.Config.RGB_565, false)
        }
        drawFrame()
    }

    /**
     * Draw the current frame to the surface
     */
    private fun drawFrame() {
        val canvas = holder.lockCanvas() ?: return
        try {
            // Clear canvas
            canvas.drawColor(Color.BLACK)

            synchronized(this) {
                currentBitmap?.let { bitmap ->
                    // Apply rotation if needed
                    if (rotation != 0f) {
                        canvas.save()
                        canvas.rotate(rotation, canvas.width / 2f, canvas.height / 2f)
                    }

                    // Draw bitmap
                    srcRect.set(0, 0, bitmap.width, bitmap.height)
                    canvas.drawBitmap(bitmap, srcRect, drawRect, paint)

                    if (rotation != 0f) {
                        canvas.restore()
                    }
                }
            }
        } finally {
            holder.unlockCanvasAndPost(canvas)
        }
    }

    /**
     * Calculate the drawing rectangle based on scale mode and aspect ratio
     */
    private fun calculateDrawRect() {
        if (videoWidth <= 0 || videoHeight <= 0 || surfaceWidth <= 0 || surfaceHeight <= 0) {
            return
        }

        val videoAspect = videoWidth.toFloat() / videoHeight
        val surfaceAspect = surfaceWidth.toFloat() / surfaceHeight

        when (scaleMode) {
            ScaleMode.FIT -> {
                // Letterbox/pillarbox to fit
                if (videoAspect > surfaceAspect) {
                    // Video is wider - fit width
                    val scaledHeight = (surfaceWidth / videoAspect).toInt()
                    val top = (surfaceHeight - scaledHeight) / 2
                    drawRect.set(0, top, surfaceWidth, top + scaledHeight)
                } else {
                    // Video is taller - fit height
                    val scaledWidth = (surfaceHeight * videoAspect).toInt()
                    val left = (surfaceWidth - scaledWidth) / 2
                    drawRect.set(left, 0, left + scaledWidth, surfaceHeight)
                }
            }

            ScaleMode.FILL -> {
                // Fill screen, may crop
                if (videoAspect > surfaceAspect) {
                    // Crop width
                    val scaledWidth = (surfaceHeight * videoAspect).toInt()
                    val left = (surfaceWidth - scaledWidth) / 2
                    drawRect.set(left, 0, left + scaledWidth, surfaceHeight)
                } else {
                    // Crop height
                    val scaledHeight = (surfaceWidth / videoAspect).toInt()
                    val top = (surfaceHeight - scaledHeight) / 2
                    drawRect.set(0, top, surfaceWidth, top + scaledHeight)
                }
            }

            ScaleMode.STRETCH -> {
                // Stretch to fill (may distort)
                drawRect.set(0, 0, surfaceWidth, surfaceHeight)
            }

            ScaleMode.ORIGINAL -> {
                // Center at original size
                val left = (surfaceWidth - videoWidth) / 2
                val top = (surfaceHeight - videoHeight) / 2
                drawRect.set(left, top, left + videoWidth, top + videoHeight)
            }
        }
    }

    /**
     * Update FPS statistics
     */
    private fun updateStats(frameSize: Int) {
        frameCount++
        val currentTime = System.currentTimeMillis()
        val elapsed = currentTime - lastFrameTime

        if (elapsed >= 1000) {
            fps = ((frameCount * 1000f) / elapsed).toInt()
            frameCount = 0
            lastFrameTime = currentTime
        }
    }

    /**
     * Clear current frame
     */
    fun clearFrame() {
        synchronized(this) {
            currentBitmap?.recycle()
            currentBitmap = null
        }

        val canvas = holder.lockCanvas() ?: return
        try {
            canvas.drawColor(Color.BLACK)
        } finally {
            holder.unlockCanvasAndPost(canvas)
        }
    }

    /**
     * Get current FPS
     */
    fun getFps(): Int = fps

    /**
     * Get video bounds rectangle
     */
    fun getVideoBounds(): Rect = Rect(drawRect)

    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        val width = MeasureSpec.getSize(widthMeasureSpec)
        val height = MeasureSpec.getSize(heightMeasureSpec)

        if (videoWidth > 0 && videoHeight > 0) {
            val videoAspect = videoWidth.toFloat() / videoHeight
            val viewAspect = width.toFloat() / height

            val finalWidth: Int
            val finalHeight: Int

            if (viewAspect > videoAspect) {
                // View is wider than video
                finalHeight = height
                finalWidth = (height * videoAspect).toInt()
            } else {
                // View is taller than video
                finalWidth = width
                finalHeight = (width / videoAspect).toInt()
            }

            setMeasuredDimension(finalWidth, finalHeight)
        } else {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec)
        }
    }
}

/**
 * MJPEG stream decoder for continuous video playback
 */
class MjpegDecoder {
    private var isRunning = false
    private var frameCallback: ((ByteArray) -> Unit)? = null
    private val frameBuffer = mutableListOf<ByteArray>()
    private val lock = Object()

    /**
     * Start decoding
     */
    fun start(onFrame: (ByteArray) -> Unit) {
        frameCallback = onFrame
        isRunning = true
    }

    /**
     * Stop decoding
     */
    fun stop() {
        isRunning = false
        frameCallback = null
        synchronized(lock) {
            frameBuffer.clear()
        }
    }

    /**
     * Add a JPEG frame to be decoded
     */
    fun addFrame(jpegData: ByteArray) {
        if (!isRunning) return

        synchronized(lock) {
            // Limit buffer size to prevent memory issues
            if (frameBuffer.size < 10) {
                frameBuffer.add(jpegData)
            }
        }

        processFrames()
    }

    /**
     * Process buffered frames
     */
    private fun processFrames() {
        val framesToProcess = synchronized(lock) {
            val frames = frameBuffer.toList()
            frameBuffer.clear()
            frames
        }

        framesToProcess.forEach { frame ->
            frameCallback?.invoke(frame)
        }
    }
}

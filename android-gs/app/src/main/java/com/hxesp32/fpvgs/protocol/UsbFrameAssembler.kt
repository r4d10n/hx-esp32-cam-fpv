package com.hxesp32.fpvgs.protocol

import android.util.Log
import java.io.ByteArrayOutputStream
import java.util.concurrent.ConcurrentHashMap

/**
 * Assembles multi-part video frames into complete frames
 */
class UsbFrameAssembler(
    private val maxPendingFrames: Int = 10,
    private val frameTimeoutMs: Long = 1000
) {
    companion object {
        private const val TAG = "UsbFrameAssembler"
    }

    private data class FrameAssembly(
        val frameIndex: UInt,
        val resolution: Resolution,
        val parts: MutableMap<UByte, ByteArray> = ConcurrentHashMap(),
        val startTime: Long = System.currentTimeMillis(),
        var expectedParts: Int = -1  // -1 until we receive the last part
    )

    private val pendingFrames = ConcurrentHashMap<UInt, FrameAssembly>()
    private var lastCompletedFrameIndex: UInt = 0u
    private var droppedFrames = 0L
    private var assembledFrames = 0L

    /**
     * Add a video frame part and try to assemble complete frame
     * @return Complete frame if all parts are received, null otherwise
     */
    fun addFramePart(frame: UsbFrame.VideoFrame): AssembledVideoFrame? {
        // Clean up old frames
        cleanupOldFrames()

        // Get or create frame assembly
        val assembly = pendingFrames.getOrPut(frame.frameIndex) {
            FrameAssembly(
                frameIndex = frame.frameIndex,
                resolution = frame.resolution
            )
        }

        // Check if frame is too old (out of order)
        if (isFrameOutOfOrder(frame.frameIndex)) {
            Log.w(TAG, "Dropping out-of-order frame ${frame.frameIndex}, last completed was $lastCompletedFrameIndex")
            pendingFrames.remove(frame.frameIndex)
            droppedFrames++
            return null
        }

        // Add part
        assembly.parts[frame.partIndex] = frame.data

        // Update expected parts if this is the last part
        if (frame.isLastPart) {
            assembly.expectedParts = frame.partIndex.toInt() + 1
        }

        // Check if frame is complete
        if (isFrameComplete(assembly)) {
            return assembleFrame(assembly)
        }

        // Limit number of pending frames
        if (pendingFrames.size > maxPendingFrames) {
            val oldestFrameIndex = pendingFrames.keys.minOrNull()
            if (oldestFrameIndex != null) {
                Log.w(TAG, "Too many pending frames, dropping oldest frame $oldestFrameIndex")
                pendingFrames.remove(oldestFrameIndex)
                droppedFrames++
            }
        }

        return null
    }

    /**
     * Check if frame is complete (all parts received)
     */
    private fun isFrameComplete(assembly: FrameAssembly): Boolean {
        if (assembly.expectedParts < 0) return false
        if (assembly.parts.size != assembly.expectedParts) return false

        // Verify all parts are present (0 to expectedParts-1)
        for (i in 0 until assembly.expectedParts) {
            if (!assembly.parts.containsKey(i.toUByte())) {
                return false
            }
        }

        return true
    }

    /**
     * Assemble complete frame from parts
     */
    private fun assembleFrame(assembly: FrameAssembly): AssembledVideoFrame? {
        return try {
            val output = ByteArrayOutputStream()

            // Combine parts in order
            for (i in 0 until assembly.expectedParts) {
                val part = assembly.parts[i.toUByte()]
                if (part == null) {
                    Log.e(TAG, "Missing part $i for frame ${assembly.frameIndex}")
                    droppedFrames++
                    return null
                }
                output.write(part)
            }

            // Remove from pending
            pendingFrames.remove(assembly.frameIndex)

            // Update stats
            lastCompletedFrameIndex = assembly.frameIndex
            assembledFrames++

            AssembledVideoFrame(
                resolution = assembly.resolution,
                frameIndex = assembly.frameIndex,
                data = output.toByteArray()
            )
        } catch (e: Exception) {
            Log.e(TAG, "Error assembling frame ${assembly.frameIndex}", e)
            pendingFrames.remove(assembly.frameIndex)
            droppedFrames++
            null
        }
    }

    /**
     * Check if frame is out of order (too old)
     */
    private fun isFrameOutOfOrder(frameIndex: UInt): Boolean {
        // Allow some backward tolerance for reordering
        val tolerance = 100u
        return if (lastCompletedFrameIndex > tolerance) {
            frameIndex < lastCompletedFrameIndex - tolerance
        } else {
            false
        }
    }

    /**
     * Clean up frames that have timed out
     */
    private fun cleanupOldFrames() {
        val now = System.currentTimeMillis()
        val timedOutFrames = pendingFrames.filter { (_, assembly) ->
            now - assembly.startTime > frameTimeoutMs
        }

        timedOutFrames.forEach { (frameIndex, _) ->
            Log.w(TAG, "Frame $frameIndex timed out")
            pendingFrames.remove(frameIndex)
            droppedFrames++
        }
    }

    /**
     * Get statistics
     */
    fun getStats(): FrameAssemblyStats {
        return FrameAssemblyStats(
            pendingFrames = pendingFrames.size,
            assembledFrames = assembledFrames,
            droppedFrames = droppedFrames,
            lastCompletedFrameIndex = lastCompletedFrameIndex
        )
    }

    /**
     * Reset assembler state
     */
    fun reset() {
        pendingFrames.clear()
        lastCompletedFrameIndex = 0u
        droppedFrames = 0
        assembledFrames = 0
    }

    /**
     * Force assembly of pending frames (used when connection is lost)
     */
    fun flushPendingFrames(): List<AssembledVideoFrame> {
        val frames = mutableListOf<AssembledVideoFrame>()

        // Try to assemble frames that might be complete enough
        pendingFrames.values.sortedBy { it.frameIndex }.forEach { assembly ->
            if (assembly.parts.isNotEmpty()) {
                // If we have at least some parts, try to assemble what we have
                val output = ByteArrayOutputStream()
                val sortedParts = assembly.parts.keys.sorted()

                sortedParts.forEach { partIndex ->
                    assembly.parts[partIndex]?.let { output.write(it) }
                }

                if (output.size() > 0) {
                    frames.add(
                        AssembledVideoFrame(
                            resolution = assembly.resolution,
                            frameIndex = assembly.frameIndex,
                            data = output.toByteArray()
                        )
                    )
                }
            }
        }

        pendingFrames.clear()
        return frames
    }
}

/**
 * Statistics for frame assembly
 */
data class FrameAssemblyStats(
    val pendingFrames: Int,
    val assembledFrames: Long,
    val droppedFrames: Long,
    val lastCompletedFrameIndex: UInt
)

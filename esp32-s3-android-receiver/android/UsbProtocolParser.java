package com.example.esp32usb;

import android.util.Log;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.List;

/**
 * USB OTG Protocol Parser for ESP32-S3 Video Streaming
 *
 * This class implements the USB streaming protocol parser for Android,
 * compatible with the ESP32-S3 USB streamer component.
 *
 * Protocol Format:
 * [SYNC(2)][TYPE(1)][FLAGS(1)][SIZE(2)][SEQ(2)][TIMESTAMP(4)][PAYLOAD(n)][CRC16(2)]
 *
 * @author ESP32-S3 USB Streamer Team
 * @version 1.0
 */
public class UsbProtocolParser {
    private static final String TAG = "UsbProtocolParser";

    // Protocol constants
    private static final short SYNC_MARKER = (short) 0xA55A;
    private static final int HEADER_SIZE = 12;
    private static final int CRC_SIZE = 2;
    private static final int MAX_PAYLOAD_SIZE = 16384;
    private static final int MAX_PACKET_SIZE = HEADER_SIZE + MAX_PAYLOAD_SIZE + CRC_SIZE;

    // Packet types
    public static final byte PACKET_TYPE_VIDEO = 0x01;
    public static final byte PACKET_TYPE_METADATA = 0x02;
    public static final byte PACKET_TYPE_CONTROL = 0x03;
    public static final byte PACKET_TYPE_ACK = 0x04;
    public static final byte PACKET_TYPE_HEARTBEAT = 0x05;
    public static final byte PACKET_TYPE_DEBUG = 0x06;

    // Packet flags
    public static final byte FLAG_FRAGMENT = 0x01;
    public static final byte FLAG_LAST_FRAGMENT = 0x02;
    public static final byte FLAG_PRIORITY_HIGH = 0x04;
    public static final byte FLAG_REQUIRE_ACK = 0x08;

    // NAL unit types
    public static final byte NAL_UNIT_TYPE_NON_IDR = 0x01;
    public static final byte NAL_UNIT_TYPE_IDR = 0x05;
    public static final byte NAL_UNIT_TYPE_SPS = 0x07;
    public static final byte NAL_UNIT_TYPE_PPS = 0x08;

    // Parser state
    private ByteBuffer receiveBuffer;
    private PacketCallback callback;
    private ParserStatistics stats;

    // Fragment reassembly
    private ByteBuffer fragmentBuffer;
    private int fragmentSequence = -1;

    /**
     * Packet data structure
     */
    public static class Packet {
        public byte type;
        public byte flags;
        public int payloadSize;
        public int sequence;
        public long timestamp;
        public byte[] payload;

        public boolean isFragmented() {
            return (flags & FLAG_FRAGMENT) != 0;
        }

        public boolean isLastFragment() {
            return (flags & FLAG_LAST_FRAGMENT) != 0;
        }

        public boolean isHighPriority() {
            return (flags & FLAG_PRIORITY_HIGH) != 0;
        }

        public boolean requiresAck() {
            return (flags & FLAG_REQUIRE_ACK) != 0;
        }
    }

    /**
     * Video packet (NAL unit)
     */
    public static class VideoPacket {
        public byte nalType;
        public int nalSize;
        public byte[] nalData;

        public boolean isKeyFrame() {
            return nalType == NAL_UNIT_TYPE_IDR || nalType == NAL_UNIT_TYPE_SPS || nalType == NAL_UNIT_TYPE_PPS;
        }
    }

    /**
     * Metadata packet
     */
    public static class MetadataPacket {
        public byte rssi;           // WiFi RSSI in dBm
        public int fps;             // FPS × 10 (e.g., 300 = 30.0 fps)
        public long bitrate;        // Bitrate in kbps
        public long frameCount;     // Total frames transmitted
        public long bytesTransmitted; // Total bytes transmitted
        public byte bufferUsage;    // Buffer usage percentage (0-100)
        public byte temperature;    // Temperature in Celsius
    }

    /**
     * Parser statistics
     */
    public static class ParserStatistics {
        public long packetsReceived = 0;
        public long bytesReceived = 0;
        public long crcErrors = 0;
        public long syncErrors = 0;
        public long fragmentsReceived = 0;
        public long packetsDropped = 0;
        public long lastSequence = 0;
        public long sequenceGaps = 0;

        public void reset() {
            packetsReceived = 0;
            bytesReceived = 0;
            crcErrors = 0;
            syncErrors = 0;
            fragmentsReceived = 0;
            packetsDropped = 0;
            lastSequence = 0;
            sequenceGaps = 0;
        }
    }

    /**
     * Callback interface for parsed packets
     */
    public interface PacketCallback {
        void onVideoPacket(VideoPacket video);
        void onMetadataPacket(MetadataPacket metadata);
        void onHeartbeat(long uptime);
        void onDebugMessage(String message);
        void onParseError(String error);
    }

    /**
     * Constructor
     *
     * @param callback Packet callback handler
     */
    public UsbProtocolParser(PacketCallback callback) {
        this.callback = callback;
        this.receiveBuffer = ByteBuffer.allocate(MAX_PACKET_SIZE * 2);
        this.receiveBuffer.order(ByteOrder.LITTLE_ENDIAN);
        this.fragmentBuffer = ByteBuffer.allocate(MAX_PAYLOAD_SIZE * 4);
        this.stats = new ParserStatistics();
    }

    /**
     * Parse incoming USB data
     *
     * @param data Raw data received from USB
     * @param length Length of data
     */
    public void parse(byte[] data, int length) {
        if (data == null || length <= 0) {
            return;
        }

        // Append to receive buffer
        if (receiveBuffer.remaining() < length) {
            // Buffer full, compact it
            receiveBuffer.compact();
        }

        receiveBuffer.put(data, 0, length);
        receiveBuffer.flip();

        // Try to parse packets
        while (receiveBuffer.remaining() >= HEADER_SIZE + CRC_SIZE) {
            // Mark position to revert if packet is incomplete
            receiveBuffer.mark();

            // Find sync marker
            short sync = receiveBuffer.getShort();
            if (sync != SYNC_MARKER) {
                // Invalid sync, search for next sync marker
                receiveBuffer.reset();
                if (!seekToSync()) {
                    break;
                }
                stats.syncErrors++;
                continue;
            }

            // Read header
            byte type = receiveBuffer.get();
            byte flags = receiveBuffer.get();
            int payloadSize = receiveBuffer.getShort() & 0xFFFF;
            int sequence = receiveBuffer.getShort() & 0xFFFF;
            long timestamp = receiveBuffer.getInt() & 0xFFFFFFFFL;

            // Validate payload size
            if (payloadSize > MAX_PAYLOAD_SIZE) {
                Log.w(TAG, "Invalid payload size: " + payloadSize);
                receiveBuffer.reset();
                seekToSync();
                stats.packetsDropped++;
                continue;
            }

            // Check if we have complete packet
            int totalPacketSize = HEADER_SIZE + payloadSize + CRC_SIZE;
            if (receiveBuffer.remaining() < payloadSize + CRC_SIZE) {
                // Incomplete packet, wait for more data
                receiveBuffer.reset();
                break;
            }

            // Read payload
            byte[] payload = new byte[payloadSize];
            receiveBuffer.get(payload);

            // Read CRC
            int receivedCrc = receiveBuffer.getShort() & 0xFFFF;

            // Verify CRC
            byte[] packetData = new byte[HEADER_SIZE + payloadSize];
            receiveBuffer.reset();
            receiveBuffer.get(packetData);
            int calculatedCrc = calculateCrc16(packetData, packetData.length);

            if (receivedCrc != calculatedCrc) {
                Log.w(TAG, String.format("CRC mismatch: received=0x%04X, calculated=0x%04X",
                        receivedCrc, calculatedCrc));
                stats.crcErrors++;
                stats.packetsDropped++;
                continue;
            }

            // Valid packet received
            stats.packetsReceived++;
            stats.bytesReceived += totalPacketSize;

            // Check sequence
            if (stats.lastSequence != 0 && sequence != (stats.lastSequence + 1) % 65536) {
                stats.sequenceGaps++;
                Log.d(TAG, String.format("Sequence gap: expected %d, got %d",
                        (stats.lastSequence + 1) % 65536, sequence));
            }
            stats.lastSequence = sequence;

            // Create packet object
            Packet packet = new Packet();
            packet.type = type;
            packet.flags = flags;
            packet.payloadSize = payloadSize;
            packet.sequence = sequence;
            packet.timestamp = timestamp;
            packet.payload = payload;

            // Process packet
            processPacket(packet);
        }

        // Compact buffer for next read
        receiveBuffer.compact();
    }

    /**
     * Process parsed packet
     */
    private void processPacket(Packet packet) {
        try {
            switch (packet.type) {
                case PACKET_TYPE_VIDEO:
                    processVideoPacket(packet);
                    break;

                case PACKET_TYPE_METADATA:
                    processMetadataPacket(packet);
                    break;

                case PACKET_TYPE_HEARTBEAT:
                    processHeartbeatPacket(packet);
                    break;

                case PACKET_TYPE_DEBUG:
                    processDebugPacket(packet);
                    break;

                case PACKET_TYPE_ACK:
                    Log.d(TAG, "ACK received for sequence: " + packet.sequence);
                    break;

                default:
                    Log.w(TAG, "Unknown packet type: 0x" + Integer.toHexString(packet.type));
                    break;
            }
        } catch (Exception e) {
            Log.e(TAG, "Error processing packet: " + e.getMessage());
            if (callback != null) {
                callback.onParseError("Packet processing error: " + e.getMessage());
            }
        }
    }

    /**
     * Process video packet (NAL unit)
     */
    private void processVideoPacket(Packet packet) {
        if (packet.payload.length < 4) {
            Log.w(TAG, "Invalid video packet: too small");
            return;
        }

        // Handle fragmentation
        if (packet.isFragmented()) {
            stats.fragmentsReceived++;

            if (fragmentSequence == -1 || packet.sequence == fragmentSequence + 1) {
                // First fragment or continuation
                fragmentBuffer.put(packet.payload);
                fragmentSequence = packet.sequence;

                if (packet.isLastFragment()) {
                    // Reassemble complete NAL unit
                    fragmentBuffer.flip();
                    byte[] completePayload = new byte[fragmentBuffer.remaining()];
                    fragmentBuffer.get(completePayload);
                    fragmentBuffer.clear();
                    fragmentSequence = -1;

                    // Parse NAL unit
                    parseNalUnit(completePayload);
                }
            } else {
                // Fragment sequence mismatch, discard
                Log.w(TAG, "Fragment sequence mismatch");
                fragmentBuffer.clear();
                fragmentSequence = -1;
            }
        } else {
            // Complete NAL unit
            parseNalUnit(packet.payload);
        }
    }

    /**
     * Parse NAL unit from payload
     */
    private void parseNalUnit(byte[] payload) {
        if (payload.length < 4) {
            return;
        }

        ByteBuffer buffer = ByteBuffer.wrap(payload);
        buffer.order(ByteOrder.LITTLE_ENDIAN);

        byte nalType = buffer.get();
        int nalSize = buffer.get() & 0xFF;
        nalSize |= (buffer.get() & 0xFF) << 8;
        nalSize |= (buffer.get() & 0xFF) << 16;

        if (nalSize > payload.length - 4) {
            Log.w(TAG, "Invalid NAL size: " + nalSize);
            return;
        }

        byte[] nalData = new byte[nalSize];
        buffer.get(nalData);

        VideoPacket video = new VideoPacket();
        video.nalType = nalType;
        video.nalSize = nalSize;
        video.nalData = nalData;

        if (callback != null) {
            callback.onVideoPacket(video);
        }
    }

    /**
     * Process metadata packet
     */
    private void processMetadataPacket(Packet packet) {
        if (packet.payload.length < 24) {
            Log.w(TAG, "Invalid metadata packet: too small");
            return;
        }

        ByteBuffer buffer = ByteBuffer.wrap(packet.payload);
        buffer.order(ByteOrder.LITTLE_ENDIAN);

        MetadataPacket metadata = new MetadataPacket();
        metadata.rssi = buffer.get();
        metadata.fps = buffer.getShort() & 0xFFFF;
        metadata.bitrate = buffer.getInt() & 0xFFFFFFFFL;
        metadata.frameCount = buffer.getInt() & 0xFFFFFFFFL;
        metadata.bytesTransmitted = buffer.getLong();
        metadata.bufferUsage = buffer.get();
        metadata.temperature = buffer.get();

        if (callback != null) {
            callback.onMetadataPacket(metadata);
        }
    }

    /**
     * Process heartbeat packet
     */
    private void processHeartbeatPacket(Packet packet) {
        if (packet.payload.length < 4) {
            return;
        }

        ByteBuffer buffer = ByteBuffer.wrap(packet.payload);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        long uptime = buffer.getInt() & 0xFFFFFFFFL;

        if (callback != null) {
            callback.onHeartbeat(uptime);
        }
    }

    /**
     * Process debug packet
     */
    private void processDebugPacket(Packet packet) {
        if (packet.payload.length < 2) {
            return;
        }

        byte level = packet.payload[0];
        String message = new String(packet.payload, 1, packet.payload.length - 1);

        if (callback != null) {
            callback.onDebugMessage(message);
        }

        Log.d(TAG, "Debug [" + level + "]: " + message);
    }

    /**
     * Seek to next sync marker
     */
    private boolean seekToSync() {
        while (receiveBuffer.remaining() >= 2) {
            receiveBuffer.mark();
            short sync = receiveBuffer.getShort();
            if (sync == SYNC_MARKER) {
                receiveBuffer.reset();
                return true;
            }
            receiveBuffer.reset();
            receiveBuffer.get(); // Advance by 1 byte
        }
        return false;
    }

    /**
     * Calculate CRC16-CCITT
     */
    private static int calculateCrc16(byte[] data, int length) {
        int crc = 0xFFFF;
        for (int i = 0; i < length; i++) {
            crc ^= (data[i] & 0xFF) << 8;
            for (int j = 0; j < 8; j++) {
                if ((crc & 0x8000) != 0) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc <<= 1;
                }
            }
        }
        return crc & 0xFFFF;
    }

    /**
     * Get parser statistics
     */
    public ParserStatistics getStatistics() {
        return stats;
    }

    /**
     * Reset parser state
     */
    public void reset() {
        receiveBuffer.clear();
        fragmentBuffer.clear();
        fragmentSequence = -1;
        stats.reset();
    }
}

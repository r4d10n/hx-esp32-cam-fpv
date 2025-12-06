#pragma once

#include <cstdint>
#include <cstddef>

/**
 * Frame Assembler for Video Reconstruction
 *
 * Reassembles video frames from decoded FEC packets.
 * Each video frame may span multiple packets, identified by frame_index
 * and part_index fields in the Air2Ground_Video_Packet header.
 */

// Maximum frame size (typical JPEG max ~100KB at high quality)
static constexpr size_t MAX_FRAME_SIZE = 150 * 1024;

// Maximum number of parts per frame
static constexpr size_t MAX_PARTS_PER_FRAME = 128;

// Frame types from Air2Ground_Header
enum class PacketType : uint8_t {
    Video = 0,
    Telemetry = 1,
    OSD = 2,
    Config = 3
};

/**
 * Video packet header (matches Air2Ground_Video_Packet)
 */
#pragma pack(push, 1)
struct VideoPacketHeader {
    // Air2Ground_Header
    uint8_t type;           // PacketType::Video
    uint32_t size;
    uint8_t pong;           // Latency measurement
    uint8_t version;        // PACKET_VERSION
    uint8_t crc;
    uint16_t airDeviceId;
    uint16_t gsDeviceId;
    // Video-specific fields
    uint8_t resolution;
    uint8_t part_index : 7;
    uint8_t last_part : 1;
    uint32_t frame_index;
};
#pragma pack(pop)

static_assert(sizeof(VideoPacketHeader) == 18, "VideoPacketHeader size mismatch");

/**
 * Callback type for decoded frames
 *
 * @param frame_data Pointer to complete JPEG frame data
 * @param frame_size Size of frame data in bytes
 * @param frame_index Frame sequence number
 * @param resolution Resolution enum value
 */
typedef void (*FrameDecodedCallback)(const uint8_t* frame_data, size_t frame_size,
                                     uint32_t frame_index, uint8_t resolution);

/**
 * Callback type for telemetry/OSD data
 *
 * @param data Pointer to packet data
 * @param size Size of data
 * @param type Packet type
 */
typedef void (*DataCallback)(const uint8_t* data, size_t size, PacketType type);

/**
 * Frame state for tracking partial frames
 */
struct FrameState {
    uint32_t frame_index = 0;
    uint8_t resolution = 0;
    uint8_t parts_received = 0;
    uint8_t expected_parts = 0;     // Set when last_part packet received
    uint32_t parts_mask_low = 0;    // Bitmask for parts 0-31
    uint32_t parts_mask_high = 0;   // Bitmask for parts 32-63
    uint32_t parts_mask_ext[2] = {0}; // Bitmask for parts 64-127
    size_t current_size = 0;
    bool complete = false;

    void reset() {
        frame_index = 0;
        resolution = 0;
        parts_received = 0;
        expected_parts = 0;
        parts_mask_low = 0;
        parts_mask_high = 0;
        parts_mask_ext[0] = 0;
        parts_mask_ext[1] = 0;
        current_size = 0;
        complete = false;
    }

    bool has_part(uint8_t part_index) const {
        if (part_index < 32)
            return (parts_mask_low & (1u << part_index)) != 0;
        else if (part_index < 64)
            return (parts_mask_high & (1u << (part_index - 32))) != 0;
        else if (part_index < 96)
            return (parts_mask_ext[0] & (1u << (part_index - 64))) != 0;
        else
            return (parts_mask_ext[1] & (1u << (part_index - 96))) != 0;
    }

    void set_part(uint8_t part_index) {
        if (part_index < 32)
            parts_mask_low |= (1u << part_index);
        else if (part_index < 64)
            parts_mask_high |= (1u << (part_index - 32));
        else if (part_index < 96)
            parts_mask_ext[0] |= (1u << (part_index - 64));
        else
            parts_mask_ext[1] |= (1u << (part_index - 96));
    }
};

/**
 * Frame Assembler class
 *
 * Reassembles video frames from packets and dispatches to callbacks.
 */
class FrameAssembler {
public:
    FrameAssembler();
    ~FrameAssembler();

    /**
     * Initialize the frame assembler
     *
     * @param use_psram Whether to allocate frame buffer in PSRAM
     * @return true if initialization succeeded
     */
    bool init(bool use_psram = true);

    /**
     * Deinitialize and free resources
     */
    void deinit();

    /**
     * Process a decoded packet
     *
     * Determines packet type and routes to appropriate handler.
     *
     * @param data Packet data (without FEC header)
     * @param size Packet size
     */
    void process_packet(const uint8_t* data, size_t size);

    /**
     * Flush any incomplete frame
     *
     * Call when switching contexts or timing out.
     *
     * @return true if an incomplete frame was flushed
     */
    bool flush();

    /**
     * Set callback for decoded frames
     */
    void set_frame_callback(FrameDecodedCallback callback) {
        m_frame_callback = callback;
    }

    /**
     * Set callback for telemetry/OSD data
     */
    void set_data_callback(DataCallback callback) {
        m_data_callback = callback;
    }

    // Statistics
    uint32_t get_frames_decoded() const { return m_frames_decoded; }
    uint32_t get_frames_incomplete() const { return m_frames_incomplete; }
    uint32_t get_current_frame_index() const { return m_current_frame.frame_index; }

private:
    void process_video_packet(const uint8_t* data, size_t size);
    void dispatch_frame();

    bool m_initialized = false;
    bool m_use_psram = true;

    // Frame buffer
    uint8_t* m_frame_buffer = nullptr;
    size_t m_frame_buffer_size = 0;

    // Part offset tracking for reassembly
    size_t m_part_offsets[MAX_PARTS_PER_FRAME] = {0};

    // Current frame state
    FrameState m_current_frame;

    // Callbacks
    FrameDecodedCallback m_frame_callback = nullptr;
    DataCallback m_data_callback = nullptr;

    // Statistics
    uint32_t m_frames_decoded = 0;
    uint32_t m_frames_incomplete = 0;
};

/**
 * WebCodecs Video Player
 *
 * Receives H.264/H.265 video frames via WebSocket and decodes using WebCodecs API.
 * Renders decoded frames to a canvas element.
 */

// Message types from server
const MSG_TYPE = {
    FRAME: 0x01,
    CODEC_CONFIG: 0x02,
    STATS: 0x03,
    STREAM_START: 0x04,
    STREAM_STOP: 0x05
};

// Codec types
const CODEC = {
    H264: 0x01,
    H265: 0x02,
    MJPEG: 0x03
};

// Frame header structure (must match server)
const FRAME_HDR_SIZE = 14;
const CONFIG_HDR_SIZE = 16;

class WebCodecsPlayer {
    constructor(canvasId, options = {}) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.options = options;

        this.ws = null;
        this.decoder = null;
        this.connected = false;

        // Codec configuration
        this.codecType = null;
        this.width = 0;
        this.height = 0;
        this.fps = 0;
        this.sps = null;
        this.pps = null;
        this.vps = null;
        this.configured = false;

        // Statistics
        this.stats = {
            frames: 0,
            fps: 0,
            bitrate: 0,
            latency: 0,
            decodedFrames: 0,
            droppedFrames: 0,
            resolution: '',
            codec: ''
        };

        this.lastFrameTime = 0;
        this.frameCount = 0;
        this.bytesReceived = 0;
        this.statsInterval = null;

        // Pending frames for decode
        this.pendingFrames = [];
        this.maxPendingFrames = 30;
    }

    log(level, message) {
        console[level](`[WebCodecsPlayer] ${message}`);
        if (this.options.onLog) {
            this.options.onLog(level, message);
        }
    }

    isConnected() {
        return this.connected;
    }

    connect() {
        if (this.connected) return;

        const url = this.options.url;
        this.log('info', `Connecting to ${url}`);

        if (this.options.onStatusChange) {
            this.options.onStatusChange(false, 'connecting');
        }

        this.ws = new WebSocket(url);
        this.ws.binaryType = 'arraybuffer';

        this.ws.onopen = () => {
            this.connected = true;
            this.log('info', 'WebSocket connected');

            if (this.options.onStatusChange) {
                this.options.onStatusChange(true);
            }

            // Request stream start
            this.ws.send('start');

            // Start stats update
            this.startStatsUpdate();
        };

        this.ws.onclose = (event) => {
            this.connected = false;
            this.log('warn', `WebSocket closed: ${event.reason || 'unknown'}`);

            if (this.options.onStatusChange) {
                this.options.onStatusChange(false, 'Disconnected');
            }

            this.cleanup();
        };

        this.ws.onerror = (error) => {
            this.log('error', `WebSocket error: ${error.message || 'unknown'}`);
        };

        this.ws.onmessage = (event) => {
            this.handleMessage(event.data);
        };
    }

    disconnect() {
        if (this.ws) {
            this.ws.send('stop');
            this.ws.close();
        }
        this.cleanup();
    }

    cleanup() {
        if (this.decoder) {
            this.decoder.close();
            this.decoder = null;
        }
        if (this.statsInterval) {
            clearInterval(this.statsInterval);
            this.statsInterval = null;
        }
        this.configured = false;
        this.connected = false;
        this.pendingFrames = [];
    }

    handleMessage(data) {
        if (!(data instanceof ArrayBuffer)) {
            this.log('warn', 'Received non-binary message');
            return;
        }

        const view = new DataView(data);
        const msgType = view.getUint8(0);

        this.bytesReceived += data.byteLength;

        switch (msgType) {
            case MSG_TYPE.FRAME:
                this.handleFrame(data);
                break;
            case MSG_TYPE.CODEC_CONFIG:
                this.handleConfig(data);
                break;
            case MSG_TYPE.STATS:
                this.handleServerStats(data);
                break;
            default:
                this.log('warn', `Unknown message type: ${msgType}`);
        }
    }

    handleConfig(data) {
        const view = new DataView(data);

        // Parse codec config header
        this.codecType = view.getUint8(1);
        this.width = view.getUint16(2, true);
        this.height = view.getUint16(4, true);
        this.fps = view.getUint8(6);

        const spsLen = view.getUint16(8, true);
        const ppsLen = view.getUint16(10, true);
        const vpsLen = view.getUint16(12, true);

        // Extract parameter sets
        let offset = CONFIG_HDR_SIZE;

        if (vpsLen > 0) {
            this.vps = new Uint8Array(data, offset, vpsLen);
            offset += vpsLen;
        }
        if (spsLen > 0) {
            this.sps = new Uint8Array(data, offset, spsLen);
            offset += spsLen;
        }
        if (ppsLen > 0) {
            this.pps = new Uint8Array(data, offset, ppsLen);
        }

        this.log('info', `Config: ${this.width}x${this.height} @ ${this.fps}fps, codec=${this.codecType}`);

        // Update stats
        this.stats.resolution = `${this.width}x${this.height}`;
        this.stats.codec = this.getCodecName();

        // Resize canvas
        this.canvas.width = this.width;
        this.canvas.height = this.height;

        // Initialize decoder
        this.initDecoder();
    }

    getCodecName() {
        switch (this.codecType) {
            case CODEC.H264: return 'H.264';
            case CODEC.H265: return 'H.265';
            case CODEC.MJPEG: return 'MJPEG';
            default: return 'Unknown';
        }
    }

    getCodecString() {
        // WebCodecs codec strings
        if (this.codecType === CODEC.H264) {
            // Parse profile from SPS if available
            if (this.sps && this.sps.length >= 4) {
                const profile = this.sps[1];
                const constraint = this.sps[2];
                const level = this.sps[3];
                return `avc1.${profile.toString(16).padStart(2, '0')}${constraint.toString(16).padStart(2, '0')}${level.toString(16).padStart(2, '0')}`;
            }
            return 'avc1.42E01E'; // Default: Baseline profile, level 3.0
        } else if (this.codecType === CODEC.H265) {
            // HEVC codec string
            return 'hvc1.1.6.L93.B0';
        }
        return null;
    }

    async initDecoder() {
        if (this.decoder) {
            this.decoder.close();
        }

        const codecString = this.getCodecString();
        if (!codecString) {
            this.log('error', 'Unsupported codec for WebCodecs');
            return;
        }

        // Check codec support
        const support = await VideoDecoder.isConfigSupported({
            codec: codecString,
            codedWidth: this.width,
            codedHeight: this.height
        });

        if (!support.supported) {
            this.log('error', `Codec ${codecString} not supported`);
            return;
        }

        this.decoder = new VideoDecoder({
            output: (frame) => this.onDecodedFrame(frame),
            error: (e) => this.onDecoderError(e)
        });

        // Build description (codec-specific extradata)
        let description;
        if (this.codecType === CODEC.H264 && this.sps && this.pps) {
            description = this.buildAVCDecoderConfig();
        } else if (this.codecType === CODEC.H265 && this.sps && this.pps && this.vps) {
            description = this.buildHEVCDecoderConfig();
        }

        this.decoder.configure({
            codec: codecString,
            codedWidth: this.width,
            codedHeight: this.height,
            description: description,
            optimizeForLatency: true
        });

        this.configured = true;
        this.log('info', `Decoder configured: ${codecString}`);
    }

    // Build AVC Decoder Configuration Record (avcC box)
    buildAVCDecoderConfig() {
        if (!this.sps || !this.pps) return null;

        const configSize = 11 + this.sps.length + this.pps.length;
        const config = new Uint8Array(configSize);
        let offset = 0;

        config[offset++] = 0x01; // configurationVersion
        config[offset++] = this.sps[1]; // AVCProfileIndication
        config[offset++] = this.sps[2]; // profile_compatibility
        config[offset++] = this.sps[3]; // AVCLevelIndication
        config[offset++] = 0xFF; // lengthSizeMinusOne = 3 (4 bytes NAL length)
        config[offset++] = 0xE1; // numOfSequenceParameterSets = 1

        // SPS length (big endian)
        config[offset++] = (this.sps.length >> 8) & 0xFF;
        config[offset++] = this.sps.length & 0xFF;

        // SPS data
        config.set(this.sps, offset);
        offset += this.sps.length;

        config[offset++] = 0x01; // numOfPictureParameterSets = 1

        // PPS length (big endian)
        config[offset++] = (this.pps.length >> 8) & 0xFF;
        config[offset++] = this.pps.length & 0xFF;

        // PPS data
        config.set(this.pps, offset);

        return config;
    }

    // Build HEVC Decoder Configuration Record (hvcC box)
    buildHEVCDecoderConfig() {
        if (!this.vps || !this.sps || !this.pps) return null;

        // Simplified HEVC config
        const arrays = [
            { type: 32, data: this.vps }, // VPS
            { type: 33, data: this.sps }, // SPS
            { type: 34, data: this.pps }  // PPS
        ];

        let totalSize = 23; // Base header size
        for (const arr of arrays) {
            totalSize += 3 + 2 + arr.data.length;
        }

        const config = new Uint8Array(totalSize);
        let offset = 0;

        // HEVC config header (simplified)
        config[offset++] = 0x01; // configurationVersion
        // ... (remaining header fields would need proper parsing from SPS/VPS)
        // For simplicity, we'll use a basic config

        // This is a simplified version - proper implementation would
        // parse VPS/SPS for profile_tier_level info

        return null; // Fall back to Annex B format
    }

    handleFrame(data) {
        if (!this.configured) {
            this.log('warn', 'Received frame before config');
            return;
        }

        const view = new DataView(data);

        // Parse frame header
        const codec = view.getUint8(1);
        const flags = view.getUint8(2);
        const timestamp = view.getUint32(4, true);
        const frameIndex = view.getUint32(8, true);
        const dataLen = view.getUint16(12, true);

        const isKeyframe = (flags & 0x01) !== 0;

        // Extract video data
        const videoData = new Uint8Array(data, FRAME_HDR_SIZE, dataLen);

        this.stats.frames++;
        this.frameCount++;

        // Create encoded chunk
        try {
            const chunk = new EncodedVideoChunk({
                type: isKeyframe ? 'key' : 'delta',
                timestamp: timestamp,
                data: videoData
            });

            // Queue for decoding
            if (this.decoder.decodeQueueSize < this.maxPendingFrames) {
                this.decoder.decode(chunk);
            } else {
                this.stats.droppedFrames++;
                this.log('warn', 'Decoder queue full, dropping frame');
            }
        } catch (e) {
            this.log('error', `Failed to create chunk: ${e.message}`);
        }
    }

    onDecodedFrame(frame) {
        this.stats.decodedFrames++;

        // Draw frame to canvas
        this.ctx.drawImage(frame, 0, 0, this.canvas.width, this.canvas.height);
        frame.close();

        // Calculate latency (frame timestamp is in 90kHz units)
        const now = performance.now();
        this.stats.latency = now - this.lastFrameTime;
        this.lastFrameTime = now;
    }

    onDecoderError(error) {
        this.log('error', `Decoder error: ${error.message}`);

        // Try to recover by waiting for next keyframe
        this.configured = false;
    }

    handleServerStats(data) {
        // Parse server-side statistics
        const view = new DataView(data);

        const stats = {
            framesReceived: view.getUint32(4, true),
            framesDecoded: view.getUint32(8, true),
            fecRecovered: view.getUint32(12, true),
            packetsLost: view.getUint32(16, true),
            bitrateKbps: view.getUint16(20, true),
            rssi: view.getInt8(22),
            snr: view.getUint8(23)
        };

        // Update bitrate from server
        this.stats.bitrate = stats.bitrateKbps;
    }

    startStatsUpdate() {
        this.lastStatsTime = performance.now();
        this.lastFrameCount = 0;
        this.lastBytesReceived = 0;

        this.statsInterval = setInterval(() => {
            const now = performance.now();
            const elapsed = (now - this.lastStatsTime) / 1000;

            if (elapsed > 0) {
                // Calculate FPS
                const framesDelta = this.frameCount - this.lastFrameCount;
                this.stats.fps = framesDelta / elapsed;

                // Calculate bitrate (if not from server)
                const bytesDelta = this.bytesReceived - this.lastBytesReceived;
                const calculatedBitrate = (bytesDelta * 8) / 1000 / elapsed;
                if (this.stats.bitrate === 0) {
                    this.stats.bitrate = calculatedBitrate;
                }

                this.lastStatsTime = now;
                this.lastFrameCount = this.frameCount;
                this.lastBytesReceived = this.bytesReceived;
            }

            if (this.options.onStats) {
                this.options.onStats(this.stats);
            }
        }, 1000);
    }
}

// Export for module usage
if (typeof module !== 'undefined' && module.exports) {
    module.exports = WebCodecsPlayer;
}

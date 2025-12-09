/**
 * FPV Video Player
 *
 * Multi-codec video player supporting:
 * - MJPEG (native, no WebCodecs required)
 * - H.264 (WebCodecs VideoDecoder)
 * - H.265/HEVC (WebCodecs VideoDecoder)
 *
 * Receives video frames via WebSocket and renders to canvas.
 */

// Message types (must match server)
const MSG_TYPE = {
    FRAME: 0x01,
    CODEC_CONFIG: 0x02,
    STATS: 0x03,
    STREAM_START: 0x04,
    STREAM_STOP: 0x05
};

// Codec types
const CODEC = {
    MJPEG: 0x01,
    H264: 0x02,
    H265: 0x03
};

// Frame header size
const FRAME_HDR_SIZE = 14;
const CONFIG_HDR_SIZE = 16;

class FPVPlayer {
    constructor(canvasId, options = {}) {
        this.canvas = document.getElementById(canvasId);
        this.ctx = this.canvas.getContext('2d');
        this.options = options;

        this.ws = null;
        this.decoder = null;
        this.connected = false;

        // Codec state
        this.codec = CODEC.MJPEG;
        this.width = 800;
        this.height = 600;
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
            dropped: 0
        };

        // Timing
        this.lastFrameTime = 0;
        this.frameCount = 0;
        this.bytesReceived = 0;
        this.lastStatsTime = performance.now();
        this.statsInterval = null;

        // MJPEG blob URL for reuse
        this.mjpegUrl = null;
        this.mjpegImg = new Image();
        this.mjpegImg.onload = () => {
            this.ctx.drawImage(this.mjpegImg, 0, 0, this.canvas.width, this.canvas.height);
            if (this.mjpegUrl) {
                URL.revokeObjectURL(this.mjpegUrl);
                this.mjpegUrl = null;
            }
        };

        // Pending frames queue for WebCodecs
        this.pendingFrames = [];
        this.maxPendingFrames = 5;
    }

    isConnected() {
        return this.connected;
    }

    connect() {
        if (this.connected) return;

        const url = this.options.wsUrl || `ws://${window.location.host}/video`;
        console.log(`[FPVPlayer] Connecting to ${url}`);

        if (this.options.onStatus) {
            this.options.onStatus(false, 'connecting');
        }

        this.ws = new WebSocket(url);
        this.ws.binaryType = 'arraybuffer';

        this.ws.onopen = () => {
            this.connected = true;
            console.log('[FPVPlayer] Connected');

            if (this.options.onStatus) {
                this.options.onStatus(true);
            }

            // Request stream
            this.ws.send('start');
            this.startStatsUpdate();
        };

        this.ws.onclose = (event) => {
            this.connected = false;
            console.log('[FPVPlayer] Disconnected');

            if (this.options.onStatus) {
                this.options.onStatus(false, 'Disconnected');
            }

            this.cleanup();
        };

        this.ws.onerror = (error) => {
            console.error('[FPVPlayer] WebSocket error:', error);
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
            try {
                this.decoder.close();
            } catch (e) {}
            this.decoder = null;
        }
        if (this.statsInterval) {
            clearInterval(this.statsInterval);
            this.statsInterval = null;
        }
        if (this.mjpegUrl) {
            URL.revokeObjectURL(this.mjpegUrl);
            this.mjpegUrl = null;
        }
        this.configured = false;
        this.connected = false;
        this.pendingFrames = [];
    }

    handleMessage(data) {
        if (!(data instanceof ArrayBuffer)) return;

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
        }
    }

    handleConfig(data) {
        const view = new DataView(data);

        const codec = view.getUint8(1);
        const width = view.getUint16(2, true);
        const height = view.getUint16(4, true);
        const fps = view.getUint8(6);
        const spsLen = view.getUint16(8, true);
        const ppsLen = view.getUint16(10, true);
        const vpsLen = view.getUint16(12, true);

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

        this.codec = codec;
        this.width = width;
        this.height = height;

        console.log(`[FPVPlayer] Config: ${width}x${height}, codec=${codec}, fps=${fps}`);

        // Resize canvas
        this.canvas.width = width;
        this.canvas.height = height;

        // Initialize decoder for H.264/H.265
        if (codec === CODEC.H264 || codec === CODEC.H265) {
            this.initVideoDecoder(codec);
        } else {
            this.configured = true;
        }

        if (this.options.onCodecChange) {
            this.options.onCodecChange(codec, width, height);
        }
    }

    async initVideoDecoder(codec) {
        if (typeof VideoDecoder === 'undefined') {
            console.warn('[FPVPlayer] WebCodecs not supported, H.264/H.265 will not work');
            return;
        }

        if (this.decoder) {
            try { this.decoder.close(); } catch (e) {}
        }

        const codecString = this.getCodecString(codec);
        if (!codecString) {
            console.error('[FPVPlayer] Unsupported codec');
            return;
        }

        // Check support
        try {
            const support = await VideoDecoder.isConfigSupported({
                codec: codecString,
                codedWidth: this.width,
                codedHeight: this.height
            });

            if (!support.supported) {
                console.error(`[FPVPlayer] Codec ${codecString} not supported`);
                return;
            }
        } catch (e) {
            console.error('[FPVPlayer] Error checking codec support:', e);
            return;
        }

        this.decoder = new VideoDecoder({
            output: (frame) => this.onDecodedFrame(frame),
            error: (e) => this.onDecoderError(e)
        });

        // Build decoder config
        let description = null;
        if (codec === CODEC.H264 && this.sps && this.pps) {
            description = this.buildAVCConfig();
        }

        try {
            this.decoder.configure({
                codec: codecString,
                codedWidth: this.width,
                codedHeight: this.height,
                description: description,
                optimizeForLatency: true
            });

            this.configured = true;
            console.log(`[FPVPlayer] Decoder configured: ${codecString}`);
        } catch (e) {
            console.error('[FPVPlayer] Failed to configure decoder:', e);
        }
    }

    getCodecString(codec) {
        if (codec === CODEC.H264) {
            if (this.sps && this.sps.length >= 4) {
                const profile = this.sps[1];
                const constraint = this.sps[2];
                const level = this.sps[3];
                return `avc1.${profile.toString(16).padStart(2, '0')}${constraint.toString(16).padStart(2, '0')}${level.toString(16).padStart(2, '0')}`;
            }
            return 'avc1.42E01E'; // Baseline L3.0
        } else if (codec === CODEC.H265) {
            return 'hvc1.1.6.L93.B0';
        }
        return null;
    }

    buildAVCConfig() {
        if (!this.sps || !this.pps) return null;

        const size = 11 + this.sps.length + this.pps.length;
        const config = new Uint8Array(size);
        let i = 0;

        config[i++] = 0x01; // version
        config[i++] = this.sps[1]; // profile
        config[i++] = this.sps[2]; // compat
        config[i++] = this.sps[3]; // level
        config[i++] = 0xFF; // 4-byte NAL length
        config[i++] = 0xE1; // 1 SPS

        config[i++] = (this.sps.length >> 8) & 0xFF;
        config[i++] = this.sps.length & 0xFF;
        config.set(this.sps, i);
        i += this.sps.length;

        config[i++] = 0x01; // 1 PPS
        config[i++] = (this.pps.length >> 8) & 0xFF;
        config[i++] = this.pps.length & 0xFF;
        config.set(this.pps, i);

        return config;
    }

    handleFrame(data) {
        const view = new DataView(data);

        const codec = view.getUint8(1);
        const flags = view.getUint8(2);
        const timestamp = view.getUint32(4, true);
        const frameIndex = view.getUint32(8, true);
        const dataLen = view.getUint16(12, true);

        const isKeyframe = (flags & 0x01) !== 0;
        const frameData = new Uint8Array(data, FRAME_HDR_SIZE, dataLen);

        this.stats.frames++;
        this.frameCount++;

        // Route to appropriate decoder
        if (codec === CODEC.MJPEG) {
            this.decodeMJPEG(frameData);
        } else if ((codec === CODEC.H264 || codec === CODEC.H265) && this.configured && this.decoder) {
            this.decodeH26x(frameData, timestamp, isKeyframe);
        }

        // Update latency
        const now = performance.now();
        this.stats.latency = now - this.lastFrameTime;
        this.lastFrameTime = now;
    }

    decodeMJPEG(data) {
        // MJPEG: create blob and load into Image
        const blob = new Blob([data], { type: 'image/jpeg' });
        if (this.mjpegUrl) {
            URL.revokeObjectURL(this.mjpegUrl);
        }
        this.mjpegUrl = URL.createObjectURL(blob);
        this.mjpegImg.src = this.mjpegUrl;
    }

    decodeH26x(data, timestamp, isKeyframe) {
        if (!this.decoder || this.decoder.state !== 'configured') {
            return;
        }

        try {
            const chunk = new EncodedVideoChunk({
                type: isKeyframe ? 'key' : 'delta',
                timestamp: timestamp,
                data: data
            });

            if (this.decoder.decodeQueueSize < this.maxPendingFrames) {
                this.decoder.decode(chunk);
            } else {
                this.stats.dropped++;
            }
        } catch (e) {
            console.warn('[FPVPlayer] Decode error:', e.message);
        }
    }

    onDecodedFrame(frame) {
        // Draw to canvas
        this.ctx.drawImage(frame, 0, 0, this.canvas.width, this.canvas.height);
        frame.close();
    }

    onDecoderError(error) {
        console.error('[FPVPlayer] Decoder error:', error.message);
        // Try to recover on next keyframe
        this.configured = false;
    }

    handleServerStats(data) {
        const view = new DataView(data);
        // Parse server stats if needed
    }

    startStatsUpdate() {
        this.lastStatsTime = performance.now();
        this.frameCount = 0;
        this.bytesReceived = 0;

        this.statsInterval = setInterval(() => {
            const now = performance.now();
            const elapsed = (now - this.lastStatsTime) / 1000;

            if (elapsed > 0) {
                this.stats.fps = this.frameCount / elapsed;
                this.stats.bitrate = (this.bytesReceived * 8) / 1000 / elapsed;

                this.frameCount = 0;
                this.bytesReceived = 0;
                this.lastStatsTime = now;
            }

            if (this.options.onStats) {
                this.options.onStats(this.stats);
            }
        }, 1000);
    }
}

// Export for use
if (typeof module !== 'undefined' && module.exports) {
    module.exports = FPVPlayer;
}

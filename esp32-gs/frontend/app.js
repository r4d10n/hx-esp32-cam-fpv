/**
 * ESP32 FPV Ground Station - Web Client
 * Slick FPV UI with camera controls and OSD telemetry
 */

// Resolution names for OSD display
const RESOLUTION_NAMES = [
    'QVGA', 'CIF', 'HVGA', 'VGA', 'VGA16', 'SVGA',
    'SVGA16', 'XGA', 'XGA16', 'SXGA', 'HD', 'UXGA'
];

// Elements
const videoEl = document.getElementById('video');
const noSignalEl = document.getElementById('no-signal');
const controlPanel = document.getElementById('control-panel');
const statsPanel = document.getElementById('stats-panel');

// OSD Elements
const osdConnection = document.getElementById('connection-status');
const osdRssi = document.getElementById('osd-rssi');
const osdTemp = document.getElementById('osd-temp');
const osdLatency = document.getElementById('osd-latency');
const osdFps = document.getElementById('osd-fps');
const osdChannel = document.getElementById('osd-channel');
const osdResolution = document.getElementById('osd-resolution');
const osdQuality = document.getElementById('osd-quality');
const osdRecStatus = document.getElementById('osd-rec-status');
const osdFec = document.getElementById('osd-fec');

// State
let ws = null;
let frameCount = 0;
let lastFpsTime = Date.now();
let lastFrameTime = 0;
let displayedFps = 0;
let reconnectTimer = null;
let statsTimer = null;
let airStatsTimer = null;
let currentChannel = 7;
let cameraConfig = {};
let airStats = {};

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    startStatsPolling();
    loadCameraConfig();
    loadAirStats();
});

// ==========================================
// WebSocket Connection
// ==========================================
function connectWebSocket() {
    if (ws && ws.readyState === WebSocket.OPEN) {
        return;
    }

    const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${location.host}/ws`;

    console.log('Connecting to', wsUrl);
    ws = new WebSocket(wsUrl);
    ws.binaryType = 'arraybuffer';

    ws.onopen = () => {
        console.log('WebSocket connected');
        setConnectionStatus(true);
        if (reconnectTimer) {
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
    };

    ws.onclose = () => {
        console.log('WebSocket disconnected');
        setConnectionStatus(false);
        showNoSignal();
        scheduleReconnect();
    };

    ws.onerror = (err) => {
        console.error('WebSocket error:', err);
        setConnectionStatus(false);
        showNoSignal();
    };

    ws.onmessage = (event) => {
        handleFrame(event.data);
    };
}

function scheduleReconnect() {
    if (reconnectTimer) return;
    reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        connectWebSocket();
    }, 2000);
}

function setConnectionStatus(connected) {
    osdConnection.textContent = connected ? 'CONNECTED' : 'DISCONNECTED';
    osdConnection.className = connected ? 'connected' : 'disconnected';
}

// ==========================================
// Frame Handling
// ==========================================
function handleFrame(data) {
    const blob = new Blob([data], { type: 'image/jpeg' });
    const url = URL.createObjectURL(blob);

    if (videoEl.src && videoEl.src.startsWith('blob:')) {
        URL.revokeObjectURL(videoEl.src);
    }

    videoEl.src = url;
    hideNoSignal();

    // Update FPS counter
    frameCount++;
    const now = Date.now();
    if (now - lastFpsTime >= 1000) {
        displayedFps = Math.round(frameCount * 1000 / (now - lastFpsTime));
        osdFps.textContent = displayedFps;
        frameCount = 0;
        lastFpsTime = now;
    }

    lastFrameTime = now;
}

function showNoSignal() {
    noSignalEl.classList.remove('hidden');
    videoEl.classList.add('hidden');
}

function hideNoSignal() {
    noSignalEl.classList.add('hidden');
    videoEl.classList.remove('hidden');
}

// ==========================================
// Stats Polling
// ==========================================
function startStatsPolling() {
    updateStats();
    statsTimer = setInterval(updateStats, 1000);
    airStatsTimer = setInterval(loadAirStats, 2000);
}

async function updateStats() {
    try {
        const resp = await fetch('/api/stats');
        if (!resp.ok) return;

        const stats = await resp.json();

        // Update OSD
        osdChannel.textContent = stats.channel;
        currentChannel = stats.channel;

        // Update stats panel
        updateElement('stat-pkts-rx', stats.packets_received);
        updateElement('stat-pkts-valid', stats.packets_valid);
        updateElement('stat-frames-ok', stats.frames_complete);
        updateElement('stat-frames-lost', stats.frames_incomplete);
        updateElement('stat-ws', stats.websocket_clients);

        // FEC stats if available
        if (stats.fec) {
            updateElement('stat-fec-rx', stats.fec.blocks_received || 0);
            updateElement('stat-fec-ok', stats.fec.blocks_complete || 0);
            updateElement('stat-fec-rec', stats.fec.blocks_recovered || 0);
            updateElement('stat-fec-fail', stats.fec.blocks_failed || 0);
            updateElement('stat-pkts-rec', stats.fec.packets_recovered || 0);

            // Update OSD FEC indicator
            const recovered = stats.fec.packets_recovered || 0;
            const failed = stats.fec.blocks_failed || 0;
            osdFec.textContent = `${recovered}/${failed}`;
        }

        // Highlight current channel button
        document.querySelectorAll('.channel-grid button').forEach((btn, idx) => {
            btn.classList.toggle('active', idx + 1 === currentChannel);
        });

        // Check for stale video
        if (lastFrameTime > 0 && Date.now() - lastFrameTime > 3000) {
            showNoSignal();
        }

    } catch (err) {
        console.error('Failed to fetch stats:', err);
    }
}

async function loadAirStats() {
    try {
        const resp = await fetch('/api/air');
        if (!resp.ok) return;

        airStats = await resp.json();

        // Update OSD telemetry
        if (airStats.rssi_dbm !== undefined) {
            osdRssi.textContent = `${airStats.rssi_dbm} dBm`;
        }
        if (airStats.temperature !== undefined) {
            osdTemp.textContent = `${airStats.temperature}°C`;
        }
        if (airStats.latency_ms !== undefined) {
            osdLatency.textContent = `${airStats.latency_ms} ms`;
        }
        if (airStats.resolution !== undefined) {
            osdResolution.textContent = RESOLUTION_NAMES[airStats.resolution] || '---';
        }
        if (airStats.curr_quality !== undefined) {
            osdQuality.textContent = airStats.curr_quality;
        }

        // Recording status
        if (airStats.air_record_state !== undefined) {
            const recording = airStats.air_record_state === 1;
            osdRecStatus.textContent = recording ? 'REC' : 'OFF';
            document.querySelector('.rec-dot').classList.toggle('active', recording);
        }

        // Update stats panel - Air Unit section
        updateElement('stat-air-rssi', `${airStats.rssi_dbm || '--'} dBm`);
        updateElement('stat-air-noise', `${airStats.noise_floor_dbm || '--'} dBm`);
        updateElement('stat-air-temp', `${airStats.temperature || '--'}°C`);
        updateElement('stat-air-fps', airStats.capture_fps || '--');
        updateElement('stat-air-tx', `${airStats.out_packet_rate || 0} pkt/s`);
        updateElement('stat-air-rx', `${airStats.in_packet_rate || 0} pkt/s`);

        // SD card status
        if (airStats.sd_detected !== undefined) {
            let sdStatus = '--';
            if (airStats.sd_detected) {
                const freeGB = (airStats.sd_free_space_gb16 || 0) / 16;
                sdStatus = airStats.sd_error ? 'ERROR' : `${freeGB.toFixed(1)} GB`;
                if (airStats.sd_slow) sdStatus += ' (SLOW)';
            } else {
                sdStatus = 'No Card';
            }
            updateElement('stat-sd', sdStatus);
        }

    } catch (err) {
        // Air stats endpoint may not be available yet
        console.debug('Air stats not available:', err.message);
    }
}

function updateElement(id, value) {
    const el = document.getElementById(id);
    if (el) el.textContent = value;
}

// ==========================================
// Camera Configuration
// ==========================================
async function loadCameraConfig() {
    try {
        const resp = await fetch('/api/camera');
        if (!resp.ok) return;

        cameraConfig = await resp.json();

        // Update UI controls with current values
        setControlValue('ctrl-resolution', cameraConfig.resolution);
        setControlValue('ctrl-fps', cameraConfig.fps_limit);
        setControlValue('ctrl-quality', cameraConfig.quality);
        setControlValue('ctrl-brightness', cameraConfig.brightness);
        setControlValue('ctrl-contrast', cameraConfig.contrast);
        setControlValue('ctrl-saturation', cameraConfig.saturation);
        setControlValue('ctrl-sharpness', cameraConfig.sharpness);
        setControlValue('ctrl-aec', cameraConfig.aec);
        setControlValue('ctrl-aec2', cameraConfig.aec2);
        setControlValue('ctrl-ae-level', cameraConfig.ae_level);
        setControlValue('ctrl-aec-value', cameraConfig.aec_value);
        setControlValue('ctrl-agc', cameraConfig.agc);
        setControlValue('ctrl-agc-gain', cameraConfig.agc_gain);
        setControlValue('ctrl-gainceiling', cameraConfig.gainceiling);
        setControlValue('ctrl-hmirror', cameraConfig.hmirror);
        setControlValue('ctrl-vflip', cameraConfig.vflip);

        // Update value displays
        showValue(document.getElementById('ctrl-fps'), 'fps-val');
        showValue(document.getElementById('ctrl-quality'), 'quality-val');
        showValue(document.getElementById('ctrl-ae-level'), 'ae-level-val');
        showValue(document.getElementById('ctrl-aec-value'), 'aec-value-val');
        showValue(document.getElementById('ctrl-agc-gain'), 'agc-gain-val');
        showValue(document.getElementById('ctrl-brightness'), 'brightness-val');
        showValue(document.getElementById('ctrl-contrast'), 'contrast-val');
        showValue(document.getElementById('ctrl-saturation'), 'saturation-val');
        showValue(document.getElementById('ctrl-sharpness'), 'sharpness-val');

        // Update conditional UI
        updateExposureUI();
        updateGainUI();

    } catch (err) {
        console.debug('Camera config not available:', err.message);
    }
}

function setControlValue(id, value) {
    const el = document.getElementById(id);
    if (!el || value === undefined) return;

    if (el.type === 'checkbox') {
        el.checked = !!value;
    } else {
        el.value = value;
    }
}

async function setCamera(param, value) {
    // Convert checkbox boolean to int
    if (typeof value === 'boolean') {
        value = value ? 1 : 0;
    }
    value = parseInt(value);

    // Update local state
    cameraConfig[param] = value;

    try {
        const resp = await fetch('/api/camera', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ [param]: value })
        });

        if (resp.ok) {
            console.log(`Camera ${param} set to ${value}`);
        } else {
            showNotification(`Failed to set ${param}`);
        }
    } catch (err) {
        console.error('Failed to set camera param:', err);
        showNotification('Connection error');
    }
}

// Update exposure UI based on AEC state
function updateExposureUI() {
    const aecEnabled = document.getElementById('ctrl-aec').checked;
    document.getElementById('row-ae-level').classList.toggle('hidden', !aecEnabled);
    document.getElementById('row-aec-value').classList.toggle('hidden', aecEnabled);
}

// Update gain UI based on AGC state
function updateGainUI() {
    const agcEnabled = document.getElementById('ctrl-agc').checked;
    document.getElementById('row-gain-ceil').classList.toggle('hidden', !agcEnabled);
    document.getElementById('row-agc-gain').classList.toggle('hidden', agcEnabled);
}

// Show value next to slider
function showValue(input, valId) {
    if (!input) return;
    const valEl = document.getElementById(valId);
    if (valEl) valEl.textContent = input.value;
}

// ==========================================
// Channel Control
// ==========================================
async function setChannel(channel) {
    if (channel === currentChannel) return;

    showNotification('Changing channel...');

    try {
        const resp = await fetch('/api/channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ channel: channel })
        });

        if (resp.ok) {
            currentChannel = channel;
            osdChannel.textContent = channel;
            showNotification(`Channel set to ${channel}`);

            // Update channel buttons
            document.querySelectorAll('.channel-grid button').forEach((btn, idx) => {
                btn.classList.toggle('active', idx + 1 === channel);
            });
        }
    } catch (err) {
        console.error('Failed to set channel:', err);
        showNotification('Failed to change channel');
    }
}

// ==========================================
// UI Controls
// ==========================================
function toggleControls() {
    const panel = document.getElementById('control-panel');
    const btn = document.getElementById('btn-controls');
    panel.classList.toggle('hidden');
    btn.classList.toggle('active', !panel.classList.contains('hidden'));

    // Close stats panel if open
    if (!panel.classList.contains('hidden')) {
        document.getElementById('stats-panel').classList.add('hidden');
        document.getElementById('btn-stats').classList.remove('active');
    }
}

function toggleStats() {
    const panel = document.getElementById('stats-panel');
    const btn = document.getElementById('btn-stats');
    panel.classList.toggle('hidden');
    btn.classList.toggle('active', !panel.classList.contains('hidden'));

    // Close control panel if open
    if (!panel.classList.contains('hidden')) {
        document.getElementById('control-panel').classList.add('hidden');
        document.getElementById('btn-controls').classList.remove('active');
    }
}

function showNotification(message) {
    let notif = document.getElementById('notification');
    if (!notif) {
        notif = document.createElement('div');
        notif.id = 'notification';
        document.body.appendChild(notif);
    }
    notif.textContent = message;
    notif.style.display = 'block';

    setTimeout(() => {
        notif.style.display = 'none';
    }, 2500);
}

// ==========================================
// Fullscreen & Touch
// ==========================================
function toggleFullscreen() {
    const container = document.getElementById('video-container');
    if (document.fullscreenElement) {
        document.exitFullscreen();
    } else {
        container.requestFullscreen().catch(err => {
            console.log('Fullscreen not supported');
        });
    }
}

// Double-tap to fullscreen
let lastTap = 0;
videoEl.addEventListener('touchend', (e) => {
    const now = Date.now();
    if (now - lastTap < 300) {
        toggleFullscreen();
    }
    lastTap = now;
});

// Double-click to fullscreen
videoEl.addEventListener('dblclick', () => {
    toggleFullscreen();
});

// Prevent context menu on video
document.addEventListener('contextmenu', (e) => {
    if (e.target === videoEl) {
        e.preventDefault();
    }
});

// Handle visibility changes
document.addEventListener('visibilitychange', () => {
    if (!document.hidden) {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            connectWebSocket();
        }
    }
});

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
    // F - Fullscreen
    if (e.key === 'f' || e.key === 'F') {
        toggleFullscreen();
    }
    // C - Toggle camera controls
    if (e.key === 'c' || e.key === 'C') {
        toggleControls();
    }
    // S - Toggle stats
    if (e.key === 's' || e.key === 'S') {
        toggleStats();
    }
    // Escape - Close panels
    if (e.key === 'Escape') {
        document.getElementById('control-panel').classList.add('hidden');
        document.getElementById('stats-panel').classList.add('hidden');
        document.getElementById('btn-controls').classList.remove('active');
        document.getElementById('btn-stats').classList.remove('active');
    }
    // Number keys 1-9, 0 for channels
    if (e.key >= '1' && e.key <= '9') {
        setChannel(parseInt(e.key));
    }
    if (e.key === '0') {
        setChannel(10);
    }
});

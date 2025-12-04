/**
 * ESP32 FPV Ground Station - Web Client
 */

// Elements
const videoEl = document.getElementById('video');
const noSignalEl = document.getElementById('no-signal');
const fpsEl = document.getElementById('fps');
const rssiEl = document.getElementById('rssi');
const channelEl = document.getElementById('channel');
const settingsPanel = document.getElementById('settings-panel');

// State
let ws = null;
let frameCount = 0;
let lastFpsTime = Date.now();
let lastFrameTime = 0;
let reconnectTimer = null;
let statsTimer = null;
let currentChannel = 1;

// Initialize
document.addEventListener('DOMContentLoaded', () => {
    connectWebSocket();
    startStatsPolling();
    loadConfig();
});

// WebSocket connection
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
        if (reconnectTimer) {
            clearTimeout(reconnectTimer);
            reconnectTimer = null;
        }
    };

    ws.onclose = () => {
        console.log('WebSocket disconnected');
        showNoSignal();
        scheduleReconnect();
    };

    ws.onerror = (err) => {
        console.error('WebSocket error:', err);
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

// Frame handling
function handleFrame(data) {
    // Create blob URL for JPEG
    const blob = new Blob([data], { type: 'image/jpeg' });
    const url = URL.createObjectURL(blob);

    // Revoke old URL to prevent memory leak
    if (videoEl.src && videoEl.src.startsWith('blob:')) {
        URL.revokeObjectURL(videoEl.src);
    }

    videoEl.src = url;
    hideNoSignal();

    // Update FPS counter
    frameCount++;
    const now = Date.now();
    if (now - lastFpsTime >= 1000) {
        const fps = Math.round(frameCount * 1000 / (now - lastFpsTime));
        fpsEl.textContent = `${fps} FPS`;
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

// Stats polling
function startStatsPolling() {
    updateStats();
    statsTimer = setInterval(updateStats, 1000);
}

async function updateStats() {
    try {
        const resp = await fetch('/api/stats');
        if (!resp.ok) return;

        const stats = await resp.json();

        rssiEl.textContent = `RSSI: ${stats.rssi_dbm} dBm`;
        channelEl.textContent = `CH: ${stats.channel}`;
        currentChannel = stats.channel;

        // Update detailed stats
        document.getElementById('stat-packets').textContent = stats.packets_received;
        document.getElementById('stat-valid').textContent = stats.packets_valid;
        document.getElementById('stat-frames').textContent = stats.frames_complete;
        document.getElementById('stat-incomplete').textContent = stats.frames_incomplete;
        document.getElementById('stat-clients').textContent = stats.websocket_clients;

        // Highlight current channel button
        document.querySelectorAll('.channel-buttons button').forEach((btn, idx) => {
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

// Configuration
async function loadConfig() {
    try {
        const resp = await fetch('/api/config');
        if (!resp.ok) return;

        const config = await resp.json();
        document.getElementById('ap-ssid').value = config.ssid || '';
        currentChannel = config.channel;

    } catch (err) {
        console.error('Failed to load config:', err);
    }
}

async function setChannel(channel) {
    if (channel === currentChannel) return;

    // Show notification that connection will reset
    showNotification('Changing channel... Connection will reset.');

    try {
        const resp = await fetch('/api/channel', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ channel: channel })
        });

        if (resp.ok) {
            currentChannel = channel;
            channelEl.textContent = `CH: ${channel}`;
            console.log('Channel set to', channel);
            // Connection will likely be lost, show reconnecting message
            showNotification('Channel changed. Reconnecting...');
            showNoSignal();
            // Force reconnect after a short delay
            setTimeout(() => {
                if (ws) ws.close();
                connectWebSocket();
            }, 1000);
        }
    } catch (err) {
        console.error('Failed to set channel:', err);
        showNotification('Failed to change channel');
    }
}

function showNotification(message) {
    // Create or update notification element
    let notif = document.getElementById('notification');
    if (!notif) {
        notif = document.createElement('div');
        notif.id = 'notification';
        notif.style.cssText = 'position:fixed;top:10px;left:50%;transform:translateX(-50%);' +
            'background:rgba(0,0,0,0.8);color:#fff;padding:10px 20px;border-radius:5px;' +
            'z-index:1000;font-size:14px;';
        document.body.appendChild(notif);
    }
    notif.textContent = message;
    notif.style.display = 'block';

    // Auto-hide after 3 seconds
    setTimeout(() => {
        notif.style.display = 'none';
    }, 3000);
}

async function saveApConfig() {
    const ssid = document.getElementById('ap-ssid').value;
    const pass = document.getElementById('ap-pass').value;

    if (!ssid) {
        alert('SSID is required');
        return;
    }

    if (pass && pass.length < 8) {
        alert('Password must be at least 8 characters');
        return;
    }

    try {
        const resp = await fetch('/api/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ ssid: ssid, password: pass })
        });

        if (resp.ok) {
            alert('Configuration saved. Restart the device to apply changes.');
        }
    } catch (err) {
        console.error('Failed to save config:', err);
        alert('Failed to save configuration');
    }
}

// UI toggles
function toggleSettings() {
    settingsPanel.classList.toggle('hidden');
}

// Handle visibility changes (pause/resume)
document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
        // Page hidden - could pause polling
    } else {
        // Page visible - ensure connected
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            connectWebSocket();
        }
    }
});

// Prevent context menu on long press (mobile)
document.addEventListener('contextmenu', (e) => {
    if (e.target === videoEl) {
        e.preventDefault();
    }
});

// Double-tap to fullscreen (mobile)
let lastTap = 0;
videoEl.addEventListener('touchend', (e) => {
    const now = Date.now();
    if (now - lastTap < 300) {
        toggleFullscreen();
    }
    lastTap = now;
});

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

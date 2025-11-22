# Ground Station Architecture

## Overview

The Ground Station (GS) receives video and telemetry from the air unit, decodes and renders it, provides an OSD interface, and sends back configuration and RC commands. This document details the complete GS architecture.

## Hardware Platform

### Supported Platforms

| Platform | CPU | RAM | GPU | WiFi | Status |
|----------|-----|-----|-----|------|--------|
| Radxa Zero 3W | ARM Cortex-A55 4-core @ 1.8GHz | 1-8GB LPDDR4 | Mali-G52 | Built-in | **Recommended** |
| Raspberry Pi 4B | ARM Cortex-A72 4-core @ 1.5GHz | 1-8GB LPDDR4 | VideoCore VI | External | Supported |
| Raspberry Pi Zero 2W | ARM Cortex-A53 4-core @ 1GHz | 512MB | VideoCore IV | External | Legacy |
| x86_64 Desktop | Varies | 4GB+ | Any | External | Development |

### Required WiFi Cards

**RTL8812AU (Recommended):**
- Chipset: Realtek RTL8812AU
- Bands: 2.4GHz + 5.8GHz (only 2.4GHz used)
- Features:
  - Monitor mode support
  - Packet injection
  - Antenna diversity (2 antennas)
  - Low noise amplifier (LNA)
- Power: ~500mA @ 5V

**AR9271 (Alternative):**
- Chipset: Atheros AR9271
- Bands: 2.4GHz only
- Features:
  - Excellent monitor mode support
  - Good packet injection
  - Single antenna
- Power: ~300mA @ 5V

### Dual WiFi Configuration

```
┌────────────────────────────────────────────────┐
│              Ground Station                    │
├────────────────────────────────────────────────┤
│                                                │
│  ┌──────────────┐      ┌──────────────┐      │
│  │ RTL8812AU #1 │      │ RTL8812AU #2 │      │
│  │   (wlan1)    │      │   (wlan2)    │      │
│  └──────┬───────┘      └──────┬───────┘      │
│         │                     │               │
│         │ Antenna 1           │ Antenna 3     │
│         │ Antenna 2           │ Antenna 4     │
│         │                     │               │
│  ┌──────▼─────────────────────▼───────┐      │
│  │    Diversity Combiner               │      │
│  │    - Deduplicate packets            │      │
│  │    - Select best RSSI               │      │
│  │    - Spatial diversity              │      │
│  └──────────────┬──────────────────────┘      │
│                 │                              │
│                 ▼                              │
│         FEC Decoder                            │
│                                                │
└────────────────────────────────────────────────┘
```

## Software Architecture

### Thread Model

```
┌────────────────────────────────────────────────────────┐
│                    Main Thread                         │
│  - OpenGL/EGL rendering                                │
│  - OSD overlay                                         │
│  - ImGui UI                                            │
│  - Input handling (GPIO/keyboard/mouse)                │
│  - 60 FPS rendering loop                               │
└────────────────────────────────────────────────────────┘

┌─────────────────┐  ┌─────────────────┐
│ WiFi RX Thread  │  │ WiFi RX Thread  │
│    (wlan1)      │  │    (wlan2)      │
│  - pcap capture │  │  - pcap capture │
│  - Packet filter│  │  - Packet filter│
│  - RSSI extract │  │  - RSSI extract │
└────────┬────────┘  └────────┬────────┘
         │                    │
         └──────────┬─────────┘
                    │
         ┌──────────▼──────────┐
         │  Packet Merger      │
         │  - Deduplication    │
         │  - RSSI selection   │
         └──────────┬──────────┘
                    │
         ┌──────────▼──────────┐
         │  FEC Decoder Thread │
         │  - Block assembly   │
         │  - Reed-Solomon     │
         │  - Frame extraction │
         └──────────┬──────────┘
                    │
         ┌──────────▼──────────┐
         │  JPEG Decoder Thread│
         │  - TurboJPEG        │
         │  - RGB conversion   │
         │  - Frame queue      │
         └──────────┬──────────┘
                    │
         ┌──────────▼──────────┐
         │  DVR Writer Thread  │
         │  - AVI formatting   │
         │  - SD card writes   │
         └─────────────────────┘

┌─────────────────────────────────┐
│  Config TX Thread               │
│  - Send config packets          │
│  - Send telemetry (Mavlink RC)  │
│  - Packet injection             │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│  Telemetry Thread               │
│  - UART handling                │
│  - Mavlink parsing              │
│  - OSD data extraction          │
└─────────────────────────────────┘
```

### Component Diagram

```
┌────────────────────────────────────────────────────┐
│                   Main Application                 │
├────────────────────────────────────────────────────┤
│                                                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │   HAL    │  │  Comms   │  │  Video   │        │
│  │Interface │  │  Layer   │  │ Decoder  │        │
│  └─────┬────┘  └────┬─────┘  └────┬─────┘        │
│        │            │             │               │
│  ┌─────▼─────┐ ┌───▼──────┐ ┌────▼─────┐        │
│  │ PI_HAL /  │ │  WiFi    │ │ TurboJPEG│        │
│  │ X11_HAL   │ │ Monitor  │ │  Library │        │
│  └─────┬─────┘ └───┬──────┘ └────┬─────┘        │
│        │           │             │               │
│  ┌─────▼────────────▼─────────────▼─────┐       │
│  │         OpenGL ES / EGL                │       │
│  │         - Texture upload               │       │
│  │         - Fragment shaders             │       │
│  │         - Framebuffer rendering        │       │
│  └────────────────────────────────────────┘       │
│                                                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐        │
│  │   OSD    │  │OSD Menu  │  │  Stats   │        │
│  │ Renderer │  │  System  │  │ Monitor  │        │
│  └──────────┘  └──────────┘  └──────────┘        │
│                                                    │
└────────────────────────────────────────────────────┘
```

## Comms Layer

### WiFi Monitor Mode

**Setup Process:**

```bash
# 1. Stop NetworkManager (if running)
systemctl stop NetworkManager

# 2. Bring down interface
ifconfig wlan1 down

# 3. Set monitor mode
iw dev wlan1 set monitor otherbss
# or
iwconfig wlan1 mode monitor

# 4. Set channel
iw dev wlan1 set channel 7

# 5. Bring up interface
ifconfig wlan1 up

# 6. Set TX power (for uplink)
iw dev wlan1 set txpower fixed 2000  # 20dBm
```

**C++ Implementation:**

```cpp
bool Comms::setMonitorMode(const std::vector<std::string>& interfaces) {
    for (auto& iface : interfaces) {
        // Execute commands
        system(fmt::format("ifconfig {} down", iface).c_str());
        system(fmt::format("iw dev {} set monitor otherbss", iface).c_str());
        system(fmt::format("iw dev {} set channel {}", iface, channel).c_str());
        system(fmt::format("ifconfig {} up", iface).c_str());
        system(fmt::format("iw dev {} set txpower fixed {}",
                          iface, txpower_mbm).c_str());
    }
    return true;
}
```

### Packet Capture (libpcap)

```cpp
bool Comms::prepare_pcap(std::string const& interface, PCap& pcap) {
    char errbuf[PCAP_ERRBUF_SIZE];

    // Open device
    pcap.pcap = pcap_open_live(interface.c_str(),
                              PCAP_SNAPLEN,
                              1,  // Promiscuous
                              1,  // Timeout ms
                              errbuf);
    if (!pcap.pcap) {
        fprintf(stderr, "pcap_open_live failed: %s\n", errbuf);
        return false;
    }

    // Set non-blocking
    pcap_setnonblock(pcap.pcap, 1, errbuf);

    // Get file descriptor for select()
    pcap.fd = pcap_get_selectable_fd(pcap.pcap);

    // Compile filter
    if (!prepare_filter(pcap)) {
        return false;
    }

    return true;
}
```

### Packet Filter

**BPF Filter:**

```cpp
bool Comms::prepare_filter(PCap& pcap) {
    struct bpf_program filter;

    // Filter: 802.11 data frames, correct BSSID
    std::string filter_str = "type data";

    if (pcap_compile(pcap.pcap, &filter, filter_str.c_str(),
                     1, PCAP_NETMASK_UNKNOWN) < 0) {
        fprintf(stderr, "Filter compile failed\n");
        return false;
    }

    if (pcap_setfilter(pcap.pcap, &filter) < 0) {
        fprintf(stderr, "Set filter failed\n");
        return false;
    }

    pcap_freecode(&filter);
    return true;
}
```

### Radiotap Header Parsing

```cpp
struct ieee80211_radiotap_header {
    uint8_t it_version;
    uint8_t it_pad;
    uint16_t it_len;
    uint32_t it_present;
} __attribute__((packed));

int8_t extract_rssi(uint8_t* packet, size_t len) {
    auto* rtap = (ieee80211_radiotap_header*)packet;

    if (rtap->it_version != 0) {
        return 0;
    }

    // Parse present fields
    size_t offset = sizeof(ieee80211_radiotap_header);

    // Look for antenna signal field
    if (rtap->it_present & (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL)) {
        // Navigate to signal field
        // ... (complex bit field parsing)
        int8_t signal_dbm = *(int8_t*)(packet + offset);
        return signal_dbm;
    }

    return 0;
}
```

### Packet Reception

```cpp
bool Comms::process_rx_packet(PCap& pcap) {
    struct pcap_pkthdr header;
    const uint8_t* packet = pcap_next(pcap.pcap, &header);

    if (!packet) {
        return false;
    }

    // Extract radiotap header
    auto* rtap = (ieee80211_radiotap_header*)packet;
    size_t rtap_len = rtap->it_len;

    // Extract RSSI
    int8_t rssi = extract_rssi(packet, header.caplen);

    // Update best RSSI
    if (rssi < m_best_input_dBm.load()) {
        m_best_input_dBm = rssi;
    }

    // Skip radiotap + 802.11 header
    size_t header_offset = rtap_len + IEEE80211_HEADER_SIZE;
    const uint8_t* payload = packet + header_offset;
    size_t payload_len = header.caplen - header_offset;

    // Process payload
    packetFilter.processPacket(payload, payload_len, rssi);

    return true;
}
```

### Packet Injection

```cpp
void Comms::send(void const* data, size_t size, bool flush) {
    if (!m_impl->tx_pcap) {
        return;
    }

    // Prepare radiotap header
    uint8_t packet[2048];
    size_t offset = 0;

    // Copy radiotap header (prepared earlier)
    memcpy(packet + offset, m_impl->radiotap_header, m_impl->radiotap_size);
    offset += m_impl->radiotap_size;

    // Copy 802.11 header
    memcpy(packet + offset, m_impl->ieee80211_header, m_impl->ieee80211_size);
    offset += m_impl->ieee80211_size;

    // Copy payload
    memcpy(packet + offset, data, size);
    offset += size;

    // Inject packet
    if (pcap_inject(m_impl->tx_pcap, packet, offset) < 0) {
        fprintf(stderr, "Packet injection failed\n");
    }
}
```

### Radiotap TX Header

```cpp
void Comms::prepare_radiotap_header(size_t rate_hz) {
    struct {
        uint8_t version;
        uint8_t pad;
        uint16_t len;
        uint32_t present;
        uint8_t flags;
        uint8_t rate;  // 500kbps units
        uint16_t channel_freq;
        uint16_t channel_flags;
    } __attribute__((packed)) rtap;

    rtap.version = 0;
    rtap.pad = 0;
    rtap.len = sizeof(rtap);
    rtap.present = (1 << IEEE80211_RADIOTAP_FLAGS) |
                   (1 << IEEE80211_RADIOTAP_RATE) |
                   (1 << IEEE80211_RADIOTAP_CHANNEL);
    rtap.flags = 0;
    rtap.rate = (rate_hz / 500000);  // Convert to 500kbps units

    // Channel 7 = 2442 MHz
    rtap.channel_freq = 2442;
    rtap.channel_flags = 0x00a0;  // 2GHz, OFDM

    memcpy(m_impl->radiotap_header, &rtap, sizeof(rtap));
    m_impl->radiotap_size = sizeof(rtap);
}
```

## Video Decoding

### TurboJPEG Integration

```cpp
#include <turbojpeg.h>

class Video_Decoder {
public:
    Video_Decoder() {
        // Create decoder instances (one per thread)
        for (size_t i = 0; i < NUM_DECODER_THREADS; i++) {
            m_decoders[i] = tjInitDecompress();
        }
    }

    ~Video_Decoder() {
        for (auto decoder : m_decoders) {
            tjDestroy(decoder);
        }
    }

    bool decode_data(void const* jpeg_data, size_t jpeg_size) {
        // Parse JPEG header
        int width, height, subsample, colorspace;

        int ret = tjDecompressHeader3(m_decoders[0],
                                      (uint8_t*)jpeg_data, jpeg_size,
                                      &width, &height,
                                      &subsample, &colorspace);
        if (ret < 0) {
            fprintf(stderr, "JPEG header parse error\n");
            return false;
        }

        // Allocate RGB buffer
        size_t rgb_size = width * height * 3;
        uint8_t* rgb_buffer = new uint8_t[rgb_size];

        // Decompress
        auto start = Clock::now();

        ret = tjDecompress2(m_decoders[0],
                           (uint8_t*)jpeg_data, jpeg_size,
                           rgb_buffer,
                           width, 0, height,
                           TJPF_RGB,
                           TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE);

        auto duration = Clock::now() - start;
        auto ms = duration_cast<milliseconds>(duration).count();

        if (ret < 0) {
            fprintf(stderr, "JPEG decode error: %s\n", tjGetErrorStr());
            delete[] rgb_buffer;
            return false;
        }

        // Upload to GPU texture
        upload_texture(rgb_buffer, width, height);

        delete[] rgb_buffer;
        return true;
    }

private:
    tjhandle m_decoders[NUM_DECODER_THREADS];
};
```

### Multi-threaded Decoding

```cpp
void Video_Decoder::decoder_thread_proc(size_t thread_index) {
    tjhandle decoder = m_decoders[thread_index];

    while (!m_exit) {
        // Wait for JPEG frame
        Frame* frame = nullptr;
        if (frame_queue.pop(frame, 100ms)) {

            // Decode
            uint8_t* rgb = new uint8_t[frame->width * frame->height * 3];

            tjDecompress2(decoder,
                         frame->jpeg_data, frame->jpeg_size,
                         rgb,
                         frame->width, 0, frame->height,
                         TJPF_RGB,
                         TJFLAG_FASTDCT);

            // Queue for rendering
            decoded_frame_queue.push(rgb, frame->width, frame->height);

            delete frame;
        }
    }
}
```

## OpenGL Rendering

### EGL Context Setup

```cpp
bool PI_HAL::init() {
    // Get EGL display
    m_egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(m_egl_display, nullptr, nullptr);

    // Choose config
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    EGLConfig config;
    EGLint num_configs;
    eglChooseConfig(m_egl_display, attribs, &config, 1, &num_configs);

    // Create context
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };
    m_egl_context = eglCreateContext(m_egl_display, config,
                                     EGL_NO_CONTEXT, context_attribs);

    // Create window surface (platform-specific)
    m_egl_surface = create_window_surface(config);

    // Make current
    eglMakeCurrent(m_egl_display, m_egl_surface,
                   m_egl_surface, m_egl_context);

    return true;
}
```

### Video Texture Rendering

```cpp
void render_video_frame() {
    lock_texture();

    // Bind video texture
    glBindTexture(GL_TEXTURE_2D, m_video_texture);

    // Full-screen quad
    glBegin(GL_QUADS);
        glTexCoord2f(0, 0); glVertex2f(-1, -1);  // Bottom-left
        glTexCoord2f(1, 0); glVertex2f( 1, -1);  // Bottom-right
        glTexCoord2f(1, 1); glVertex2f( 1,  1);  // Top-right
        glTexCoord2f(0, 1); glVertex2f(-1,  1);  // Top-left
    glEnd();

    unlock_texture();
}
```

### Texture Upload

```cpp
void Video_Decoder::upload_texture(uint8_t* rgb, int width, int height) {
    std::lock_guard<std::mutex> lock(m_texture_mutex);

    // Update resolution if changed
    if (width != m_resolution.x || height != m_resolution.y) {
        m_resolution = ImVec2(width, height);
    }

    // Upload to GPU
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                width, height, 0,
                GL_RGB, GL_UNSIGNED_BYTE, rgb);

    // Set texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
```

## OSD System

### DisplayPort MSP OSD

**Character Grid:** 53 columns × 20 rows

```cpp
struct OSDBuffer {
    uint8_t screenLow[20][53];   // Low 8 bits of character
    uint8_t screenHigh[20][7];   // High bits (packed)
};

// Get character at position
uint16_t get_osd_char(OSDBuffer& osd, int row, int col) {
    uint8_t low = osd.screenLow[row][col];
    uint8_t high_byte = osd.screenHigh[row][col / 8];
    uint8_t high_bit = (high_byte >> (col % 8)) & 1;
    return (high_bit << 8) | low;
}
```

**Font Rendering:**

```cpp
void render_osd_character(uint16_t char_code, int x, int y) {
    // Load font texture (24x18 pixels per character)
    int tex_x = (char_code % FONT_COLS) * 24;
    int tex_y = (char_code / FONT_COLS) * 18;

    // Render textured quad
    glBindTexture(GL_TEXTURE_2D, m_osd_font_texture);
    glBegin(GL_QUADS);
        glTexCoord2f(tex_x, tex_y);
        glVertex2f(x, y);
        // ... other corners ...
    glEnd();
}

void render_osd(OSDBuffer& osd) {
    for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 53; col++) {
            uint16_t ch = get_osd_char(osd, row, col);
            if (ch != 0) {  // Non-transparent
                int x = col * CHAR_WIDTH;
                int y = row * CHAR_HEIGHT;
                render_osd_character(ch, x, y);
            }
        }
    }
}
```

### Statistics Overlay

**Camera OSD Elements:**

```cpp
void render_camera_osd(AirStats& stats) {
    char buf[256];

    // RSSI
    snprintf(buf, sizeof(buf), "AIR:%ddBm GS:%ddBm:%ddBm",
            -stats.rssiDbm, -gsRssi1, -gsRssi2);
    draw_text(buf, 10, 10);

    // WiFi queue
    float queue_pct = (stats.wifi_queue_max / 255.0f) * 100;
    draw_bar(queue_pct, 10, 30, queue_pct > 70 ? RED : GREEN);

    // Bandwidth
    float mbps = (stats.outMavlinkRate * 8 / 1000000.0f);
    snprintf(buf, sizeof(buf), "%.1f Mbps", mbps);
    draw_text(buf, 10, 50);

    // Resolution + FPS
    snprintf(buf, sizeof(buf), "%s %dfps",
            resolution_name(stats.resolution), stats.captureFPS);
    draw_text(buf, 10, 70);

    // Recording indicators
    if (stats.air_record_state) {
        draw_text("AIR", 10, 90, RED);
    }
    if (gs_recording) {
        draw_text("GS", 50, 90, RED);
    }

    // Warnings
    if (stats.SDSlow) {
        draw_text("!SD SLOW!", 10, 110, YELLOW);
    }
    if (stats.overheatTrottling) {
        snprintf(buf, sizeof(buf), "Air: %d°C", stats.temperature);
        draw_text(buf, 10, 130, RED);
    }
}
```

## DVR Recording

### AVI Format

```cpp
class AVIWriter {
public:
    bool open(const char* filename, int width, int height, int fps) {
        m_file = fopen(filename, "wb");

        // Write RIFF header
        write_fourcc("RIFF");
        m_riff_size_offset = ftell(m_file);
        write_uint32(0);  // Placeholder
        write_fourcc("AVI ");

        // Write hdrl list
        write_hdrl_list(width, height, fps);

        // Start movi list
        write_fourcc("LIST");
        m_movi_size_offset = ftell(m_file);
        write_uint32(0);  // Placeholder
        write_fourcc("movi");

        return true;
    }

    void write_frame(uint8_t* jpeg, size_t size) {
        write_fourcc("00dc");  // Compressed video
        write_uint32(size);
        fwrite(jpeg, 1, size, m_file);

        // Pad to even boundary
        if (size % 2) {
            fputc(0, m_file);
        }

        // Add to index
        m_index.push_back({
            .fourcc = fourcc("00dc"),
            .flags = 0x10,  // AVIIF_KEYFRAME
            .offset = m_movi_offset,
            .size = (uint32_t)size
        });

        m_movi_offset += size + 8;
        m_frame_count++;
    }

    void close() {
        // Write index
        write_idx1();

        // Update sizes
        fseek(m_file, m_riff_size_offset, SEEK_SET);
        write_uint32(m_total_size - 8);

        fseek(m_file, m_movi_size_offset, SEEK_SET);
        write_uint32(m_movi_size);

        fclose(m_file);
    }

private:
    FILE* m_file;
    size_t m_riff_size_offset;
    size_t m_movi_size_offset;
    size_t m_movi_offset = 0;
    std::vector<IndexEntry> m_index;
    int m_frame_count = 0;
};
```

## Input Handling

### GPIO Joystick

**Raspberry Pi GPIO:**

```cpp
class GPIO_Buttons {
public:
    bool init() {
        // Open /dev/gpiochip0
        m_chip = gpiod_chip_open("/dev/gpiochip0");

        // Request lines
        m_joystick_up = gpiod_chip_get_line(m_chip, PIN_JOY_UP);
        m_joystick_down = gpiod_chip_get_line(m_chip, PIN_JOY_DOWN);
        m_joystick_left = gpiod_chip_get_line(m_chip, PIN_JOY_LEFT);
        m_joystick_right = gpiod_chip_get_line(m_chip, PIN_JOY_RIGHT);
        m_joystick_center = gpiod_chip_get_line(m_chip, PIN_JOY_CENTER);
        m_button_air_rec = gpiod_chip_get_line(m_chip, PIN_AIR_REC);
        m_button_gs_rec = gpiod_chip_get_line(m_chip, PIN_GS_REC);

        // Configure as inputs with pull-up
        gpiod_line_request_input(m_joystick_up, "joystick");
        // ... other lines ...

        return true;
    }

    void poll() {
        // Read button states
        bool up = gpiod_line_get_value(m_joystick_up) == 0;  // Active low
        bool down = gpiod_line_get_value(m_joystick_down) == 0;
        // ... other buttons ...

        // Map to keyboard events
        if (up && !m_last_up) {
            inject_key(SDLK_UP);
        }
        // ... other mappings ...

        m_last_up = up;
        // ... update other states ...
    }
};
```

### Keyboard/Mouse

**SDL2 Integration:**

```cpp
void handle_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Pass to ImGui
        ImGui_ImplSDL2_ProcessEvent(&event);

        switch (event.type) {
            case SDL_KEYDOWN:
                handle_keydown(event.key.keysym.sym);
                break;

            case SDL_MOUSEBUTTONDOWN:
                handle_mouse_button(event.button);
                break;

            // ... other events ...
        }
    }
}

void handle_keydown(SDL_Keycode key) {
    switch (key) {
        case SDLK_r:
            toggle_air_recording();
            break;

        case SDLK_g:
            toggle_gs_recording();
            break;

        case SDLK_RETURN:
            open_osd_menu();
            break;

        case SDLK_ESCAPE:
            if (menu_open) {
                close_menu();
            } else {
                quit_application();
            }
            break;
    }
}
```

## Configuration Management

### Settings File

**INI Format:**

```ini
[WiFi]
Channel=7
TxPower=45  ; 0-63 (5-63 dBm)

[Camera]
Resolution=6  ; SVGA16 (800x456)
FPS=30
Quality=0  ; Auto

[FEC]
K=6
N=12
MTU=1488

[OSD]
Font=inav.png
Brightness=100

[Recording]
DVR_Path=/home/pi/videos/
DVR_Enabled=false
```

**Loading:**

```cpp
#include "utils/ini.h"

void load_config() {
    ini_t* config = ini_load("config.ini");

    m_wifi_channel = ini_get_int(config, "WiFi", "Channel", 7);
    m_tx_power = ini_get_int(config, "WiFi", "TxPower", 45);

    m_resolution = (Resolution)ini_get_int(config, "Camera", "Resolution", 6);
    m_fps = ini_get_int(config, "Camera", "FPS", 30);

    // ... load other settings ...

    ini_free(config);
}
```

## Performance Monitoring

### FPS Counter

```cpp
class FPS_Counter {
public:
    void frame() {
        m_frame_count++;

        auto now = Clock::now();
        auto elapsed = duration_cast<milliseconds>(now - m_last_time);

        if (elapsed.count() >= 1000) {
            m_fps = m_frame_count * 1000.0 / elapsed.count();
            m_frame_count = 0;
            m_last_time = now;
        }
    }

    float get_fps() const { return m_fps; }

private:
    int m_frame_count = 0;
    float m_fps = 0;
    Clock::time_point m_last_time = Clock::now();
};
```

### Latency Measurement

```cpp
// Air unit sends ping
config_packet.ping = millis() & 0xFF;

// Ground station echoes as pong
video_packet.pong = config_packet.ping;

// Air unit calculates round-trip time
uint32_t rtt = millis() - config_packet.ping;
uint32_t latency = rtt / 2;  // Estimate one-way
```

## See Also

- [01_SYSTEM_OVERVIEW.md](01_SYSTEM_OVERVIEW.md) - System architecture
- [02_PROTOCOL_SPECIFICATION.md](02_PROTOCOL_SPECIFICATION.md) - Protocol details
- [03_VIDEO_PIPELINE.md](03_VIDEO_PIPELINE.md) - Video processing
- [04_FEC_IMPLEMENTATION.md](04_FEC_IMPLEMENTATION.md) - FEC details

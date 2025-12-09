# FPV Protocol Comparison

This directory contains detailed architectural analysis of major open-source FPV
ground station systems and a plan for implementing multi-protocol support in ESP32-FPV-GS.

## Documents

| Document | Description |
|----------|-------------|
| [01-wfb-ng-architecture.md](01-wfb-ng-architecture.md) | WFB-NG (WiFiBroadcast Next Generation) protocol analysis |
| [02-openhd-architecture.md](02-openhd-architecture.md) | OpenHD wifibroadcast library and ecosystem |
| [03-openipc-architecture.md](03-openipc-architecture.md) | OpenIPC FPV with Majestic streamer |
| [04-rubyfpv-architecture.md](04-rubyfpv-architecture.md) | RubyFPV custom protocol and multi-link system |
| [05-esp32-fpv-gs-architecture.md](05-esp32-fpv-gs-architecture.md) | Current ESP32-FPV-GS implementation |
| [06-comparison-matrix.md](06-comparison-matrix.md) | Feature comparison and implementation plan |

## Quick Comparison

| System | Protocol Base | Air Unit | FEC | Encryption | Latency |
|--------|--------------|----------|-----|------------|---------|
| WFB-NG | Custom wifibroadcast | Linux (Pi) | RS | ChaCha20 | 80-120ms |
| OpenHD | WFB-NG derived | Linux (Pi) | RS | ChaCha20 | 80-120ms |
| OpenIPC | WFB-NG | IP Camera | RS | ChaCha20 | 60-100ms |
| RubyFPV | Custom | Linux (Pi) | RS | Yes* | 100-150ms |
| ESP32-FPV-GS | Custom | ESP32 | RS | No | 80-110ms |

*RubyFPV encryption temporarily disabled

## Key Findings

### Protocol Commonalities
- All systems use WiFi monitor mode + packet injection
- All implement Reed-Solomon FEC
- All support bidirectional links (video down, telemetry/RC up)
- IEEE 802.11 data frames with custom headers

### Protocol Differences
- **WFB-NG/OpenHD**: Standard wifibroadcast with encryption, large MTU
- **RubyFPV**: Rich metadata, multi-link, relay support, proprietary
- **ESP32-FPV-GS**: Optimized for embedded, MJPEG, no encryption

### Implementation Priority for ESP32-GS
1. **WFB-NG** - Most widely used, enables OpenIPC compatibility
2. **OpenHD** - Growing ecosystem, similar to WFB-NG
3. **OpenIPC** - Best air unit for HD video
4. **RubyFPV** - Complex protocol, lower priority

## Architecture Diagrams

Each document contains detailed ASCII architecture diagrams showing:
- Data flow from camera to display
- Packet structure layouts
- Protocol stack layers
- Memory allocation

## Related Resources

- [WFB-NG GitHub](https://github.com/svpcom/wfb-ng)
- [OpenHD Website](https://openhdfpv.org/)
- [OpenHD Wifibroadcast](https://github.com/OpenHD/wifibroadcast)
- [OpenIPC](https://openipc.org/)
- [RubyFPV](https://rubyfpv.com/)
- [ESP32-FPV-GS](https://github.com/RomanLut/hx-esp32-cam-fpv)

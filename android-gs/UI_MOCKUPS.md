# Android Ground Station - UI Mockups

Visual mockups and layouts for the HX ESP32 FPV Android Ground Station application.

## Main FPV Screen (Landscape Mode)

### Full View with All OSD Elements
```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ≡ HX ESP32 FPV                           ● Connected        ⚙ Settings  ℹ  │ ← TopBar
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ RSSI: -45 dBm  SNR: 45 dB                                   12.6V (3S) 2.5A │
│ [████░]  Excellent                                          [████░]  75%    │
│                                                                              │
│ Link: 95%  Loss: 0.5%                                  37.7749, -122.4194  │
│ 1280x720 @ 30 FPS  8.5 Mbps                            Alt: 150.5m          │
│ Latency: 95 ms (85-120)                                                     │
│                                                            [●] REC           │
│                                                                              │
│                                                                              │
│                           ┌─────────────────┐                               │
│                           │                 │                               │
│                           │   ─── +20°      │                               │
│                           │     ── +10°     │                               │
│                           │ ══════════ 0°   │  ← Artificial Horizon         │
│                           │     ── -10°     │     (Optional)                │
│                           │   ─── -20°      │                               │
│                           │                 │                               │
│                           │      ─●─        │  ← Aircraft Symbol            │
│                           └─────────────────┘                               │
│                                  │                                           │
│                                  │                                           │
│                              ────┼────  ← Crosshair                          │
│                                  │                                           │
│                                  │                                           │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                        Air Pkt: 850/s       │
│                                                        GS Pkt: 850/s         │
│ Custom Callsign N123AB                                 FEC: 98.5%           │
│                                                        WiFi: Ch7             │
│                                                        Rate: 36M ODFM        │
│                                                        Temp: 45°C            │
│                                                   ●  ← Recording FAB         │
└──────────────────────────────────────────────────────────────────────────────┘
        1920 x 1080 pixels (or device resolution)
```

### Minimalist View (Controls Hidden)
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                     👁 Hide   │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ RSSI: -45 dBm  SNR: 45 dB                                   12.6V (3S) 75%  │
│ [████░]                                                     [████░]          │
│                                                                              │
│ Link: 95%  Loss: 0.5%                                                       │
│ 1280x720 @ 30 FPS  8.5 Mbps                                                 │
│ Latency: 95 ms                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                  │                                           │
│                              ────┼────                                       │
│                                  │                                           │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

### Racing Mode (Minimal OSD)
```
┌──────────────────────────────────────────────────────────────────────────────┐
│                                                                              │
│                                                                              │
│ RSSI: -45 dBm                                                               │
│ [████░]                                                                      │
│                                                                              │
│ 95 ms                                                                        │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                              ────┼────  ← Crosshair only                    │
│                                  │                                           │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Settings Screen

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ← Settings                                                                   │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ OSD Display                                                                  │
│ ──────────────────────────────────────────────────────────────────────────  │
│                                                                              │
│ ● Enable OSD                                                     [ON]  ◀──  │
│   Show on-screen display overlay                                            │
│                                                                              │
│ ● RSSI Indicator                                                 [ON]       │
│   Show signal strength                                                       │
│                                                                              │
│ ● Link Quality                                                   [ON]       │
│   Show link quality and packet loss                                         │
│                                                                              │
│ ● Video Stats                                                    [ON]       │
│   Show FPS and bitrate                                                       │
│                                                                              │
│ ● Latency                                                        [ON]       │
│   Show video latency                                                         │
│                                                                              │
│ ● Battery                                                        [ON]       │
│   Show battery voltage and status                                           │
│                                                                              │
│ ● GPS                                                            [ON]       │
│   Show GPS coordinates                                                       │
│                                                                              │
│ ● Recording Indicator                                            [ON]       │
│   Show recording status                                                      │
│                                                                              │
│ ● Artificial Horizon                                            [OFF]  ◀──  │
│   Show flight attitude indicator                                            │
│                                                                              │
│ ● Crosshair                                                      [ON]       │
│   Show center crosshair                                                      │
│                                                                              │
│ ● Detailed Stats                                                 [ON]       │
│   Show additional statistics                                                │
│                                                                              │
│                                                                              │
│ Appearance                                                                   │
│ ──────────────────────────────────────────────────────────────────────────  │
│                                                                              │
│ ● Font Size                                                      24 px  ◀── │
│   Adjust OSD text size                                                       │
│   [────────●─────────]  ← Slider (16-48px)                                  │
│                                                                              │
│ ● Transparency                                                    100%  ◀── │
│   Adjust OSD transparency                                                    │
│   [─────────────────●]  ← Slider (30-100%)                                  │
│                                                                              │
│                                                                              │
│ Custom Text                                                                  │
│ ──────────────────────────────────────────────────────────────────────────  │
│                                                                              │
│ ● Show Custom Text                                              [OFF]       │
│   Display custom text overlay                                               │
│                                                                              │
│ ┌──────────────────────────────────────────────────────────────────────┐   │
│ │ Enter custom text...                                                  │   │
│ └──────────────────────────────────────────────────────────────────────┘   │
│                                                                              │
│                                                                              │
│ Video                                                                        │
│ ──────────────────────────────────────────────────────────────────────────  │
│                                                                              │
│ ● Scale Mode                                                      Fit   ▼   │
│   How video is displayed on screen                                          │
│                                                                              │
│ ● Rotation                                                         0°   ▼   │
│   Rotate video display                                                       │
│                                                                              │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Statistics Screen

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ ← Statistics                                                                 │
├──────────────────────────────────────────────────────────────────────────────┤
│                                                                              │
│ ┌──────────────────────────────────────────────────────────────────────────┐│
│ │ [≈] Connection                                                           ││
│ │                                                                          ││
│ │ Status:                Connected                                         ││
│ │ Link Quality:          95%                                               ││
│ │ WiFi Channel:          Ch 7                                              ││
│ │ WiFi Rate:             36M ODFM                                          ││
│ │ FEC Codec:             6/12                                              ││
│ └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│ ┌──────────────────────────────────────────────────────────────────────────┐│
│ │ [|] Signal Quality                                                       ││
│ │                                                                          ││
│ │ Air RSSI:              -45 dBm                                           ││
│ │ Air Noise Floor:       -90 dBm                                           ││
│ │ Air SNR:               45 dB                                             ││
│ │ ────────────────────────────────────────────────────────────────────     ││
│ │ GS RSSI 1:             -45 dBm                                           ││
│ │ GS RSSI 2:             -50 dBm                                           ││
│ │ GS Noise Floor:        -90 dBm                                           ││
│ │ GS SNR:                40 dB                                             ││
│ └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│ ┌──────────────────────────────────────────────────────────────────────────┐│
│ │ [▶] Video                                                                ││
│ │                                                                          ││
│ │ Resolution:            1280x720                                          ││
│ │ Capture FPS:           30                                                ││
│ │ Display FPS:           30                                                ││
│ │ Bitrate:               8.50 Mbps                                         ││
│ │ Frame Size:            8000-15000 bytes                                  ││
│ │ Frames Decoded:        1500                                              ││
│ │ Frames Dropped:        5                                                 ││
│ │ Drop Rate:             0.33%                                             ││
│ └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│ ┌──────────────────────────────────────────────────────────────────────────┐│
│ │ Latency                                                                  ││
│ │                                                                          ││
│ │ Current:               95 ms                                             ││
│ │ Min Ping:              85 ms                                             ││
│ │ Max Ping:              120 ms                                            ││
│ │ Avg Frame Time:        33.3 ms                                           ││
│ └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│ ┌──────────────────────────────────────────────────────────────────────────┐│
│ │ Packet Statistics                                                        ││
│ │                                                                          ││
│ │ Air Out Rate:          850 pkt/s                                         ││
│ │ Air In Rate:           50 pkt/s                                          ││
│ │ Air Rejected:          2 pkt/s                                           ││
│ │ ────────────────────────────────────────────────────────────────────     ││
│ │ GS Out Rate:           50 pkt/s                                          ││
│ │ GS In Rate:            850 pkt/s                                         ││
│ │ GS Unique:             840 pkt/s                                         ││
│ │ GS Duplicated:         10 pkt/s                                          ││
│ │ Packet Loss:           0.5%                                              ││
│ │ FEC Success:           98.5%                                             ││
│ └──────────────────────────────────────────────────────────────────────────┘│
│                                                                              │
│ [Scroll for more statistics...]                                             │
└──────────────────────────────────────────────────────────────────────────────┘
```

## OSD Element Details

### RSSI Indicator (Expanded)
```
┌─────────────────────────────┐
│ RSSI: -45 dBm  SNR: 45 dB   │  ← Text display
│ [████░]  Excellent Signal   │  ← 5 signal bars
└─────────────────────────────┘
     ^^^^^
     Color coded:
     Green  : < 30 dBm (Excellent)
     Yellow : 30-60 dBm (Good)
     Orange : 60-80 dBm (Fair)
     Red    : > 80 dBm (Poor)
```

### Battery Display (Expanded)
```
┌──────────────────────────┐
│ 12.6V (3S) 2.5A  75%     │  ← Voltage, cells, current, %
│ ┌────────────┬┐          │
│ │████████░░░░││  ← Icon  │  ← Battery icon with fill
│ └────────────┴┘          │
└──────────────────────────┘
  ^^^^^^^^
  Color coded:
  Green  : > 3.7V per cell
  Yellow : 3.5-3.7V per cell
  Red    : < 3.5V per cell (Warning!)
```

### Artificial Horizon (Detail)
```
┌───────────────────────┐
│        ──── +20°      │  ← Pitch ladder
│          ── +10°      │
│  SKY (Blue tint)      │
│                       │
│ ═══════════════  0°   │  ← Horizon line (White)
│                       │
│ GROUND (Brown tint)   │
│          ── -10°      │  ← Pitch ladder
│        ──── -20°      │
│                       │
│         ─●─           │  ← Aircraft symbol (Yellow)
│        ─   ─          │
└───────────────────────┘
  Roll indicator: Entire view rotates
```

### Link Quality Indicator
```
┌──────────────────────────────┐
│ Link: 95%  Loss: 0.5%        │
│ Quality: ████████░  Excellent│
└──────────────────────────────┘
   Color coded bar:
   Green  : > 80% (Excellent)
   Yellow : 60-80% (Good)
   Orange : 40-60% (Fair)
   Red    : < 40% (Poor)
```

### Recording Indicator (Animation)
```
Frame 1:  [●] REC  ← Red, visible
Frame 2:  [ ] REC  ← Hidden
Frame 3:  [●] REC  ← Red, visible
Frame 4:  [ ] REC  ← Hidden
...
(Blinks at 2Hz / 500ms interval)
```

## Color Palette

### Signal Quality Colors
```
Excellent (Green):   #00FF00  ████
Good (Yellow):       #FFFF00  ████
Fair (Orange):       #FFA500  ████
Poor (Red):          #FF0000  ████
```

### UI Colors
```
Background:          #000000  ████  (Black)
Text:                #FFFFFF  ████  (White)
Primary:             #00FF00  ████  (Green)
Secondary:           #00BFFF  ████  (Blue)
Surface:             #121212  ████  (Dark Gray)
Error:               #FF0000  ████  (Red)
```

### OSD Overlay
```
Text:                #FFFFFF  with shadow #000000
Background:          #80000000 (50% transparent black)
Warning:             #FFFF00
Critical:            #FF0000
```

## Responsive Layouts

### Tablet (10" landscape)
```
Same layout as phone but with:
- Larger font sizes (1.5x)
- More spacing between elements
- Bigger touch targets (56dp minimum)
```

### Phone (6" landscape)
```
Standard layout as shown in mockups above
```

### Small Phone (5" landscape)
```
Reduced layout:
- Smaller font sizes (0.8x)
- Compact OSD elements
- Condensed statistics
```

## Touch Interaction Zones

```
┌──────────────────────────────────────────────────────────────────────────────┐
│ [Top Bar Touch Zone - Show/Hide Menu]                                       │
├──────────────────────────────────────────────────────────────────────────────┤
│ [Left Touch]                                              [Right Touch]     │
│  Cycle OSD                                                Cycle stats       │
│  presets                                                  panels            │
│                                                                              │
│                         [Center Touch Zone]                                 │
│                         Show/Hide All Controls                              │
│                         (Double tap = Toggle fullscreen)                    │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                                              │
│                                                   [FAB Touch]                │
│                                                   Recording Toggle           │
└──────────────────────────────────────────────────────────────────────────────┘
```

## Animation Details

### Screen Transitions
```
Main <──> Settings:  Slide left/right (300ms)
Main <──> Stats:     Slide up/down (300ms)
FAB expand/collapse: Scale + fade (200ms)
OSD show/hide:       Fade in/out (150ms)
```

### OSD Updates
```
RSSI bars:           Smooth transition (100ms)
Battery fill:        Smooth fill (200ms)
Text values:         Fade between values (50ms)
Recording indicator: Blink (500ms on, 500ms off)
```

## Accessibility

### Large Text Mode
```
All OSD elements scale with system font size
Minimum touch target: 48dp
High contrast mode available
```

### Color Blind Modes
```
Protanopia:  Red/Green → Blue/Yellow
Deuteranopia: Red/Green → Blue/Brown
Tritanopia:  Blue/Yellow → Red/Magenta
```

---

**Note**: These mockups represent the implemented design. Actual rendering may vary slightly based on device screen size and Android version.

**Created**: 2025-11-22
**For**: HX ESP32 FPV Android Ground Station v1.0.0

# WT32-SC01 PC Hardware Monitor — ESP32 Touchscreen System Monitor

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange.svg)](https://platformio.org/)
[![ESP32](https://img.shields.io/badge/ESP32-WT32--SC01-blue.svg)](https://www.wireless-tag.com/product/wt32-sc01)

> Real-time PC hardware monitoring on a WT32-SC01 ESP32 touchscreen display. An open-source alternative to AIDA64 sensor panels — using LibreHardwareMonitor, Python and a 480×320 TFT with touch navigation.

Monitor your **CPU temperature, GPU load, RAM usage, disk health, network speed and fan RPM** in real time on a standalone ESP32 touchscreen. Connects via USB serial to any Windows PC running [LibreHardwareMonitor](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor).

**Perfect for:** PC enthusiasts, overclockers, streamers, workstation monitoring, homelab dashboards, and anyone who wants a dedicated hardware stats display.

## Why This Project?

Most PC hardware monitoring solutions either require a second monitor or rely on expensive proprietary sensor panels. This project turns a cheap (~$15) WT32-SC01 ESP32 board into a fully featured system monitor — no WiFi needed, just USB.

## Features

- **CPU Monitoring**: Load (%), temperature, clock speed, power, voltage, history graph, per-core display
- **GPU Monitoring**: Load (%), temperature, VRAM, core/mem clock, power, hot spot, fan RPM, history graph
- **RAM Monitoring**: Usage (%), used/total/free in GB, visual block, history graph
- **Storage Monitoring**: Total capacity across all drives (TB), per-drive details with size and temperature
- **Network Monitoring**: Download/upload speed (KB/s to GB/s), auto-scaled history graphs
- **Fan Speed Display**: System fans (motherboard) + GPU fan with RPM bars
- **Disk Temperature Display**: Color-coded per drive (green/yellow/red)
- **Touch Navigation**: Tap any area for a detail view, back button top-left — 7 detail screens
- **Anti-Flicker Rendering**: Direct LCD updates without full-screen redraws
- **Auto-Reconnect**: Detects USB connection loss (5s timeout), Python script survives port changes
- **Color-Coded Temperatures**: Green (<60°C) → Yellow (60–80°C) → Red (>80°C)
- **Standby Clock Screen**: When disconnected, displays time, date and disconnect duration — minimalist Apple-style design with dimmed brightness
- **Time Sync**: PC sends Unix timestamp + timezone offset, ESP32 keeps the clock running independently

## Architecture

```
Windows PC                          WT32-SC01 (ESP32)
┌────────────────┐    USB Serial     ┌────────────────────┐
│ LibreHardware  │    115200 Baud    │                    │
│ Monitor        │──── JSON/line ──→ │  480×320 TFT       │
│ (HTTP :8085)   │    2× per sec     │  Touch Dashboard   │
│                │                    │                    │
│ pc_monitor.py  │                    │  LovyanGFX +       │
│ (Python 3)     │                    │  ArduinoJson v7    │
└────────────────┘                    └────────────────────┘
```

## Hardware

| Component  | Details                                             |
|------------|-----------------------------------------------------|
| Board      | WT32-SC01 (ESP32-D0WD, 4 MB Flash, 320 KB RAM)     |
| Display    | ST7796S 480×320 TFT (SPI)                           |
| Touch      | FT5x06 capacitive (I2C)                             |
| Connection | USB (CP210x / CH340)                                |
| PSRAM      | Not available — rendering directly to LCD            |

## Quick Start

### 1. Flash ESP32

```bash
# PlatformIO CLI
pio run -t upload
```

### 2. Set Up Windows Side

```bash
# Start LibreHardwareMonitor as administrator, enable web server
# Install Python dependencies
cd windows
pip install -r requirements.txt

# Start the script
python pc_monitor.py
```

Detailed instructions: [docs/SETUP.md](docs/SETUP.md)

## Display Layout (480×320)

### Main Screen

```
┌───────────────────────┬───────────────────────┐
│  CPU  Ryzen 9 9950X   │  GPU  RTX 5090        │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓░░ 45% │  ▓▓▓▓▓▓▓░░░░░░░░ 30% │
│  62°C  5800 MHz 170 W │  48°C  16384/32768 MB │
│  [History graph 60s]  │  [History graph 60s]  │
├───────────────────────┴───────────────────────┤
│  RAM  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░ 58%  74/128 GB    │
├───────────────────────────────────────────────┤
│  DSK  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░ 72%  5.8/8.0 TB  │
├───────────────────────────────────────────────┤
│  NET  DL: 125 KB/s             UL: 42 KB/s   │
├───────────────────────────────────────────────┤
│  FAN1: 1100 RPM  FAN2: 850 RPM              │
│  990PRO  35°C   T700    38°C                 │
│  T500    32°C   SN850X  36°C                 │
└───────────────────────────────────────────────┘
```

### Standby Screen (on disconnect)

```
┌───────────────────────────────────────────────┐
│                                               │
│                  14:32                         │
│                                               │
│           Do, 12. Maerz 2026                  │
│                                               │
│                                               │
│  ...                                   5 min  │
└───────────────────────────────────────────────┘
```

### Touch Zones

| Tap Area                  | Opens Detail View |
|---------------------------|-------------------|
| CPU (top-left)            | CPU Detail        |
| GPU (top-right)           | GPU Detail        |
| RAM row                   | RAM Detail        |
| DSK row                   | Disk Detail       |
| NET row                   | Network Detail    |
| Bottom-left (Fans)        | Fan Detail        |
| Bottom-right (Disk Temps) | Disk Detail       |

## Project Structure

```
wt32-hw-monitor/
├── platformio.ini          PlatformIO configuration
├── partitions.csv          ESP32 partition table
├── src/
│   ├── main.cpp            Setup + loop, serial receive
│   ├── display.cpp         Dashboard rendering (LovyanGFX)
│   ├── display.h           Data structures, screen states
│   ├── parser.cpp          JSON parsing (ArduinoJson v7)
│   ├── parser.h            Parser interface
│   └── config.h            Pins, colors, layout constants
├── windows/
│   ├── pc_monitor.py       Python: LHM HTTP → serial JSON
│   ├── start_monitor.bat   Autostart script
│   ├── requirements.txt    Python dependencies
│   └── README.md           Windows setup guide
├── docs/
│   ├── PROTOCOL.md         Serial protocol documentation
│   ├── SETUP.md            Detailed installation guide
│   └── TROUBLESHOOTING.md  Troubleshooting guide
├── CHANGELOG.md            Version history
├── LICENSE                 MIT License
└── README.md               This file
```

## Technology

| Area            | Technology                                        |
|-----------------|---------------------------------------------------|
| ESP32 Framework | Arduino (via PlatformIO)                          |
| Display         | LovyanGFX (direct LCD drawing, no sprite buffer)  |
| JSON Parser     | ArduinoJson v7                                    |
| Touch           | FT5x06 via LovyanGFX (400 ms debounce)           |
| Windows         | Python 3 + LibreHardwareMonitor (open source)     |
| Protocol        | Compact JSON over USB serial, 115200 baud, 2 Hz  |

## Keywords

ESP32 hardware monitor, WT32-SC01, PC system monitor, CPU temperature display, GPU monitoring, LibreHardwareMonitor, PlatformIO, touchscreen dashboard, real-time system stats, Arduino ESP32, serial hardware monitor, AIDA64 alternative, sensor panel DIY

## License

MIT — see [LICENSE](LICENSE)

---

**Found this useful?** Give it a star and share it with the PC modding community!

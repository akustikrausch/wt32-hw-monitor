# PC Hardware Monitor für WT32-SC01

Echtzeit-PC-Hardware-Monitor auf dem WT32-SC01 ESP32-Board mit 480×320 TFT-Touchscreen.
Zeigt CPU, GPU, RAM, Speicher, Netzwerk, Lüfter und Temperaturen — mit Touch-Navigation zu 7 Detail-Ansichten.

---

**[English version below](#english)**

---

## Features

- **CPU**: Auslastung (%), Temperatur, Taktfrequenz, Leistung, Spannung, Verlaufsgraph, Pro-Kern-Anzeige
- **GPU**: Auslastung (%), Temperatur, VRAM, Core/Mem-Takt, Leistung, Hot Spot, Lüfter-RPM, Verlaufsgraph
- **RAM**: Belegung (%), Used/Total/Free in GB, visueller Block, Verlaufsgraph
- **Speicher**: Gesamtkapazität über alle Laufwerke (TB), pro-Laufwerk-Details mit Größe und Temperatur
- **Netzwerk**: Download/Upload-Geschwindigkeit (KB/s bis GB/s), auto-skalierte Verlaufsgraphen
- **Lüfter**: System-Lüfter (Mainboard) + GPU-Lüfter mit RPM-Balken
- **Disk-Temperaturen**: Farbkodiert pro Laufwerk (Grün/Gelb/Rot)
- **Touch-Navigation**: Tippe auf jeden Bereich für eine Detail-Ansicht, Zurück-Button oben links
- **Anti-Flicker**: Direkte LCD-Updates ohne Vollbild-Neuzeichnung
- **Auto-Reconnect**: Erkennt USB-Verbindungsverlust (5s Timeout)
- **Farbkodierung**: Grün (<60 C) → Gelb (60–80 C) → Rot (>80 C)

## Architektur

```
Windows PC                          WT32-SC01 (ESP32)
┌────────────────┐    USB Serial     ┌────────────────────┐
│ LibreHardware  │    115200 Baud    │                    │
│ Monitor        │──── JSON/line ──→ │  480×320 TFT       │
│ (HTTP :8085)   │    2× pro Sek.    │  Touch-Dashboard   │
│                │                    │                    │
│ pc_monitor.py  │                    │  LovyanGFX +       │
│ (Python 3)     │                    │  ArduinoJson v7    │
└────────────────┘                    └────────────────────┘
```

## Hardware

| Komponente   | Details                                           |
|-------------|---------------------------------------------------|
| Board       | WT32-SC01 (ESP32-D0WD, 4 MB Flash, 320 KB RAM)    |
| Display     | ST7796S 480×320 TFT (SPI)                          |
| Touch       | FT5x06 kapazitiv (I2C)                              |
| Verbindung  | USB (CP210x / CH340)                                |
| PSRAM       | Nicht vorhanden — Rendering direkt auf LCD          |

## Schnellstart

### 1. ESP32 flashen

```bash
# PlatformIO CLI
pio run -t upload
```

### 2. Windows-Seite einrichten

```bash
# LibreHardwareMonitor als Administrator starten, Web Server aktivieren
# Python-Abhängigkeiten installieren
cd windows
pip install -r requirements.txt

# Script starten
python pc_monitor.py
```

Detaillierte Anleitung: [windows/README.md](windows/README.md)

## Display-Layout (480×320)

### Hauptbildschirm

```
┌───────────────────────┬───────────────────────┐
│  CPU  Ryzen 9 5950X   │  GPU  RTX 2060        │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓░░ 45% │  ▓▓▓▓▓▓▓░░░░░░░░ 30% │
│  68 C  4283 MHz 137 W │  55 C  2048/6144 MB   │
│  [Verlaufsgraph 60s]  │  [Verlaufsgraph 60s]  │
├───────────────────────┴───────────────────────┤
│  RAM  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░ 62%  40/64 GB     │
├───────────────────────────────────────────────┤
│  DSK  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░ 79%  92/117 TB    │
├───────────────────────────────────────────────┤
│  NET  DL: 31 KB/s              UL: 5 KB/s     │
├───────────────────────────────────────────────┤
│  FAN1: 1200 RPM  FAN2: 980 RPM               │
│  980PRO  38 C    870EVO  42 C                 │
│  WD10T   35 C    ST24T   40 C                 │
└───────────────────────────────────────────────┘
```

### Touch-Zonen

| Bereich tippen            | Öffnet Detail-Ansicht |
|--------------------------|----------------------|
| CPU (links oben)          | CPU-Detail            |
| GPU (rechts oben)         | GPU-Detail            |
| RAM-Zeile                 | RAM-Detail            |
| DSK-Zeile                 | Disk-Detail           |
| NET-Zeile                 | Netzwerk-Detail       |
| Unten links (Fans)        | Lüfter-Detail         |
| Unten rechts (Disk Temps) | Disk-Detail           |

## Projektstruktur

```
pc-monitor/
├── platformio.ini          PlatformIO-Konfiguration
├── partitions.csv          ESP32-Partitionstabelle
├── src/
│   ├── main.cpp            Setup + Loop, Serial-Empfang
│   ├── display.cpp         Dashboard-Rendering (LovyanGFX)
│   ├── display.h           Datenstrukturen, Screen-States
│   ├── parser.cpp          JSON-Parsing (ArduinoJson v7)
│   ├── parser.h            Parser-Interface
│   └── config.h            Pins, Farben, Layout-Konstanten
├── windows/
│   ├── pc_monitor.py       Python: LHM HTTP → Serial JSON
│   ├── start_monitor.bat   Autostart-Script
│   ├── requirements.txt    Python-Abhängigkeiten
│   └── README.md           Windows-Setup-Anleitung
├── docs/
│   ├── PROTOCOL.md         Serial-Protokoll-Dokumentation
│   ├── SETUP.md            Ausführliche Installationsanleitung
│   └── TROUBLESHOOTING.md  Fehlerbehebung
├── CHANGELOG.md            Versionshistorie
├── LICENSE                 MIT-Lizenz
└── README.md               Diese Datei
```

## Technologie

| Bereich         | Technologie                                              |
|----------------|----------------------------------------------------------|
| ESP32 Framework | Arduino (via PlatformIO)                                 |
| Display         | LovyanGFX (direkte LCD-Zeichnung, kein Sprite-Buffer)   |
| JSON-Parser     | ArduinoJson v7                                           |
| Touch           | FT5x06 via LovyanGFX (400 ms Debounce)                  |
| Windows         | Python 3 + LibreHardwareMonitor (Open Source)            |
| Protokoll       | Kompaktes JSON über USB Serial, 115200 Baud, 2 Hz       |

## Lizenz

MIT — siehe [LICENSE](LICENSE)

---

<a id="english"></a>

# PC Hardware Monitor for WT32-SC01

Real-time PC hardware monitor on the WT32-SC01 ESP32 board with a 480×320 TFT touchscreen.
Displays CPU, GPU, RAM, storage, network, fans and temperatures — with touch navigation to 7 detail views.

## Features

- **CPU**: Load (%), temperature, clock speed, power, voltage, history graph, per-core display
- **GPU**: Load (%), temperature, VRAM, core/mem clock, power, hot spot, fan RPM, history graph
- **RAM**: Usage (%), used/total/free in GB, visual block, history graph
- **Storage**: Total capacity across all drives (TB), per-drive details with size and temperature
- **Network**: Download/upload speed (KB/s to GB/s), auto-scaled history graphs
- **Fans**: System fans (motherboard) + GPU fan with RPM bars
- **Disk Temperatures**: Color-coded per drive (green/yellow/red)
- **Touch Navigation**: Tap any area for a detail view, back button top-left
- **Anti-Flicker**: Direct LCD updates without full-screen redraws
- **Auto-Reconnect**: Detects USB connection loss (5s timeout)
- **Color Coding**: Green (<60 C) → Yellow (60–80 C) → Red (>80 C)

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

| Component   | Details                                            |
|------------|-----------------------------------------------------|
| Board      | WT32-SC01 (ESP32-D0WD, 4 MB Flash, 320 KB RAM)      |
| Display    | ST7796S 480×320 TFT (SPI)                            |
| Touch      | FT5x06 capacitive (I2C)                              |
| Connection | USB (CP210x / CH340)                                 |
| PSRAM      | Not available — rendering directly to LCD             |

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

Detailed instructions: [windows/README.md](windows/README.md)

## Display Layout (480×320)

### Main Screen

```
┌───────────────────────┬───────────────────────┐
│  CPU  Ryzen 9 5950X   │  GPU  RTX 2060        │
│  ▓▓▓▓▓▓▓▓▓▓▓▓▓░░ 45% │  ▓▓▓▓▓▓▓░░░░░░░░ 30% │
│  68 C  4283 MHz 137 W │  55 C  2048/6144 MB   │
│  [History graph 60s]  │  [History graph 60s]  │
├───────────────────────┴───────────────────────┤
│  RAM  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░░ 62%  40/64 GB     │
├───────────────────────────────────────────────┤
│  DSK  ▓▓▓▓▓▓▓▓▓▓▓▓▓▓▓░░░░ 79%  92/117 TB    │
├───────────────────────────────────────────────┤
│  NET  DL: 31 KB/s              UL: 5 KB/s     │
├───────────────────────────────────────────────┤
│  FAN1: 1200 RPM  FAN2: 980 RPM               │
│  980PRO  38 C    870EVO  42 C                 │
│  WD10T   35 C    ST24T   40 C                 │
└───────────────────────────────────────────────┘
```

### Touch Zones

| Tap Area                   | Opens Detail View   |
|---------------------------|---------------------|
| CPU (top-left)             | CPU Detail           |
| GPU (top-right)            | GPU Detail           |
| RAM row                    | RAM Detail           |
| DSK row                    | Disk Detail          |
| NET row                    | Network Detail       |
| Bottom-left (Fans)         | Fan Detail           |
| Bottom-right (Disk Temps)  | Disk Detail          |

## Project Structure

```
pc-monitor/
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

| Area           | Technology                                            |
|---------------|-------------------------------------------------------|
| ESP32 Framework | Arduino (via PlatformIO)                              |
| Display        | LovyanGFX (direct LCD drawing, no sprite buffer)      |
| JSON Parser    | ArduinoJson v7                                        |
| Touch          | FT5x06 via LovyanGFX (400 ms debounce)               |
| Windows        | Python 3 + LibreHardwareMonitor (open source)         |
| Protocol       | Compact JSON over USB serial, 115200 baud, 2 Hz       |

## License

MIT — see [LICENSE](LICENSE)

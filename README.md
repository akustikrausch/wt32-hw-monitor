# PC Hardware Monitor for WT32-SC01

Ein Echtzeit-PC-Hardware-Monitor auf dem WT32-SC01 ESP32-Board mit 480×320 TFT-Display.

## Features

- **CPU**: Auslastung (%), Temperatur, Verlaufsgraph
- **GPU**: Auslastung (%), Temperatur, Verlaufsgraph
- **RAM**: Belegung (%), Used/Total in GB
- **Fans**: Bis zu 4 Lüfter mit RPM-Anzeige
- **Farbkodierung**: Grün (<60°C) → Gelb (60-80°C) → Rot (>80°C)
- **Auto-Reconnect**: Erkennt USB-Verbindungsverlust

## Architektur

```
Windows PC                          WT32-SC01 (ESP32)
┌──────────────┐    USB Serial     ┌──────────────────┐
│ LibreHardware│    115200 Baud    │                  │
│ Monitor      │──→ JSON/line ──→  │  480×320 TFT     │
│ (HTTP :8085) │    2× pro Sek.    │  Dashboard       │
│              │                    │                  │
│ pc_monitor.py│                    │  LovyanGFX +     │
│ (Python)     │                    │  ArduinoJson     │
└──────────────┘                    └──────────────────┘
```

## Hardware

- **Board**: WT32-SC01 (ESP32-D0WD, 4MB Flash, 320KB RAM)
- **Display**: ST7796S 480×320 TFT (SPI)
- **Touch**: FT5x06 (I2C) - für zukünftige Erweiterungen
- **Verbindung**: USB (CP210x, COM3)

## Schnellstart

### 1. ESP32 flashen
```bash
# PlatformIO CLI
pio run -t upload
```

### 2. Windows-Seite einrichten
```bash
# LibreHardwareMonitor als Admin starten, Web Server aktivieren
# Python-Abhängigkeiten installieren
cd windows
pip install -r requirements.txt

# Script starten
python pc_monitor.py
```

Detaillierte Anleitung: [windows/README.md](windows/README.md)

## Serial-Protokoll

Kompaktes JSON, 2× pro Sekunde, eine Zeile:
```json
{"cpu":45.2,"gpuload":30.0,"cputemp":68.0,"gputemp":55.0,"ram":62.0,"ramused":19.8,"ramtotal":32.0,"fan1":1200,"fan2":980,"fan3":-1,"fan4":-1,"cpuname":"i7-12700K","gpuname":"RTX 3070"}
```

## Display-Layout

```
┌─────────────────────────────────────────────┐
│  PC HARDWARE MONITOR                    [●] │
├──────────────────────┬──────────────────────┤
│  CPU  i7-12700K      │  GPU  RTX 3070       │
│  ██████████░░ 45%    │  ██████░░░░░░ 30%    │
│  Temp: 68°C          │  Temp: 55°C          │
│  [Verlaufsgraph]     │  [Verlaufsgraph]     │
├──────────────────────┴──────────────────────┤
│  RAM  ████████████████░░░░ 62%  19.8/32 GB  │
├─────────────────────────────────────────────┤
│  FAN1: 1200 RPM        FAN2: 980 RPM        │
│  FAN3: ---             FAN4: ---             │
└─────────────────────────────────────────────┘
```

## Projektstruktur

```
pc-monitor/
├── platformio.ini          PlatformIO-Konfiguration
├── partitions.csv          Partitionstabelle
├── src/
│   ├── main.cpp            Setup + Loop
│   ├── display.cpp/h       Dashboard-Rendering (LovyanGFX)
│   ├── parser.cpp/h        JSON-Parsing (ArduinoJson)
│   └── config.h            Pins, Farben, Layout
├── windows/
│   ├── pc_monitor.py       Python: LHM → Serial
│   ├── requirements.txt    Python-Abhängigkeiten
│   └── README.md           Windows-Setup-Anleitung
└── README.md               Diese Datei
```

## Technologie

- **ESP32 Framework**: Arduino (PlatformIO)
- **Display-Bibliothek**: LovyanGFX (Double-Buffered Sprites)
- **JSON-Parser**: ArduinoJson v7
- **Windows**: Python 3 + LibreHardwareMonitor (Open Source)

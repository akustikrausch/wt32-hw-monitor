# PC Hardware Monitor for WT32-SC01

Ein Echtzeit-PC-Hardware-Monitor auf dem WT32-SC01 ESP32-Board mit 480x320 TFT-Display.

## Features

- **CPU**: Auslastung (%), Temperatur, Taktfrequenz (MHz), Leistung (W), Verlaufsgraph
- **GPU**: Auslastung (%), Temperatur, VRAM-Belegung (MB), Verlaufsgraph
- **RAM**: Belegung (%), Used/Total in GB
- **Storage**: Gesamtspeicher in TB (Used/Free/Total) uber alle Laufwerke
- **Disk-Temperaturen**: Individuelle SSD/HDD-Temperaturen mit farbkodierter Anzeige
- **Fans**: 2 Lufter mit RPM-Anzeige
- **Farbkodierung**: Grun (<60 C) -> Gelb (60-80 C) -> Rot (>80 C)
- **Anti-Flicker**: Direkte LCD-Updates ohne Vollbild-Neuzeichnung
- **Auto-Reconnect**: Erkennt USB-Verbindungsverlust (5s Timeout)

## Architektur

```
Windows PC                          WT32-SC01 (ESP32)
+----------------+    USB Serial     +--------------------+
| LibreHardware  |    115200 Baud    |                    |
| Monitor        |-> JSON/line ->    |  480x320 TFT       |
| (HTTP :8085)   |    2x pro Sek.    |  Dashboard          |
|                |                    |                    |
| pc_monitor.py  |                    |  LovyanGFX +       |
| (Python)       |                    |  ArduinoJson       |
+----------------+                    +--------------------+
```

## Hardware

- **Board**: WT32-SC01 (ESP32-D0WD, 4MB Flash, 320KB RAM, kein PSRAM)
- **Display**: ST7796S 480x320 TFT (SPI)
- **Touch**: FT5x06 (I2C) - fur zukunftige Erweiterungen
- **Verbindung**: USB (CP210x)

## Schnellstart

### 1. ESP32 flashen
```bash
# PlatformIO CLI
pio run -t upload
```

### 2. Windows-Seite einrichten
```bash
# LibreHardwareMonitor als Admin starten, Web Server aktivieren
# Python-Abhangigkeiten installieren
cd windows
pip install -r requirements.txt

# Script starten
python pc_monitor.py
```

Detaillierte Anleitung: [windows/README.md](windows/README.md)

## Serial-Protokoll

Kompaktes JSON, 2x pro Sekunde, eine Zeile:
```json
{"cpu":45.2,"gpuload":30.0,"cputemp":68.0,"gputemp":55.0,"cpuclk":4283,"cpupwr":137,"gpuvram":2048,"gpuvtot":6144,"ram":62.0,"ramused":19.8,"ramtotal":64.0,"fan1":1200,"fan2":980,"stotal":116.5,"sused":92.3,"sfree":24.2,"dtemp":[38,42,35],"dname":["980PRO","870EVO","WD10T"],"cpuname":"Ryzen 9 5950X","gpuname":"RTX 2060"}
```

### Felder

| Feld     | Typ      | Beschreibung                      |
|----------|----------|-----------------------------------|
| cpu      | float    | CPU Auslastung %                  |
| gpuload  | float    | GPU Auslastung %                  |
| cputemp  | float    | CPU Temperatur C                  |
| gputemp  | float    | GPU Temperatur C                  |
| cpuclk   | float    | CPU Durchschnittstakt MHz         |
| cpupwr   | float    | CPU Package Power W               |
| gpuvram  | float    | GPU VRAM belegt MB                |
| gpuvtot  | float    | GPU VRAM gesamt MB                |
| ram      | float    | RAM Auslastung %                  |
| ramused  | float    | RAM belegt GB                     |
| ramtotal | float    | RAM gesamt GB                     |
| fan1/2   | int      | Lufter RPM (-1 = nicht vorhanden) |
| stotal   | float    | Speicher gesamt TB                |
| sused    | float    | Speicher belegt TB                |
| sfree    | float    | Speicher frei TB                  |
| dtemp    | int[]    | Disk-Temperaturen C               |
| dname    | string[] | Disk-Kurznamen                    |
| cpuname  | string   | CPU Modellname                    |
| gpuname  | string   | GPU Modellname                    |

## Display-Layout (480x320)

```
+------------------------+------------------------+
|  CPU  Ryzen 9 5950X    |  GPU  RTX 2060         |
|  =================== 45%|  ============== 30%    |
|  68C  4283MHz  137W    |  55C  2048/6144MB      |
|  [Verlaufsgraph 60s]   |  [Verlaufsgraph 60s]   |
+------------------------+------------------------+
|  RAM  ====================== 62%  40/64GB        |
+-------------------------------------------------+
|  DSK  ====================== 79%  92/117TB       |
+-------------------------------------------------+
|  FAN1:1200RPM FAN2:980RPM  980PRO:38C 870EVO:42C|
+-------------------------------------------------+
```

## Projektstruktur

```
pc-monitor/
+-- platformio.ini          PlatformIO-Konfiguration
+-- partitions.csv          Partitionstabelle
+-- src/
|   +-- main.cpp            Setup + Loop, Serial-Empfang
|   +-- display.cpp/h       Dashboard-Rendering (LovyanGFX, direkt auf LCD)
|   +-- parser.cpp/h        JSON-Parsing (ArduinoJson v7)
|   +-- config.h            Pins, Farben, Layout-Konstanten
+-- windows/
|   +-- pc_monitor.py       Python: LHM HTTP-API -> Serial JSON
|   +-- requirements.txt    Python-Abhangigkeiten
|   +-- README.md           Windows-Setup-Anleitung
+-- README.md               Diese Datei
```

## Technologie

- **ESP32 Framework**: Arduino (PlatformIO)
- **Display-Bibliothek**: LovyanGFX (direkte LCD-Zeichnung, kein Sprite-Buffer wegen fehlendem PSRAM)
- **JSON-Parser**: ArduinoJson v7
- **Windows**: Python 3 + LibreHardwareMonitor (Open Source)

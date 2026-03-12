# Installationsanleitung / Installation Guide

Schritt-für-Schritt-Anleitung zur Einrichtung des PC Hardware Monitors.

---

**[English version below](#english)**

---

## Übersicht

Das System besteht aus zwei Teilen:

1. **ESP32-Firmware** (WT32-SC01) — zeigt die Daten an
2. **Python-Script** (Windows) — sammelt die Daten und sendet sie per USB

## Teil 1: ESP32-Firmware flashen

### Voraussetzungen

- [PlatformIO](https://platformio.org/) installiert (als CLI oder VS Code Extension)
- WT32-SC01 per USB verbunden
- CP210x-Treiber installiert (wird meist automatisch erkannt)

### Schritte

1. Repository klonen oder herunterladen:

```bash
git clone <repository-url>
cd pc-monitor
```

2. Firmware kompilieren und flashen:

```bash
pio run -t upload
```

3. Nach erfolgreichem Upload zeigt das Display "Waiting for PC..."

### PlatformIO-Konfiguration

Die `platformio.ini` ist bereits vorkonfiguriert:

- Board: `esp-wrover-kit` (kompatibel mit WT32-SC01)
- Framework: Arduino
- Flash-Modus: QIO, 80 MHz
- Bibliotheken: LovyanGFX, ArduinoJson v7

### Speicherverbrauch

- Flash: ~20 % (von 4 MB)
- RAM: ~9 % (von 320 KB)
- Kein PSRAM nötig

## Teil 2: Windows-Seite einrichten

### 2.1 LibreHardwareMonitor installieren

**Option A — Download:**

https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases

**Option B — winget:**

```bash
winget install LibreHardwareMonitor.LibreHardwareMonitor
```

**Konfiguration:**

1. LibreHardwareMonitor als **Administrator** starten
2. Menü: Options → Remote Web Server → **Run**
3. Prüfen: http://localhost:8085/data.json sollte JSON-Daten anzeigen

### 2.2 Python einrichten

1. Python 3 installieren: https://www.python.org/downloads/
2. Abhängigkeiten installieren:

```bash
cd windows
pip install -r requirements.txt
```

### 2.3 Script starten

```bash
python pc_monitor.py
```

Das Script erkennt den ESP32 automatisch am CP210x/CH340-Treiber. Falls nicht:

```bash
python pc_monitor.py --port COM3
```

### 2.4 Autostart einrichten (optional)

1. `Win+R` → `shell:startup` → Enter
2. Die Datei `start_monitor.bat` (im `windows/`-Ordner) als Verknüpfung dorthin kopieren
3. LibreHardwareMonitor ebenfalls auf Autostart stellen

## Verifikation

Nach erfolgreicher Einrichtung:

1. LibreHardwareMonitor läuft (als Admin, Web Server aktiv)
2. Python-Script zeigt Statuszeile in der Konsole
3. ESP32-Display zeigt Echtzeit-Daten
4. Grüner Punkt oben rechts = Verbindung aktiv
5. Tippe auf CPU/GPU/RAM/etc. für Detail-Ansichten

---

<a id="english"></a>

# Installation Guide

Step-by-step guide for setting up the PC Hardware Monitor.

## Overview

The system consists of two parts:

1. **ESP32 Firmware** (WT32-SC01) — displays the data
2. **Python Script** (Windows) — collects data and sends it via USB

## Part 1: Flash ESP32 Firmware

### Prerequisites

- [PlatformIO](https://platformio.org/) installed (CLI or VS Code extension)
- WT32-SC01 connected via USB
- CP210x driver installed (usually detected automatically)

### Steps

1. Clone or download the repository:

```bash
git clone <repository-url>
cd pc-monitor
```

2. Compile and flash the firmware:

```bash
pio run -t upload
```

3. After successful upload, the display shows "Waiting for PC..."

### PlatformIO Configuration

The `platformio.ini` is pre-configured:

- Board: `esp-wrover-kit` (compatible with WT32-SC01)
- Framework: Arduino
- Flash mode: QIO, 80 MHz
- Libraries: LovyanGFX, ArduinoJson v7

### Memory Usage

- Flash: ~20% (of 4 MB)
- RAM: ~9% (of 320 KB)
- No PSRAM required

## Part 2: Set Up Windows Side

### 2.1 Install LibreHardwareMonitor

**Option A — Download:**

https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases

**Option B — winget:**

```bash
winget install LibreHardwareMonitor.LibreHardwareMonitor
```

**Configuration:**

1. Start LibreHardwareMonitor as **Administrator**
2. Menu: Options → Remote Web Server → **Run**
3. Verify: http://localhost:8085/data.json should display JSON data

### 2.2 Set Up Python

1. Install Python 3: https://www.python.org/downloads/
2. Install dependencies:

```bash
cd windows
pip install -r requirements.txt
```

### 2.3 Start the Script

```bash
python pc_monitor.py
```

The script auto-detects the ESP32 via CP210x/CH340 driver. If not:

```bash
python pc_monitor.py --port COM3
```

### 2.4 Set Up Autostart (optional)

1. `Win+R` → `shell:startup` → Enter
2. Copy the `start_monitor.bat` file (in `windows/` folder) as a shortcut there
3. Also set LibreHardwareMonitor to autostart

## Verification

After successful setup:

1. LibreHardwareMonitor is running (as admin, web server active)
2. Python script shows status line in the console
3. ESP32 display shows real-time data
4. Green dot in top-right = connection active
5. Tap on CPU/GPU/RAM/etc. for detail views

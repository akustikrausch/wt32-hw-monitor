# PC Hardware Monitor — Windows Setup

Anleitung zur Einrichtung der Windows-Seite des PC Hardware Monitors.

---

**[English version below](#english)**

---

## Voraussetzungen

### 1. LibreHardwareMonitor

Download: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases

Oder per winget:

```bash
winget install LibreHardwareMonitor.LibreHardwareMonitor
```

**Wichtig:**

- **Als Administrator starten** (nötig für Hardware-Sensorzugriff)
- Im Menü: Options → Remote Web Server → Run
- Standardport: **8085** (http://localhost:8085/data.json)
- LibreHardwareMonitor muss laufen, bevor das Python-Script gestartet wird

### 2. Python 3

Download: https://www.python.org/downloads/

Abhängigkeiten installieren:

```bash
cd windows
pip install -r requirements.txt
```

Benötigte Pakete: `pyserial`, `requests`

### 3. ESP32 (WT32-SC01)

- Per USB mit dem PC verbinden
- Treiber: CP210x oder CH340 (wird meist automatisch installiert)
- Der COM-Port wird automatisch erkannt

## Verwendung

```bash
# Auto-Erkennung des ESP32 COM-Ports
python pc_monitor.py

# COM-Port manuell angeben
python pc_monitor.py --port COM3

# Test-Modus (Fake-Daten, kein ESP32 nötig)
python pc_monitor.py --test
```

### Konsolenausgabe

Das Script zeigt eine fortlaufende Statuszeile:

```
CPU: 21.8% 83.0C | GPU:  8.0% 46.0C | RAM:  67% | DISK:98.0/116.5TB | FAN:1435/945
```

## Gesendete Daten

Das Script liest 2× pro Sekunde folgende Werte aus LibreHardwareMonitor und sendet sie als kompaktes JSON über USB Serial:

| Kategorie | Werte |
|-----------|-------|
| CPU       | Auslastung, Temperatur, Durchschnittstakt, Package Power, Spannung, Pro-Kern-Lasten |
| GPU       | Auslastung, Temperatur, VRAM, Core-Takt, Memory-Takt, Leistung, Hot Spot, Lüfter-RPM |
| RAM       | Auslastung (%), Used/Total in GB |
| Speicher  | Gesamtkapazität über alle Laufwerke (Total/Used/Free in TB) |
| Disks     | Temperatur, Kurzname und Kapazität pro Laufwerk |
| Lüfter    | RPM-Werte aktiver Mainboard-Lüfter |
| Netzwerk  | Download/Upload-Durchsatz in KB/s |

Vollständige Protokoll-Dokumentation: [docs/PROTOCOL.md](../docs/PROTOCOL.md)

## Autostart einrichten

### Option A: Startup-Ordner (empfohlen)

1. `Win+R` → `shell:startup` → Enter
2. Verknüpfung zu `start_monitor.bat` in diesen Ordner kopieren
3. `start_monitor.bat` liegt im `windows/`-Verzeichnis

### Option B: Aufgabenplanung

1. Aufgabenplanung öffnen (`taskschd.msc`)
2. Neue Aufgabe erstellen
3. Trigger: Bei Anmeldung
4. Aktion: `start_monitor.bat` ausführen
5. "Mit höchsten Privilegien ausführen" aktivieren (optional)

**Hinweis:** LibreHardwareMonitor muss ebenfalls automatisch starten — am besten über dessen eigene Autostart-Option.

## Fehlerbehebung

Siehe [docs/TROUBLESHOOTING.md](../docs/TROUBLESHOOTING.md) für eine vollständige Liste.

Kurzübersicht:

| Problem | Lösung |
|---------|--------|
| "Cannot reach LibreHardwareMonitor" | LHM als Admin starten, Web Server aktivieren |
| "No serial port found" | USB-Kabel prüfen, `--port COM3` nutzen |
| Alle Werte 0 | LHM braucht einige Sekunden nach dem Start |
| COM-Port belegt | Andere Programme (Arduino IDE, etc.) schließen |

---

<a id="english"></a>

# PC Hardware Monitor — Windows Setup

Guide for setting up the Windows side of the PC Hardware Monitor.

## Prerequisites

### 1. LibreHardwareMonitor

Download: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases

Or via winget:

```bash
winget install LibreHardwareMonitor.LibreHardwareMonitor
```

**Important:**

- **Run as administrator** (required for hardware sensor access)
- In the menu: Options → Remote Web Server → Run
- Default port: **8085** (http://localhost:8085/data.json)
- LibreHardwareMonitor must be running before starting the Python script

### 2. Python 3

Download: https://www.python.org/downloads/

Install dependencies:

```bash
cd windows
pip install -r requirements.txt
```

Required packages: `pyserial`, `requests`

### 3. ESP32 (WT32-SC01)

- Connect to the PC via USB
- Drivers: CP210x or CH340 (usually installed automatically)
- The COM port is auto-detected

## Usage

```bash
# Auto-detect ESP32 COM port
python pc_monitor.py

# Specify COM port manually
python pc_monitor.py --port COM3

# Test mode (fake data, no ESP32 required)
python pc_monitor.py --test
```

### Console Output

The script shows a continuously updated status line:

```
CPU: 21.8% 83.0C | GPU:  8.0% 46.0C | RAM:  67% | DISK:98.0/116.5TB | FAN:1435/945
```

## Data Sent

The script reads the following values from LibreHardwareMonitor 2× per second and sends them as compact JSON over USB serial:

| Category | Values |
|----------|--------|
| CPU      | Load, temperature, average clock, package power, voltage, per-core loads |
| GPU      | Load, temperature, VRAM, core clock, memory clock, power, hot spot, fan RPM |
| RAM      | Usage (%), used/total in GB |
| Storage  | Total capacity across all drives (total/used/free in TB) |
| Disks    | Temperature, short name and capacity per drive |
| Fans     | RPM values of active motherboard fans |
| Network  | Download/upload throughput in KB/s |

Full protocol documentation: [docs/PROTOCOL.md](../docs/PROTOCOL.md)

## Autostart Setup

### Option A: Startup Folder (recommended)

1. `Win+R` → `shell:startup` → Enter
2. Copy shortcut to `start_monitor.bat` into this folder
3. `start_monitor.bat` is located in the `windows/` directory

### Option B: Task Scheduler

1. Open Task Scheduler (`taskschd.msc`)
2. Create a new task
3. Trigger: At log on
4. Action: Run `start_monitor.bat`
5. Enable "Run with highest privileges" (optional)

**Note:** LibreHardwareMonitor also needs to auto-start — best done via its own autostart option.

## Troubleshooting

See [docs/TROUBLESHOOTING.md](../docs/TROUBLESHOOTING.md) for a complete list.

Quick reference:

| Problem | Solution |
|---------|----------|
| "Cannot reach LibreHardwareMonitor" | Start LHM as admin, enable web server |
| "No serial port found" | Check USB cable, use `--port COM3` |
| All values 0 | LHM needs a few seconds after startup |
| COM port busy | Close other programs (Arduino IDE, etc.) |

# PC Hardware Monitor - Windows Setup

## Voraussetzungen

### 1. LibreHardwareMonitor installieren
- Download: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases
- **Als Administrator starten** (nötig für Hardware-Zugriff)
- Im Menü: Options → Remote Web Server → Run
- Standardport: **8085** (http://localhost:8085/data.json)

### 2. Python 3 installieren
- Download: https://www.python.org/downloads/
- Pakete installieren:
```bash
pip install -r requirements.txt
```

## Verwendung

```bash
# Auto-Erkennung des ESP32 COM-Ports
python pc_monitor.py

# COM-Port manuell angeben
python pc_monitor.py --port COM3

# Test-Modus (Fake-Daten, kein Serial nötig)
python pc_monitor.py --test
```

## Autostart einrichten (optional)

1. `Win+R` → `shell:startup`
2. Verknüpfung erstellen zu: `pythonw pc_monitor.py`
   (oder als .bat Datei mit `python pc_monitor.py`)

## Fehlerbehebung

- **"Cannot reach LibreHardwareMonitor"**: LHM als Admin starten, Web Server aktivieren
- **"No serial port found"**: ESP32 USB-Kabel prüfen, COM-Port mit `--port` angeben
- **Falsche Werte**: LHM zeigt manchmal 0 für nicht unterstützte Sensoren

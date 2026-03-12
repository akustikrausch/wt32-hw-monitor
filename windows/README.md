# PC Hardware Monitor - Windows Setup

## Voraussetzungen

### 1. LibreHardwareMonitor installieren

Download: https://github.com/LibreHardwareMonitor/LibreHardwareMonitor/releases

Alternativ per winget:
```bash
winget install LibreHardwareMonitor.LibreHardwareMonitor
```

**Wichtig:**
- **Als Administrator starten** (notig fur Hardware-Zugriff)
- Im Menu: Options -> Remote Web Server -> Run
- Standardport: **8085** (http://localhost:8085/data.json)

### 2. Python 3 installieren
- Download: https://www.python.org/downloads/
- Pakete installieren:
```bash
pip install -r requirements.txt
```

## Verwendung

```bash
# Auto-Erkennung des ESP32 COM-Ports (CP210x / CH340)
python pc_monitor.py

# COM-Port manuell angeben
python pc_monitor.py --port COM3

# Test-Modus (Fake-Daten, kein Serial notig)
python pc_monitor.py --test
```

## Gesendete Daten

Das Script liest folgende Werte aus LibreHardwareMonitor und sendet sie 2x pro Sekunde als JSON:

- CPU: Auslastung, Temperatur, Durchschnittstakt, Package Power
- GPU: Auslastung, Temperatur, VRAM-Belegung
- RAM: Auslastung, Used/Total
- Storage: Gesamtspeicher uber alle Laufwerke (Total/Used/Free in TB)
- Disk-Temperaturen: Individuelle Temps mit Kurznamen fur jedes Laufwerk
- Fans: RPM-Werte (nur aktive Lufter)

## Autostart einrichten (optional)

1. `Win+R` -> `shell:startup`
2. Verknupfung erstellen zu: `pythonw pc_monitor.py`
   (oder als .bat Datei mit `python pc_monitor.py`)

## Fehlerbehebung

- **"Cannot reach LibreHardwareMonitor"**: LHM als Admin starten, Web Server aktivieren (Options -> Remote Web Server -> Run)
- **"No serial port found"**: ESP32 USB-Kabel prufen, COM-Port mit `--port` angeben
- **Alle Werte 0**: LHM braucht einige Sekunden nach dem Start, um Sensoren zu initialisieren
- **Falsche Temperaturen**: Manche Sensoren liefern erst nach einigen Updates korrekte Werte

# Changelog / Änderungsprotokoll

Alle wesentlichen Änderungen an diesem Projekt.

---

**[English version below](#english)**

---

## [1.0.0] — 2026-03-12

### Hinzugefügt

- **Touch-Navigation**: Tippe auf CPU/GPU/RAM/Disk/Netzwerk/Fans für 7 Detail-Ansichten
- **Zurück-Button** oben links in allen Detail-Ansichten
- **CPU-Detail**: Auslastung, Temperatur, Takt, Leistung, Spannung, Pro-Kern-Balken, Verlaufsgraph
- **GPU-Detail**: Auslastung, Temperatur, Core/Memory-Takt, Leistung, Hot Spot, VRAM-Balken, Lüfter-RPM, Graph
- **RAM-Detail**: Prozent, Used/Free/Total, visueller Block, Verlaufsgraph
- **Disk-Detail**: Gesamtspeicher-Balken, pro-Laufwerk-Liste mit Name, Kapazität, Temperatur, Größenbalken
- **Lüfter-Detail**: System-Lüfter 1 & 2, GPU-Lüfter mit RPM-Balken
- **Netzwerk-Detail**: Download/Upload-Geschwindigkeit, auto-skalierte Verlaufsgraphen
- **Netzwerk-Anzeige** auf dem Hauptbildschirm (DL/UL in KB/s bis GB/s)
- **Autostart**: `start_monitor.bat` für Windows Startup-Ordner
- **Dokumentation**: `docs/PROTOCOL.md`, `docs/SETUP.md`, `docs/TROUBLESHOOTING.md`

### Verbessert

- **Bottom-Bereich** nutzt den gesamten verbleibenden Platz (114 px statt 40 px)
- **Schriftgröße**: Bottom-Bereich von Font0 auf Font2 umgestellt
- **Disk-Temperaturen**: 2-Spalten-Layout für bessere Lesbarkeit
- **Einheiten-Abstände**: Leerzeichen vor MHz, W, MB, nach DL:/UL:
- **Textkontrast**: Schwarzer Text auf farbigen Balken (statt weiß auf gelb)
- **Disk-Namen**: Aggressivere Verkürzung (z.B. ST24000NM007H → ST24T)
- **JSON-Puffer** auf 2048 Bytes erhöht

### Behoben

- Grad-Symbol (°) wird in LovyanGFX-Fonts nicht korrekt dargestellt → Leerzeichen vor "C"
- Weißer Text auf gelben/farbigen Balken unleserlich → schwarzer Text
- Verschwendeter Platz am unteren Bildschirmrand
- Zu kleine Schrift im unteren Bereich

## [0.2.0] — 2026-03-12

### Hinzugefügt

- CPU-Taktfrequenz (MHz) und Package Power (W)
- GPU VRAM-Anzeige (Used/Total MB)
- Individuelle Disk-Temperaturen mit Farbkodierung
- Lüfter-RPM-Anzeige (2 System-Lüfter)
- Gesamtspeicher-Anzeige (TB)

### Verbessert

- Header entfernt — mehr Platz für Daten
- Verlaufsgraphen für CPU und GPU (60 Sekunden)

## [0.1.0] — 2026-03-12

### Erster Release

- Grundlegendes Dashboard: CPU (%), GPU (%), RAM (%)
- Anti-Flicker-Technik (direkte LCD-Zeichnung)
- Auto-Reconnect bei USB-Verbindungsverlust
- Farbkodierte Temperaturanzeige (Grün/Gelb/Rot)
- Python-Script mit LibreHardwareMonitor-Anbindung
- Automatische COM-Port-Erkennung
- Test-Modus mit Fake-Daten

---

<a id="english"></a>

# Changelog

All notable changes to this project.

## [1.0.0] — 2026-03-12

### Added

- **Touch navigation**: Tap on CPU/GPU/RAM/Disk/Network/Fans for 7 detail views
- **Back button** in the top-left corner of all detail views
- **CPU detail**: Load, temperature, clock, power, voltage, per-core bars, history graph
- **GPU detail**: Load, temperature, core/memory clock, power, hot spot, VRAM bar, fan RPM, graph
- **RAM detail**: Percentage, used/free/total, visual block, history graph
- **Disk detail**: Total storage bar, per-drive list with name, capacity, temperature, size bar
- **Fan detail**: System fans 1 & 2, GPU fan with RPM bars
- **Network detail**: Download/upload speed, auto-scaled history graphs
- **Network display** on the main screen (DL/UL in KB/s to GB/s)
- **Autostart**: `start_monitor.bat` for Windows startup folder
- **Documentation**: `docs/PROTOCOL.md`, `docs/SETUP.md`, `docs/TROUBLESHOOTING.md`

### Improved

- **Bottom section** uses all remaining space (114 px instead of 40 px)
- **Font size**: Bottom section upgraded from Font0 to Font2
- **Disk temperatures**: 2-column layout for better readability
- **Unit spacing**: Spaces before MHz, W, MB, after DL:/UL:
- **Text contrast**: Black text on colored bars (instead of white on yellow)
- **Disk names**: More aggressive shortening (e.g., ST24000NM007H → ST24T)
- **JSON buffer** increased to 2048 bytes

### Fixed

- Degree symbol (°) does not render correctly in LovyanGFX fonts → space before "C"
- White text on yellow/colored bars was unreadable → black text
- Wasted space at the bottom of the screen
- Too-small font in the bottom section

## [0.2.0] — 2026-03-12

### Added

- CPU clock speed (MHz) and package power (W)
- GPU VRAM display (used/total MB)
- Individual disk temperatures with color coding
- Fan RPM display (2 system fans)
- Total storage display (TB)

### Improved

- Header removed — more space for data
- History graphs for CPU and GPU (60 seconds)

## [0.1.0] — 2026-03-12

### Initial Release

- Basic dashboard: CPU (%), GPU (%), RAM (%)
- Anti-flicker technique (direct LCD drawing)
- Auto-reconnect on USB connection loss
- Color-coded temperature display (green/yellow/red)
- Python script with LibreHardwareMonitor integration
- Automatic COM port detection
- Test mode with fake data

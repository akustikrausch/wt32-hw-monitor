# Fehlerbehebung / Troubleshooting

Häufige Probleme und deren Lösungen.

---

**[English version below](#english)**

---

## Verbindungsprobleme

### "Cannot reach LibreHardwareMonitor"

**Ursache:** Das Python-Script kann die LHM-API nicht erreichen.

**Lösung:**

1. LibreHardwareMonitor als **Administrator** starten
2. Menü: Options → Remote Web Server → **Run** aktivieren
3. Prüfen: http://localhost:8085/data.json im Browser öffnen
4. Falls Port 8085 blockiert ist: Firewall-Einstellungen prüfen

### "No serial port found"

**Ursache:** ESP32 wird nicht erkannt.

**Lösung:**

1. USB-Kabel prüfen (muss ein Datenkabel sein, kein reines Ladekabel)
2. WT32-SC01 im Geräte-Manager prüfen (sollte als "CP210x" oder "CH340" erscheinen)
3. Treiber installieren: [CP210x](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) oder [CH340](http://www.wch-ic.com/downloads/CH341SER_ZIP.html)
4. COM-Port manuell angeben: `python pc_monitor.py --port COM3`

### COM-Port belegt

**Ursache:** Ein anderes Programm nutzt den COM-Port (Arduino IDE, Serial Monitor, etc.).

**Lösung:**

1. Alle Programme schließen, die den COM-Port nutzen könnten
2. Im Task-Manager nach `python.exe` suchen und ggf. beenden
3. Alternativ: ESP32 kurz trennen und neu verbinden

### Display zeigt "Waiting for PC..."

**Ursache:** ESP32 empfängt keine Daten.

**Lösung:**

1. Python-Script läuft? → Konsole prüfen
2. LibreHardwareMonitor läuft? → Web Server aktiv?
3. Richtiger COM-Port? → `--port` Parameter nutzen
4. USB-Kabel defekt? → Anderes Kabel testen

## Anzeige-Probleme

### Alle Werte zeigen 0

**Ursache:** LibreHardwareMonitor hat die Sensoren noch nicht initialisiert.

**Lösung:** Einige Sekunden warten. LHM braucht nach dem Start etwas Zeit, bis alle Sensoren Werte liefern.

### Temperaturen erscheinen falsch

**Ursache:** Manche Sensoren liefern erst nach einigen Update-Zyklen korrekte Werte.

**Lösung:** 5–10 Sekunden warten, die Werte stabilisieren sich automatisch.

### Display flackert

**Ursache:** Sollte nicht auftreten (Anti-Flicker-Technik aktiv).

**Lösung:** Falls dennoch Flackern auftritt:

1. USB-Kabel auf festen Sitz prüfen
2. Firmware neu flashen: `pio run -t upload`

### Text zu klein / unleserlich

**Ursache:** Display-Auflösung (480×320) begrenzt die Textgröße.

**Lösung:** Die Hauptwerte nutzen Font2/Font4 für Lesbarkeit. Detail-Ansichten (Touch) zeigen größere Werte.

## Python-Script-Probleme

### Script stürzt ab mit "SerialException"

**Ursache:** USB-Verbindung unterbrochen.

**Lösung:** Das Script erkennt Verbindungsverlust und wartet auf erneute Verbindung. Falls nicht:

1. ESP32 trennen und neu verbinden
2. Script neu starten

### Hohe CPU-Auslastung durch das Script

**Ursache:** Normalerweise minimal (<1 %).

**Lösung:** Falls unerwartet hoch:

1. Update-Intervall erhöhen (Standard: 0,5s)
2. LHM-Web-Server auf Erreichbarkeit prüfen (Timeouts verursachen Retries)

## Build-Probleme (PlatformIO)

### Kompilierungsfehler

**Lösung:**

1. PlatformIO aktualisieren: `pio upgrade`
2. Bibliotheken neu installieren: `pio lib install`
3. Build-Cache löschen: `.pio/`-Ordner löschen, dann `pio run`

### Upload schlägt fehl

**Lösung:**

1. COM-Port durch anderes Programm belegt? → Schließen
2. Boot-Modus: Auf manchen Boards GPIO0 beim Einschalten halten
3. Upload-Geschwindigkeit reduzieren: In `platformio.ini` → `upload_speed = 460800`

---

<a id="english"></a>

# Troubleshooting

Common problems and their solutions.

## Connection Issues

### "Cannot reach LibreHardwareMonitor"

**Cause:** The Python script cannot reach the LHM API.

**Solution:**

1. Start LibreHardwareMonitor as **Administrator**
2. Menu: Options → Remote Web Server → Enable **Run**
3. Verify: Open http://localhost:8085/data.json in your browser
4. If port 8085 is blocked: Check firewall settings

### "No serial port found"

**Cause:** ESP32 is not detected.

**Solution:**

1. Check USB cable (must be a data cable, not a charge-only cable)
2. Check Device Manager for "CP210x" or "CH340"
3. Install drivers: [CP210x](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) or [CH340](http://www.wch-ic.com/downloads/CH341SER_ZIP.html)
4. Specify COM port manually: `python pc_monitor.py --port COM3`

### COM Port Busy

**Cause:** Another program is using the COM port (Arduino IDE, Serial Monitor, etc.).

**Solution:**

1. Close all programs that might be using the COM port
2. Check Task Manager for `python.exe` and terminate if needed
3. Alternatively: Disconnect and reconnect the ESP32

### Display Shows "Waiting for PC..."

**Cause:** ESP32 is not receiving data.

**Solution:**

1. Is the Python script running? → Check the console
2. Is LibreHardwareMonitor running? → Web server active?
3. Correct COM port? → Use `--port` parameter
4. Defective USB cable? → Try a different cable

## Display Issues

### All Values Show 0

**Cause:** LibreHardwareMonitor hasn't initialized the sensors yet.

**Solution:** Wait a few seconds. LHM needs some time after startup to provide sensor values.

### Temperatures Appear Wrong

**Cause:** Some sensors need a few update cycles before reporting correct values.

**Solution:** Wait 5–10 seconds, values will stabilize automatically.

### Display Flickering

**Cause:** Should not occur (anti-flicker technique active).

**Solution:** If flickering does occur:

1. Check USB cable is firmly connected
2. Re-flash the firmware: `pio run -t upload`

### Text Too Small / Unreadable

**Cause:** Display resolution (480×320) limits text size.

**Solution:** Main values use Font2/Font4 for readability. Detail views (touch) show larger values.

## Python Script Issues

### Script Crashes with "SerialException"

**Cause:** USB connection interrupted.

**Solution:** The script detects connection loss and waits for reconnection. If not:

1. Disconnect and reconnect the ESP32
2. Restart the script

### High CPU Usage from the Script

**Cause:** Normally minimal (<1%).

**Solution:** If unexpectedly high:

1. Increase update interval (default: 0.5s)
2. Check LHM web server reachability (timeouts cause retries)

## Build Issues (PlatformIO)

### Compilation Errors

**Solution:**

1. Update PlatformIO: `pio upgrade`
2. Reinstall libraries: `pio lib install`
3. Clear build cache: Delete `.pio/` folder, then `pio run`

### Upload Fails

**Solution:**

1. COM port occupied by another program? → Close it
2. Boot mode: On some boards, hold GPIO0 during power-on
3. Reduce upload speed: In `platformio.ini` → `upload_speed = 460800`

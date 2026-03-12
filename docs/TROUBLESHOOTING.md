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

### Display Shows Standby Clock Instead of Data

**Cause:** ESP32 is not receiving data from the Python script.

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

**Solution:** The script automatically detects connection loss and reconnects. If it doesn't recover:

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

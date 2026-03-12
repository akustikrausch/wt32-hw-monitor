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
CPU: 38.5% 62.0C | GPU: 42.0% 48.0C | RAM:  58% | DISK:5.8/8.0TB | FAN:1100/850
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
| Time     | Unix timestamp (UTC) and timezone offset for standby clock |

Full protocol documentation: [docs/PROTOCOL.md](../docs/PROTOCOL.md)

## Autostart Setup

The monitor runs completely hidden in the background — no console window. It auto-restarts on crash and logs output to `pc_monitor.log`.

### Option A: Startup Folder (recommended)

1. `Win+R` → `shell:startup` → Enter
2. Create a shortcut with:
   - **Target:** `C:\path\to\pythonw.exe "C:\path\to\windows\start_hidden.pyw"`
   - **Start in:** `C:\path\to\wt32-hw-monitor\windows`
3. The hidden launcher waits 15 seconds after boot for USB drivers and LibreHardwareMonitor to be ready

### Option B: Task Scheduler

1. Open Task Scheduler (`taskschd.msc`)
2. Create a new task
3. Trigger: At log on
4. Action: Run `pythonw.exe` with argument `start_hidden.pyw`
5. Start in: `windows/` directory

**Note:** LibreHardwareMonitor also needs to auto-start — best done via its own autostart option.

### Stopping the Monitor

Since there is no visible window, use Task Manager or:

```bash
taskkill /F /IM python.exe
taskkill /F /IM pythonw.exe
```

## Troubleshooting

See [docs/TROUBLESHOOTING.md](../docs/TROUBLESHOOTING.md) for a complete list.

Quick reference:

| Problem | Solution |
|---------|----------|
| "Cannot reach LibreHardwareMonitor" | Start LHM as admin, enable web server |
| "No serial port found" | Check USB cable, use `--port COM3` |
| All values 0 | LHM needs a few seconds after startup |
| COM port busy | Close other programs (Arduino IDE, etc.) |

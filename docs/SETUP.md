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
cd wt32-hw-monitor
```

2. Compile and flash the firmware:

```bash
pio run -t upload
```

3. After successful upload, the display shows a standby clock screen

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

# Changelog

All notable changes to this project.

## [1.5.0] — 2026-09-30

### Fixed

- **Display fell back to the standby clock and stayed there**: The sender only transmitted after a successful LibreHardwareMonitor poll. On PCs with several HDDs a single LHM request can take seconds (SMART reads, drives spinning up), so the ESP32 received nothing for 5 s and switched to standby. LHM is now polled in a background thread; the sender transmits at a fixed 2 Hz regardless and repeats the last good data set for up to 30 s
- **Sender could freeze forever**: `serial.write()` had no write timeout; a stalled USB-UART bridge blocked the script without any log entry. Now `write_timeout=2` plus reconnect
- **ESP32 rebooted on every reconnect**: Opening the COM port toggled DTR/RTS (auto-reset lines). The port is now opened with both lines released
- **Lost bytes on the ESP32**: Serial RX buffer raised from 256 bytes to 4 KB, so a screen redraw during reception no longer drops parts of a line
- **Touch on the standby clock** left standby and showed a black screen until data arrived
- **PCIe Rx/Tx bars** on the GPU Advanced screen were hard-wired to 0 %; they now scale to the peak seen since boot
- **Oversized lines** are discarded instead of being parsed truncated
- **Log file grew to gigabytes**: The status line was written to `pc_monitor.log` twice per second. In hidden mode only events with timestamps and a summary every 10 minutes are logged; the launcher rotates the log above 5 MB
- **Wrong COM port**: Auto-detection no longer falls back to the first available port (it would send JSON to an unrelated device)

### Added

- **Heartbeat message** (`{"hb":1,...}`): sent when LibreHardwareMonitor has been silent for 30 s. The standby screen now names the cause: "LibreHardwareMonitor antwortet nicht" or "Keine Daten vom PC"
- **Data age** (`age` field): the connection dot turns yellow when the shown values are older than 3 s
- **Keep-alive HTTP session** to LibreHardwareMonitor instead of a new TCP connection per poll

### Improved

- Clock time is saved to NVS every 10 minutes instead of every 30 seconds (less flash wear)
- Disk arrays capped at 8 entries (the display shows 8), storage totals still include all drives

## [1.4.0] — 2026-03-19

### Added

- **Advanced View**: New "..." button on main screen opens advanced monitoring dashboard
- **Advanced Main Screen**: Overview with motherboard voltages, CPU die temps/TDC/clocks, DIMM temps, disk I/O throughput, GPU D3D loads/PCIe stats
- **5 Advanced Detail Screens**: Motherboard, CPU Advanced, GPU Advanced, RAM Advanced, Disk I/O — each with Back/Next navigation
- **Section header bars**: Color-coded section dividers on advanced main screen for better readability

### Fixed

- **Disk I/O activity display**: Changed `disk_act` from int to float to preserve sub-1% activity values (NVMe idle activity is often 0.1–0.3%)
- **JSON buffer mismatch**: `jsonBuf` in main.cpp increased from 2048 to 4096 bytes to match parser's `lineBuf`
- **Purple readability**: Changed COL_PURPLE from 0x780F to 0xC01F (brighter), added COL_VIOLET for headers, redesigned advanced screen with gray section bars

### Improved

- **Serial buffer**: Parser `lineBuf` increased to 4096 bytes for ~30 new advanced data fields
- **Disk I/O precision**: Activity shown with 1 decimal place (e.g., "0.3%" instead of "0%"), read/write throughput with 0.1 KB/s resolution

## [1.3.0] — 2026-03-18

### Fixed

- **Network speed parsing**: Handle LibreHardwareMonitor dynamic unit switching (KB/s ↔ MB/s)
- **Network adapter detection**: Use total transferred data instead of current throughput for reliable active adapter identification

### Improved

- **Network detail view**: Enhanced display with MB/s support and better adapter detection

## [1.2.0] — 2026-03-15

### Added

- **Next button**: Cycle through all detail views without returning to the main screen
- **Hidden background launcher** (`start_hidden.pyw`): Windowless autostart — no console window, auto-restart on crash, logging to `pc_monitor.log`

### Fixed

- **Standby clock**: Fixed clock display after ESP32 reboot
- **Autostart reliability**: Improved startup sequence timing

### Improved

- **README**: SEO optimization with badges, keywords, and better descriptions
- **Documentation**: English-only, cleaned up for public release
- **Privacy**: Removed private hardware details from docs, replaced with generic examples

## [1.1.0] — 2026-03-12

### Added

- **Standby clock**: Minimalist clock screen on connection loss (Apple-style, dimmed brightness)
- **Time sync**: PC sends Unix timestamp (`ts`) and timezone offset (`tzo`), ESP32 continues counting independently
- **Date display**: German format (e.g., "Do, 12. Maerz 2026")
- **Disconnect timer**: Shown bottom-right (minutes/seconds since connection lost)
- **Dot animation**: Three dots bottom-left indicate ongoing connection search
- **Auto-reconnect**: Python script detects USB port changes and reconnects automatically

### Improved

- **Project name**: Renamed from `pc-monitor` to `wt32-hw-monitor`

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
- **Disk names**: More aggressive shortening (e.g., WD_BLACK SN850X → SN850X)
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

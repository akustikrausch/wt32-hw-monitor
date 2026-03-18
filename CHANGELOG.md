# Changelog

All notable changes to this project.

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

# Serial Protocol

Documentation of the communication protocol between the Python script and ESP32.

## Overview

- **Interface**: USB Serial (CP210x / CH340)
- **Baud Rate**: 115200
- **Format**: JSON, one line per message, `\n`-terminated
- **Frequency**: 2 Hz (every 0.5 seconds)
- **Direction**: PC → ESP32 only (unidirectional)
- **Buffer Size**: 2048 bytes (ESP32 side)

## JSON Example

```json
{"cpu":38.5,"gpuload":42.0,"cputemp":62.0,"gputemp":48.0,"cpuclk":5800,"cpupwr":170,"cpuvolt":1.280,"gpuvram":16384,"gpuvtot":32768,"gpuclk":2850,"gpumclk":12000,"gpupwr":380,"gpuhs":55,"gpufan":1450,"ram":58.0,"ramused":74.2,"ramtotal":128.0,"fan1":1100,"fan2":850,"stotal":8.0,"sused":5.8,"sfree":2.2,"dtemp":[35,38,32,36],"dname":["990PRO","T700","T500","SN850X"],"dsize":[2000,2000,2000,2000],"netdl":125.8,"netul":42.3,"ccores":[38,42,55,28,62,35,48,22,58,30,45,52,40,33,50,27],"cpuname":"Ryzen 9 9950X","gpuname":"RTX 5090","ts":1741784400,"tzo":3600}
```

## Fields

### Main Data

| Field     | Type     | Unit | Description                        |
|-----------|----------|------|------------------------------------|
| cpu       | float    | %    | CPU total load                     |
| cputemp   | float    | °C   | CPU temperature (Tctl/Package)     |
| cpuclk    | float    | MHz  | CPU average clock                  |
| cpupwr    | float    | W    | CPU package power                  |
| cpuvolt   | float    | V    | CPU core voltage                   |
| gpuload   | float    | %    | GPU load                           |
| gputemp   | float    | °C   | GPU core temperature               |
| gpuvram   | float    | MB   | GPU VRAM used                      |
| gpuvtot   | float    | MB   | GPU VRAM total                     |
| gpuclk    | float    | MHz  | GPU core clock                     |
| gpumclk   | float    | MHz  | GPU memory clock                   |
| gpupwr    | float    | W    | GPU power                          |
| gpuhs     | float    | °C   | GPU hot spot temperature           |
| gpufan    | int      | RPM  | GPU fan (-1 = not available)       |
| ram       | float    | %    | RAM usage                          |
| ramused   | float    | GB   | RAM used                           |
| ramtotal  | float    | GB   | RAM total                          |
| fan1      | int      | RPM  | System fan 1 (-1 = not available)  |
| fan2      | int      | RPM  | System fan 2 (-1 = not available)  |

### Storage

| Field  | Type     | Unit | Description                       |
|--------|----------|------|-----------------------------------|
| stotal | float    | TB   | Total storage (all drives)        |
| sused  | float    | TB   | Storage used                      |
| sfree  | float    | TB   | Storage free                      |
| dtemp  | int[]    | °C   | Temperature per drive (-1 = n/a)  |
| dname  | string[] | —    | Short name per drive (max 8 chars)|
| dsize  | int[]    | GB   | Capacity per drive                |

### Network

| Field | Type  | Unit | Description         |
|-------|-------|------|---------------------|
| netdl | float | KB/s | Download throughput |
| netul | float | KB/s | Upload throughput   |

### CPU Cores

| Field  | Type    | Unit | Description                   |
|--------|---------|------|-------------------------------|
| ccores | float[] | %    | Per-core load (max 16 cores)  |

### Time Sync

| Field | Type | Unit    | Description                                     |
|-------|------|---------|-------------------------------------------------|
| ts    | int  | seconds | Unix timestamp (UTC) from PC                    |
| tzo   | int  | seconds | Timezone offset (e.g. 3600 = CET, 7200 = CEST) |

The ESP32 uses this data to display a clock in standby mode. The time continues to be calculated via `millis()` even after connection loss.

### Names

| Field   | Type   | Description                   |
|---------|--------|-------------------------------|
| cpuname | string | CPU model name (max 30 chars) |
| gpuname | string | GPU model name (max 30 chars) |

## Disk Name Shortening

The Python script automatically shortens drive names for display:

| Original               | Short Name |
|------------------------|------------|
| Samsung 990 PRO 2TB    | 990PRO     |
| Crucial T700 2TB       | T700       |
| Crucial T500 2TB       | T500       |
| WD Black SN850X 2TB    | SN850X     |

Rules: Remove manufacturer prefixes, strip suffixes like "NVMe"/"SATA", truncate to max 8 characters. Seagate models are mapped to their capacity in TB/GB.

## Data Sources (LibreHardwareMonitor)

| ImageURL     | Hardware Type     | Sensor Groups                                  |
|-------------|-------------------|------------------------------------------------|
| cpu.png      | CPU               | Load, Temperatures, Clocks, Powers, Voltages   |
| nvidia.png   | NVIDIA GPU        | Load, Temperatures, Clocks, Powers, Data, Fans |
| amd.png      | AMD GPU           | (same as nvidia.png)                           |
| ram.png      | RAM               | Load, Data                                     |
| hdd.png      | Drives            | Temperatures, Data                             |
| chip.png     | Motherboard Chips | Fans                                           |
| network.png  | Network Adapters  | Throughput                                     |

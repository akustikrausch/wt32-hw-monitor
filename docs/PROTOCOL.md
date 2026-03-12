# Serial-Protokoll / Serial Protocol

Dokumentation des Kommunikationsprotokolls zwischen Python-Script und ESP32.

---

**[English version below](#english)**

---

## Übersicht

- **Schnittstelle**: USB Serial (CP210x / CH340)
- **Baudrate**: 115200
- **Format**: JSON, eine Zeile pro Nachricht, `\n`-terminiert
- **Frequenz**: 2 Hz (alle 0,5 Sekunden)
- **Richtung**: Nur PC → ESP32 (unidirektional)
- **Puffergröße**: 2048 Bytes (ESP32-seitig)

## JSON-Beispiel

```json
{"cpu":45.2,"gpuload":30.0,"cputemp":68.0,"gputemp":55.0,"cpuclk":4283,"cpupwr":137,"cpuvolt":1.125,"gpuvram":2048,"gpuvtot":6144,"gpuclk":1950,"gpumclk":7000,"gpupwr":120,"gpuhs":62,"gpufan":1200,"ram":62.0,"ramused":39.8,"ramtotal":64.0,"fan1":1200,"fan2":980,"stotal":116.5,"sused":92.3,"sfree":24.2,"dtemp":[38,42,35,40],"dname":["980PRO","870EVO","WD10T","ST24T"],"dsize":[1000,500,10000,24000],"netdl":31.5,"netul":5.2,"ccores":[45,32,78,12,55,66,23,89,10,44,33,67,22,90,15,50],"cpuname":"Ryzen 9 5950X","gpuname":"RTX 2060"}
```

## Felder

### Hauptdaten

| Feld      | Typ      | Einheit | Beschreibung                         |
|-----------|----------|---------|--------------------------------------|
| cpu       | float    | %       | CPU Gesamtauslastung                  |
| cputemp   | float    | C       | CPU Temperatur (Tctl/Package)         |
| cpuclk    | float    | MHz     | CPU Durchschnittstakt                 |
| cpupwr    | float    | W       | CPU Package Leistung                  |
| cpuvolt   | float    | V       | CPU Core Spannung                     |
| gpuload   | float    | %       | GPU Auslastung                        |
| gputemp   | float    | C       | GPU Kerntemperatur                    |
| gpuvram   | float    | MB      | GPU VRAM belegt                       |
| gpuvtot   | float    | MB      | GPU VRAM gesamt                       |
| gpuclk    | float    | MHz     | GPU Core-Takt                         |
| gpumclk   | float    | MHz     | GPU Memory-Takt                       |
| gpupwr    | float    | W       | GPU Leistung                          |
| gpuhs     | float    | C       | GPU Hot Spot Temperatur               |
| gpufan    | int      | RPM     | GPU Lüfter (-1 = nicht vorhanden)     |
| ram       | float    | %       | RAM Auslastung                        |
| ramused   | float    | GB      | RAM belegt                            |
| ramtotal  | float    | GB      | RAM gesamt                            |
| fan1      | int      | RPM     | System-Lüfter 1 (-1 = nicht vorhanden) |
| fan2      | int      | RPM     | System-Lüfter 2 (-1 = nicht vorhanden) |

### Speicher

| Feld   | Typ      | Einheit | Beschreibung                        |
|--------|----------|---------|-------------------------------------|
| stotal | float    | TB      | Speicher gesamt (alle Laufwerke)     |
| sused  | float    | TB      | Speicher belegt                      |
| sfree  | float    | TB      | Speicher frei                        |
| dtemp  | int[]    | C       | Temperatur pro Laufwerk (-1 = n.v.)  |
| dname  | string[] | —       | Kurzname pro Laufwerk (max. 8 Zeichen) |
| dsize  | int[]    | GB      | Kapazität pro Laufwerk               |

### Netzwerk

| Feld  | Typ   | Einheit | Beschreibung         |
|-------|-------|---------|----------------------|
| netdl | float | KB/s    | Download-Durchsatz    |
| netul | float | KB/s    | Upload-Durchsatz      |

### CPU-Kerne

| Feld   | Typ     | Einheit | Beschreibung                    |
|--------|---------|---------|---------------------------------|
| ccores | float[] | %       | Pro-Kern-Auslastung (max. 16)   |

### Bezeichnungen

| Feld    | Typ    | Beschreibung                    |
|---------|--------|---------------------------------|
| cpuname | string | CPU-Modellname (max. 30 Zeichen) |
| gpuname | string | GPU-Modellname (max. 30 Zeichen) |

## Disk-Namensverkürzung

Das Python-Script kürzt Laufwerksnamen automatisch für die Anzeige:

| Original                  | Kurzname |
|--------------------------|----------|
| Samsung 980 PRO          | 980PRO   |
| Samsung SSD 870 EVO      | 870EVO   |
| WDC WD101EFBX            | WD101EF  |
| Seagate ST24000NM007H    | ST24T    |

Regeln: Hersteller-Präfixe entfernen, Suffixe wie "NVMe"/"SATA" entfernen, auf max. 8 Zeichen kürzen. Seagate-Modelle werden auf Kapazität in TB/GB abgebildet.

## Datenquellen (LibreHardwareMonitor)

| ImageURL     | Hardware-Typ     | Sensor-Gruppen                                |
|-------------|-----------------|-----------------------------------------------|
| cpu.png      | CPU              | Load, Temperatures, Clocks, Powers, Voltages  |
| nvidia.png   | NVIDIA GPU       | Load, Temperatures, Clocks, Powers, Data, Fans |
| amd.png      | AMD GPU          | (gleich wie nvidia.png)                        |
| ram.png      | Arbeitsspeicher  | Load, Data                                    |
| hdd.png      | Laufwerke        | Temperatures, Data                            |
| chip.png     | Mainboard-Chips  | Fans                                          |
| network.png  | Netzwerkadapter  | Throughput                                    |

---

<a id="english"></a>

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
{"cpu":45.2,"gpuload":30.0,"cputemp":68.0,"gputemp":55.0,"cpuclk":4283,"cpupwr":137,"cpuvolt":1.125,"gpuvram":2048,"gpuvtot":6144,"gpuclk":1950,"gpumclk":7000,"gpupwr":120,"gpuhs":62,"gpufan":1200,"ram":62.0,"ramused":39.8,"ramtotal":64.0,"fan1":1200,"fan2":980,"stotal":116.5,"sused":92.3,"sfree":24.2,"dtemp":[38,42,35,40],"dname":["980PRO","870EVO","WD10T","ST24T"],"dsize":[1000,500,10000,24000],"netdl":31.5,"netul":5.2,"ccores":[45,32,78,12,55,66,23,89,10,44,33,67,22,90,15,50],"cpuname":"Ryzen 9 5950X","gpuname":"RTX 2060"}
```

## Fields

### Main Data

| Field     | Type     | Unit | Description                         |
|-----------|----------|------|-------------------------------------|
| cpu       | float    | %    | CPU total load                       |
| cputemp   | float    | C    | CPU temperature (Tctl/Package)       |
| cpuclk    | float    | MHz  | CPU average clock                    |
| cpupwr    | float    | W    | CPU package power                    |
| cpuvolt   | float    | V    | CPU core voltage                     |
| gpuload   | float    | %    | GPU load                             |
| gputemp   | float    | C    | GPU core temperature                 |
| gpuvram   | float    | MB   | GPU VRAM used                        |
| gpuvtot   | float    | MB   | GPU VRAM total                       |
| gpuclk    | float    | MHz  | GPU core clock                       |
| gpumclk   | float    | MHz  | GPU memory clock                     |
| gpupwr    | float    | W    | GPU power                            |
| gpuhs     | float    | C    | GPU hot spot temperature             |
| gpufan    | int      | RPM  | GPU fan (-1 = not available)         |
| ram       | float    | %    | RAM usage                            |
| ramused   | float    | GB   | RAM used                             |
| ramtotal  | float    | GB   | RAM total                            |
| fan1      | int      | RPM  | System fan 1 (-1 = not available)    |
| fan2      | int      | RPM  | System fan 2 (-1 = not available)    |

### Storage

| Field  | Type     | Unit | Description                         |
|--------|----------|------|-------------------------------------|
| stotal | float    | TB   | Total storage (all drives)           |
| sused  | float    | TB   | Storage used                         |
| sfree  | float    | TB   | Storage free                         |
| dtemp  | int[]    | C    | Temperature per drive (-1 = n/a)     |
| dname  | string[] | —    | Short name per drive (max 8 chars)   |
| dsize  | int[]    | GB   | Capacity per drive                   |

### Network

| Field | Type  | Unit | Description          |
|-------|-------|------|----------------------|
| netdl | float | KB/s | Download throughput   |
| netul | float | KB/s | Upload throughput     |

### CPU Cores

| Field  | Type    | Unit | Description                     |
|--------|---------|------|---------------------------------|
| ccores | float[] | %    | Per-core load (max 16 cores)     |

### Names

| Field   | Type   | Description                     |
|---------|--------|---------------------------------|
| cpuname | string | CPU model name (max 30 chars)    |
| gpuname | string | GPU model name (max 30 chars)    |

## Disk Name Shortening

The Python script automatically shortens drive names for display:

| Original                  | Short Name |
|--------------------------|------------|
| Samsung 980 PRO          | 980PRO     |
| Samsung SSD 870 EVO      | 870EVO     |
| WDC WD101EFBX            | WD101EF    |
| Seagate ST24000NM007H    | ST24T      |

Rules: Remove manufacturer prefixes, strip suffixes like "NVMe"/"SATA", truncate to max 8 characters. Seagate models are mapped to their capacity in TB/GB.

## Data Sources (LibreHardwareMonitor)

| ImageURL     | Hardware Type    | Sensor Groups                                  |
|-------------|-----------------|------------------------------------------------|
| cpu.png      | CPU              | Load, Temperatures, Clocks, Powers, Voltages   |
| nvidia.png   | NVIDIA GPU       | Load, Temperatures, Clocks, Powers, Data, Fans |
| amd.png      | AMD GPU          | (same as nvidia.png)                           |
| ram.png      | RAM              | Load, Data                                     |
| hdd.png      | Drives           | Temperatures, Data                             |
| chip.png     | Motherboard Chips | Fans                                          |
| network.png  | Network Adapters | Throughput                                     |

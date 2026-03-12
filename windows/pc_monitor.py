#!/usr/bin/env python3
"""
PC Hardware Monitor - Windows Serial Sender
Reads hardware data from LibreHardwareMonitor's HTTP server
and sends it as JSON over serial to an ESP32 display.

Requirements:
  1. LibreHardwareMonitor running as admin with web server enabled (port 8085)
  2. Python 3 with: pip install pyserial requests
  3. ESP32 connected via USB (COM port)

Usage:
  python pc_monitor.py              # Auto-detect COM port
  python pc_monitor.py --port COM3  # Specify COM port
  python pc_monitor.py --test       # Test mode (fake data, no serial)
"""

import json
import time
import sys
import argparse
import serial
import serial.tools.list_ports
import requests

LHM_URL = "http://localhost:8085/data.json"
BAUD_RATE = 115200
UPDATE_INTERVAL = 0.5  # seconds (2 Hz)


def find_esp32_port():
    """Auto-detect ESP32 COM port (CP210x or CH340)."""
    ports = serial.tools.list_ports.comports()
    for p in ports:
        desc = (p.description or "").lower()
        if "cp210" in desc or "ch340" in desc:
            print(f"Found ESP32 on {p.device}: {p.description}")
            return p.device
    if ports:
        print(f"No ESP32 detected, using first port: {ports[0].device}")
        return ports[0].device
    return None


def parse_value(val_str):
    """Parse LHM value string like '35,3 %' or '82,3 °C' or '1427 RPM' to float."""
    if not val_str:
        return 0.0
    # Take first token, replace comma with dot
    token = val_str.strip().split()[0].replace(",", ".")
    try:
        return float(token)
    except ValueError:
        return 0.0


def find_hw_node(node, image_key):
    """Find a hardware node by its ImageURL containing the key (e.g. 'cpu', 'nvidia')."""
    results = []
    img = node.get("ImageURL", "")
    if image_key in img.lower():
        results.append(node)
    for child in node.get("Children", []):
        results.extend(find_hw_node(child, image_key))
    return results


def find_sensor_group(node, group_name):
    """Find a child node whose Text matches group_name (e.g. 'Load', 'Temperatures')."""
    for child in node.get("Children", []):
        if child.get("Text", "").lower() == group_name.lower():
            return child
    return None


def get_sensor_value(group_node, name_contains):
    """Get a sensor value from a group node by partial name match."""
    if group_node is None:
        return None
    for child in group_node.get("Children", []):
        text = child.get("Text", "").lower()
        if name_contains.lower() in text:
            return parse_value(child.get("Value", ""))
    return None


def get_all_sensor_values(group_node):
    """Get all sensor name/value pairs from a group node."""
    results = []
    if group_node is None:
        return results
    for child in group_node.get("Children", []):
        results.append({
            "name": child.get("Text", ""),
            "value": parse_value(child.get("Value", ""))
        })
    return results


def collect_hw_data(root):
    """Extract hardware metrics from LibreHardwareMonitor JSON tree."""

    # Find CPU node (image contains 'cpu')
    cpu_nodes = find_hw_node(root, "cpu.png")
    cpu_name = "Unknown CPU"
    cpu_load = 0.0
    cpu_temp = 0.0

    for cpu in cpu_nodes:
        cpu_name = cpu.get("Text", "Unknown CPU")
        # Shorten name
        for prefix in ["Intel ", "AMD "]:
            cpu_name = cpu_name.replace(prefix, "")
        if len(cpu_name) > 30:
            cpu_name = cpu_name[:30]

        # Load
        load_group = find_sensor_group(cpu, "Load")
        val = get_sensor_value(load_group, "cpu total")
        if val is not None:
            cpu_load = val

        # Temperature
        temp_group = find_sensor_group(cpu, "Temperatures")
        # Prefer "Core (Tctl" for AMD, "CPU Package" for Intel
        val = get_sensor_value(temp_group, "tctl")
        if val is None:
            val = get_sensor_value(temp_group, "package")
        if val is None:
            val = get_sensor_value(temp_group, "core")
        if val is not None:
            cpu_temp = val
        break  # Use first CPU

    # Find GPU node (nvidia or amd/ati gpu)
    gpu_nodes = find_hw_node(root, "nvidia.png")
    if not gpu_nodes:
        gpu_nodes = find_hw_node(root, "amd.png")
    gpu_name = "Unknown GPU"
    gpu_load = 0.0
    gpu_temp = 0.0

    for gpu in gpu_nodes:
        gpu_name = gpu.get("Text", "Unknown GPU")
        for prefix in ["NVIDIA ", "AMD ", "GeForce "]:
            gpu_name = gpu_name.replace(prefix, "")
        if len(gpu_name) > 30:
            gpu_name = gpu_name[:30]

        load_group = find_sensor_group(gpu, "Load")
        val = get_sensor_value(load_group, "gpu core")
        if val is not None:
            gpu_load = val

        temp_group = find_sensor_group(gpu, "Temperatures")
        val = get_sensor_value(temp_group, "gpu core")
        if val is not None:
            gpu_temp = val
        break  # Use first GPU

    # CPU clock (average)
    cpu_clock = 0.0
    for cpu in cpu_nodes:
        clk_group = find_sensor_group(cpu, "Clocks")
        val = get_sensor_value(clk_group, "cores (average)")
        if val is None:
            val = get_sensor_value(clk_group, "core #1")
        if val is not None:
            cpu_clock = val
        break

    # CPU power (package)
    cpu_power = 0.0
    for cpu in cpu_nodes:
        pwr_group = find_sensor_group(cpu, "Powers")
        val = get_sensor_value(pwr_group, "package")
        if val is not None:
            cpu_power = val
        break

    # GPU VRAM
    gpu_vram_used = 0.0
    gpu_vram_total = 0.0
    for gpu in gpu_nodes:
        data_group = find_sensor_group(gpu, "Data")
        if data_group:
            val = get_sensor_value(data_group, "gpu memory used")
            if val is not None:
                gpu_vram_used = val
            val = get_sensor_value(data_group, "gpu memory total")
            if val is not None:
                gpu_vram_total = val
        break

    # RAM — find "Total Memory" or "Virtual Memory" node
    ram_nodes = find_hw_node(root, "ram.png")
    ram_pct = 0.0
    ram_used = 0.0
    ram_total = 0.0

    # Prefer "Total Memory" over "Virtual Memory"
    for ram in ram_nodes:
        text = ram.get("Text", "").lower()
        if "total memory" in text:
            load_group = find_sensor_group(ram, "Load")
            val = get_sensor_value(load_group, "memory")
            if val is not None:
                ram_pct = val
            data_group = find_sensor_group(ram, "Data")
            val = get_sensor_value(data_group, "memory used")
            if val is not None:
                ram_used = val
            val = get_sensor_value(data_group, "memory available")
            if val is not None:
                ram_total = ram_used + val
            break

    if ram_total == 0:
        # Fallback: use Virtual Memory
        for ram in ram_nodes:
            text = ram.get("Text", "").lower()
            if "virtual" in text:
                load_group = find_sensor_group(ram, "Load")
                val = get_sensor_value(load_group, "memory")
                if val is not None:
                    ram_pct = val
                data_group = find_sensor_group(ram, "Data")
                val = get_sensor_value(data_group, "memory used")
                if val is not None:
                    ram_used = val
                val = get_sensor_value(data_group, "memory available")
                if val is not None:
                    ram_total = ram_used + val
                break

    if ram_total == 0:
        ram_total = 64.0  # fallback

    # Fans — from mainboard chip node
    fan_list = [-1, -1, -1, -1]
    mb_nodes = find_hw_node(root, "chip.png")
    for chip in mb_nodes:
        fan_group = find_sensor_group(chip, "Fans")
        if fan_group:
            fans = get_all_sensor_values(fan_group)
            idx = 0
            for f in fans:
                if f["value"] > 0 and idx < 4:
                    fan_list[idx] = int(f["value"])
                    idx += 1
            break

    # Storage — sum up all drives (Total Space / Free Space from Data groups under hdd.png nodes)
    hdd_nodes = find_hw_node(root, "hdd.png")
    storage_total_gb = 0.0
    storage_free_gb = 0.0
    for hdd in hdd_nodes:
        data_group = find_sensor_group(hdd, "Data")
        if data_group:
            total = get_sensor_value(data_group, "total space")
            free = get_sensor_value(data_group, "free space")
            if total is not None:
                storage_total_gb += total
            if free is not None:
                storage_free_gb += free

    storage_total_tb = storage_total_gb / 1000.0
    storage_used_tb = (storage_total_gb - storage_free_gb) / 1000.0
    storage_free_tb = storage_free_gb / 1000.0

    # Disk temperatures — get temp + short name for each drive
    disk_temps = []
    disk_names = []
    for hdd in hdd_nodes:
        temp_group = find_sensor_group(hdd, "Temperatures")
        if temp_group:
            temps_list = get_all_sensor_values(temp_group)
            # Use first temp (Composite or Temperature)
            temp_val = -1
            for t in temps_list:
                name_l = t["name"].lower()
                if "warning" in name_l or "critical" in name_l or "resolution" in name_l or "limit" in name_l:
                    continue
                temp_val = int(t["value"])
                break
            # Shorten drive name
            raw_name = hdd.get("Text", "?").strip()
            # Extract short model name
            short = raw_name.replace("Samsung ", "").replace("SSD ", "")
            short = short.replace("TOSHIBA ", "T:").replace("WDC ", "WD:")
            short = short.split()[0] if short else "?"
            if len(short) > 10:
                short = short[:10]
            disk_temps.append(temp_val)
            disk_names.append(short)

    return {
        "cpu": round(cpu_load, 1),
        "gpuload": round(gpu_load, 1),
        "cputemp": round(cpu_temp, 1),
        "gputemp": round(gpu_temp, 1),
        "ram": round(ram_pct, 1),
        "ramused": round(ram_used, 1),
        "ramtotal": round(ram_total, 1),
        "cpuclk": round(cpu_clock, 0),
        "cpupwr": round(cpu_power, 0),
        "gpuvram": round(gpu_vram_used, 0),
        "gpuvtot": round(gpu_vram_total, 0),
        "fan1": fan_list[0],
        "fan2": fan_list[1],
        "stotal": round(storage_total_tb, 1),
        "sused": round(storage_used_tb, 1),
        "sfree": round(storage_free_tb, 1),
        "dtemp": disk_temps,
        "dname": disk_names,
        "cpuname": cpu_name,
        "gpuname": gpu_name,
    }


def fake_data():
    """Generate fake data for testing without LibreHardwareMonitor."""
    import random
    return {
        "cpu": round(random.uniform(5, 95), 1),
        "gpuload": round(random.uniform(0, 80), 1),
        "cputemp": round(random.uniform(35, 85), 1),
        "gputemp": round(random.uniform(30, 75), 1),
        "ram": round(random.uniform(30, 80), 1),
        "ramused": round(random.uniform(8, 48), 1),
        "ramtotal": 64.0,
        "cpuclk": round(random.uniform(3000, 5000), 0),
        "cpupwr": round(random.uniform(30, 200), 0),
        "gpuvram": round(random.uniform(500, 5000), 0),
        "gpuvtot": 6144.0,
        "fan1": random.randint(800, 1500),
        "fan2": random.randint(600, 1200),
        "stotal": 116.5,
        "sused": 92.3,
        "sfree": 24.2,
        "dtemp": [random.randint(28, 55) for _ in range(4)],
        "dname": ["980PRO", "870EVO", "WD10T", "TOSHIBA"],
        "cpuname": "Ryzen 9 5950X",
        "gpuname": "RTX 2060",
    }


def main():
    parser = argparse.ArgumentParser(description="PC Hardware Monitor - Serial Sender")
    parser.add_argument("--port", help="Serial port (e.g. COM3)", default=None)
    parser.add_argument("--test", action="store_true", help="Test mode with fake data")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help="Baud rate")
    args = parser.parse_args()

    # Open serial port
    ser = None
    if not args.test:
        port = args.port or find_esp32_port()
        if not port:
            print("ERROR: No serial port found. Use --port COM3 or --test")
            sys.exit(1)

        try:
            ser = serial.Serial(port, args.baud, timeout=1)
            print(f"Serial port {port} opened at {args.baud} baud")
        except serial.SerialException as e:
            print(f"ERROR: Cannot open {port}: {e}")
            sys.exit(1)
    else:
        print("TEST MODE - generating fake data")

    print(f"Sending data every {UPDATE_INTERVAL}s. Press Ctrl+C to stop.\n")

    while True:
        try:
            if args.test:
                data = fake_data()
            else:
                try:
                    resp = requests.get(LHM_URL, timeout=2)
                    lhm = resp.json()
                    data = collect_hw_data(lhm)
                except requests.RequestException as e:
                    print(f"\nWARNING: Cannot reach LibreHardwareMonitor: {e}")
                    time.sleep(2)
                    continue

            line = json.dumps(data, separators=(",", ":")) + "\n"

            if ser:
                ser.write(line.encode("utf-8"))

            # Print to console
            sys.stdout.write(
                f"\rCPU:{data['cpu']:5.1f}% {data['cputemp']:4.1f}C | "
                f"GPU:{data['gpuload']:5.1f}% {data['gputemp']:4.1f}C | "
                f"RAM:{data['ram']:4.0f}% | "
                f"DISK:{data['sused']:.1f}/{data['stotal']:.1f}TB | "
                f"FAN:{data['fan1']}/{data['fan2']}  "
            )
            sys.stdout.flush()

            time.sleep(UPDATE_INTERVAL)

        except KeyboardInterrupt:
            print("\nStopped.")
            break
        except Exception as e:
            print(f"\nERROR: {e}")
            time.sleep(2)

    if ser:
        ser.close()


if __name__ == "__main__":
    main()

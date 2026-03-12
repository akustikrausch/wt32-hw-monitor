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
        if "cp210" in desc or "ch340" in desc or "usb" in desc:
            print(f"Found ESP32 on {p.device}: {p.description}")
            return p.device
    if ports:
        print(f"No ESP32 detected, using first port: {ports[0].device}")
        return ports[0].device
    return None


def find_sensor(node, sensor_type, name_contains=None):
    """Recursively find a sensor value in the LHM JSON tree."""
    if "Children" in node:
        for child in node["Children"]:
            # Check if this node has matching sensor
            if child.get("SensorType") == sensor_type:
                if name_contains is None or name_contains.lower() in child.get("Text", "").lower():
                    val = child.get("Value", "0")
                    # Parse value like "45.2 %" or "68 °C" or "1200 RPM"
                    val = val.split()[0].replace(",", ".")
                    try:
                        return float(val)
                    except ValueError:
                        return 0.0
            # Recurse
            result = find_sensor(child, sensor_type, name_contains)
            if result is not None:
                return result
    return None


def find_all_sensors(node, sensor_type, results=None):
    """Find all sensors of a given type."""
    if results is None:
        results = []
    if "Children" in node:
        for child in node["Children"]:
            if child.get("SensorType") == sensor_type:
                val_str = child.get("Value", "0").split()[0].replace(",", ".")
                try:
                    val = float(val_str)
                except ValueError:
                    val = 0.0
                results.append({"name": child.get("Text", ""), "value": val})
            find_all_sensors(child, sensor_type, results)
    return results


def find_hw_name(node, hw_type):
    """Find hardware name (CPU, GPU) from the tree."""
    if "Children" in node:
        for child in node["Children"]:
            text = child.get("Text", "")
            image = child.get("ImageURL", "")
            if hw_type.lower() in image.lower() or hw_type.lower() in text.lower():
                return text
            result = find_hw_name(child, hw_type)
            if result:
                return result
    return None


def collect_hw_data(lhm_data):
    """Extract hardware metrics from LibreHardwareMonitor JSON."""
    root = lhm_data

    # Find CPU and GPU nodes
    cpu_name = find_hw_name(root, "cpu") or "Unknown CPU"
    gpu_name = find_hw_name(root, "gpu") or "Unknown GPU"

    # Shorten names
    for prefix in ["Intel ", "AMD ", "NVIDIA ", "GeForce "]:
        cpu_name = cpu_name.replace(prefix, "")
        gpu_name = gpu_name.replace(prefix, "")
    if len(cpu_name) > 30:
        cpu_name = cpu_name[:30]
    if len(gpu_name) > 30:
        gpu_name = gpu_name[:30]

    # CPU load (total)
    loads = find_all_sensors(root, "Load")
    cpu_load = 0
    gpu_load = 0
    ram_pct = 0
    for s in loads:
        name = s["name"].lower()
        if "cpu total" in name:
            cpu_load = s["value"]
        elif "gpu core" in name:
            gpu_load = s["value"]
        elif "memory" in name and "used" not in name and s["value"] < 101:
            ram_pct = s["value"]

    # Temperatures
    temps = find_all_sensors(root, "Temperature")
    cpu_temp = 0
    gpu_temp = 0
    for s in temps:
        name = s["name"].lower()
        if "cpu package" in name or "cpu" in name and cpu_temp == 0:
            cpu_temp = s["value"]
        elif "gpu core" in name or "gpu" in name and gpu_temp == 0:
            gpu_temp = s["value"]

    # Fans
    fans = find_all_sensors(root, "Fan")
    fan_list = [int(f["value"]) for f in fans[:4]]
    while len(fan_list) < 4:
        fan_list.append(-1)

    # RAM
    data_nodes = find_all_sensors(root, "Data")
    ram_used = 0
    ram_total = 0
    for s in data_nodes:
        name = s["name"].lower()
        if "used memory" in name or "memory used" in name:
            ram_used = s["value"]
        elif "available memory" in name:
            ram_total = ram_used + s["value"]

    if ram_total == 0:
        ram_total = 32.0  # fallback
    if ram_pct == 0 and ram_total > 0:
        ram_pct = (ram_used / ram_total) * 100

    return {
        "cpu": round(cpu_load, 1),
        "gpuload": round(gpu_load, 1),
        "cputemp": round(cpu_temp, 1),
        "gputemp": round(gpu_temp, 1),
        "ram": round(ram_pct, 1),
        "ramused": round(ram_used, 1),
        "ramtotal": round(ram_total, 1),
        "fan1": fan_list[0],
        "fan2": fan_list[1],
        "fan3": fan_list[2],
        "fan4": fan_list[3],
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
        "ramused": round(random.uniform(8, 24), 1),
        "ramtotal": 32.0,
        "fan1": random.randint(800, 1500),
        "fan2": random.randint(600, 1200),
        "fan3": -1,
        "fan4": -1,
        "cpuname": "i7-12700K",
        "gpuname": "RTX 3070",
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
                    print(f"WARNING: Cannot reach LibreHardwareMonitor: {e}")
                    time.sleep(2)
                    continue

            line = json.dumps(data, separators=(",", ":")) + "\n"

            if ser:
                ser.write(line.encode("utf-8"))

            # Print to console (compact)
            print(f"CPU:{data['cpu']:5.1f}% {data['cputemp']:4.1f}C | "
                  f"GPU:{data['gpuload']:5.1f}% {data['gputemp']:4.1f}C | "
                  f"RAM:{data['ram']:4.0f}% | "
                  f"FAN:{data['fan1']}/{data['fan2']}", end="\r")

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

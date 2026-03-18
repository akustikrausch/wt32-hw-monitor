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

    # Disk details — temp + short name + size for each drive
    disk_temps = []
    disk_names = []
    disk_sizes = []  # Total GB per drive
    for hdd in hdd_nodes:
        # Temperature
        temp_group = find_sensor_group(hdd, "Temperatures")
        temp_val = -1
        if temp_group:
            temps_list = get_all_sensor_values(temp_group)
            for t in temps_list:
                name_l = t["name"].lower()
                if "warning" in name_l or "critical" in name_l or "resolution" in name_l or "limit" in name_l:
                    continue
                temp_val = int(t["value"])
                break
        # Shorten drive name aggressively for display
        raw_name = hdd.get("Text", "?").strip()
        short = raw_name
        # Remove common prefixes
        for prefix in ["Samsung ", "SSD ", "TOSHIBA ", "WDC ", "Seagate ", "Western Digital ",
                        "HGST ", "Crucial ", "Kingston ", "Intel ", "Micron ", "SK hynix "]:
            short = short.replace(prefix, "")
        # Remove common suffixes/noise
        for noise in [" Series", " SSD", " NVMe", " SATA"]:
            short = short.replace(noise, "")
        # Take first word only
        short = short.split()[0] if short else "?"
        # Truncate long model numbers (e.g. ST24000NM007H -> ST24T)
        if len(short) > 7:
            # Try to make it meaningful: keep prefix + capacity hint
            if short.startswith("ST") and any(c.isdigit() for c in short):
                # Seagate: ST + capacity in TB estimate
                digits = ''.join(c for c in short if c.isdigit())
                if digits:
                    gb = int(digits[:5]) if len(digits) >= 5 else int(digits)
                    if gb > 1000:
                        short = f"ST{gb//1000}T"
                    else:
                        short = f"ST{gb}G"
            elif short.startswith("MG") or short.startswith("MD"):
                short = short[:6]
            else:
                short = short[:7]
        if len(short) > 8:
            short = short[:8]
        # Drive size
        data_group = find_sensor_group(hdd, "Data")
        total_gb = 0
        if data_group:
            val = get_sensor_value(data_group, "total space")
            if val is not None:
                total_gb = round(val, 0)
        disk_temps.append(temp_val)
        disk_names.append(short)
        disk_sizes.append(int(total_gb))

    # Network — find active adapter (most total data transferred)
    net_dl = 0.0  # KB/s
    net_ul = 0.0  # KB/s
    net_util = 0.0  # Network utilization %
    net_data_up = 0.0  # Total data uploaded GB
    net_data_dl = 0.0  # Total data downloaded GB
    net_adapter = "No Network"
    net_nodes = find_hw_node(root, "nic.png")
    if not net_nodes:
        net_nodes = find_hw_node(root, "network.png")
    best_total_data = 0.0
    for net in net_nodes:
        # Pick adapter by total data transferred (most reliable indicator)
        data_group = find_sensor_group(net, "Data")
        total_data = 0.0
        _up = 0.0
        _dl = 0.0
        if data_group:
            val = get_sensor_value(data_group, "uploaded")
            if val is not None:
                _up = val
            val = get_sensor_value(data_group, "downloaded")
            if val is not None:
                _dl = val
            total_data = _up + _dl
        if total_data > best_total_data:
            best_total_data = total_data
            net_data_up = _up
            net_data_dl = _dl
            net_adapter = net.get("Text", "Unknown")
            # Get throughput
            throughput = find_sensor_group(net, "Throughput")
            if throughput:
                val = get_sensor_value(throughput, "download")
                if val is not None:
                    net_dl = val
                val = get_sensor_value(throughput, "upload")
                if val is not None:
                    net_ul = val
            # Get utilization
            load_group = find_sensor_group(net, "Load")
            val = get_sensor_value(load_group, "utilization")
            if val is not None:
                net_util = val

    # GPU extra: core clock, memory clock, power, hot spot temp
    gpu_core_clk = 0.0
    gpu_mem_clk = 0.0
    gpu_power = 0.0
    gpu_hotspot = 0.0
    gpu_fan = -1
    for gpu in gpu_nodes:
        clk_group = find_sensor_group(gpu, "Clocks")
        if clk_group:
            val = get_sensor_value(clk_group, "gpu core")
            if val is not None:
                gpu_core_clk = val
            val = get_sensor_value(clk_group, "gpu memory")
            if val is not None:
                gpu_mem_clk = val
        pwr_group = find_sensor_group(gpu, "Powers")
        if pwr_group:
            val = get_sensor_value(pwr_group, "gpu")
            if val is None:
                val = get_sensor_value(pwr_group, "power")
            if val is None:
                val = get_sensor_value(pwr_group, "package")
            if val is not None:
                gpu_power = val
        temp_group = find_sensor_group(gpu, "Temperatures")
        if temp_group:
            val = get_sensor_value(temp_group, "hot spot")
            if val is not None:
                gpu_hotspot = val
        fan_group = find_sensor_group(gpu, "Fans")
        if fan_group:
            fans = get_all_sensor_values(fan_group)
            for f in fans:
                if f["value"] > 0:
                    gpu_fan = int(f["value"])
                    break
        break

    # CPU per-core loads (up to 16 cores)
    cpu_cores = []
    for cpu in cpu_nodes:
        load_group = find_sensor_group(cpu, "Load")
        if load_group:
            for child in load_group.get("Children", []):
                text = child.get("Text", "").lower()
                if "cpu core" in text and "#" in text:
                    val = parse_value(child.get("Value", ""))
                    cpu_cores.append(round(val, 0))
        break
    # Limit to 16 cores
    cpu_cores = cpu_cores[:16]

    # CPU voltage
    cpu_voltage = 0.0
    for cpu in cpu_nodes:
        volt_group = find_sensor_group(cpu, "Voltages")
        if volt_group:
            val = get_sensor_value(volt_group, "core")
            if val is not None:
                cpu_voltage = val
        break

    # Timestamp: UTC Unix epoch + local timezone offset
    import datetime
    now_local = datetime.datetime.now(datetime.timezone.utc).astimezone()
    utc_offset_sec = int(now_local.utcoffset().total_seconds())

    return {
        "ts": int(time.time()),
        "tzo": utc_offset_sec,
        "cpu": round(cpu_load, 1),
        "gpuload": round(gpu_load, 1),
        "cputemp": round(cpu_temp, 1),
        "gputemp": round(gpu_temp, 1),
        "ram": round(ram_pct, 1),
        "ramused": round(ram_used, 1),
        "ramtotal": round(ram_total, 1),
        "cpuclk": round(cpu_clock, 0),
        "cpupwr": round(cpu_power, 0),
        "cpuvolt": round(cpu_voltage, 3),
        "gpuvram": round(gpu_vram_used, 0),
        "gpuvtot": round(gpu_vram_total, 0),
        "gpuclk": round(gpu_core_clk, 0),
        "gpumclk": round(gpu_mem_clk, 0),
        "gpupwr": round(gpu_power, 0),
        "gpuhs": round(gpu_hotspot, 0),
        "gpufan": gpu_fan,
        "fan1": fan_list[0],
        "fan2": fan_list[1],
        "stotal": round(storage_total_tb, 1),
        "sused": round(storage_used_tb, 1),
        "sfree": round(storage_free_tb, 1),
        "dtemp": disk_temps,
        "dname": disk_names,
        "dsize": disk_sizes,
        "netdl": round(net_dl, 1),
        "netul": round(net_ul, 1),
        "netutil": round(net_util, 1),
        "netdup": round(net_data_up, 1),
        "netddl": round(net_data_dl, 1),
        "netname": net_adapter,
        "ccores": cpu_cores,
        "cpuname": cpu_name,
        "gpuname": gpu_name,
    }


def fake_data():
    """Generate fake data for testing without LibreHardwareMonitor."""
    import random, datetime
    now_local = datetime.datetime.now(datetime.timezone.utc).astimezone()
    utc_offset_sec = int(now_local.utcoffset().total_seconds())
    return {
        "ts": int(time.time()),
        "tzo": utc_offset_sec,
        "cpu": round(random.uniform(5, 95), 1),
        "gpuload": round(random.uniform(0, 80), 1),
        "cputemp": round(random.uniform(35, 85), 1),
        "gputemp": round(random.uniform(30, 75), 1),
        "ram": round(random.uniform(30, 80), 1),
        "ramused": round(random.uniform(8, 48), 1),
        "ramtotal": 64.0,
        "cpuclk": round(random.uniform(3000, 5000), 0),
        "cpupwr": round(random.uniform(30, 200), 0),
        "cpuvolt": round(random.uniform(0.8, 1.45), 3),
        "gpuvram": round(random.uniform(500, 5000), 0),
        "gpuvtot": 6144.0,
        "gpuclk": round(random.uniform(1200, 2100), 0),
        "gpumclk": round(random.uniform(6000, 8000), 0),
        "gpupwr": round(random.uniform(50, 180), 0),
        "gpuhs": round(random.uniform(35, 85), 0),
        "gpufan": random.randint(800, 2000),
        "fan1": random.randint(800, 1500),
        "fan2": random.randint(600, 1200),
        "stotal": 116.5,
        "sused": 92.3,
        "sfree": 24.2,
        "dtemp": [random.randint(28, 55) for _ in range(4)],
        "dname": ["980PRO", "870EVO", "WD10T", "TOSHIBA"],
        "dsize": [1000, 500, 10000, 8000],
        "netdl": round(random.uniform(0, 50000), 1),
        "netul": round(random.uniform(0, 10000), 1),
        "netutil": round(random.uniform(0, 15), 1),
        "netdup": round(random.uniform(5, 200), 1),
        "netddl": round(random.uniform(10, 500), 1),
        "netname": "Ethernet 6",
        "ccores": [round(random.uniform(0, 100), 0) for _ in range(16)],
        "cpuname": "Ryzen 9 9950X",
        "gpuname": "RTX 5090",
    }


def open_serial(port_name, baud):
    """Open serial port, return (serial_obj, port_name) or (None, None)."""
    try:
        ser = serial.Serial(port_name, baud, timeout=1)
        print(f"\nSerial port {port_name} opened at {baud} baud")
        return ser, port_name
    except serial.SerialException as e:
        print(f"\nCannot open {port_name}: {e}")
        return None, None


def wait_for_esp32(fixed_port, baud):
    """Wait until an ESP32 is found, return (serial_obj, port_name)."""
    print("\nWaiting for ESP32...", end="", flush=True)
    while True:
        port = fixed_port or find_esp32_port()
        if port:
            ser, name = open_serial(port, baud)
            if ser:
                return ser, name
        sys.stdout.write(".")
        sys.stdout.flush()
        time.sleep(2)


def wait_for_lhm(timeout=120):
    """Wait until LibreHardwareMonitor web server is reachable."""
    print("Waiting for LibreHardwareMonitor...", end="", flush=True)
    start = time.time()
    while time.time() - start < timeout:
        try:
            resp = requests.get(LHM_URL, timeout=2)
            if resp.status_code == 200:
                print(" OK")
                return True
        except requests.RequestException:
            pass
        sys.stdout.write(".")
        sys.stdout.flush()
        time.sleep(3)
    print(" TIMEOUT (will keep retrying in main loop)")
    return False


def main():
    parser = argparse.ArgumentParser(description="PC Hardware Monitor - Serial Sender")
    parser.add_argument("--port", help="Serial port (e.g. COM3)", default=None)
    parser.add_argument("--test", action="store_true", help="Test mode with fake data")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help="Baud rate")
    args = parser.parse_args()

    ser = None
    current_port = None

    if args.test:
        print("TEST MODE - generating fake data")
    else:
        # Wait for LHM to be available (important for autostart after boot)
        wait_for_lhm()
        ser, current_port = wait_for_esp32(args.port, args.baud)

    print(f"Sending data every {UPDATE_INTERVAL}s. Press Ctrl+C to stop.\n")

    lhm_fail_count = 0

    while True:
        try:
            if args.test:
                data = fake_data()
            else:
                try:
                    resp = requests.get(LHM_URL, timeout=2)
                    lhm = resp.json()
                    data = collect_hw_data(lhm)
                    lhm_fail_count = 0
                except requests.RequestException as e:
                    lhm_fail_count += 1
                    if lhm_fail_count <= 3 or lhm_fail_count % 30 == 0:
                        print(f"\nWARNING: Cannot reach LHM ({lhm_fail_count}x): {e}")
                    time.sleep(2)
                    continue

            line = json.dumps(data, separators=(",", ":")) + "\n"

            if ser:
                try:
                    ser.write(line.encode("utf-8"))
                except (serial.SerialException, OSError):
                    # Connection lost — close and reconnect
                    print(f"\nConnection lost on {current_port}!")
                    try:
                        ser.close()
                    except Exception:
                        pass
                    ser = None
                    ser, current_port = wait_for_esp32(args.port, args.baud)
                    continue

            elif not args.test:
                # No serial connection — try to reconnect
                ser, current_port = wait_for_esp32(args.port, args.baud)
                continue

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

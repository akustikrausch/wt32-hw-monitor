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

Design:
  LibreHardwareMonitor is polled in a background thread. On PCs with several HDDs a
  single /data.json request can take seconds (SMART reads, drives spinning up). The
  sender loop therefore never waits for LHM: it transmits at a fixed 2 Hz, repeating
  the last good data set (with its age) and falling back to a heartbeat once LHM has
  been silent for STALE_LIMIT seconds. The display can tell "PC gone" from "LHM hangs".
"""

import argparse
import datetime
import json
import sys
import threading
import time

import requests
import serial
import serial.tools.list_ports

LHM_URL = "http://localhost:8085/data.json"
BAUD_RATE = 115200
UPDATE_INTERVAL = 0.5      # seconds between serial messages (2 Hz)
LHM_POLL_INTERVAL = 1.0    # LHM refreshes its sensors once per second, polling faster is waste
LHM_TIMEOUT = (2, 10)      # connect / read timeout for LibreHardwareMonitor
STALE_LIMIT = 30           # seconds: after this, send heartbeats instead of old data
WRITE_TIMEOUT = 2          # seconds: a blocked COM port must never freeze the sender
STATUS_EVERY = 600         # seconds between status lines when not running in a console
MAX_DISKS = 8              # the ESP32 displays up to 8 drives

# USB-UART bridges used on ESP32 boards: Silicon Labs CP210x, WCH CH340/CH343
ESP32_USB_VIDS = {0x10C4, 0x1A86}


def log(msg):
    """Event line with timestamp (goes to pc_monitor.log when started hidden)."""
    print(f"{time.strftime('%Y-%m-%d %H:%M:%S')} {msg}", flush=True)


def find_esp32_port():
    """Auto-detect the ESP32 COM port. Never guesses: a wrong port would receive JSON."""
    for p in serial.tools.list_ports.comports():
        desc = (p.description or "").lower()
        if p.vid in ESP32_USB_VIDS or "cp210" in desc or "ch340" in desc or "ch343" in desc:
            return p.device, p.description
    return None, None


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


def parse_speed_kbps(val_str):
    """Parse LHM speed string to KB/s. Handles dynamic units like '5,8 MB/s' or '69,6 KB/s'."""
    if not val_str:
        return 0.0
    val_str = val_str.strip().replace(",", ".")
    parts = val_str.split()
    if len(parts) < 2:
        return parse_value(val_str)
    try:
        num = float(parts[0])
    except ValueError:
        return 0.0
    unit = parts[1].upper()
    if "GB/S" in unit:
        return num * 1024.0 * 1024.0  # GB/s -> KB/s
    elif "MB/S" in unit:
        return num * 1024.0  # MB/s -> KB/s
    elif "KB/S" in unit:
        return num
    elif "B/S" in unit:
        return num / 1024.0  # B/s -> KB/s
    return num  # fallback: assume KB/s


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


def get_sensor_raw(group_node, name_contains):
    """Get raw value string from a sensor by partial name match."""
    if group_node is None:
        return None
    for child in group_node.get("Children", []):
        text = child.get("Text", "").lower()
        if name_contains.lower() in text:
            return child.get("Value", "")
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
    for hdd in hdd_nodes[:MAX_DISKS]:
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
            # Get throughput (use raw string + parse_speed_kbps for unit awareness)
            throughput = find_sensor_group(net, "Throughput")
            if throughput:
                raw = get_sensor_raw(throughput, "download")
                if raw is not None:
                    net_dl = parse_speed_kbps(raw)
                raw = get_sensor_raw(throughput, "upload")
                if raw is not None:
                    net_ul = parse_speed_kbps(raw)
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

    # === ADVANCED DATA ===

    # Motherboard voltages + temps
    mb_vcore = 0.0
    mb_33v = 0.0
    mb_cmos = 0.0
    mb_temps = []       # Board temperatures (up to 6)
    mb_tnames = []      # Short names for board temps
    mb_fan_ctrl = []    # Fan control percentages

    for chip in mb_nodes:
        volt_group = find_sensor_group(chip, "Voltages")
        if volt_group:
            val = get_sensor_value(volt_group, "vcore")
            if val is not None:
                mb_vcore = val
            val = get_sensor_value(volt_group, "+3.3v")
            if val is None:
                val = get_sensor_value(volt_group, "3.3v")
            if val is not None:
                mb_33v = val
            val = get_sensor_value(volt_group, "cmos")
            if val is None:
                val = get_sensor_value(volt_group, "battery")
            if val is not None:
                mb_cmos = val

        temp_group = find_sensor_group(chip, "Temperatures")
        if temp_group:
            for child in temp_group.get("Children", []):
                name = child.get("Text", "")
                val = parse_value(child.get("Value", ""))
                if val > 0 and len(mb_temps) < 6:
                    # Shorten temp name
                    short_name = name.replace("Temperature ", "T")
                    short_name = short_name.replace("temperature ", "T")
                    if len(short_name) > 8:
                        short_name = short_name[:8]
                    mb_temps.append(round(val, 0))
                    mb_tnames.append(short_name)

        ctrl_group = find_sensor_group(chip, "Controls")
        if ctrl_group:
            for child in ctrl_group.get("Children", []):
                val = parse_value(child.get("Value", ""))
                if len(mb_fan_ctrl) < 4:
                    mb_fan_ctrl.append(round(val, 0))
        break

    # CPU advanced: SoC temp, CCD temps, TDC current, Fabric/Bus/Mem clocks
    cpu_soc_temp = 0.0
    cpu_ccd1_temp = 0.0
    cpu_ccd2_temp = 0.0
    cpu_tdc = 0.0
    cpu_fabric_clk = 0.0
    cpu_bus_speed = 0.0
    cpu_mem_clk = 0.0

    for cpu in cpu_nodes:
        temp_group = find_sensor_group(cpu, "Temperatures")
        if temp_group:
            val = get_sensor_value(temp_group, "soc")
            if val is not None:
                cpu_soc_temp = val
            val = get_sensor_value(temp_group, "ccd1")
            if val is None:
                val = get_sensor_value(temp_group, "ccd 1")
            if val is not None:
                cpu_ccd1_temp = val
            val = get_sensor_value(temp_group, "ccd2")
            if val is None:
                val = get_sensor_value(temp_group, "ccd 2")
            if val is not None:
                cpu_ccd2_temp = val

        curr_group = find_sensor_group(cpu, "Currents")
        if curr_group:
            val = get_sensor_value(curr_group, "tdc")
            if val is None:
                val = get_sensor_value(curr_group, "core")
            if val is not None:
                cpu_tdc = val

        clk_group = find_sensor_group(cpu, "Clocks")
        if clk_group:
            val = get_sensor_value(clk_group, "fabric")
            if val is None:
                val = get_sensor_value(clk_group, "uncore")
            if val is not None:
                cpu_fabric_clk = val
            val = get_sensor_value(clk_group, "bus")
            if val is not None:
                cpu_bus_speed = val
            val = get_sensor_value(clk_group, "memory")
            if val is not None:
                cpu_mem_clk = val
        break

    # GPU advanced: voltage, D3D loads, PCIe bandwidth, board power, memory controller load
    gpu_voltage = 0.0
    gpu_mem_ctrl_load = 0.0
    gpu_vid_eng_load = 0.0
    gpu_bus_load = 0.0
    gpu_board_pwr = 0.0
    gpu_fan_ctrl = 0.0
    gpu_pcie_rx = 0.0
    gpu_pcie_tx = 0.0
    gpu_d3d_3d = 0.0
    gpu_d3d_copy = 0.0
    gpu_d3d_vdec = 0.0
    gpu_d3d_venc = 0.0
    gpu_dmem = 0.0
    gpu_smem = 0.0

    for gpu in gpu_nodes:
        volt_group = find_sensor_group(gpu, "Voltages")
        if volt_group:
            val = get_sensor_value(volt_group, "gpu core")
            if val is not None:
                gpu_voltage = val

        load_group = find_sensor_group(gpu, "Load")
        if load_group:
            val = get_sensor_value(load_group, "memory controller")
            if val is not None:
                gpu_mem_ctrl_load = val
            val = get_sensor_value(load_group, "video engine")
            if val is not None:
                gpu_vid_eng_load = val
            val = get_sensor_value(load_group, "bus")
            if val is not None:
                gpu_bus_load = val
            val = get_sensor_value(load_group, "d3d 3d")
            if val is not None:
                gpu_d3d_3d = val
            val = get_sensor_value(load_group, "d3d copy")
            if val is not None:
                gpu_d3d_copy = val
            val = get_sensor_value(load_group, "d3d video decode")
            if val is not None:
                gpu_d3d_vdec = val
            val = get_sensor_value(load_group, "d3d video encode")
            if val is not None:
                gpu_d3d_venc = val

        pwr_group = find_sensor_group(gpu, "Powers")
        if pwr_group:
            val = get_sensor_value(pwr_group, "board")
            if val is None:
                val = get_sensor_value(pwr_group, "total")
            if val is not None:
                gpu_board_pwr = val

        ctrl_group = find_sensor_group(gpu, "Controls")
        if ctrl_group:
            val = get_sensor_value(ctrl_group, "fan")
            if val is not None:
                gpu_fan_ctrl = val

        throughput_group = find_sensor_group(gpu, "Throughput")
        if throughput_group:
            raw = get_sensor_raw(throughput_group, "pcie rx")
            if raw is None:
                raw = get_sensor_raw(throughput_group, "rx")
            if raw is not None:
                gpu_pcie_rx = parse_speed_kbps(raw)
            raw = get_sensor_raw(throughput_group, "pcie tx")
            if raw is None:
                raw = get_sensor_raw(throughput_group, "tx")
            if raw is not None:
                gpu_pcie_tx = parse_speed_kbps(raw)

        data_group = find_sensor_group(gpu, "Data")
        if data_group:
            val = get_sensor_value(data_group, "d3d dedicated")
            if val is not None:
                gpu_dmem = val
            val = get_sensor_value(data_group, "d3d shared")
            if val is not None:
                gpu_smem = val
        break

    # RAM advanced: per-DIMM temperatures, virtual memory
    dimm_temps = []
    vm_used = 0.0
    vm_total = 0.0

    for ram in ram_nodes:
        text = ram.get("Text", "").lower()
        if "virtual" in text:
            data_group = find_sensor_group(ram, "Data")
            if data_group:
                val = get_sensor_value(data_group, "memory used")
                if val is not None:
                    vm_used = val
                val = get_sensor_value(data_group, "memory available")
                if val is not None:
                    vm_total = vm_used + val

    # DIMM temps from RAM nodes or motherboard chip
    for ram in ram_nodes:
        temp_group = find_sensor_group(ram, "Temperatures")
        if temp_group:
            for child in temp_group.get("Children", []):
                val = parse_value(child.get("Value", ""))
                if val > 0 and len(dimm_temps) < 4:
                    dimm_temps.append(round(val, 0))

    # If no DIMM temps from RAM nodes, try motherboard chip
    if not dimm_temps:
        for chip in mb_nodes:
            temp_group = find_sensor_group(chip, "Temperatures")
            if temp_group:
                for child in temp_group.get("Children", []):
                    name = child.get("Text", "").lower()
                    if "dimm" in name or "memory" in name:
                        val = parse_value(child.get("Value", ""))
                        if val > 0 and len(dimm_temps) < 4:
                            dimm_temps.append(round(val, 0))
            break

    # Disk advanced: per-disk read/write throughput, activity
    disk_read = []   # KB/s per disk
    disk_write = []  # KB/s per disk
    disk_act = []    # Activity % per disk

    for hdd in hdd_nodes[:MAX_DISKS]:
        throughput = find_sensor_group(hdd, "Throughput")
        r_speed = 0.0
        w_speed = 0.0
        if throughput:
            raw = get_sensor_raw(throughput, "read")
            if raw is not None:
                r_speed = parse_speed_kbps(raw)
            raw = get_sensor_raw(throughput, "write")
            if raw is not None:
                w_speed = parse_speed_kbps(raw)
        disk_read.append(round(r_speed, 1))
        disk_write.append(round(w_speed, 1))

        load_group = find_sensor_group(hdd, "Load")
        act = 0.0
        if load_group:
            # Try "Total Activity" first, then "Used Space" won't work for activity
            val = get_sensor_value(load_group, "total activity")
            if val is None:
                val = get_sensor_value(load_group, "activity")
            if val is not None:
                act = val
        disk_act.append(round(act, 1))

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
        # Advanced: Motherboard
        "mbvc": round(mb_vcore, 3),
        "mb33": round(mb_33v, 2),
        "mbcm": round(mb_cmos, 2),
        "mbtp": mb_temps,
        "mbtn": mb_tnames,
        "mbfc": mb_fan_ctrl,
        # Advanced: CPU
        "csoc": round(cpu_soc_temp, 0),
        "ccd1": round(cpu_ccd1_temp, 0),
        "ccd2": round(cpu_ccd2_temp, 0),
        "ctdc": round(cpu_tdc, 1),
        "cfab": round(cpu_fabric_clk, 0),
        "cbus": round(cpu_bus_speed, 1),
        "cmcl": round(cpu_mem_clk, 0),
        # Advanced: GPU
        "gvlt": round(gpu_voltage, 3),
        "gmcl": round(gpu_mem_ctrl_load, 0),
        "gvel": round(gpu_vid_eng_load, 0),
        "gbl": round(gpu_bus_load, 0),
        "gbpw": round(gpu_board_pwr, 0),
        "gfct": round(gpu_fan_ctrl, 0),
        "gprx": round(gpu_pcie_rx, 0),
        "gptx": round(gpu_pcie_tx, 0),
        "gd3d": round(gpu_d3d_3d, 0),
        "gdcp": round(gpu_d3d_copy, 0),
        "gdvd": round(gpu_d3d_vdec, 0),
        "gdve": round(gpu_d3d_venc, 0),
        "gdm": round(gpu_dmem, 0),
        "gsm": round(gpu_smem, 0),
        # Advanced: RAM
        "dimt": dimm_temps,
        "vmu": round(vm_used, 1),
        "vmt": round(vm_total, 1),
        # Advanced: Disk I/O
        "drd": disk_read,
        "dwr": disk_write,
        "dact": disk_act,
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
        # Advanced: Motherboard
        "mbvc": round(random.uniform(0.9, 1.45), 3),
        "mb33": round(random.uniform(3.2, 3.4), 2),
        "mbcm": round(random.uniform(2.9, 3.1), 2),
        "mbtp": [random.randint(30, 60) for _ in range(4)],
        "mbtn": ["T1 VRM", "T2 Chip", "T3 PCH", "T4 SB"],
        "mbfc": [random.randint(30, 100) for _ in range(3)],
        # Advanced: CPU
        "csoc": random.randint(35, 55),
        "ccd1": random.randint(50, 75),
        "ccd2": random.randint(50, 75),
        "ctdc": round(random.uniform(10, 120), 1),
        "cfab": random.randint(1600, 2000),
        "cbus": round(random.uniform(99.5, 100.5), 1),
        "cmcl": random.randint(1600, 2000),
        # Advanced: GPU
        "gvlt": round(random.uniform(0.7, 1.1), 3),
        "gmcl": random.randint(0, 80),
        "gvel": random.randint(0, 50),
        "gbl": random.randint(0, 30),
        "gbpw": round(random.uniform(50, 200), 0),
        "gfct": random.randint(20, 100),
        "gprx": round(random.uniform(0, 500000), 0),
        "gptx": round(random.uniform(0, 100000), 0),
        "gd3d": random.randint(0, 95),
        "gdcp": random.randint(0, 20),
        "gdvd": random.randint(0, 30),
        "gdve": random.randint(0, 10),
        "gdm": round(random.uniform(500, 5000), 0),
        "gsm": round(random.uniform(0, 200), 0),
        # Advanced: RAM
        "dimt": [random.randint(40, 55) for _ in range(4)],
        "vmu": round(random.uniform(10, 40), 1),
        "vmt": round(random.uniform(60, 80), 1),
        # Advanced: Disk I/O
        "drd": [round(random.uniform(0, 500000), 0) for _ in range(4)],
        "dwr": [round(random.uniform(0, 200000), 0) for _ in range(4)],
        "dact": [random.randint(0, 100) for _ in range(4)],
    }


def time_fields():
    """UTC Unix time + local UTC offset, sent with every message for the standby clock."""
    now_local = datetime.datetime.now(datetime.timezone.utc).astimezone()
    return {"ts": int(time.time()), "tzo": int(now_local.utcoffset().total_seconds())}


class LhmPoller(threading.Thread):
    """Polls LibreHardwareMonitor continuously and keeps the latest parsed data set."""

    def __init__(self, url):
        super().__init__(daemon=True)
        self.url = url
        self.session = requests.Session()   # keep-alive instead of a new TCP connection per poll
        self.lock = threading.Lock()
        self.data = None
        self.stamp = 0.0
        self.polls = 0
        self.failures = 0
        self.slowest = 0.0

    def snapshot(self):
        """(data, age in seconds) of the latest good poll, or (None, inf)."""
        with self.lock:
            if self.data is None:
                return None, float("inf")
            return self.data, time.monotonic() - self.stamp

    def run(self):
        fail_streak = 0
        while True:
            start = time.monotonic()
            try:
                resp = self.session.get(self.url, timeout=LHM_TIMEOUT)
                resp.raise_for_status()
                data = collect_hw_data(resp.json())
                with self.lock:
                    self.data = data
                    self.stamp = time.monotonic()
                self.polls += 1
                self.slowest = max(self.slowest, time.monotonic() - start)
                if fail_streak:
                    log(f"LibreHardwareMonitor answers again (after {fail_streak} failed polls)")
                fail_streak = 0
            except Exception as e:  # network errors, bad JSON, unexpected sensor tree
                self.failures += 1
                fail_streak += 1
                if fail_streak == 1 or fail_streak % 30 == 0:
                    log(f"WARNING: LibreHardwareMonitor not usable ({fail_streak}x): {e}")
                if fail_streak % 5 == 0:
                    # Drop a possibly broken keep-alive connection
                    self.session.close()
                    self.session = requests.Session()
            time.sleep(max(0.0, LHM_POLL_INTERVAL - (time.monotonic() - start)))


def open_serial(port_name, baud):
    """Open the port without toggling DTR/RTS: on ESP32 boards these lines drive
    EN/IO0 (auto-reset), so the default open would reboot the display."""
    ser = serial.Serial()
    ser.port = port_name
    ser.baudrate = baud
    ser.timeout = 1
    ser.write_timeout = WRITE_TIMEOUT
    ser.dtr = False
    ser.rts = False
    try:
        ser.open()
        log(f"Serial port {port_name} opened at {baud} baud")
        return ser
    except (serial.SerialException, OSError) as e:
        log(f"Cannot open {port_name}: {e}")
        return None


def wait_for_esp32(fixed_port, baud):
    """Block until the ESP32 port can be opened, return the serial object."""
    log("Waiting for ESP32...")
    attempts = 0
    while True:
        port, desc = (fixed_port, fixed_port) if fixed_port else find_esp32_port()
        if port:
            if not fixed_port and attempts == 0:
                log(f"Found ESP32 on {port}: {desc}")
            ser = open_serial(port, baud)
            if ser:
                return ser
        attempts += 1
        time.sleep(2)


def build_message(poller):
    """Latest data with its age, or a heartbeat when LHM has been silent too long."""
    data, age = poller.snapshot()
    if data is not None and age < STALE_LIMIT:
        msg = dict(data)
        msg["age"] = int(age)
    else:
        msg = {"hb": 1}
    msg.update(time_fields())
    return msg


def main():
    parser = argparse.ArgumentParser(description="PC Hardware Monitor - Serial Sender")
    parser.add_argument("--port", help="Serial port (e.g. COM3)", default=None)
    parser.add_argument("--test", action="store_true", help="Test mode with fake data, no serial")
    parser.add_argument("--baud", type=int, default=BAUD_RATE, help="Baud rate")
    parser.add_argument("--url", default=LHM_URL, help="LibreHardwareMonitor data.json URL")
    args = parser.parse_args()

    interactive = sys.stdout.isatty()

    if args.test:
        log("TEST MODE - generating fake data")
        while True:
            data = fake_data()
            data.update(time_fields())
            line = json.dumps(data, separators=(",", ":"))
            sys.stdout.write(f"\r{len(line)} bytes | CPU:{data['cpu']:5.1f}% GPU:{data['gpuload']:5.1f}%  ")
            sys.stdout.flush()
            time.sleep(UPDATE_INTERVAL)

    poller = LhmPoller(args.url)
    poller.start()
    ser = wait_for_esp32(args.port, args.baud)
    log(f"Sending every {UPDATE_INTERVAL}s")

    sent = 0
    heartbeats = 0
    last_status = time.monotonic()
    next_send = time.monotonic()

    while True:
        msg = build_message(poller)
        line = json.dumps(msg, separators=(",", ":")) + "\n"
        try:
            ser.write(line.encode("utf-8"))
            sent += 1
            heartbeats += "hb" in msg
        except (serial.SerialException, OSError) as e:
            # Includes SerialTimeoutException: the port stopped draining
            log(f"Connection lost on {ser.port}: {type(e).__name__}: {e}")
            try:
                ser.close()
            except Exception:
                pass
            time.sleep(1)
            ser = wait_for_esp32(args.port, args.baud)
            continue

        now = time.monotonic()
        if interactive:
            if "hb" in msg:
                sys.stdout.write("\rNo data from LibreHardwareMonitor - sending heartbeat          ")
            else:
                sys.stdout.write(
                    f"\rCPU:{msg['cpu']:5.1f}% {msg['cputemp']:4.1f}C | "
                    f"GPU:{msg['gpuload']:5.1f}% {msg['gputemp']:4.1f}C | "
                    f"RAM:{msg['ram']:4.0f}% | age {msg['age']}s  "
                )
            sys.stdout.flush()
        elif now - last_status >= STATUS_EVERY:
            # Hidden mode: one summary line every 10 minutes instead of a line per message
            log(f"Status: {sent} sent ({heartbeats} heartbeats), LHM polls {poller.polls}, "
                f"failures {poller.failures}, slowest {poller.slowest:.1f}s")
            poller.slowest = 0.0
            last_status = now

        next_send += UPDATE_INTERVAL
        delay = next_send - time.monotonic()
        if delay > 0:
            time.sleep(delay)
        else:
            next_send = time.monotonic()   # fell behind (e.g. after reconnect), resync


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        log("Stopped.")

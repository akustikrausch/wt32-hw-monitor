"""
Per-process CPU, RAM, GPU and VRAM usage via Windows performance counters (PDH),
the same source Task Manager uses.

One PDH query covers all processes: about 30 ms and 0.05 % CPU on a PC with 650
processes. psutil needs about 9 s (and 14 % CPU) for the same data because it opens
every process individually, so it is not used here.

Only ctypes, no extra packages.
"""

import ctypes
import re
import sys
from ctypes import wintypes

PDH_FMT_LONG = 0x00000100
PDH_FMT_DOUBLE = 0x00000200
PDH_FMT_LARGE = 0x00000400
PDH_FMT_NOCAP100 = 0x00008000
PDH_MORE_DATA = 0x800007D2
PDH_CSTATUS_VALID_DATA = 0x0
PDH_CSTATUS_NEW_DATA = 0x1

# Not real applications
SKIP_NAMES = {"_Total", "Idle"}


class _Value(ctypes.Union):
    _fields_ = [("longValue", ctypes.c_long),
                ("doubleValue", ctypes.c_double),
                ("largeValue", ctypes.c_longlong)]


class _FmtValue(ctypes.Structure):
    _fields_ = [("CStatus", wintypes.DWORD), ("u", _Value)]


class _Item(ctypes.Structure):
    _fields_ = [("szName", wintypes.LPWSTR), ("FmtValue", _FmtValue)]


class ProcessSampler:
    """Call sample() periodically (every ~2 s). The first call only primes the rate counters."""

    def __init__(self):
        if sys.platform != "win32":
            raise OSError("ProcessSampler needs Windows")
        self._pdh = ctypes.WinDLL("pdh")
        self._query = wintypes.HANDLE()
        self._check(self._pdh.PdhOpenQueryW(None, None, ctypes.byref(self._query)), "PdhOpenQuery")

        # "Process V2" (Windows 10 1709+) names instances "name:pid", which is unambiguous.
        # The classic "Process" object would need an extra "ID Process" counter per instance.
        self._cpu = self._add(r"\Process V2(*)\% Processor Time")
        self._ram = self._add(r"\Process V2(*)\Working Set - Private")
        # Missing without a WDDM 2.x GPU driver; then GPU columns stay 0
        self._gpu = self._add(r"\GPU Engine(*)\Utilization Percentage", optional=True)
        self._vram = self._add(r"\GPU Process Memory(*)\Dedicated Usage", optional=True)

        import os
        self._ncpu = os.cpu_count() or 1
        self._pdh.PdhCollectQueryData(self._query)
        self.total_processes = 0

    def _check(self, status, what):
        if status != 0:
            raise OSError(f"{what} failed: 0x{status & 0xFFFFFFFF:08X}")

    def _add(self, path, optional=False):
        handle = wintypes.HANDLE()
        status = self._pdh.PdhAddEnglishCounterW(self._query, path, None, ctypes.byref(handle))
        if status != 0:
            if optional:
                return None
            self._check(status, f"PdhAddEnglishCounter {path}")
        return handle

    def _array(self, handle, fmt):
        """{instance name: value} for a wildcard counter."""
        if handle is None:
            return {}
        get = self._pdh.PdhGetFormattedCounterArrayW
        for _ in range(3):  # instance list can change between the two calls
            size = wintypes.DWORD(0)
            count = wintypes.DWORD(0)
            status = get(handle, fmt, ctypes.byref(size), ctypes.byref(count), None) & 0xFFFFFFFF
            if status != PDH_MORE_DATA:
                return {}
            buf = (ctypes.c_byte * size.value)()
            status = get(handle, fmt, ctypes.byref(size), ctypes.byref(count), buf) & 0xFFFFFFFF
            if status == PDH_MORE_DATA:
                continue
            if status != 0:
                return {}
            items = ctypes.cast(buf, ctypes.POINTER(_Item * count.value)).contents
            out = {}
            for it in items:
                if it.FmtValue.CStatus not in (PDH_CSTATUS_VALID_DATA, PDH_CSTATUS_NEW_DATA):
                    continue
                v = it.FmtValue.u
                out[it.szName] = v.doubleValue if fmt & PDH_FMT_DOUBLE else v.largeValue
            return out
        return {}

    def sample(self):
        """List of dicts grouped by process name: name, count, cpu (% of machine),
        ram (bytes, private working set), gpu (%, busiest engine), vram (bytes)."""
        self._pdh.PdhCollectQueryData(self._query)
        cpu = self._array(self._cpu, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100)
        ram = self._array(self._ram, PDH_FMT_LARGE)

        # Per PID: busiest GPU engine (like Task Manager) and summed dedicated memory
        gpu_by_pid = {}
        for inst, v in self._array(self._gpu, PDH_FMT_DOUBLE).items():
            m = re.match(r"pid_(\d+)_", inst)
            if m:
                pid = int(m.group(1))
                gpu_by_pid[pid] = max(gpu_by_pid.get(pid, 0.0), v)
        vram_by_pid = {}
        for inst, v in self._array(self._vram, PDH_FMT_LARGE).items():
            m = re.match(r"pid_(\d+)_", inst)
            if m:
                pid = int(m.group(1))
                vram_by_pid[pid] = vram_by_pid.get(pid, 0) + v

        groups = {}
        total = 0
        for inst, c in cpu.items():
            name, _, pid = inst.rpartition(":")
            if not pid.isdigit() or name in SKIP_NAMES:
                continue
            total += 1
            pid = int(pid)
            g = groups.setdefault(name, {"name": name, "count": 0, "cpu": 0.0, "ram": 0, "gpu": 0.0, "vram": 0})
            g["count"] += 1
            g["cpu"] += c / self._ncpu
            g["ram"] += ram.get(inst, 0)
            g["gpu"] += gpu_by_pid.get(pid, 0.0)
            g["vram"] += vram_by_pid.get(pid, 0)

        for g in groups.values():
            g["cpu"] = min(g["cpu"], 100.0)
            g["gpu"] = min(g["gpu"], 100.0)
        self.total_processes = total
        return list(groups.values())


def top_union(groups, per_key=12):
    """Top N by CPU, by RAM and by GPU combined, so the display can re-sort locally
    by any of these columns without a back channel to the PC."""
    chosen = {}
    for key in ("cpu", "ram", "gpu"):
        ranked = sorted(groups, key=lambda g: g[key], reverse=True)
        for g in ranked[:per_key]:
            if key == "gpu" and g["gpu"] <= 0:
                break
            chosen[g["name"]] = g
    return sorted(chosen.values(), key=lambda g: g["cpu"], reverse=True)


if __name__ == "__main__":
    import time
    s = ProcessSampler()
    time.sleep(2)
    t = time.perf_counter()
    rows = s.sample()
    print(f"{s.total_processes} processes, {len(rows)} names, {1000 * (time.perf_counter() - t):.0f} ms")
    for g in top_union(rows)[:15]:
        print(f"  {g['name'][:24]:24} x{g['count']:<3} cpu {g['cpu']:5.1f}%  ram {g['ram'] / 2**20:7.0f} MB"
              f"  gpu {g['gpu']:5.1f}%  vram {g['vram'] / 2**20:6.0f} MB")

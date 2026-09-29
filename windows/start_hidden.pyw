"""
PC Hardware Monitor - Hidden Launcher
Runs pc_monitor.py without a console window.
Uses pythonw.exe (.pyw extension) so no window appears.
Includes startup delay and automatic restart on crash.
Logs output to pc_monitor.log for debugging.
"""

import subprocess
import sys
import os
import time

# Change to script directory
os.chdir(os.path.dirname(os.path.abspath(__file__)))

PYTHON = sys.executable.replace("pythonw.exe", "python.exe")
SCRIPT = "pc_monitor.py"
LOG_FILE = "pc_monitor.log"
LOG_MAX_BYTES = 5 * 1024 * 1024  # rotate to pc_monitor.log.1 above 5 MB
STARTUP_DELAY = 15  # seconds to wait after boot
RESTART_DELAY = 5   # seconds between restarts


def rotate_log():
    """Keep one previous log; older versions logged a status line twice per second
    and grew to gigabytes."""
    try:
        if os.path.getsize(LOG_FILE) > LOG_MAX_BYTES:
            os.replace(LOG_FILE, LOG_FILE + ".1")
    except OSError:
        pass


# Wait for system to be ready (USB drivers, LHM, etc.)
time.sleep(STARTUP_DELAY)

while True:
    rotate_log()
    try:
        with open(LOG_FILE, "a", encoding="utf-8") as log:
            log.write(f"\n--- Starting {SCRIPT} at {time.strftime('%Y-%m-%d %H:%M:%S')} ---\n")
            log.flush()
            proc = subprocess.Popen(
                [PYTHON, "-u", SCRIPT],
                stdout=log,
                stderr=log,
                creationflags=subprocess.CREATE_NO_WINDOW,
            )
            proc.wait()
            log.write(f"--- Exited with code {proc.returncode} at {time.strftime('%Y-%m-%d %H:%M:%S')} ---\n")
    except Exception as e:
        try:
            with open(LOG_FILE, "a") as log:
                log.write(f"--- Launcher error: {e} ---\n")
        except Exception:
            pass
    time.sleep(RESTART_DELAY)

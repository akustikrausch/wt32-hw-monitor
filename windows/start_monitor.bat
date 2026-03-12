@echo off
:: PC Hardware Monitor - Autostart Script
:: Wait for system to be ready (LHM, USB drivers, etc.)
echo Waiting 15 seconds for system startup...
timeout /t 15 /nobreak >nul

cd /d "%~dp0"

:loop
echo Starting PC Hardware Monitor...
C:\Users\andre\AppData\Local\Programs\Python\Python314-32\python.exe -u pc_monitor.py
echo Script exited. Restarting in 5 seconds...
timeout /t 5 /nobreak >nul
goto loop

@echo off
echo Return boot console to HDMI (remove Composite-1 from cmdline)
echo Pi OFF, SD in reader, Administrator required
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix-hdmi-console.ps1" -DiskNumber 2
pause

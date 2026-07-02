@echo off
echo ============================================
echo  FULL re-flash SD + correct config
echo  Erases SD disk 2, writes Pi OS, sets network
echo  Administrator required
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare-sd-card.ps1" -DiskNumber 2
echo.
pause

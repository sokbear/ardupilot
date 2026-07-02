@echo off
echo ============================================
echo  RESET Pi network (cloud-init re-run)
echo  SD in reader, disk 2, Administrator required
echo  Do NOT format any Windows prompts!
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0reset-pi-network.ps1" -DiskNumber 2
echo.
pause

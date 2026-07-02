@echo off
echo ============================================
echo  Patch boot on SD (disk 2) - NO re-flash
echo  Administrator required - approve UAC
echo  Do NOT format if Windows asks!
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0patch-boot-only.ps1" -DiskNumber 2
echo.
pause

@echo off
echo Use minimal user-data (no apt on first boot) and re-patch boot on Z:
echo Administrator required. SD card must be in reader (disk 2).
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "Copy-Item '%~dp0sd-card\user-data-minimal' '%~dp0sd-card\user-data' -Force; ^
   & '%~dp0fix-sd-drive-letter.ps1' -DiskNumber 2"
pause

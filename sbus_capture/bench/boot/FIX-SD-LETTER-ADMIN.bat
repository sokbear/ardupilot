@echo off
echo ============================================
echo  Fix SD card drive letter (disk 2)
echo  Assigns letter to Pi BOOT partition only
echo  Do NOT format drive F or any prompt!
echo  Administrator required
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix-sd-drive-letter.ps1" -DiskNumber 2
echo.
pause

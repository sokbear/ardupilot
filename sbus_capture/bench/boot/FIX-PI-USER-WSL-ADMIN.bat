@echo off
echo ============================================
echo  Create pilot user inside Pi root (WSL/ext4)
echo  Pi OFF, SD in reader, Administrator required
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0fix-pi-root-user.ps1" -DiskNumber 2
echo.
pause

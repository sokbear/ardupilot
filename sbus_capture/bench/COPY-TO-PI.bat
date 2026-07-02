@echo off
echo ============================================
echo  Copy sbus_capture to Raspberry Pi (scp)
echo  Run in Windows PowerShell — NOT inside SSH
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0copy-to-pi.ps1" %*
echo.
pause

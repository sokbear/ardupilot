@echo off
echo ============================================
echo  Flash SD for Raspberry Pi 5 (drive H:)
echo  Administrator rights required - approve UAC
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare-sd-card.ps1" -DriveLetter H
echo.
if errorlevel 1 (
  echo ERROR - see messages above.
) else (
  echo OK - eject SD and insert into Pi 5.
)
pause

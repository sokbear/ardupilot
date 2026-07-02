@echo off
echo ============================================
echo  Create SSH user pilot on SD (userconf.txt)
echo  Pi must be OFF. SD in reader. Admin required.
echo ============================================
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0set-pilot-user.ps1" -DiskNumber 2
echo.
pause

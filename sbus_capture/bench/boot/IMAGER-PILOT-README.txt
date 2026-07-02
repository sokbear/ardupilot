Raspberry Pi Imager — create pilot user (no WSL needed)
========================================================

If FIX-PI-USER-WSL-ADMIN.bat fails, use Imager GUI:

1. Install "Raspberry Pi Imager" (winget install RaspberryPiFoundation.RaspberryPiImager)

2. Open Imager:
   - OS: Raspberry Pi OS (other) -> Raspberry Pi OS Lite (64-bit)
   - Storage: your SD card (disk 2, ~119 GB)

3. Click GEAR icon (Customize):
   - Hostname: epilot-bench
   - Username: pilot
   - Password: epilot2026
   - Configure wireless: OFF (use Ethernet)
   - Enable SSH: YES (password authentication)

4. Write and wait.

5. After write, run as Admin (optional, for bench PAL/I2C):
   FIX-HDMI-CONSOLE-ADMIN.bat
   (patches config.txt only, does not re-flash)

6. Boot Pi. Login:
   ssh pilot@192.168.1.100
   password: epilot2026

NOTE: Imager ERASES the card. You will lose current install but get working SSH.

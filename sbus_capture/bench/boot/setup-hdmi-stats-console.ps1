# HDMI-консоль только для PERF-статистики bench: без getty/login на tty1.
$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$BenchDir  = Split-Path -Parent $ScriptDir
. (Join-Path $BenchDir "pi-ssh-common.ps1")

Write-Host "disable getty@tty1 (no login on HDMI) ..."
Invoke-PiSsh "echo epilot2026 | sudo -S systemctl disable --now getty@tty1.service"
Invoke-PiSsh "ls -l /dev/tty1"

Write-Host "restart epilot-bench ..."
Invoke-PiSsh "echo epilot2026 | sudo -S systemctl restart epilot-bench.service"
Start-Sleep -Seconds 12
Invoke-PiSsh "systemctl is-active epilot-bench.service; journalctl -u epilot-bench -n 20 --no-pager"

Write-Host "Done."

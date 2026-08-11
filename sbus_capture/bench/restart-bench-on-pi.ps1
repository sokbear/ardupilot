$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "pi-ssh-common.ps1")

Write-Host "status:"
Invoke-PiSsh "pgrep -af electronic_pilot_bench | grep -v pgrep || echo not_running"

Write-Host "start bench:"
Invoke-PiSsh "bash -c 'cd /home/pilot/ardupilot/sbus_capture/bench && setsid ./build/electronic_pilot_bench >> /home/pilot/bench.log 2>&1 < /dev/null & exit 0'"

Start-Sleep -Seconds 3
Write-Host "after start:"
Invoke-PiSsh "pgrep -af electronic_pilot_bench | grep -v pgrep; tail -n 12 /home/pilot/bench.log"

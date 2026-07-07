$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "pi-ssh-common.ps1")

Write-Host "status:"
Invoke-PiSsh "pgrep -a electronic_pilot_bench || echo not_running"

Write-Host "start bench:"
Invoke-PiSsh "bash -c 'setsid /home/pilot/ardupilot/sbus_capture/bench/build/electronic_pilot_bench >> /home/pilot/bench.log 2>&1 < /dev/null & exit 0'"

Start-Sleep -Seconds 3
Write-Host "after start:"
Invoke-PiSsh "pgrep -a electronic_pilot_bench; tail -n 12 /home/pilot/bench.log"

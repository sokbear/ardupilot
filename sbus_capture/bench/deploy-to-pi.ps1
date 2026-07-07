# Deploy bench sources to Pi and rebuild.
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "pi-ssh-common.ps1")

$remote = "/home/pilot/ardupilot/sbus_capture/bench"
$files  = @(
    "marker_tracker.cpp",
    "marker_tracker.h",
    "config.h",
    "CMakeLists.txt"
)

Write-Host "ensure remote dir ..."
Invoke-PiSsh "mkdir -p ~/ardupilot/sbus_capture/bench/build"

foreach ($f in $files) {
    $local = Join-Path $PSScriptRoot $f
    Write-Host "scp $f ..."
    Invoke-PiScp -LocalPath $local -RemotePath $remote
}

Write-Host "cmake + make on Pi ..."
Invoke-PiSsh "cd ~/ardupilot/sbus_capture/bench && mkdir -p build && cd build && cmake .. && make -j4"

Write-Host "restart bench ..."
Invoke-PiSsh "bash -c 'pkill -f electronic_pilot_bench || true; exit 0'"
Start-Sleep -Seconds 1
Invoke-PiSsh "sh -c 'setsid /home/pilot/ardupilot/sbus_capture/bench/build/electronic_pilot_bench >> /home/pilot/bench.log 2>&1 < /dev/null &'"
Start-Sleep -Seconds 3
Invoke-PiSsh "pgrep -a electronic_pilot_bench; tail -n 15 /home/pilot/bench.log"

Write-Host "Done."

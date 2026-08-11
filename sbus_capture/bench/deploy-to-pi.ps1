# Deploy bench sources to Pi and rebuild.
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "pi-ssh-common.ps1")

$remote = "/home/pilot/ardupilot/sbus_capture/bench"
$files  = @(
    "main.cpp",
    "marker_tracker.cpp",
    "marker_tracker.h",
    "target_verifier.cpp",
    "target_verifier.h",
    "console_stats.cpp",
    "console_stats.h",
    "config.h",
    "osd_renderer.cpp",
    "osd_renderer.h",
    "pca9685.h",
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
Invoke-PiSsh "killall electronic_pilot_bench 2>/dev/null; true"
Start-Sleep -Seconds 1
Invoke-PiSsh "cd /home/pilot/ardupilot/sbus_capture/bench && setsid ./build/electronic_pilot_bench >> /home/pilot/bench.log 2>&1 < /dev/null &"
Start-Sleep -Seconds 3
Invoke-PiSsh "pgrep -af electronic_pilot_bench | grep -v pgrep; strings /home/pilot/bench.log | tail -n 10"

Write-Host "Done."

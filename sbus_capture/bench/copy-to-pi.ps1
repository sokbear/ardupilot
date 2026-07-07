# Copy sbus_capture to Raspberry Pi via scp.
# Run from Windows PowerShell (not inside SSH on the Pi).

param(
    [string]$PiHost = "pilot@192.168.1.100",
    [string]$RemoteDir = "~/ardupilot"
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "pi-ssh-common.ps1")

$SbusDir = (Get-Item -LiteralPath (Join-Path $PSScriptRoot "..")).FullName
Write-Host "Source: $SbusDir"
Write-Host "Target: ${PiHost}:${RemoteDir}/"
Write-Host ""

if (-not (Test-Path -LiteralPath $SbusDir)) {
    throw "Folder not found: $SbusDir"
}

Write-Host "Creating remote directory..."
Invoke-PiSsh "mkdir -p $RemoteDir"

Write-Host "Copying..."
Invoke-PiScp -LocalPath $SbusDir -RemotePath $RemoteDir -Recursive

Write-Host ""
Write-Host "Done. On the Pi run:"
Write-Host "  ls ~/ardupilot/sbus_capture/bench"
Write-Host "  cd ~/ardupilot/sbus_capture/bench && mkdir -p build && cd build && cmake .. && make"

# Download ~/perf_after.log from Pi.
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "pi-ssh-common.ps1")

$dest = Join-Path $PSScriptRoot "logs\perf_after.log"
New-Item -ItemType Directory -Force -Path (Split-Path $dest) | Out-Null

Write-Host "remote size:"
Invoke-PiSsh "wc -c /home/pilot/perf_after.log"

Write-Host "download ..."
& pscp -batch -pw $script:PiSshPassword "$($script:PiSshHost):/home/pilot/perf_after.log" $dest
if ($LASTEXITCODE -ne 0) { throw "pscp failed ($LASTEXITCODE)" }

Get-Item $dest | Format-Table Name, Length, LastWriteTime -AutoSize
Write-Host "Saved to $dest"

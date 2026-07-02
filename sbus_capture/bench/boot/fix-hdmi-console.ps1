# Move boot console back to HDMI (remove video=Composite-1 from cmdline).
# Keeps dtoverlay=vc4-kms-v3d,composite in config.txt for electronic_pilot_bench.

param([int]$DiskNumber = 2)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "boot-common.ps1")

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { Write-Error "Run as Administrator." }

$bootRoot = Find-BootRootOnDisk -Num $DiskNumber
if (-not $bootRoot) { throw "Boot partition not found on disk $DiskNumber" }

$cmdlineTxt = Join-Path $bootRoot "cmdline.txt"
if (-not (Test-Path $cmdlineTxt)) { $cmdlineTxt = Join-Path $bootRoot "firmware\cmdline.txt" }

$cmd = (Get-Content $cmdlineTxt -Raw).Trim()
if ($cmd -notmatch 'root=PARTUUID=') { throw "cmdline.txt missing root=PARTUUID=" }

$cmd = $cmd -replace '\s*video=Composite-1:[^\s]+', ''
$cmd = $cmd -replace '\s*video=HDMI-A-1:[^\s]+', ''
$cmd = $cmd -replace '\s*fbcon=map:\d+', ''
if ($cmd -notmatch 'console=tty1') {
    if ($cmd -match 'console=ttyAMA10,115200') {
        $cmd = $cmd -replace 'console=ttyAMA10,115200', 'console=ttyAMA10,115200 console=tty1'
    } else {
        $cmd = $cmd -replace 'console=serial0,115200', 'console=serial0,115200 console=tty1'
    }
}
if ($cmd -notmatch 'net\.ifnames=0') { $cmd = $cmd + ' net.ifnames=0' }

Set-Content -Path $cmdlineTxt -Value $cmd.Trim() -NoNewline -Encoding ascii
Write-Host "Fixed: $cmdlineTxt"
Write-Host $cmd
Write-Host ""
Write-Host "Login will use HDMI. Composite stays for electronic_pilot_bench (config.txt overlay)."

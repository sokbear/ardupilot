# Force cloud-init to run again (network fix for Pi 5 end0).
# Required because cloud-init only applies network-config on FIRST boot.

param([int]$DiskNumber = 2)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SdCardDir = Join-Path $ScriptDir "sd-card"
. (Join-Path $ScriptDir "boot-common.ps1")

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { Write-Error "Run as Administrator." }

$bootRoot = Find-BootRootOnDisk -Num $DiskNumber
if (-not $bootRoot) { throw "Boot partition not found on disk $DiskNumber" }

Write-Host "Network recovery on $bootRoot"
Apply-NetworkRecovery -BootRoot $bootRoot -SdCardDir $SdCardDir

Write-Host ""
Write-Host "Done. Safely eject SD, boot Pi 5, wait 3-5 min."
Write-Host "  ssh pilot@epilot-bench   password: epilot2026"
Write-Host "  Check router DHCP list if hostname fails."

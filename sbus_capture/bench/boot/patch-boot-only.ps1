# Patch boot on SD card (image already written). Run as Administrator.

param(
    [string]$BootDrive = "",
    [int]$DiskNumber = 2
)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SdCardDir = Join-Path $ScriptDir "sd-card"
. (Join-Path $ScriptDir "boot-common.ps1")

if ($BootDrive) {
    $root = if ($BootDrive.EndsWith('\')) { $BootDrive } else { "$BootDrive`:\" }
    Apply-BootPatches -BootRoot $root -SdCardDir $SdCardDir
    exit 0
}

$bootRoot = Find-BootRootOnDisk -Num $DiskNumber
if (-not $bootRoot) {
    throw "Pi boot partition not found on disk $DiskNumber. Insert SD card and retry."
}

Apply-BootPatches -BootRoot $bootRoot -SdCardDir $SdCardDir
Write-Host ""
Write-Host "OK. Eject SD and boot Pi 5."
Write-Host "  User: pilot  Password: epilot2026"

# Assign drive letter to Pi boot partition (FAT32 part 1) on SD card.
# Windows often assigns F: to the Linux ext4 partition instead - do NOT format that.

param([int]$DiskNumber = 2)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "boot-common.ps1")

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { Write-Error "Run as Administrator." }

$p1 = Get-Partition -DiskNumber $DiskNumber -PartitionNumber 1 -ErrorAction Stop
$p2 = Get-Partition -DiskNumber $DiskNumber -PartitionNumber 2 -ErrorAction SilentlyContinue

Write-Host "Disk $DiskNumber :"
Write-Host "  part 1 (boot FAT32): letter=$($p1.DriveLetter) $([math]::Round($p1.Size/1MB)) MB"
if ($p2) {
    Write-Host "  part 2 (Linux ext4): letter=$($p2.DriveLetter) $([math]::Round($p2.Size/1MB)) MB  <- Windows cannot read this"
}

if ($p2 -and $p2.DriveLetter) {
    $bad = "$($p2.DriveLetter):\"
    Write-Host "Removing letter $($p2.DriveLetter): from Linux partition (optional, reduces format prompts)..."
    Remove-PartitionAccessPath -DiskNumber $DiskNumber -PartitionNumber 2 -AccessPath $bad -ErrorAction SilentlyContinue
}

$letter = $p1.DriveLetter
if (-not $letter) {
    $letter = Get-FreeDriveLetter
    Write-Host "Assigning ${letter}: to boot partition 1 ..."
    Set-Partition -DiskNumber $DiskNumber -PartitionNumber 1 -NewDriveLetter $letter
    Start-Sleep -Seconds 2
} else {
    Write-Host "Boot partition already has letter ${letter}:"
}

$bootRoot = "${letter}:\"
if (-not (Test-BootRoot $bootRoot)) {
    throw "Boot files not found on ${letter}:. Image may be corrupted - re-flash required."
}

Write-Host "Boot OK: $bootRoot"
$SdCardDir = Join-Path $ScriptDir "sd-card"
Apply-BootPatches -BootRoot $bootRoot -SdCardDir $SdCardDir

Write-Host ""
Write-Host "SUCCESS. Open ${letter}: in Explorer - you should see firmware folder."
Write-Host "Eject SD safely and insert into Pi 5."
Write-Host "  ssh pilot@epilot-bench   password: epilot2026"

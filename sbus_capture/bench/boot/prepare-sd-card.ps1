# Prepare microSD for Raspberry Pi 5 (electronic_pilot_bench).
# Run as Administrator:
#   powershell -ExecutionPolicy Bypass -File prepare-sd-card.ps1 -DriveLetter H

param(
    [string]$DriveLetter = "H",
    [int]$DiskNumber = -1,
    [switch]$PatchOnly
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$SdCardDir = Join-Path $ScriptDir "sd-card"
. (Join-Path $ScriptDir "boot-common.ps1")
$WorkDir   = Join-Path $env:TEMP "epilot-sd-prep"
$ImageUrl  = "https://downloads.raspberrypi.com/raspios_lite_arm64/images/raspios_lite_arm64-2026-06-19/2026-06-18-raspios-trixie-arm64-lite.img.xz"
$XzPath    = Join-Path $WorkDir "raspios-lite-arm64.img.xz"
$ImgPath   = Join-Path $WorkDir "raspios-lite-arm64.img"

function Test-Admin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    $p  = New-Object Security.Principal.WindowsPrincipal($id)
    return $p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Resolve-DiskNumber {
    param([string]$Letter)
    $part = Get-Partition -DriveLetter $Letter -ErrorAction Stop
    return [int]$part.DiskNumber
}

function Dismount-AllDiskVolumes {
    param([int]$Num)
    Write-Host "Dismounting all volumes on disk $Num ..."
    Get-Partition -DiskNumber $Num -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.DriveLetter) {
            $path = "$($_.DriveLetter):\"
            Write-Host "  removing $path"
            Remove-PartitionAccessPath -DiskNumber $Num -PartitionNumber $_.PartitionNumber -AccessPath $path -ErrorAction SilentlyContinue
        }
    }
    Start-Sleep -Seconds 2
}

function Clear-DiskWithDiskpart {
    param([int]$Num)
    Write-Host "diskpart: clean disk $Num ..."
    $lines = @(
        "select disk $Num",
        "attributes disk clear readonly",
        "clean",
        "exit"
    )
    $lines | diskpart | ForEach-Object { Write-Host "  $_" }
    Start-Sleep -Seconds 2
}

function Write-RawImage {
    param(
        [string]$ImagePath,
        [string]$PhysicalDrive,
        [int]$Num
    )

    Dismount-AllDiskVolumes -Num $Num
    Get-Disk -Number $Num | Set-Disk -IsReadOnly $false -ErrorAction SilentlyContinue
    Clear-DiskWithDiskpart -Num $Num

    $imgSize = (Get-Item $ImagePath).Length
    Write-Host "Writing $([math]::Round($imgSize/1GB, 2)) GB to $PhysicalDrive (several minutes)..."

    $src = [System.IO.File]::Open(
        $ImagePath,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read
    )
    try {
        $dst = [System.IO.File]::Open(
            $PhysicalDrive,
            [System.IO.FileMode]::Open,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::ReadWrite
        )
        try {
            $buffer = New-Object byte[] (4MB)
            $total = 0L
            while (($read = $src.Read($buffer, 0, $buffer.Length)) -gt 0) {
                $dst.Write($buffer, 0, $read)
                $total += $read
                $pct = [math]::Min(100, [math]::Round(100.0 * $total / $imgSize, 1))
                Write-Progress -Activity "Writing SD image" -Status "$pct %" -PercentComplete $pct
            }
            $dst.Flush()
            Write-Progress -Activity "Writing SD image" -Completed
        } finally {
            $dst.Close()
        }
    } finally {
        $src.Close()
    }
    Write-Host "Write complete."
}

function Invoke-RpiImagerCli {
    param(
        [string]$ImagePath,
        [int]$Num
    )
    $imager = @(
        "${env:ProgramFiles}\Raspberry Pi Imager\rpi-imager.exe",
        "${env:ProgramFiles(x86)}\Raspberry Pi Imager\rpi-imager.exe",
        "$env:LOCALAPPDATA\Programs\Raspberry Pi Imager\rpi-imager.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if (-not $imager) {
        $found = Get-ChildItem "${env:ProgramFiles}" -Filter "rpi-imager.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $imager = $found.FullName }
    }
    if (-not $imager) { return $false }

    $userdata = Join-Path $SdCardDir "user-data"
    $netcfg   = Join-Path $SdCardDir "network-config"
    $dst      = "\\.\PHYSICALDRIVE$Num"
    Write-Host "Fallback: Raspberry Pi Imager CLI ..."
    $args = @(
        "--cli", "--disable-verify",
        "--cloudinit-userdata", $userdata,
        "--cloudinit-networkconfig", $netcfg,
        $ImagePath,
        $dst
    )
    & $imager @args
    return ($LASTEXITCODE -eq 0)
}

if (-not (Test-Admin)) {
    Write-Error "Run PowerShell as Administrator."
}

if ($DiskNumber -lt 0) {
    if ($PatchOnly) {
        $DiskNumber = 2
    } else {
        try {
            $DiskNumber = Resolve-DiskNumber -Letter $DriveLetter
        } catch {
            throw "Drive ${DriveLetter}: not found. Re-insert SD card or pass -DiskNumber 2"
        }
    }
}

$disk = Get-Disk -Number $DiskNumber
Write-Host "Target disk: #$DiskNumber $($disk.FriendlyName) $([math]::Round($disk.Size/1GB,1)) GB"
Write-Host "WARNING: all data on this disk will be erased."
if ($disk.Size -lt 8GB) { throw "Disk too small (need >= 8 GB)." }

if (-not $PatchOnly) {
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

if (-not (Test-Path $ImgPath)) {
    if (-not (Test-Path $XzPath)) {
        Write-Host "Downloading Raspberry Pi OS Lite 64-bit..."
        Invoke-WebRequest -Uri $ImageUrl -OutFile $XzPath -UseBasicParsing
    }
    Write-Host "Extracting .img.xz..."
    $sevenZip = @(
        "${env:ProgramFiles}\7-Zip\7z.exe",
        "${env:ProgramFiles(x86)}\7-Zip\7z.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1

    if ($sevenZip) {
        & $sevenZip x $XzPath "-o$WorkDir" -y | Out-Null
        $extracted = Get-ChildItem $WorkDir -Filter "*.img" | Select-Object -First 1
        if ($extracted -and $extracted.FullName -ne $ImgPath) {
            Move-Item $extracted.FullName $ImgPath -Force
        }
    } else {
        Write-Host "7-Zip not found, trying tar..."
        tar -xf $XzPath -C $WorkDir
        $extracted = Get-ChildItem $WorkDir -Filter "*.img" | Select-Object -First 1
        if ($extracted -and $extracted.FullName -ne $ImgPath) {
            Move-Item $extracted.FullName $ImgPath -Force
        }
    }
}

if (-not (Test-Path $ImgPath)) {
    throw "Image .img not found after extract. Install 7-Zip or run tar manually."
}

$physical = "\\.\PhysicalDrive$DiskNumber"
$written = $false
try {
    Write-RawImage -ImagePath $ImgPath -PhysicalDrive $physical -Num $DiskNumber
    $written = $true
} catch {
    Write-Host "Raw write failed: $($_.Exception.Message)"
    if (Invoke-RpiImagerCli -ImagePath $ImgPath -Num $DiskNumber) {
        $written = $true
    } else {
        throw @"
Could not write to $physical .
Try: Raspberry Pi Imager GUI -> OS Lite 64-bit -> select SD card.
Then run: powershell -File patch-boot-only.ps1
"@
    }
}

if (-not $written) { throw "SD write failed." }
} else {
    Write-Host "PatchOnly: skipping image write (OS already on disk $DiskNumber)."
}

Write-Host "Looking for boot partition on disk $DiskNumber ..."
$bootRoot = Find-BootRootOnDisk -Num $DiskNumber
if (-not $bootRoot) {
    throw "Boot partition not found on disk $DiskNumber. Run PATCH-SD-ADMIN.bat after re-inserting SD."
}

Write-Host "Configuring $bootRoot"
Apply-NetworkRecovery -BootRoot $bootRoot -SdCardDir $SdCardDir

Write-Host ""
Write-Host "Done. Eject SD and insert into Pi 5."
Write-Host "  User:     pilot"
Write-Host "  Password: epilot2026"
Write-Host "  SSH:      ssh pilot@epilot-bench"

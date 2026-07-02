# Shared helpers for SD card preparation (Pi 5 bench).

function Test-BootRoot {
    param([string]$Root)
    if (Test-Path (Join-Path $Root "firmware\config.txt")) { return $true }
    if (Test-Path (Join-Path $Root "config.txt")) { return $true }
    return $false
}

function Get-FreeDriveLetter {
    $used = @(Get-PSDrive -PSProvider FileSystem | ForEach-Object { $_.Name })
    foreach ($code in 90..68) {
        $ch = [string][char]$code
        if ($ch -notin $used) { return $ch }
    }
    throw "No free drive letter (D-Z)."
}

function Find-BootRootOnDisk {
    param([int]$Num)

    Update-HostStorageCache
    Start-Sleep -Seconds 2

    $parts = Get-Partition -DiskNumber $Num -ErrorAction Stop |
        Where-Object { $_.PartitionNumber -gt 0 } |
        Sort-Object PartitionNumber

    Write-Host "Partitions on PhysicalDrive${Num}:"
    foreach ($p in $parts) {
        $mb = [math]::Round($p.Size / 1MB, 0)
        Write-Host "  part $($p.PartitionNumber): letter='$($p.DriveLetter)' ${mb} MB"
    }

    foreach ($p in $parts) {
        $letter = $p.DriveLetter
        if (-not $letter) {
            $vol = Get-Volume -Partition $p -ErrorAction SilentlyContinue
            if ($vol -and $vol.FileSystem -eq 'FAT32') {
                $letter = Get-FreeDriveLetter
                Write-Host "Assigning ${letter}: to partition $($p.PartitionNumber) ..."
                Set-Partition -DiskNumber $Num -PartitionNumber $p.PartitionNumber -NewDriveLetter $letter
                Start-Sleep -Seconds 2
                $letter = (Get-Partition -DiskNumber $Num -PartitionNumber $p.PartitionNumber).DriveLetter
            }
        }
        if ($letter) {
            $root = "${letter}:\"
            if (Test-BootRoot $root) {
                Write-Host "Boot partition found: $root"
                return $root
            }
        }
    }

    $letter = Get-FreeDriveLetter
    Write-Host "diskpart: assign ${letter}: to disk $Num partition 1 ..."
    @(
        "select disk $Num",
        "select partition 1",
        "assign letter=$letter",
        "exit"
    ) | diskpart | ForEach-Object { Write-Host "  $_" }
    Start-Sleep -Seconds 2

    $root = "${letter}:\"
    if (Test-BootRoot $root) {
        Write-Host "Boot partition found: $root"
        return $root
    }

    return $null
}

function Apply-BootPatches {
    param(
        [string]$BootRoot,
        [string]$SdCardDir
    )

    $fw = Join-Path $BootRoot "firmware"
    if (-not (Test-Path $fw)) { $fw = $BootRoot }

    $configTxt = Join-Path $fw "config.txt"
    $cmdlineTxt = Join-Path $fw "cmdline.txt"

    if (-not (Test-Path $configTxt)) { throw "config.txt not found in $fw" }
    if (-not (Test-Path $cmdlineTxt)) { throw "cmdline.txt not found in $fw" }

    $appendCfg = Get-Content (Join-Path $SdCardDir "config-firmware-append.txt") -Raw
    $cfg = Get-Content $configTxt -Raw
    if ($cfg -notmatch 'vc4-kms-v3d,composite') {
        if ($cfg -match 'dtoverlay=vc4-kms-v3d') {
            $cfg = $cfg -replace 'dtoverlay=vc4-kms-v3d', 'dtoverlay=vc4-kms-v3d,composite'
        } else {
            $cfg = $cfg.TrimEnd() + "`n`n" + $appendCfg
        }
        if ($cfg -notmatch 'i2c_arm=on') {
            $cfg = $cfg.TrimEnd() + "`ndtparam=i2c_arm=on`n"
        }
        Set-Content -Path $configTxt -Value $cfg -NoNewline -Encoding ascii
        Write-Host "  config.txt: PAL composite + I2C"
    }

    $pal = (Get-Content (Join-Path $SdCardDir "cmdline-append.txt") -Raw).Trim()
    $cmd = (Get-Content $cmdlineTxt -Raw).Trim()
    if ($cmd -notmatch 'root=PARTUUID=') {
        throw "cmdline.txt has no root=PARTUUID= - refused to edit (SD image may be corrupt)"
    }
    if ($cmd -notmatch 'vc4\.tv_norm=PAL') {
        Set-Content -Path $cmdlineTxt -Value ($cmd + ' ' + $pal) -NoNewline -Encoding ascii
        Write-Host "  cmdline.txt: PAL + net.ifnames=0"
    }

    foreach ($name in @('user-data', 'network-config', 'meta-data')) {
        $src = Join-Path $SdCardDir $name
        $dst = Join-Path $BootRoot $name
        Copy-Item $src $dst -Force
        Write-Host "  copied $name"
    }

    $readme = @"
Electronic Pilot bench SD card - prepared $(Get-Date -Format 'yyyy-MM-dd HH:mm')

SSH:      ssh pilot@epilot-bench
Password: epilot2026  (change with: passwd)

After boot:
  libcamera-hello -t 5000
  sudo i2cdetect -y 1

Docs: sbus_capture/bench/README.md
"@
    Set-Content -Path (Join-Path $BootRoot "EPILOT_README.txt") -Value $readme -Encoding UTF8
}

function Write-NewMetaData {
    param(
        [string]$BootRoot,
        [string]$SdCardDir
    )
    $stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
    $id = "epilot-bench-$stamp"
    $meta = @"
instance-id: $id
local-hostname: epilot-bench
"@
    Set-Content -Path (Join-Path $SdCardDir "meta-data") -Value $meta -Encoding ascii -NoNewline
    Set-Content -Path (Join-Path $BootRoot "meta-data") -Value $meta -Encoding ascii -NoNewline
    Write-Host "  meta-data: new instance-id $id (forces cloud-init re-run)"
}

function Reset-CloudInitOnBoot {
    param([string]$BootRoot)

    foreach ($name in @('cloud-init.disabled', 'cloud-init.status')) {
        $path = Join-Path $BootRoot $name
        if (Test-Path $path) {
            Remove-Item $path -Force
            Write-Host "  removed $name"
        }
    }
}

function Apply-NetworkRecovery {
    param(
        [string]$BootRoot,
        [string]$SdCardDir
    )

    $minimal = Join-Path $SdCardDir "user-data-minimal"
    if (-not (Test-Path $minimal)) { throw "user-data-minimal not found" }
    Copy-Item $minimal (Join-Path $SdCardDir "user-data") -Force
    Write-Host "  user-data: minimal (no apt on boot)"

    Apply-BootPatches -BootRoot $BootRoot -SdCardDir $SdCardDir

    $cmdlineTxt = Join-Path $BootRoot "cmdline.txt"
    if (-not (Test-Path $cmdlineTxt)) {
        $cmdlineTxt = Join-Path $BootRoot "firmware\cmdline.txt"
    }
    $cmd = (Get-Content $cmdlineTxt -Raw).Trim()
    if ($cmd -notmatch 'root=PARTUUID=') {
        throw "cmdline.txt has no root=PARTUUID= - refused to edit"
    }
    if ($cmd -notmatch 'net\.ifnames=0') {
        Set-Content -Path $cmdlineTxt -Value ($cmd + ' net.ifnames=0') -NoNewline -Encoding ascii
        Write-Host "  cmdline.txt: net.ifnames=0 (eth0 naming)"
    }

    Write-NewMetaData -BootRoot $BootRoot -SdCardDir $SdCardDir
    Reset-CloudInitOnBoot -BootRoot $BootRoot
}

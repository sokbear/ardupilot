# Create / reset user pilot in Pi root (ext4) via WSL.
# Pi OFF, SD in reader, Administrator required.

param([int]$DiskNumber = 2)

$ErrorActionPreference = "Stop"
$Username = "pilot"
$Password = "epilot2026"
$Physical = "\\.\PHYSICALDRIVE$DiskNumber"

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { Write-Error "Run as Administrator." }

if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
    throw "WSL required. In admin PowerShell: wsl --install"
}

$disk = Get-Disk -Number $DiskNumber -ErrorAction SilentlyContinue
if (-not $disk) { throw "Disk $DiskNumber not found. Insert SD card." }

Write-Host "Disk #$DiskNumber $($disk.FriendlyName)"

function Dismount-DiskVolumes {
    param([int]$Num)
    Get-Partition -DiskNumber $Num -ErrorAction SilentlyContinue | ForEach-Object {
        if ($_.DriveLetter) {
            $letter = $_.DriveLetter
            $path = "${letter}:\"
            Write-Host "Dismounting $path ..."
            Remove-PartitionAccessPath -DiskNumber $Num -PartitionNumber $_.PartitionNumber -AccessPath $path -ErrorAction SilentlyContinue
        }
    }
    Start-Sleep -Seconds 2
    $vol = Get-Volume | Where-Object { $_.DriveLetter -eq 'Z' }
    if ($vol) {
        Write-Host "Trying mountvol to release Z: ..."
        mountvol Z: /d 2>$null | Out-Null
    }
}

Dismount-DiskVolumes -Num $DiskNumber
wsl --shutdown 2>$null | Out-Null
Start-Sleep -Seconds 2
wsl --unmount $Physical 2>$null | Out-Null

Write-Host "Mounting partition 2 (ext4)..."
$mountOut = wsl --mount $Physical --partition 2 --type ext4 2>&1 | Out-String
Write-Host $mountOut
if ($LASTEXITCODE -ne 0) {
    Write-Host ""
    Write-Host "Trying mount without --type ext4 ..."
    $mountOut2 = wsl --mount $Physical --partition 2 2>&1 | Out-String
    Write-Host $mountOut2
    if ($LASTEXITCODE -ne 0) {
        throw @"
wsl --mount failed.

Common fixes:
  1. Close Explorer windows showing Z: drive
  2. Run this script again as Administrator
  3. Enable WSL2: wsl --install  then reboot PC
  4. Or use Raspberry Pi Imager GUI (see IMAGER-PILOT-README.txt)

First error: $mountOut
Second error: $mountOut2
"@
    }
}

$bashFile = Join-Path $env:TEMP "fix-pi-user.sh"
@'
#!/bin/bash
set -euo pipefail

PASS='EPILOT_PASS_PLACEHOLDER'
ROOT=""

shopt -s nullglob
for d in /mnt/wsl/PHYSICALDRIVE*p2 /mnt/wsl/PHYSICALDRIVE*2; do
  if [ -f "$d/etc/passwd" ]; then ROOT="$d"; break; fi
done

if [ -z "$ROOT" ]; then
  echo "ERROR: Pi root not found. Contents of /mnt/wsl:"
  ls -la /mnt/wsl/ 2>/dev/null || true
  exit 1
fi

echo "ROOT=$ROOT"
echo ""
echo "=== Existing login users on SD ==="
grep -E ':[0-9]{4}:' "$ROOT/etc/passwd" | grep -E ':/bin/(bash|sh)$' | cut -d: -f1,3,6 || true
echo ""

run_chroot() { sudo chroot "$ROOT" "$@"; }

if ! run_chroot /usr/bin/id pilot >/dev/null 2>&1; then
  echo "Creating user pilot..."
  run_chroot /usr/sbin/useradd -m -s /bin/bash -u 1000 -G sudo,adm,dialout,audio,netdev,video,plugdev,gpio,i2c,render,input pilot 2>/dev/null \
    || run_chroot /usr/sbin/useradd -m -s /bin/bash -G sudo,adm,dialout,audio,netdev,video,plugdev,gpio,i2c,render,input pilot
else
  echo "User pilot exists, resetting password..."
fi

if ! echo "pilot:${PASS}" | run_chroot /usr/sbin/chpasswd; then
  echo "chpasswd failed, writing shadow hash via openssl..."
  HASH=$(openssl passwd -6 "$PASS")
  sudo sed -i "s|^pilot:[^:]*:|pilot:${HASH}:|" "$ROOT/etc/shadow"
fi

echo "epilot-bench" | sudo tee "$ROOT/etc/hostname" >/dev/null

sudo mkdir -p "$ROOT/etc/ssh/sshd_config.d"
sudo tee "$ROOT/etc/ssh/sshd_config.d/99-epilot.conf" >/dev/null <<'SSHD'
PasswordAuthentication yes
KbdInteractiveAuthentication yes
UsePAM yes
PermitRootLogin no
SSHD

# Unlock account if locked
sudo sed -i 's/^pilot:![^:]*:/pilot:*:/' "$ROOT/etc/shadow" 2>/dev/null || true
sudo sed -i 's/^pilot:!!:/pilot:*:/' "$ROOT/etc/shadow" 2>/dev/null || true

if run_chroot /usr/bin/id pilot >/dev/null 2>&1; then
  echo ""
  echo "OK: pilot configured"
  grep '^pilot:' "$ROOT/etc/passwd"
  grep '^pilot:' "$ROOT/etc/shadow" | cut -d: -f1-2
else
  echo "ERROR: pilot missing after fix"
  exit 1
fi
'@ -replace 'EPILOT_PASS_PLACEHOLDER', $Password | Set-Content -Path $bashFile -Encoding UTF8NoBOM

$bashUnix = (Get-Content $bashFile -Raw) -replace "`r`n", "`n"
[System.IO.File]::WriteAllText($bashFile, $bashUnix)

$wslScript = (wsl wslpath -a $bashFile 2>$null)
if (-not $wslScript) { $wslScript = "/mnt/c/" + ($bashFile -replace '\\','/' -replace '^[A-Za-z]:','' -replace ':','') }

Write-Host "Patching root filesystem..."
wsl -e bash "$wslScript"
$code = $LASTEXITCODE

wsl --unmount $Physical 2>$null | Out-Null

if ($code -ne 0) { throw "fix-pi-user.sh failed with code $code" }

Write-Host ""
Write-Host "Done. Boot Pi, then:"
Write-Host "  ssh-keygen -R 192.168.1.100"
Write-Host "  ssh pilot@192.168.1.100"
Write-Host "  password: $Password"

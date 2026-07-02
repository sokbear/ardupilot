# Create pilot user for headless SSH (no keyboard on Pi).
# Writes boot/userconf.txt — processed on next Pi boot.
# Run with SD in reader, Pi powered OFF.

param([int]$DiskNumber = 2)

$ErrorActionPreference = "Stop"
$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $ScriptDir "boot-common.ps1")

$Username = "pilot"
$Password = "epilot2026"

$admin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $admin) { Write-Error "Run as Administrator." }

function Get-PasswordHash {
    param([string]$Plain)
    $wsl = Get-Command wsl -ErrorAction SilentlyContinue
    if ($wsl) {
        $h = $Plain | & wsl -e openssl passwd -6 -stdin 2>$null
        if ($h -match '^\$6\$') { return $h.Trim() }
    }
    $openssl = @(
        "C:\Program Files\Git\usr\bin\openssl.exe",
        "C:\Program Files\OpenSSL-Win64\bin\openssl.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($openssl) {
        $h = $Plain | & $openssl passwd -6 -stdin
        if ($h -match '^\$6\$') { return $h.Trim() }
    }
    throw "Need WSL or OpenSSL to generate password hash. Install Git for Windows or run: wsl openssl passwd -6 epilot2026"
}

$bootRoot = Find-BootRootOnDisk -Num $DiskNumber
if (-not $bootRoot) { throw "Boot partition not found on disk $DiskNumber" }

Write-Host "Boot: $bootRoot"
$hash = Get-PasswordHash -Plain $Password
$userconf = "${Username}:${hash}"
[System.IO.File]::WriteAllText((Join-Path $bootRoot "userconf.txt"), $userconf + "`n")
Write-Host "  wrote userconf.txt -> user $Username"

Set-Content -Path (Join-Path $bootRoot "hostname") -Value "epilot-bench" -Encoding ascii -NoNewline
Write-Host "  wrote hostname -> epilot-bench"

# Legacy SSH enable (harmless on Trixie)
New-Item -Path (Join-Path $bootRoot "ssh") -ItemType File -Force | Out-Null
Write-Host "  touched ssh (enable SSH)"

Write-Host ""
Write-Host "Done. Eject SD, boot Pi 5, wait 2-3 min."
Write-Host "  ssh pilot@192.168.1.100"
Write-Host "  password: $Password"
Write-Host ""
Write-Host "On Windows first time, fix host key if needed:"
Write-Host "  ssh-keygen -R 192.168.1.100"

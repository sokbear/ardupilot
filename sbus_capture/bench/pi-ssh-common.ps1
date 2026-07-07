# Non-interactive SSH/SCP to Pi 5 bench (pilot@192.168.1.100).
# Dot-source: . .\pi-ssh-common.ps1

$script:PiSshUser     = "pilot"
$script:PiSshHostAddr = "192.168.1.100"
$script:PiSshHost     = "$($script:PiSshUser)@$($script:PiSshHostAddr)"
$script:PiSshPassword = "epilot2026"

function Invoke-PiSsh {
    param(
        [Parameter(Mandatory = $true)][string]$Command,
        [int]$TimeoutSec = 30
    )

    if (Get-Command plink -ErrorAction SilentlyContinue) {
        $prevEap = $ErrorActionPreference
        $ErrorActionPreference = 'Continue'
        try {
            $out = (& plink -batch -ssh -pw $script:PiSshPassword `
                "$($script:PiSshUser)@$($script:PiSshHostAddr)" $Command 2>&1 |
                Out-String).TrimEnd()
        } finally {
            $ErrorActionPreference = $prevEap
        }
        if ($LASTEXITCODE -ne 0) { throw "plink failed ($LASTEXITCODE): $out" }
        return $out
    }

    if (Get-Command wsl -ErrorAction SilentlyContinue) {
        $escaped = $Command -replace "'", "'\\''"
        $bash    = "sshpass -p '$($script:PiSshPassword)' ssh -o StrictHostKeyChecking=no -o ConnectTimeout=$TimeoutSec $($script:PiSshHost) '$escaped'"
        $out     = wsl bash -lc $bash 2>&1
        if ($LASTEXITCODE -ne 0) { throw "wsl ssh failed ($LASTEXITCODE): $out" }
        return $out
    }

    throw "Need plink (PuTTY) or WSL+sshpass for non-interactive SSH."
}

function Invoke-PiScp {
    param(
        [Parameter(Mandatory = $true)][string]$LocalPath,
        [Parameter(Mandatory = $true)][string]$RemotePath,
        [switch]$Recursive
    )

    if (Get-Command pscp -ErrorAction SilentlyContinue) {
        $args = @("-batch", "-pw", $script:PiSshPassword)
        if ($Recursive) { $args += "-r" }
        $remoteFull = if ($RemotePath -match '/[^/]+$' -and -not $Recursive) {
            $RemotePath
        } else {
            $name = Split-Path -Leaf $LocalPath
            if ($RemotePath.EndsWith('/')) { "$RemotePath$name" } else { "$RemotePath/$name" }
        }
        $args += $LocalPath, "$($script:PiSshHost):$remoteFull"
        & pscp @args
        if ($LASTEXITCODE -ne 0) { throw "pscp failed ($LASTEXITCODE)" }
        return
    }

    if (Get-Command wsl -ErrorAction SilentlyContinue) {
        $wslLocal  = wsl wslpath -a $LocalPath
        $rec       = if ($Recursive) { "-r" } else { "" }
        $bash      = "sshpass -p '$($script:PiSshPassword)' scp -o StrictHostKeyChecking=no $rec '$wslLocal' $($script:PiSshHost):'$RemotePath'"
        wsl bash -lc $bash
        if ($LASTEXITCODE -ne 0) { throw "wsl scp failed ($LASTEXITCODE)" }
        return
    }

    throw "Need pscp (PuTTY) or WSL+sshpass for non-interactive SCP."
}

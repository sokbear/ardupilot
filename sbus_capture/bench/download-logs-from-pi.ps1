# Скачать логи bench с Pi в bench/logs/.
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "pi-ssh-common.ps1")

$logsDir = Join-Path $PSScriptRoot "logs"
New-Item -ItemType Directory -Force -Path $logsDir | Out-Null

Write-Host "export journal on Pi ..."
Invoke-PiSsh "journalctl -u epilot-bench --no-pager > /home/pilot/epilot-bench.journal.log 2>/dev/null; wc -c /home/pilot/bench.log /home/pilot/epilot-bench.journal.log"

$benchLog    = Join-Path $logsDir "bench.log"
$journalLog  = Join-Path $logsDir "epilot-bench.journal.log"

Write-Host "download bench.log ..."
& pscp -batch -pw $script:PiSshPassword "$($script:PiSshHost):/home/pilot/bench.log" $benchLog
if ($LASTEXITCODE -ne 0) { throw "pscp bench.log failed ($LASTEXITCODE)" }

Write-Host "download epilot-bench.journal.log ..."
& pscp -batch -pw $script:PiSshPassword "$($script:PiSshHost):/home/pilot/epilot-bench.journal.log" $journalLog
if ($LASTEXITCODE -ne 0) { throw "pscp journal log failed ($LASTEXITCODE)" }

Get-ChildItem $logsDir | Format-Table Name, Length, LastWriteTime -AutoSize
Write-Host "Saved to $logsDir"

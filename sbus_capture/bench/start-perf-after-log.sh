#!/bin/bash
set -e
pkill -f 'journalctl -u epilot-bench -f' 2>/dev/null || true
truncate -s 0 /home/pilot/perf_after.log
setsid bash -c 'exec journalctl -u epilot-bench -f --no-hostname | tee /home/pilot/perf_after.log' </dev/null >/dev/null 2>&1 &
disown || true
sleep 1
echo OK
pgrep -af 'journalctl -u epilot-bench' || echo NOT_RUNNING
ls -la /home/pilot/perf_after.log
exit 0

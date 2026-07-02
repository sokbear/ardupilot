#!/usr/bin/env python3
"""HDMI boot: убрать fbcon=map:1 и фиксированный 1024x768 (не поддерживается монитором)."""
import pathlib
import re

p = pathlib.Path("/boot/firmware/cmdline.txt")
line = p.read_text().replace("\r", "").strip()
# fbcon=map:1 отправляет консоль на composite (fb1) — убрать полностью
line = re.sub(r"\s*fbcon=map:\d+", "", line)
# Принудительный 1024x768 не поддерживается MPI7001 (800x480) → No signal
line = re.sub(r"\s*video=HDMI-A-1:[^\s]+", "", line)
line = re.sub(r"\s*video=Composite-1:[^\s]+", "", line)
if "console=tty1" not in line:
    line = line.replace(
        "console=serial0,115200", "console=serial0,115200 console=tty1", 1
    )
    if "console=tty1" not in line:
        line = line.replace(
            "console=ttyAMA10,115200", "console=ttyAMA10,115200 console=tty1", 1
        )
p.write_text(line + "\n")
print(p.read_text(), end="")

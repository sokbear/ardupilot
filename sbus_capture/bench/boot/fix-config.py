#!/usr/bin/env python3
"""config.txt: composite overlay + firmware EDID для HDMI."""
import pathlib

p = pathlib.Path("/boot/firmware/config.txt")
lines = p.read_text().replace("\r", "").splitlines()
out = []
seen_composite = False
for line in lines:
    s = line.strip()
    if s == "dtoverlay=vc4-kms-v3d,composite":
        if seen_composite:
            continue
        seen_composite = True
        out.append(line)
        continue
    if s == "dtoverlay=vc4-kms-v3d":
        if seen_composite:
            continue
        out.append("dtoverlay=vc4-kms-v3d,composite")
        seen_composite = True
        continue
    if s == "disable_fw_kms_setup=1":
        out.append("disable_fw_kms_setup=0")
        continue
    if s.startswith("enable_tvout="):
        continue
    out.append(line)
if not seen_composite:
    out.append("dtoverlay=vc4-kms-v3d,composite")
p.write_text("\n".join(out).rstrip() + "\n")
print(p.read_text(), end="")

#!/bin/bash
# HDMI boot: режим из EDID, без fbcon=map и без video=Composite-1.
# Запуск: sudo bash fix-boot-cmdline.sh
set -eu
CMDLINE=/boot/firmware/cmdline.txt
cp -a "$CMDLINE" "${CMDLINE}.bak.$(date +%Y%m%d%H%M%S)"
LINE=$(tr -d '\015' < "$CMDLINE")
LINE=$(echo "$LINE" | sed -E 's/ fbcon=map:[0-9]+//g; s/fbcon=map:[0-9]+ //g')
LINE=$(echo "$LINE" | sed -E 's/ video=HDMI-A-1:[^ ]+//g; s/ video=Composite-1:[^ ]+//g')
if [[ "$LINE" != *"console=tty1"* ]]; then
  if [[ "$LINE" == *"console=ttyAMA10,115200"* ]]; then
    LINE="${LINE/console=ttyAMA10,115200/console=ttyAMA10,115200 console=tty1}"
  else
    LINE="${LINE/console=serial0,115200/console=serial0,115200 console=tty1}"
  fi
fi
echo "$LINE" > "$CMDLINE"
echo "Updated $CMDLINE:"
cat "$CMDLINE"

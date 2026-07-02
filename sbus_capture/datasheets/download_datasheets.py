# -*- coding: utf-8 -*-
"""Download official datasheets for Electronic Pilot project components."""
import os
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))

# (filename, url, description)
FILES = [
    # --- Final architecture (primary) ---
    ("ADV7280-M_datasheet.pdf",
     "https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7280.PDF",
     "Analog Devices ADV7280/ADV7280-M — CVBS→MIPI CSI-2, I2P"),
    ("ADV7282-M_datasheet.pdf",
     "https://www.analog.com/media/en/technical-documentation/data-sheets/ADV7282.PDF",
     "Analog Devices ADV7282/ADV7282-M — alternative video bridge"),
    ("ADV728x_hardware_reference_UG-637.pdf",
     "https://www.analog.com/media/en/technical-documentation/user-guides/adv7280_7281_7282_7283_ug-637.pdf",
     "Analog Devices UG-637 — ADV7280/7281/7282/7283 hardware manual"),
    ("MAX7456_datasheet.pdf",
     "https://www.analog.com/media/en/technical-documentation/data-sheets/max7456.pdf",
     "Analog Devices MAX7456 — OSD generator"),
    ("RP2040_datasheet.pdf",
     "https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf",
     "Raspberry Pi RP2040 — MCU (SBUS + OSD bridge)"),
    ("RP2040_hardware_design.pdf",
     "https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf",
     "Raspberry Pi — hardware design with RP2040"),
    ("THS7374_datasheet.pdf",
     "https://www.ti.com/lit/ds/symlink/ths7374.pdf",
     "Texas Instruments THS7374 — 4-ch CVBS video buffer/splitter"),
    ("RaspberryPi_Zero2W_product_brief.pdf",
     "https://datasheets.raspberrypi.com/rpizero/rpizero-2-w-product-brief.pdf",
     "Raspberry Pi Zero 2 W — product brief (BCM2710A1 in RP3A0 SiP)"),
    # --- Alternatives listed in final design ---
    ("TP9950_datasheet.pdf",
     "https://e2e.ti.com/cfs-file/__key/communityserver-discussions-components-files/138/TP9950Aspec12282018.pdf",
     "Techpoint TP9950 — HD-TVI/CVBS→MIPI CSI-2 (alt. to ADV728x)"),
]

UA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) ElectronicPilot/1.0"


def download(name, url):
    path = os.path.join(HERE, name)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=120) as resp:
        data = resp.read()
    # Basic PDF check
    if not data.startswith(b"%PDF"):
        raise ValueError(f"Not a PDF (got {data[:20]!r})")
    with open(path, "wb") as f:
        f.write(data)
    return len(data)


def main():
    ok, fail = [], []
    for name, url, desc in FILES:
        try:
            size = download(name, url)
            ok.append((name, size, desc))
            print(f"OK  {name} ({size // 1024} KB)")
        except Exception as e:
            fail.append((name, url, str(e)))
            print(f"FAIL {name}: {e}")
    print(f"\nDownloaded: {len(ok)}/{len(FILES)}")
    if fail:
        print("Failed:")
        for n, u, e in fail:
            print(f"  {n}: {e}")
    return 0 if not fail else 1


if __name__ == "__main__":
    raise SystemExit(main())

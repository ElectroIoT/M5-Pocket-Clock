#!/usr/bin/env python3
"""Real screenshots from a connected M5 device over USB (firmware `shot` command).

  python tools/shot.py COM9 one out.png
  python tools/shot.py COM9 gallery docs/screenshots            (AtomS3, files like digital_am.png)
  python tools/shot.py COM18 gallery docs/screenshots stickc_    (StickC, files like stickc_digital_am.png)
  python tools/shot.py COM9 tour docs/images/tour.gif
  python tools/shot.py COM18 tour docs/images/tour_stickc.gif

Needs: pip install pyserial pillow
"""
import sys
import time

import serial
from PIL import Image


def open_port(port):
    s = serial.Serial(port, 115200, timeout=2)
    s.dtr = False
    s.rts = False
    time.sleep(0.3)
    s.reset_input_buffer()
    return s


def cmd(s, text, wait=0.1):
    s.write((text + "\n").encode())
    time.sleep(wait)


def grab(s):
    s.reset_input_buffer()
    cmd(s, "shot", 0.0)
    head = b""
    t0 = time.time()
    while True:
        c = s.read(1)
        if c:
            head += c
            if head.endswith(b"\n"):
                if head.strip().startswith(b"SHOT"):
                    break
                head = b""
        if time.time() - t0 > 5:
            raise TimeoutError("no SHOT header (is the firmware flashed?)")
    w, h = (int(x) for x in head.split()[1:3])
    raw = b""
    while len(raw) < w * h * 2:
        chunk = s.read(w * h * 2 - len(raw))
        if not chunk:
            raise TimeoutError("short frame")
        raw += chunk
    img = Image.new("RGB", (w, h))
    px = img.load()
    for i in range(w * h):
        v = (raw[2 * i] << 8) | raw[2 * i + 1]
        r, g, b = (v >> 11) & 31, (v >> 5) & 63, v & 31
        px[i % w, i // w] = ((r * 255) // 31, (g * 255) // 63, (b * 255) // 31)
    return img


NAMES = ["digital", "ring", "night"]


def big(img):
    return img.resize((img.width * 2, img.height * 2), Image.NEAREST)


def gallery(s, outdir, prefix=""):
    cmd(s, "sync", 14.0)  # let a real sync finish so the status dot is the normal green one
    cmd(s, "theme 0")
    for fi, name in enumerate(NAMES):
        cmd(s, f"face {fi}")
        cmd(s, "settime 10 45 18", 0.6)  # a fixed, readable time (screenshots only)
        grab(s).save(f"{outdir}/{prefix}{name}_am.png")
        cmd(s, "settime 15 7 42", 0.6)
        grab(s).save(f"{outdir}/{prefix}{name}_pm.png")
        print("saved", name)
    cmd(s, "face 0")
    for ti in (1, 2, 5, 6, 7):
        cmd(s, f"theme {ti}")
        cmd(s, "settime 10 45 18", 0.6)
        grab(s).save(f"{outdir}/{prefix}theme{ti}.png")
    cmd(s, "theme 0")
    cmd(s, "portal", 4.0)  # the Wi-Fi setup screen (starts the real setup page)
    grab(s).save(f"{outdir}/{prefix}setup.png")
    cmd(s, "stop", 1.0)
    cmd(s, "sync", 0.1)  # put the real time back
    print("saved setup")


def tour(s, out):
    frames = []
    cmd(s, "sync", 14.0)
    for fi in range(3):
        cmd(s, f"face {fi}")
        cmd(s, "settime 10 44 50", 0.3)
        for _ in range(26):  # ~13 seconds of real ticking, wrapping past 10:45
            frames.append(big(grab(s)))
            time.sleep(0.25)
    cmd(s, "face 0")
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=250, loop=0, optimize=True)
    print("saved", out, len(frames), "frames")


if __name__ == "__main__":
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    ser = open_port(sys.argv[1])
    mode, target = sys.argv[2], sys.argv[3]
    if mode == "one":
        big(grab(ser)).save(target)
    elif mode == "gallery":
        gallery(ser, target, sys.argv[4] if len(sys.argv) > 4 else "")
    elif mode == "tour":
        tour(ser, target)
    else:
        sys.exit(__doc__)

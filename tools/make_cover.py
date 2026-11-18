#!/usr/bin/env python3
"""Build docs/images/m5burner_cover.png (800x600) from the real screenshots."""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SHOTS = ROOT / "docs" / "screenshots"
OUT = ROOT / "docs" / "images" / "m5burner_cover.png"
BG, INK, SOFT, BLUE = (12, 12, 24), (241, 244, 255), (140, 152, 184), (80, 200, 255)


def font(size, bold=False):
    for name in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arialbd.ttf" if bold else "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def main():
    img = Image.new("RGB", (800, 600), BG)
    d = ImageDraw.Draw(img)
    d.text((400, 62), "Pocket Clock", font=font(54, True), fill=INK, anchor="mm")
    d.text((400, 112), "12-hour clock for the AtomS3 - sets itself over Wi-Fi", font=font(24), fill=SOFT, anchor="mm")
    size, gap = 220, 30
    x0 = (800 - (3 * size + 2 * gap)) // 2
    for col, name in enumerate(["digital_pm", "ring_am", "night_am"]):
        p = SHOTS / f"{name}.png"
        if not p.exists():
            continue
        im = Image.open(p).convert("RGB").resize((size, size), Image.NEAREST)
        x, y = x0 + col * (size + gap), 165
        d.rounded_rectangle((x - 5, y - 5, x + size + 5, y + size + 5), 14, outline=(40, 46, 70), width=3)
        img.paste(im, (x, y))
    d.text((400, 450), "AM/PM  -  date  -  3 faces  -  auto timezone", font=font(26), fill=BLUE, anchor="mm")
    d.text((400, 500), "M5Stack AtomS3  -  Wi-Fi setup once from your phone", font=font(22), fill=SOFT, anchor="mm")
    d.text((400, 550), "github.com/ElectroIoT/M5-Pocket-Clock", font=font(18), fill=SOFT, anchor="mm")
    OUT.parent.mkdir(parents=True, exist_ok=True)
    img.save(OUT)
    print("saved", OUT)


if __name__ == "__main__":
    main()

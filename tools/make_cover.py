#!/usr/bin/env python3
"""Build the two M5Burner covers (800x600) from the real screenshots:
   docs/images/m5burner_cover.png          (AtomS3)
   docs/images/m5burner_cover_stickc.png   (M5StickC)
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
SHOTS = ROOT / "docs" / "screenshots"
IMAGES = ROOT / "docs" / "images"
BG, INK, SOFT, BLUE = (12, 12, 24), (241, 244, 255), (140, 152, 184), (80, 200, 255)


def font(size, bold=False):
    for name in (("segoeuib.ttf" if bold else "segoeui.ttf"), "arialbd.ttf" if bold else "arial.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            pass
    return ImageFont.load_default()


def frame(img, d, path, x, y, w, h):
    if not path.exists():
        return
    im = Image.open(path).convert("RGB").resize((w, h), Image.NEAREST)
    d.rounded_rectangle((x - 5, y - 5, x + w + 5, y + h + 5), 14, outline=(40, 46, 70), width=3)
    img.paste(im, (x, y))


def cover(out, title, sub, names, size, foot1, foot2):
    img = Image.new("RGB", (800, 600), BG)
    d = ImageDraw.Draw(img)
    d.text((400, 62), title, font=font(54, True), fill=INK, anchor="mm")
    d.text((400, 112), sub, font=font(24), fill=SOFT, anchor="mm")
    w, h = size
    gap = 30
    x0 = (800 - (len(names) * w + (len(names) - 1) * gap)) // 2
    for i, n in enumerate(names):
        frame(img, d, SHOTS / f"{n}.png", x0 + i * (w + gap), 150, w, h)
    d.text((400, 470), foot1, font=font(26), fill=BLUE, anchor="mm")
    d.text((400, 515), foot2, font=font(22), fill=SOFT, anchor="mm")
    d.text((400, 560), "github.com/ElectroIoT/M5-Pocket-Clock", font=font(18), fill=SOFT, anchor="mm")
    img.save(out)
    print("saved", out)


def main():
    cover(IMAGES / "m5burner_cover.png", "Pocket Clock", "12-hour clock for the AtomS3 - sets itself over Wi-Fi",
          ["digital_pm", "ring_am", "night_am"], (220, 220),
          "AM/PM  -  date  -  3 faces  -  8 colours  -  auto timezone",
          "M5Stack AtomS3  -  Wi-Fi setup once from your phone")
    cover(IMAGES / "m5burner_cover_stickc.png", "Pocket Clock", "12-hour clock for the M5StickC - with battery level",
          ["stickc_digital_pm", "stickc_ring_am", "stickc_night_am", "stickc_theme6", "stickc_theme7"], (110, 220),
          "AM/PM  -  date  -  battery  -  RTC keeps the time",
          "M5StickC  -  Wi-Fi setup once from your phone")


if __name__ == "__main__":
    main()

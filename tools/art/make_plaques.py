"""Draw each room's floor plaque: a dark plate with gold lettering (Art.md "Plaques").

Usage: python3 tools/art/make_plaques.py
Reads plugins/last-curtain/assets/night/rooms.csv, writes art/plaques/<id>.png in LastCurtain's assets.
"""
import csv
import random
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2] / "plugins/last-curtain"
ASSETS = ROOT / "apps/LastCurtain/assets"
FONT = ASSETS / "ui/fonts/Lora.ttf"
W, H = 1024, 192
GOLD, DARK_GOLD = (226, 186, 98), (120, 88, 36)


def plate(seed):
    # 1. A near-black bronze plate, a little lighter in the middle, with fine grain.
    rng = random.Random(seed)
    img = Image.new("RGB", (W, H), (22, 17, 12))
    d = ImageDraw.Draw(img)
    for y in range(H):
        t = 1.0 - abs(y - H / 2) / (H / 2)
        d.line([(0, y), (W, y)], fill=(int(22 + 16 * t), int(17 + 12 * t), int(12 + 7 * t)))
    for _ in range(4000):
        x, y, v = rng.randrange(W), rng.randrange(H), rng.randrange(-10, 10)
        r, g, b = img.getpixel((x, y))
        img.putpixel((x, y), (max(0, r + v), max(0, g + v), max(0, b + v)))
    # 2. A double gold border and four screws.
    d.rectangle([6, 6, W - 7, H - 7], outline=GOLD, width=6)
    d.rectangle([20, 20, W - 21, H - 21], outline=DARK_GOLD, width=2)
    for x, y in [(40, 40), (W - 41, 40), (40, H - 41), (W - 41, H - 41)]:
        d.ellipse([x - 9, y - 9, x + 9, y + 9], fill=DARK_GOLD, outline=GOLD, width=2)
        d.line([x - 6, y, x + 6, y], fill=(40, 30, 16), width=2)
    return img


def lettering(img, text):
    # 1. The largest size that fits between the screws.
    text = text.upper()
    size = 96
    while size > 20:
        font = ImageFont.truetype(str(FONT), size)
        box = ImageDraw.Draw(img).textbbox((0, 0), text, font=font)
        if box[2] - box[0] <= W - 150 and box[3] - box[1] <= H - 80:
            break
        size -= 2
    x = (W - (box[2] - box[0])) / 2 - box[0]
    y = (H - (box[3] - box[1])) / 2 - box[1]
    # 2. Engraved: a dark cut first, then the gold face a pixel up and left.
    cut = Image.new("L", (W, H), 0)
    ImageDraw.Draw(cut).text((x + 2, y + 2), text, font=font, fill=255)
    img.paste((8, 6, 4), mask=cut.filter(ImageFilter.GaussianBlur(1.5)))
    ImageDraw.Draw(img).text((x, y), text, font=font, fill=GOLD)
    return img


def main():
    out = ASSETS / "art/plaques"
    out.mkdir(parents=True, exist_ok=True)
    with open(ROOT / "assets/night/rooms.csv", newline="", encoding="utf-8") as f:
        for i, row in enumerate(csv.DictReader(f)):
            lettering(plate(i), row["name"]).save(out / f"{row['id']}.png", optimize=True)
            print(row["id"])


if __name__ == "__main__":
    main()

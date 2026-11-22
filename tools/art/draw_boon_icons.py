# Draws the three boon icons (art list section 6) as white glyphs like art/ui/match/icons.
# SDXL could not draw clean glyphs (the slingshot, 2026-11-23), so these are drawn.
# Usage: python3 draw_boon_icons.py <out_dir>   writes boon_<id>.png, 128 x 128.
import os, sys
from PIL import Image, ImageDraw

S = 512  # 1. draw at 4x, then scale down for smooth edges
W, G = (255, 255, 255, 255), (170, 170, 170, 255)


def ferry_mother(d):
    # 2. A boat hull (a wide bowl) and an oar across it.
    d.chord((40, 150, 472, 420), 0, 180, fill=W)
    d.chord((90, 150, 422, 330), 0, 180, fill=(0, 0, 0, 0))
    d.line((120, 60, 400, 440), fill=W, width=26)
    d.polygon([(345, 375), (395, 345), (470, 470), (420, 500)], fill=W)


def girl_with_hoe(d):
    # 3. A road narrowing to the horizon, and a hoe over it.
    d.polygon([(60, 490), (220, 250), (292, 250), (452, 490)], fill=G)
    d.polygon([(246, 300), (266, 300), (276, 490), (236, 490)], fill=(0, 0, 0, 0))
    d.line((140, 420, 380, 60), fill=W, width=28)
    d.polygon([(330, 40), (450, 110), (420, 170), (350, 120)], fill=W)


def wife_four_days(d):
    # 4. A small pillow with an embroidered flower.
    d.rounded_rectangle((60, 130, 452, 400), radius=90, fill=W)
    for dx, dy in ((0, -60), (0, 60), (-60, 0), (60, 0)):
        d.ellipse((256 + dx - 38, 265 + dy - 38, 256 + dx + 38, 265 + dy + 38), fill=G)
    d.ellipse((226, 235, 286, 295), fill=(0, 0, 0, 0))


out = sys.argv[1]
os.makedirs(out, exist_ok=True)
for name, draw in (('ferry_mother', ferry_mother), ('girl_with_hoe', girl_with_hoe), ('wife_four_days', wife_four_days)):
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    draw(ImageDraw.Draw(im))
    im.resize((128, 128), Image.LANCZOS).save(os.path.join(out, f'boon_{name}.png'), optimize=True)
print('drew 3 boon icons ->', out)

# The Last Curtain's mood pictures (plan 4a, Art.md "The night's mood"). Run from the repo root:
#   python tools/art/make_mood_art.py
# blood_lut.png: the colour table after the Turn (256x16, Post.md section 4): reds kept, greens and
#   blues crushed toward a dark red, shadows pushed to near black.
# drips.png: blood running down from the top edge of the screen, with alpha, for the dread layer.
import math, random
from PIL import Image, ImageDraw, ImageFilter

OUT = 'plugins/last-curtain/apps/LastCurtain/assets/art/ui/'

# 1. The colour table: cell (r, g, b) at x = b * 16 + r, y = g.
lut = Image.new('RGB', (256, 16))
for g in range(16):
    for b in range(16):
        for r in range(16):
            R, G, B = r / 15, g / 15, b / 15
            luma = 0.2126 * R + 0.7152 * G + 0.0722 * B
            # Red stays, the rest falls toward a deep red of the same brightness; darks sink.
            nr = min(1.0, 0.55 * R + 0.75 * luma)
            ng = 0.35 * G * luma + 0.05 * luma
            nb = 0.30 * B * luma + 0.04 * luma
            k = luma ** 1.35 / max(luma, 1e-4)
            lut.putpixel((b * 16 + r, g), tuple(int(255 * min(1.0, c * k * 1.1)) for c in (nr, ng, nb)))
lut.save(OUT + 'blood_lut.png')

# 2. The drips: a ragged red band along the top, runs of different lengths, soft edges.
random.seed(1960)
w, h = 1280, 260
mask = Image.new('L', (w, h), 0)
d = ImageDraw.Draw(mask)
d.rectangle([0, 0, w, 18], fill=255)
for x in range(0, w, 3):
    edge = 18 + 10 * math.sin(x * 0.05) + random.random() * 8
    d.line([(x, 0), (x, edge)], fill=255, width=3)
for _ in range(46):
    x = random.randint(0, w)
    length = random.choice([30, 50, 70, 110, 160, 230]) * random.uniform(0.7, 1.0)
    width = random.uniform(3, 9)
    d.line([(x, 10), (x, length)], fill=255, width=int(width))
    d.ellipse([x - width * 0.9, length - width, x + width * 0.9, length + width * 0.8], fill=255)
mask = mask.filter(ImageFilter.GaussianBlur(1.6))
drips = Image.new('RGBA', (w, h), (92, 4, 6, 0))
shade = Image.new('L', (w, h))
shade.putdata([int(200 + 55 * (1 - y / h)) for y in range(h) for _ in range(w)])
drips.putalpha(Image.composite(shade, Image.new('L', (w, h), 0), mask))
drips.save(OUT + 'drips.png', optimize=True)
print('wrote', OUT + 'blood_lut.png', OUT + 'drips.png')

# PlayStation pad glyphs for the prompts (gamepad plan, the user 2026-11-25: clean shapes drawn in code).
# Run from the repository root: python tools/art/pad_glyphs.py  (needs Pillow; writes art/ui/pad/*.png)
import glob, os
from PIL import Image, ImageDraw, ImageFont

OUT = 'plugins/last-curtain/apps/LastCurtain/assets/art/ui/pad'
S, GOLD, DARK, INK = 128, (214, 178, 104, 255), (28, 18, 14, 235), (240, 226, 196, 255)
C = S * 2  # drawn 4x, then shrunk: smooth edges

def font(n):
    found = sorted(glob.glob('/usr/share/fonts/**/DejaVuSans-Bold.ttf', recursive=True) + glob.glob('C:/Windows/Fonts/arialbd.ttf'))
    return ImageFont.truetype(found[0], n) if found else ImageFont.load_default()

def disc():
    im = Image.new('RGBA', (S * 4, S * 4), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)
    d.ellipse((8, 8, S * 4 - 8, S * 4 - 8), fill=DARK, outline=GOLD, width=20)
    return im, d

def label(d, text, size):
    f = font(size)
    b = d.textbbox((0, 0), text, font=f)
    d.text((C - (b[2] + b[0]) / 2, C - (b[3] + b[1]) / 2), text, font=f, fill=INK)

def save(im, name):
    im.resize((S, S), Image.LANCZOS).save(os.path.join(OUT, name + '.png'))

os.makedirs(OUT, exist_ok=True)
im, d = disc(); d.line((C - 120, C - 120, C + 120, C + 120), fill=INK, width=36); d.line((C - 120, C + 120, C + 120, C - 120), fill=INK, width=36); save(im, 'cross')
im, d = disc(); d.ellipse((C - 125, C - 125, C + 125, C + 125), outline=INK, width=36); save(im, 'circle')
im, d = disc(); d.rectangle((C - 110, C - 110, C + 110, C + 110), outline=INK, width=36); save(im, 'square')
im, d = disc(); d.polygon([(C, C - 140), (C + 135, C + 100), (C - 135, C + 100)], outline=INK, width=36); save(im, 'triangle')
for t in ('L1', 'R1', 'L2', 'R2', 'L3', 'R3'):
    im = Image.new('RGBA', (S * 4, S * 4), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle((12, 96, S * 4 - 12, S * 4 - 96), radius=120, fill=DARK, outline=GOLD, width=20)
    label(d, t, 190); save(im, t.lower())
im, d = disc()
for box in ((C - 50, C - 170, C + 50, C + 170), (C - 170, C - 50, C + 170, C + 50)):
    d.rounded_rectangle(box, radius=24, fill=INK)
save(im, 'dpad')
for name, t in (('lstick', 'L'), ('rstick', 'R')):
    im, d = disc(); d.ellipse((C - 150, C - 150, C + 150, C + 150), outline=INK, width=24); label(d, t, 170); save(im, name)
im, d = disc()
for k in (-80, 0, 80):
    d.rounded_rectangle((C - 120, C + k - 18, C + 120, C + k + 18), radius=18, fill=INK)
save(im, 'options')
im, d = disc()
for k in (-80, 0, 80):
    d.rounded_rectangle((C + k - 18, C - 110, C + k + 18, C + 110), radius=18, fill=INK)
save(im, 'create')

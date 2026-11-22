# Packs one ghost picture into a sprite (the zodiac ghosts, 2026-11-22).
# Usage: python3 pack_ghost.py <picture> <mask> <id> <assets_art_dir> <head_x> <head_y> [mirror]
# <mask> is birefnet's mask saved as an image (white = keep). head_x, head_y: the head's
# centre as fractions of the cut-out figure, after any mirror.
# Writes sprites/<id>/body_calm.png (768 px tall) and face_calm.png (512 px square).
import os, sys
from PIL import Image, ImageOps

pic, mask, gid, art, hx, hy = sys.argv[1:7]
mirror = len(sys.argv) > 7 and sys.argv[7] == 'mirror'
out = os.path.join(art, 'sprites', gid)
os.makedirs(out, exist_ok=True)

# 1. The mask becomes the alpha; every sprite faces right, so left-facing ones are mirrored.
im = Image.open(pic).convert('RGB')
im.putalpha(Image.open(mask).convert('L').resize(im.size))
if mirror:
    im = ImageOps.mirror(im)

# 2. Crop to the figure (alpha above 16) and scale the body to 768 px tall.
im = im.crop(im.getchannel('A').point(lambda a: 255 if a > 16 else 0).getbbox())
s = 768 / im.height
im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS).save(os.path.join(out, 'body_calm.png'), optimize=True)

# 3. The portrait: a square around the head, 45% of the figure's shorter side, kept inside it.
side = round(min(im.size) * 0.45)
x = min(max(round(float(hx) * im.width) - side // 2, 0), im.width - side)
y = min(max(round(float(hy) * im.height) - side // 2, 0), im.height - side)
im.crop((x, y, x + side, y + side)).resize((512, 512), Image.LANCZOS).save(os.path.join(out, 'face_calm.png'), optimize=True)
print('packed', gid, im.size, '->', out)

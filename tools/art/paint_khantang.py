# paint_khantang.py src out_prefix mirror x0 y0 x1 y1 ex ey [ex2 ey2] : paints a rough khan tang band and a dark eye smudge,
# writes <out>_paint.png and <out>_mask.png (white = repaint). Coordinates are after mirroring.
import sys, random
from PIL import Image, ImageDraw, ImageFilter, ImageOps
src, out, mirror = sys.argv[1], sys.argv[2], sys.argv[3] == '1'
x0, y0, x1, y1 = map(int, sys.argv[4:8])
eyes = list(map(int, sys.argv[8:]))  # one or two eye points
im = Image.open(src).convert('RGB')
if mirror:
    im = ImageOps.mirror(im)
band = Image.new('L', im.size, 0)
ImageDraw.Draw(band).rounded_rectangle((x0, y0, x1, y1), radius=(y1 - y0) // 3, fill=255)
band = band.filter(ImageFilter.GaussianBlur(1.5))
cloth = Image.new('RGB', im.size, (226, 221, 206))
noise = Image.effect_noise(im.size, 18).convert('RGB')
cloth = Image.blend(cloth, noise, 0.08)
im.paste(cloth, (0, 0), band)
smudge = Image.new('L', im.size, 0)
r = (y1 - y0) // 2
for ex, ey in zip(eyes[0::2], eyes[1::2]):
    ImageDraw.Draw(smudge).ellipse((ex - r, ey - r * 0.7, ex + r, ey + r * 0.7), fill=230)
smudge = smudge.filter(ImageFilter.GaussianBlur(r * 0.45))
im.paste(Image.new('RGB', im.size, (38, 30, 28)), (0, 0), smudge)
im.save(out + '_paint.png')
mask = Image.new('L', im.size, 0)
ImageDraw.Draw(mask).rounded_rectangle((x0 - 10, y0 - 10, x1 + 10, y1 + 10), radius=8, fill=255)
mask.convert('RGB').save(out + '_mask.png')
# a mirrored copy of the plain picture, for later steps
im2 = Image.open(src).convert('RGB')
(ImageOps.mirror(im2) if mirror else im2).save(out + '_plain.png')

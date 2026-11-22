# prep.py pic mask out_prefix [mirror] : from a figure and its birefnet mask (white = figure):
#  <out>_pic.png  the (mirrored) picture
#  <out>_cut.png  the picture with the mask as alpha
#  <out>_pcrop.png the portrait crop (512:627), scaled to 1024 x 1256, for an img2img pass
#  <out>_head.jpg a 3x gridded close-up of the head, for reading the eye positions
# Prints the head box (original pixels).
import sys
from PIL import Image, ImageOps, ImageDraw
pic, mask, out = sys.argv[1:4]
mirror = len(sys.argv) > 4 and sys.argv[4] == '1'
over = list(map(float, sys.argv[5:7])) if len(sys.argv) > 6 else None  # head centre x and width, when a hat or prop widens the top
im = Image.open(pic).convert('RGB')
m = Image.open(mask).convert('L')
if mirror:
    im, m = ImageOps.mirror(im), ImageOps.mirror(m)
im.save(out + '_pic.png')
cut = im.copy(); cut.putalpha(m); cut.save(out + '_cut.png')
# 1. The figure's box, then the head: the top 9% of the figure's height.
b = m.point(lambda a: 255 if a > 128 else 0)
x0, y0, x1, y1 = b.getbbox()
fh = y1 - y0
hb = b.crop((0, y0, im.width, y0 + int(fh * 0.09))).getbbox()
hx = (hb[0] + hb[2]) / 2
hw = max(hb[2] - hb[0], fh * 0.07)
if over:
    hx, hw = over
# 2. The portrait: head and shoulders, the head about half the width.
w = hw * 2.0
h = w * 627 / 512
box = (int(hx - w / 2), int(y0 - h * 0.06), int(hx + w / 2), int(y0 - h * 0.06 + h))
ImageOps.pad(im.crop(box), (1024, 1256), color=im.getpixel((5, 5))).save(out + '_pcrop.png')
# 3. A gridded close-up of the head (labels in original pixels).
hbox = (int(hx - hw), y0 - 10, int(hx + hw), y0 + int(fh * 0.16))
s = 3
c = im.crop(hbox); c = c.resize((c.width * s, c.height * s))
d = ImageDraw.Draw(c)
for x in range(hbox[0] - hbox[0] % 10 + 10, hbox[2], 10):
    d.line([((x - hbox[0]) * s, 0), ((x - hbox[0]) * s, c.height)], fill=(255, 0, 0) if x % 50 == 0 else (255, 170, 170))
    if x % 50 == 0: d.text(((x - hbox[0]) * s + 2, 2), str(x), fill='red')
for y in range(hbox[1] - hbox[1] % 10 + 10, hbox[3], 10):
    d.line([(0, (y - hbox[1]) * s), (c.width, (y - hbox[1]) * s)], fill=(0, 0, 255) if y % 50 == 0 else (170, 170, 255))
    if y % 50 == 0: d.text((2, (y - hbox[1]) * s + 2), str(y), fill='blue')
c.save(out + '_head.jpg')
print(out, 'figure', (x0, y0, x1, y1), 'head', hb, 'portrait', box)

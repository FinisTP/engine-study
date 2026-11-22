# Packs one figure for The Last Curtain (art list 2026-11-22, "Technical specs").
# Usage: python3 pack_figure.py <body_rgba.png> <sprite_id> <assets_art_dir> [faces_dir]
# <body_rgba.png> is the cut-out figure (alpha = figure). [faces_dir] holds face_<face>.png cut-outs
# (1024 x 1256 portraits). Writes sprites/<sprite_id>/body_calm.png (768 px tall),
# face_<face>.png (512 x 627) and anchors.json (estimated string anchors).
import json, os, sys
from PIL import Image

FACES = ['calm', 'angry', 'neardead', 'happy', 'scheming', 'afraid', 'talk']
body, sid, art = sys.argv[1:4]
faces = sys.argv[4] if len(sys.argv) > 4 else None
out = os.path.join(art, 'sprites', sid)
os.makedirs(out, exist_ok=True)

# 1. The body: crop to the figure, scale to 768 px tall.
im = Image.open(body).convert('RGBA')
a = im.getchannel('A').point(lambda v: 255 if v > 16 else 0)
im = im.crop(a.getbbox())
im = im.resize((round(im.width * 768 / im.height), 768), Image.LANCZOS)
im.save(os.path.join(out, 'body_calm.png'), optimize=True)

# 2. Anchors: the figure's outer edge at shoulder and wrist height, as fractions of the body.
a = im.getchannel('A').point(lambda v: 255 if v > 128 else 0)
def edges(fy):
    y = int(fy * im.height)
    box = a.crop((0, y, im.width, y + 1)).getbbox() or (0, 0, im.width, 1)
    return round(box[0] / im.width, 3), round(box[2] / im.width, 3), round(fy, 3)
sl, sr, sy = edges(0.2)
wl, wr, wy = edges(0.47)
anchors = {'estimated': True, 'shoulder_l': [sl, sy], 'shoulder_r': [sr, sy],
           'wrist_l': [wl, wy], 'wrist_r': [wr, wy]}
json.dump(anchors, open(os.path.join(out, 'anchors.json'), 'w'), indent=1)

# 3. Faces: every expression scaled from the 1024 x 1256 portrait to 512 x 627.
#    Without portraits, face_calm is the head and shoulders cut from the body.
if not faces:
    hb = a.crop((0, 0, im.width, int(im.height * 0.09))).getbbox()
    w = max(hb[2] - hb[0], im.height * 0.07) * 2.0
    x = (hb[0] + hb[2]) / 2 - w / 2
    box = (round(x), round(-w * 0.074), round(x + w), round(-w * 0.074 + w * 627 / 512))
    im.crop(box).resize((512, 627), Image.LANCZOS).save(os.path.join(out, 'face_calm.png'), optimize=True)
else:
    for f in FACES:
        p = os.path.join(faces, f'face_{f}.png')
        if os.path.exists(p):
            Image.open(p).convert('RGBA').resize((512, 627), Image.LANCZOS).save(
                os.path.join(out, f'face_{f}.png'), optimize=True)
print('packed', sid, '->', out)

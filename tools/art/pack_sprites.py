# Packs a guest's ComfyUI sprite output into game assets (plan D, docs/design/2026-11-17-performances.md).
# Usage: python3 pack_sprites.py <comfy_dir> <guest_id> <assets_art_dir>
# <comfy_dir> holds body_<face>_00001_.png and face_<face>_00001_.png with alpha.
# Writes sprites/<guest_id>/body_<face>.png (768 px tall) and face_<face>.png (512 px).
import os, sys
from PIL import Image

FACES = ['calm', 'angry', 'neardead', 'happy', 'scheming', 'afraid', 'talk']
src, gid, art = sys.argv[1:4]
out = os.path.join(art, 'sprites', gid)
os.makedirs(out, exist_ok=True)

for kind, size in (('body', 768), ('face', 512)):
    ims = {f: Image.open(os.path.join(src, f'{kind}_{f}_00001_.png')).convert('RGBA') for f in FACES}
    # 1. One crop box for all faces (their union), so frames swap without a jump.
    boxes = [im.getchannel('A').point(lambda a: 255 if a > 16 else 0).getbbox() for im in ims.values()]
    box = (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))
    for f, im in ims.items():
        im = im.crop(box)
        # 2. Bodies scale to a fixed height; faces to a fixed width.
        s = size / im.height if kind == 'body' else size / im.width
        im.resize((round(im.width * s), round(im.height * s)), Image.LANCZOS).save(
            os.path.join(out, f'{kind}_{f}.png'), optimize=True)
print('packed', gid, '->', out)

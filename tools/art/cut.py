# Cuts ComfyUI outputs into The Last Curtain's art folder (plan 4a, apps/LastCurtain/Art.md).
#   python tools/art/cut.py <comfy output>/lastcurtain/art plugins/last-curtain/apps/LastCurtain/assets/art [picks.json]
# Input: <set>/<id>_00001_.png ... (one per try). The last try wins unless picks.json says
# {"rooms/foyer": 2}. Rooms, events, portraits: 512 px JPEG. Items and cards: 256 px PNG with the
# cream background keyed out. UI: 512 px PNG.
import json, os, re, sys
from PIL import Image, ImageFilter

SIZES = {'rooms': 512, 'events': 512, 'portraits': 512, 'items': 256, 'cards': 256, 'ui': 512}


def key_out(image):
    # 1. The background colour: the median of the four corners.
    rgb = image.convert('RGB')
    w, h = rgb.size
    corners = [rgb.getpixel(p) for p in [(4, 4), (w - 5, 4), (4, h - 5), (w - 5, h - 5)]]
    bg = tuple(sorted(c[i] for c in corners)[1] for i in range(3))
    # 2. Alpha by distance from it: near = clear, far = solid, a soft edge between.
    alpha = Image.new('L', rgb.size)
    alpha.putdata([max(0, min(255, (abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) - 40) * 4)) for r, g, b in rgb.getdata()])
    out = rgb.convert('RGBA')
    out.putalpha(alpha.filter(ImageFilter.GaussianBlur(1)))
    return out


def main(src, dst, picks_file=None):
    picks = json.load(open(picks_file)) if picks_file else {}
    done = 0
    for set_name, size in SIZES.items():
        folder = os.path.join(src, set_name)
        if not os.path.isdir(folder):
            continue
        tries = {}
        for name in sorted(os.listdir(folder)):
            m = re.match(r'(.+)_(\d+)_\.png$', name)
            if m:
                tries.setdefault(m.group(1), {})[int(m.group(2))] = os.path.join(folder, name)
        os.makedirs(os.path.join(dst, set_name), exist_ok=True)
        for id_, files in tries.items():
            pick = picks.get(set_name + '/' + id_, max(files))
            image = Image.open(files[pick]).resize((size, size), Image.LANCZOS)
            if set_name in ('items', 'cards'):
                key_out(image).save(os.path.join(dst, set_name, id_ + '.png'), optimize=True)
            elif set_name == 'ui':
                image.convert('RGB').save(os.path.join(dst, set_name, id_ + '.png'), optimize=True)
            else:
                image.convert('RGB').save(os.path.join(dst, set_name, id_ + '.jpg'), quality=85, optimize=True)
            done += 1
    print(done, 'pictures cut into', dst)


if __name__ == '__main__':
    main(*sys.argv[1:])

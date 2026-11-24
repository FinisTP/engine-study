"""The theatre map's floor plan (ui-art-requests section 1, map/floor_plan.png).

1. The aged paper (tex_plan_paper) is the ground.
2. A painted SDXL plan is laid over it very faintly, blurred, so its ink feel shows but none of its
   pretend lettering can be read.
3. The rooms are drawn on top in sepia and faded gold from map.json, so each ticket sits on its room.

Usage: python tools/art/floor_plan_compose.py <paper.png> <painted plan.png> <map.json> <out.png>
The ticket's place on screen (Flow.cpp) is 80 + 1100 x, 60 + 560 y on a 1280 x 720 screen; the picture
is drawn to fit that screen, so the same formula is used here in the picture's own pixels.
"""
import json
import sys

from PIL import Image, ImageDraw, ImageFilter

W, H = 1344, 768
SEPIA, GOLD = (92, 62, 34), (176, 140, 72)


def to_pixels(x, y):
    # 1. The screen place of a ticket, then into the picture (1344 x 768 fitted into 1280 x 720).
    scale = min(1280 / W, 720 / H)
    left = (1280 - W * scale) / 2
    return ((80 + 1100 * x) - left) / scale, (60 + 560 * y) / scale


def main(paper_file, painted_file, map_file, out_file):
    # 1. The paper, warmed and darkened at the edges.
    paper = Image.open(paper_file).convert('RGB').resize((W, H), Image.LANCZOS)
    tint = Image.new('RGB', (W, H), (214, 190, 146))
    paper = Image.blend(paper, tint, 0.45)
    edge = Image.new('L', (W, H), 0)
    ImageDraw.Draw(edge).rectangle([60, 50, W - 60, H - 50], fill=255)
    edge = edge.filter(ImageFilter.GaussianBlur(70))
    paper = Image.composite(paper, Image.blend(paper, Image.new('RGB', (W, H), (90, 64, 36)), 0.55), edge)
    painted = Image.open(painted_file).convert('L').resize((W, H), Image.LANCZOS).filter(ImageFilter.GaussianBlur(3))
    # 2. The painted plan as a faint brown underdrawing (darker lines only).
    under = Image.new('RGB', (W, H), SEPIA)
    mask = painted.filter(ImageFilter.GaussianBlur(2)).point(lambda v: int(max(0, 210 - v) * 0.32))
    plan = Image.composite(under, paper, mask)
    # 3. The rooms: an outer wall, then each room's outline with doubled walls and a door gap.
    d = ImageDraw.Draw(plan, 'RGBA')
    d.rectangle([28, 22, W - 28, H - 22], outline=SEPIA + (200,), width=4)
    d.rectangle([40, 34, W - 40, H - 34], outline=GOLD + (150,), width=1)
    tickets = json.load(open(map_file, encoding='utf-8'))['tickets']
    for t in tickets:
        x0, y0 = to_pixels(t['room'][0], t['room'][1])
        x1, y1 = to_pixels(t['room'][2], t['room'][3])
        box, cx, cy, w, h = [x0, y0, x1, y1], (x0 + x1) / 2, (y0 + y1) / 2, x1 - x0, y1 - y0
        faint = t['kind'] == 'unknown'
        ink = SEPIA + ((110,) if faint else (220,))
        d.rectangle(box, outline=ink, width=3)
        d.rectangle([box[0] + 6, box[1] + 6, box[2] - 6, box[3] - 6], outline=GOLD + ((70,) if faint else (150,)), width=1)
        d.line([cx - 14, box[3], cx + 14, box[3]], fill=(222, 206, 170, 255), width=5)  # the door
        if t.get('shape') == 'stairs':
            for k in range(1, 8):
                y = box[1] + k * h / 8
                d.line([box[0] + 8, y, box[2] - 8, y], fill=ink, width=1)
        if t.get('shape') == 'stage':
            d.arc([box[0], box[1] - h * 0.6, box[2], box[3] + h * 0.2], 20, 160, fill=ink, width=2)
        if t.get('shape') == 'foyer':
            for k in range(1, 6):
                x = box[0] + k * w / 6
                d.ellipse([x - 5, cy - 5, x + 5, cy + 5], outline=ink, width=2)  # columns
    # 4. The auditorium in the middle: its walls and a horseshoe of seat rows facing the stage.
    x0, y0 = to_pixels(0.30, 0.44)
    x1, y1 = to_pixels(0.70, 0.80)
    d.rounded_rectangle([x0, y0, x1, y1], radius=60, outline=SEPIA + (200,), width=3)
    ax = (x0 + x1) / 2
    for k in range(7):
        r = 60 + k * 24
        d.arc([ax - r, y0 - r * 0.55, ax + r, y0 + r * 0.95], 25, 155, fill=SEPIA + (150 - k * 12,), width=2)
    plan = plan.filter(ImageFilter.GaussianBlur(0.6))
    plan.save(out_file)
    print('Wrote ' + out_file)


if __name__ == '__main__':
    main(*sys.argv[1:5])

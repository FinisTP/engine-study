# Composes the campaign UI's slice-1 set (ui-art-requests section 1) from the painted pictures.
#   python3 tools/art/ui_campaign_compose.py <raw folder>
# Writes art/ui/campaign/{tex,frame,icon,item}/ and frame/slices.json. Style A (red-black lacquer) as approved.
# Frames, the tick and the AP beads are drawn here; SDXL gave the textures, icons and still lifes.
import json, os, sys
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter, ImageOps

sys.path.insert(0, os.path.dirname(__file__))
from ui_style_board_compose import GOLD, ROOT, key_out, lacquer, panel, tick

OUT = os.path.join(ROOT, 'art', 'ui', 'campaign')
BASE = ((26, 12, 10), (14, 7, 6))  # style A
RAISED = tuple(tuple(c + 12 for c in col) for col in BASE)

# The picks (art/README.md "Campaign UI" says why).
ICONS = {'cmd_move': 'icon_cmd_move_341', 'cmd_attack': 'icon_cmd_attack_352', 'cmd_skills': 'icon_cmd_skills_361',
         'cmd_items': 'icon_cmd_items_372', 'cmd_end': 'icon_cmd_end_382', 'cmd_cut': 'icon_cmd_cut_171',
         'mark_skirmish': 'icon_mark_skirmish2_252', 'mark_unknown': 'icon_mark_unknown_311',
         'upgrade_ready': 'icon_upgrade_ready_321', 'rite': 'icon_rite2_241'}
ITEMS = {'gung': ('item_gung_191', 'pic_gung_502'), 'duong': ('item_duong_401', 'pic_duong_511'),
         'mut_gung': ('item_mut_gung_411', 'pic_mut_gung_522'), 'nuoc_gung': ('item_nuoc_gung_422', 'pic_nuoc_gung_532'),
         'giay_gap_tu': ('item_giay_gap_tu_432', 'pic_giay_gap_tu_541')}


def save(img, *path):
    file = os.path.join(OUT, *path)
    os.makedirs(os.path.dirname(file), exist_ok=True)
    img.save(file)


def tileable(img, size):
    # 1. Crop the middle to size; 2. lay the half-offset copy over it with a soft cross mask, so the seams vanish.
    w, h = img.size
    img = img.convert('RGB').crop(((w - size) // 2, (h - size) // 2, (w + size) // 2, (h + size) // 2)) if min(w, h) >= size else img.convert('RGB').resize((size, size))
    shifted = img.transform(img.size, Image.AFFINE, (1, 0, size // 2, 0, 1, size // 2))
    shifted.paste(img.crop((size // 2, size // 2, size, size)), (0, 0))
    shifted.paste(img.crop((0, size // 2, size // 2, size)), (size // 2, 0))
    shifted.paste(img.crop((size // 2, 0, size, size // 2)), (0, size // 2))
    shifted.paste(img.crop((0, 0, size // 2, size // 2)), (size // 2, size // 2))
    mask = Image.new('L', (size, size), 0)
    d = ImageDraw.Draw(mask)
    band = size // 8
    d.rectangle([size // 2 - band, 0, size // 2 + band, size], fill=255)
    d.rectangle([0, size // 2 - band, size, size // 2 + band], fill=255)
    return Image.composite(img, shifted, mask.filter(ImageFilter.GaussianBlur(band // 2)))


def cut(img):
    # The subject alone: rembg's isnet matte when installed (pip install rembg; it fetches the model once),
    # else the edge flood (fails where the paper is lighter in the middle).
    try:
        from rembg import new_session, remove
        cut.session = getattr(cut, 'session', None) or new_session('isnet-general-use')
        out = remove(img.convert('RGB'), session=cut.session)
        return out.crop(out.getbbox() or (0, 0) + out.size)
    except ImportError:
        return key_out(img)


def icon(file, size=128, ink=None):
    # A cut-out icon fitted into a square with a small margin; ink: its dark marks recoloured (read on lacquer).
    img = cut(Image.open(file))
    if ink:
        alpha = img.getchannel('A')
        img = Image.new('RGBA', img.size, ink + (255,))
        img.putalpha(alpha)
    img.thumbnail((size - 8, size - 8))
    out = Image.new('RGBA', (size, size))
    out.paste(img, ((size - img.width) // 2, (size - img.height) // 2), img)
    return out


def stub(size, ground, state, glow=8):
    # The map's ticket at button size: notches in both short sides, the perforation 40 px from the right end
    # (inside the right slice, so it stays at the stub however wide the name is); next: gold edge and glow.
    w, h = size
    mask = Image.new('L', size, 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([0, 0, w - 1, h - 1], 5, fill=255)
    for x in (0, w - 1):
        md.ellipse([x - 8, h // 2 - 8, x + 8, h // 2 + 8], fill=0)
    face = ground.crop((0, 0, w, h)).convert('RGBA')
    if state == 'unknown':
        face = Image.blend(face, Image.new('RGBA', size, (10, 6, 5, 255)), 0.5)
    out = Image.new('RGBA', size)
    out.paste(face, (0, 0), mask)
    d = ImageDraw.Draw(out)
    edge = GOLD if state == 'next' else (130, 104, 66) if state == 'normal' else (70, 60, 50)
    d.rounded_rectangle([3, 3, w - 4, h - 4], 4, outline=edge + (255,), width=2 if state == 'next' else 1)
    for y in range(7, h - 7, 6):
        d.line([(w - 40, y), (w - 40, y + 3)], fill=edge + (255,), width=1)
    if state != 'next':
        return out
    halo = Image.new('RGBA', (w + 2 * glow, h + 2 * glow))
    ImageDraw.Draw(halo).rounded_rectangle([glow // 2, glow // 2, w + glow * 3 // 2, h + glow * 3 // 2], 8, outline=GOLD + (130,), width=5)
    halo = halo.filter(ImageFilter.GaussianBlur(4))
    halo.alpha_composite(out, (glow, glow))
    return halo


def lozenge(size):
    # The next-stage mark: a gold diamond, its left faces lit (the painted one would not cut out whole).
    img = Image.new('RGBA', (size * 4, size * 4))
    d = ImageDraw.Draw(img)
    c, r = size * 2, size * 1.7
    top, right, bottom, left, mid = (c, c - r), (c + r * 0.7, c), (c, c + r), (c - r * 0.7, c), (c, c)
    for face, colour in [((top, mid, left), (245, 205, 110)), ((top, right, mid), (212, 169, 74)),
                         ((left, mid, bottom), (186, 140, 56)), ((mid, right, bottom), (140, 100, 40))]:
        d.polygon(face, fill=colour + (255,))
    return img.resize((size, size), Image.LANCZOS)


def bead(size, lit):
    # The AP bead: a gold bead, or an empty gold ring.
    img = Image.new('RGBA', (size * 4, size * 4))
    d = ImageDraw.Draw(img)
    r = size * 4
    if lit:
        d.ellipse([r * 0.1, r * 0.1, r * 0.9, r * 0.9], fill=GOLD + (255,))
        d.ellipse([r * 0.25, r * 0.2, r * 0.5, r * 0.42], fill=(255, 236, 180, 200))
    else:
        d.ellipse([r * 0.12, r * 0.12, r * 0.88, r * 0.88], outline=GOLD + (255,), width=int(r * 0.09))
    return img.resize((size, size), Image.LANCZOS)


def smooth(size, base):
    # A frame's ground: the lacquer's depth only. Flecks and crackle smear when a 9-slice stretches them.
    w, h = size
    top, bottom = base
    img = Image.new('RGB', size)
    d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)))
    return img


def frames(egg):
    ground, raised = smooth((512, 512), BASE), smooth((512, 512), RAISED)
    slices = {}

    def put(name, img, s):
        save(img, 'frame', name + '.png')
        slices[name] = [s, s, s, s]

    put('panel', panel((128, 128), ground), 24)
    put('panel_gold', panel((128, 128), ground, line=2), 24)
    put('button', panel((96, 48), raised, inset=4), 16)
    focus = panel((96, 48), raised, line=2, inset=4)
    ImageDraw.Draw(focus).rounded_rectangle([0, 0, 95, 47], 6, outline=GOLD + (255,), width=2)
    put('button_focus', focus, 16)
    off = panel((96, 48), ground, inset=4)
    d = ImageDraw.Draw(off)
    d.rounded_rectangle([4, 4, 91, 43], 4, outline=(26, 12, 10, 255), width=1)
    for x in range(8, 88, 8):  # the dashed dull line
        d.line([(x, 4), (x + 4, 4)], fill=(90, 74, 50, 255)), d.line([(x, 43), (x + 4, 43)], fill=(90, 74, 50, 255))
    put('button_disabled', off, 16)
    menu = Image.new('RGBA', (128, 128))
    shadow = Image.new('RGBA', (128, 128))
    ImageDraw.Draw(shadow).rounded_rectangle([6, 8, 124, 126], 8, fill=(0, 0, 0, 150))
    menu.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(4)))
    menu.alpha_composite(panel((120, 120), ground), (2, 2))
    put('menu', menu, 20)
    for name, colour in [('bubble', (214, 95, 71)), ('toast', (170, 190, 110))]:
        img = panel((96, 64) if name == 'bubble' else (96, 48), ground, inset=4)
        ImageDraw.Draw(img).rounded_rectangle([4, 4, 91, img.height - 5], 4, outline=colour + (255,), width=1)
        put(name, img, 16)
    plate = panel((96, 32), ground, inset=3, radius=4)
    put('nameplate', plate, 12)
    put('tab', panel((96, 40), ground, inset=4), 14)
    put('tab_active', panel((96, 40), raised, line=2, inset=4), 14)
    for state, name in [('normal', 'ticket'), ('next', 'ticket_next'), ('unknown', 'ticket_unknown')]:
        put(name, stub((256, 64), ground, state), 0)
        slices[name] = [18, 20, 48, 20] if state != 'next' else [26, 28, 56, 28]  # left, top, right, bottom; the glow adds 8
    with open(os.path.join(OUT, 'frame', 'slices.json'), 'w') as f:
        json.dump(slices, f, indent=1)


def main(raw):
    egg = Image.open(os.path.join(raw, 'tex_eggshell2_222.png'))
    # 1. Textures: lacquer made here, the rest from SDXL, tileable at 512.
    save(lacquer((512, 512), BASE, egg, 10), 'tex', 'lacquer_dark.png')
    save(tileable(Image.open(os.path.join(raw, 'tex_gold_leaf_121.png')), 512), 'tex', 'gold_leaf.png')
    save(tileable(egg, 512), 'tex', 'eggshell.png')
    save(Image.open(os.path.join(raw, 'tex_paper2_231.png')).convert('RGB').resize((1344, 768)), 'tex', 'paper_plan.png')
    # 2. Frames.
    frames(egg)
    # 3. Icons: the picks cut out; the tick and the beads drawn; the locked bead is the lit one, dim and grey.
    for name, pick in ICONS.items():
        # The matte took the diamond's lower half for shadow (the flood keeps it); the footprints go eggshell.
        save(icon(os.path.join(raw, pick + '.png'), ink=(240, 229, 204) if name == 'cmd_move' else None), 'icon', name + '.png')
    ready = Image.open(os.path.join(OUT, 'icon', 'upgrade_ready.png'))
    locked = ImageEnhance.Brightness(ImageEnhance.Color(ready).enhance(0.15)).enhance(0.55)
    save(locked, 'icon', 'upgrade_locked.png')
    save(tick(128), 'icon', 'mark_cleared.png')
    save(lozenge(128), 'icon', 'mark_next.png')
    save(bead(32, True), 'icon', 'ap_full.png')
    save(bead(32, False), 'icon', 'ap_empty.png')
    # 4. Ingredients: a cut-out icon and a still life (the middle, so the painted scribbles in the corners go).
    for name, (pick, pic) in ITEMS.items():
        save(icon(os.path.join(raw, pick + '.png')), 'item', name + '.png')
        img = Image.open(os.path.join(raw, pic + '.png')).convert('RGB')
        w, h = img.size
        save(img.crop((int(w * 0.06), int(h * 0.04), int(w * 0.88), int(h * 0.86))).resize((512, 512), Image.LANCZOS), 'item', name + '_pic.png')
    print('Wrote', OUT)


if __name__ == '__main__':
    main(sys.argv[1])

# Composes the campaign UI style board (ui-art-requests section 0) from the painted pictures.
#   python3 tools/art/ui_style_board_compose.py <raw folder> <out folder>
# Writes style_board_A.png (red-black lacquer) and style_board_B.png (warmer, browner lacquer), each
# 1280x720: the box office backdrop on the left, the cafe wall on the right, the UI pieces over both.
# Lacquer grounds are made here (SDXL draws reflections); SDXL gives the eggshell, paper, gold and icons.
import os, random, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont, ImageChops

ROOT = os.path.join(os.path.dirname(__file__), '..', '..', 'plugins', 'last-curtain', 'apps', 'LastCurtain', 'assets')
FONTS = os.path.join(ROOT, 'ui', 'fonts')
GOLD, TEXT, DIM = (212, 169, 74), (240, 229, 204), (150, 140, 120)


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), size)


def key_out(image):
    # The background is what floods in from the edges (backgrounds here shade, so one colour won't do):
    # 1. a smaller copy, flooded from points along the border with a tolerance; 2. that region clear.
    small = image.convert('RGB').resize((256, 256))
    magic = (255, 0, 255)
    for i in range(0, 256, 16):
        for p in [(i, 0), (i, 255), (0, i), (255, i)]:
            if small.getpixel(p) != magic:
                ImageDraw.floodfill(small, p, magic, thresh=38)
    mask = Image.eval(ImageChops.difference(small, Image.new('RGB', small.size, magic)).convert('L'), lambda v: 255 if v > 8 else 0)
    mask = mask.filter(ImageFilter.MinFilter(3)).filter(ImageFilter.GaussianBlur(1)).resize(image.size)
    out = image.convert('RGBA')
    out.putalpha(mask)
    return out.crop(out.getbbox() or (0, 0) + image.size)


def lacquer(size, base, eggshell, seed):
    # 1. A deep ground with a soft vertical depth; 2. a faint crackle; 3. a few eggshell flecks.
    w, h = size
    top, bottom = base
    ground = Image.new('RGB', size)
    d = ImageDraw.Draw(ground)
    for y in range(h):
        t = y / max(1, h - 1)
        d.line([(0, y), (w, y)], fill=tuple(int(top[i] * (1 - t) + bottom[i] * t) for i in range(3)))
    rnd = random.Random(seed)
    crackle = Image.new('L', size, 0)
    cd = ImageDraw.Draw(crackle)
    for _ in range(w * h // 5000):
        x, y = rnd.randrange(w), rnd.randrange(h)
        for _ in range(rnd.randint(2, 4)):
            nx, ny = x + rnd.randint(-9, 9), y + rnd.randint(-9, 9)
            cd.line([(x, y), (nx, ny)], fill=7, width=1)
            x, y = nx, ny
    ground = ImageChops.subtract(ground, Image.merge('RGB', [crackle] * 3))
    flecks = eggshell.convert('L').resize(size).point(lambda v: 255 if v > 170 else 0).filter(ImageFilter.GaussianBlur(0.6))
    ground.paste(Image.new('RGB', size, (225, 214, 188)), (0, 0), flecks.point(lambda v: v * 0.35))
    return ground


def panel(size, ground, line=1, radius=6, inset=6):
    # A rounded lacquer panel with a gold-leaf line inset.
    w, h = size
    mask = Image.new('L', size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, w - 1, h - 1], radius, fill=255)
    out = Image.new('RGBA', size)
    out.paste(ground.crop((0, 0, w, h)), (0, 0), mask)
    ImageDraw.Draw(out).rounded_rectangle([inset, inset, w - 1 - inset, h - 1 - inset], max(1, radius - 2), outline=GOLD + (255,), width=line)
    return out


def ticket(size, ground, state):
    # A torn theatre ticket: notches in both short sides, a perforation 70 px from the right end.
    w, h = size
    mask = Image.new('L', size, 0)
    md = ImageDraw.Draw(mask)
    md.rounded_rectangle([0, 0, w - 1, h - 1], 6, fill=255)
    for x in (0, w - 1):
        md.ellipse([x - 12, h // 2 - 12, x + 12, h // 2 + 12], fill=0)
    face = ground.crop((0, 0, w, h)).convert('RGBA')
    if state == 'unknown':
        face = Image.blend(face, Image.new('RGBA', size, (10, 6, 5, 255)), 0.5)
    out = Image.new('RGBA', size)
    out.paste(face, (0, 0), mask)
    d = ImageDraw.Draw(out)
    edge = GOLD if state == 'next' else (110, 90, 60) if state == 'normal' else (70, 60, 50)
    d.rounded_rectangle([3, 3, w - 4, h - 4], 5, outline=edge + (255,), width=2 if state == 'next' else 1)
    for y in range(8, h - 8, 7):
        d.line([(w - 70, y), (w - 70, y + 3)], fill=edge + (255,), width=1)
    if state == 'next':
        glow = Image.new('RGBA', (w + 24, h + 24))
        ImageDraw.Draw(glow).rounded_rectangle([6, 6, w + 17, h + 17], 10, outline=GOLD + (120,), width=6)
        glow = glow.filter(ImageFilter.GaussianBlur(6))
        glow.paste(out, (12, 12), out)
        return glow
    return out


def tick(size):
    # The cleared mark: one brushed gold stroke, drawn (SDXL drew no tick).
    img = Image.new('RGBA', (size, size))
    d = ImageDraw.Draw(img)
    pts = [(size * 0.18, size * 0.55), (size * 0.42, size * 0.78), (size * 0.84, size * 0.22)]
    for i, w in enumerate((int(size * 0.16), int(size * 0.12))):
        d.line(pts, fill=GOLD + (255,), width=w, joint='curve')
    return img.filter(ImageFilter.GaussianBlur(0.8))


def text(d, xy, s, f, fill=TEXT, anchor='la'):
    d.text(xy, s, font=f, fill=fill, anchor=anchor)


def board(variant, raw, out):
    base = ((26, 12, 10), (14, 7, 6)) if variant == 'A' else ((46, 24, 16), (26, 13, 9))
    egg = Image.open(os.path.join(raw, 'tex_eggshell2_222.png'))
    ground = lacquer((1280, 720), base, egg, 7 if variant == 'A' else 8)
    raised = lacquer((400, 300), tuple(tuple(min(255, c + 12) for c in col) for col in base), egg, 9)
    # 1. Places: the box office left, the cafe wall right.
    canvas = Image.new('RGBA', (1280, 720))
    left = Image.open(os.path.join(ROOT, 'art', 'backdrops', 'box_office.jpg')).convert('RGBA').resize((640, 720))
    right = Image.open(os.path.join(ROOT, 'art', 'cafe', 'back_wall.jpg')).convert('RGBA').resize((640, 720))
    canvas.paste(left, (0, 0))
    canvas.paste(right, (640, 0))
    d = ImageDraw.Draw(canvas)
    title, body, small = font('PlayfairDisplay.ttf', 26), font('BeVietnamPro-Regular.ttf', 16), font('BeVietnamPro-Regular.ttf', 12)
    strong = font('BeVietnamPro-Medium.ttf', 16)
    # 2. A panel with a heading and a line.
    p = panel((360, 150), ground)
    canvas.alpha_composite(p, (40, 40))
    text(d, (64, 64), 'Phòng vé', title, GOLD)
    text(d, (64, 104), 'Màn 1 · gợi ý cấp 3', body)
    text(d, (64, 130), 'Mục tiêu: cắt dây cho bé Tí', small, DIM)
    # 3. Buttons: normal, focused, disabled.
    for i, (label, state) in enumerate([('Vào màn', 'normal'), ('Giao tranh', 'focus'), ('Chơi lại', 'disabled')]):
        x, y = 40 + i * 124, 216
        if state == 'focus':
            b = Image.new('RGBA', (112, 44))
            ImageDraw.Draw(b).rounded_rectangle([0, 0, 111, 43], 6, fill=GOLD + (255,))
            canvas.alpha_composite(b, (x, y))
            text(d, (x + 56, y + 22), label, strong, (23, 12, 10), 'mm')
        else:
            b = panel((112, 44), raised, 1, 6, 3)
            if state == 'disabled':
                b = Image.blend(b, Image.new('RGBA', b.size, (20, 12, 10, 255)), 0.45)
                b.putalpha(panel((112, 44), raised, 1, 6, 3).getchannel('A'))
            canvas.alpha_composite(b, (x, y))
            text(d, (x + 56, y + 22), label, strong, TEXT if state == 'normal' else DIM, 'mm')
    # 4. Tickets: normal (cleared), next, unknown.
    for i, (name, state, mark) in enumerate([('Tiền sảnh', 'normal', 'tick'), ('Phòng vé', 'next', ''), ('?', 'unknown', '')]):
        t = ticket((256, 96), ground, state)
        x, y = (40, 300 + i * 112) if state != 'next' else (28, 300 + i * 112 - 12)
        canvas.alpha_composite(t, (x, y))
        cx, cy = (x + 12, y + 12) if state == 'next' else (x, y)
        text(d, (cx + 28, cy + 48), name, title if state != 'unknown' else body, GOLD if state == 'next' else TEXT, 'lm')
        if mark == 'tick':
            canvas.alpha_composite(tick(36), (cx + 194, cy + 30))
        if state == 'next':
            d.polygon([(cx + 200, cy + 38), (cx + 210, cy + 48), (cx + 200, cy + 58), (cx + 190, cy + 48)], fill=GOLD + (255,))
    # 5. Icons and the ginger on a panel (right side, over the cafe wall).
    p = panel((560, 190), ground)
    canvas.alpha_composite(p, (680, 40))
    icons = [('icon_cmd_cut_171.png', 'Cắt dây'), ('icon_mark_skirmish2_252.png', 'Giao tranh'), ('icon_rite2_241.png', 'Lễ cầu hồn'),
             (None, 'Đã qua'), ('item_gung_191.png', 'Gừng ×2')]
    for i, (f, label) in enumerate(icons):
        x = 700 + i * 106
        img = tick(84) if f is None else key_out(Image.open(os.path.join(raw, f)))
        img.thumbnail((84, 84))
        canvas.alpha_composite(img, (x + (84 - img.width) // 2, 70 + (84 - img.height) // 2))
        text(d, (x + 42, 176), label, small, TEXT, 'mm')
    # 6. The command menu beside a unit.
    menu = panel((210, 252), raised, 1, 6, 5)
    shadow = Image.new('RGBA', (230, 272))
    ImageDraw.Draw(shadow).rounded_rectangle([10, 14, 220, 266], 8, fill=(0, 0, 0, 150))
    shadow = shadow.resize((230, 272))
    canvas.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(8)), (690, 270))
    canvas.alpha_composite(menu, (700, 270))
    rows = [('Di chuyển', '1'), ('Tấn công', '2'), ('Kỹ năng ›', '3'), ('Vật phẩm ›', '4'), ('Cắt dây', '5'), ('Kết thúc lượt', 'F')]
    for i, (label, key) in enumerate(rows):
        y = 290 + i * 36
        if i == 1:
            hi = Image.new('RGBA', (186, 30))
            ImageDraw.Draw(hi).rounded_rectangle([0, 0, 185, 29], 4, fill=GOLD + (255,))
            canvas.alpha_composite(hi, (712, y - 4))
        col = (23, 12, 10) if i == 1 else TEXT
        text(d, (724, y + 11), label, strong, col, 'lm')
        text(d, (890, y + 11), key, small, col if i == 1 else DIM, 'rm')
    # 7. A sub-menu with costs, and the target bubble.
    sub = panel((220, 110), raised, 1, 6, 5)
    canvas.alpha_composite(sub, (920, 340))
    text(d, (936, 370), 'Quạt gió', strong)
    text(d, (1124, 370), '2 HP', small, DIM, 'rm')
    text(d, (936, 404), 'Hồi sức', strong)
    text(d, (1124, 404), '3 HP', small, DIM, 'rm')
    bubble = Image.new('RGBA', (120, 64))
    ImageDraw.Draw(bubble).rounded_rectangle([0, 0, 119, 63], 6, fill=base[0] + (240,), outline=(214, 95, 71, 255), width=1)
    canvas.alpha_composite(bubble, (1150, 470))
    text(d, (1210, 488), '5–7', strong, TEXT, 'mm')
    text(d, (1210, 512), 'trả 3–4', small, DIM, 'mm')
    # 8. A paper sample for the world map, and the variant's name.
    paper = Image.open(os.path.join(raw, 'tex_paper2_231.png')).convert('RGBA').resize((300, 170))
    canvas.alpha_composite(paper, (330, 420))
    for (x0, y0, x1, y1) in [(350, 440, 470, 500), (480, 440, 610, 520), (350, 510, 470, 570)]:
        d.rectangle([x0, y0, x1, y1], outline=(150, 120, 70, 255), width=1)
    text(d, (1240, 700), 'Bản ' + variant, small, DIM, 'rs')
    canvas.convert('RGB').save(os.path.join(out, 'style_board_%s.png' % variant))


if __name__ == '__main__':
    raw, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for v in 'AB':
        board(v, raw, out)
    print('Wrote', os.path.join(out, 'style_board_A.png'), 'and B')

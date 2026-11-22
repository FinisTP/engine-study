# Mock-up of the engine's sprite forms for the concept art review (art list section 00).
# It only imitates what the engine will draw (tint, glow, strings, gold knot, gold rim); the real
# look comes from the engine's sprite shader later.
# Usage: python3 style_board.py <board.json> <out.jpg>
import json, sys
from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

FONT = '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf'


def font(size):
    try:
        return ImageFont.truetype(FONT, size)
    except OSError:
        return ImageFont.load_default()


def fit(im, w, h):
    # 1. Scale to fit the box, keeping the shape.
    s = min(w / im.width, h / im.height)
    return im.resize((max(1, round(im.width * s)), max(1, round(im.height * s))), Image.LANCZOS)


def tinted(im, colour, amount, opacity):
    # 1. Mix the picture toward the tint (the engine's emissive tint), then scale the alpha.
    rgb = Image.blend(im.convert('RGB'), Image.new('RGB', im.size, colour), amount)
    a = im.getchannel('A').point(lambda v: round(v * opacity))
    rgb.putalpha(a)
    return rgb


def glow(im, colour, radius, strength):
    # 1. A soft halo: the blurred alpha, coloured, under the figure.
    a = im.getchannel('A').filter(ImageFilter.GaussianBlur(radius)).point(lambda v: min(255, round(v * strength)))
    halo = Image.new('RGBA', im.size, colour + (0,))
    halo.putalpha(a)
    return Image.alpha_composite(halo, im)


def rim(im, colour, width):
    # 1. A thin edge: the grown alpha minus the alpha, coloured and softened.
    a = im.getchannel('A')
    edge = ImageChops.subtract(a.filter(ImageFilter.MaxFilter(width * 2 + 1)), a).filter(ImageFilter.GaussianBlur(2))
    layer = Image.new('RGBA', im.size, colour + (0,))
    layer.putalpha(edge)
    return Image.alpha_composite(im, layer)


def form(im, name):
    # The forms of the art list's section 12 table.
    if name == 'specter':
        return glow(tinted(im, (170, 30, 30), 0.45, 0.9), (200, 30, 30), 14, 1.2)
    if name == 'cafe':
        return glow(tinted(im, (235, 235, 225), 0.25, 0.85), (230, 230, 220), 12, 1.0)
    if name == 'ally':
        return glow(tinted(im, (60, 120, 230), 0.25, 0.95), (70, 130, 255), 14, 1.2)
    if name == 'freed':
        return rim(glow(im, (255, 200, 90), 12, 0.9), (255, 210, 110), 3)
    if name == 'friendly':
        return glow(tinted(im, (90, 190, 110), 0.35, 0.85), (90, 200, 110), 12, 1.1)
    if name == 'neutral':
        return glow(tinted(im, (150, 165, 180), 0.4, 0.85), (160, 170, 185), 12, 1.0)
    if name == 'aggressive':
        return glow(tinted(im, (170, 30, 30), 0.45, 0.85), (200, 30, 30), 14, 1.2)
    if name == 'zodiac':
        return glow(tinted(im, (150, 20, 20), 0.3, 1.0), (220, 30, 30), 22, 1.6)
    if name == 'hostess':
        return glow(tinted(im, (255, 225, 200), 0.1, 0.95), (255, 210, 170), 12, 0.8)
    return im


def strings(draw, x, y, w, h, anchors, top, width, knot):
    # 1. Red lines from each anchor up out of view; 2. a gold knot on the first one.
    for ax, ay in anchors:
        px, py = x + ax * w, y + ay * h
        for wd, col in ((width + 4, (120, 10, 10)), (width, (235, 50, 40))):
            draw.line([(px, py), (px + (ax - 0.5) * 40, top)], fill=col, width=wd)
    if knot and anchors:
        ax, ay = anchors[0]
        kx, ky = x + ax * w + (ax - 0.5) * 20, (y + ay * h + top) / 2
        draw.ellipse([kx - 9, ky - 9, kx + 9, ky + 9], fill=(240, 190, 60), outline=(120, 80, 10), width=2)


def main(cfg_path, out_path):
    cfg = json.load(open(cfg_path))
    board = Image.new('RGB', tuple(cfg['size']), (24, 21, 20))
    draw = ImageDraw.Draw(board)
    # 1. Plain pictures (backdrops, stills, cards), placed as they are.
    for p in cfg.get('pictures', []):
        im = fit(Image.open(p['file']).convert('RGBA'), p['w'], p['h'])
        board.paste(im, (p['x'], p['y']), im)
    # 2. Figures: the form, then strings, standing on the box's bottom edge.
    for f in cfg.get('figures', []):
        im = Image.open(f['file']).convert('RGBA')
        pad = 40
        canvas = Image.new('RGBA', (im.width + 2 * pad, im.height + 2 * pad))
        canvas.paste(im, (pad, pad))
        canvas = fit(form(canvas, f.get('form', 'none')), f['w'], f['h'])
        x = f['x'] + (f['w'] - canvas.width) // 2
        y = f['y'] + f['h'] - canvas.height
        board.paste(canvas, (x, y), canvas)
        if f.get('strings'):
            s = canvas.width / (im.width + 2 * pad)
            inner_w, inner_h = im.width * s, im.height * s
            strings(draw, x + pad * s, y + pad * s, inner_w, inner_h, f['strings'], f.get('top', f['y'] - 10),
                    f.get('string_width', 2), f.get('knot', False))
    # 3. Labels.
    for t in cfg.get('labels', []):
        draw.text((t['x'], t['y']), t['text'], fill=tuple(t.get('colour', (225, 215, 195))), font=font(t.get('size', 22)))
    board.save(out_path, quality=90)


if __name__ == '__main__':
    main(sys.argv[1], sys.argv[2])

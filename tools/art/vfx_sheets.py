# Particle sheets for the VFX library: 2x2 pictures of 256 px, white on clear (the engine tints).
# Painted ones are cut from their grey ground by how far each pixel is from it; simple shapes are drawn here.
# Usage: python vfx_sheets.py <painted folder> <out folder>
import sys, glob, os
import numpy as np
from PIL import Image, ImageFilter

CELL = 256
src, out = sys.argv[1], sys.argv[2]
rng = np.random.default_rng(7)
yy, xx = np.mgrid[-1:1:CELL * 1j, -1:1:CELL * 1j]
r = np.sqrt(xx ** 2 + yy ** 2)

def cut(path, dark):
    # 1. Grey ground = the border's median; alpha = distance from it (darker for ink, lighter for smoke).
    a = np.asarray(Image.open(path).convert('L').resize((CELL, CELL), Image.LANCZOS)).astype(np.float32) / 255
    ground = np.median(np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]]))
    alpha = np.clip((((ground - a) if dark else (a - ground)) - 0.08) / 0.3, 0, 1)  # the ground's own shading stays clear
    # 2. Edges fade so no square shows.
    return alpha * np.clip((1 - r) / 0.25, 0, 1)

def glow(core):  # a soft round light
    return np.clip(1 - r, 0, 1) ** core

def star(points, k):  # a thin flare of `points` rays
    t = np.arctan2(yy, xx)
    return np.clip(np.abs(np.cos(t * points / 2)) ** k * (1 - r) ** 2, 0, 1) + glow(6) * 0.8

def streak(width):  # a long tapered line across
    return np.exp(-(yy / width) ** 2) * np.clip(1 - np.abs(xx), 0, 1) ** 0.7

def bead(light):  # a shaded ball
    inside = (r < 0.8).astype(np.float32)
    return inside * (0.55 + 0.45 * np.clip(1 - np.sqrt((xx + light) ** 2 + (yy + light) ** 2) * 1.4, 0, 1)) * np.clip((0.8 - r) / 0.05, 0, 1)

def flake(seed):  # a torn scrap: rough noise inside a wobbling outline
    g = np.random.default_rng(seed)
    t = np.arctan2(yy, xx)
    edge = 0.55 + sum(g.uniform(0.02, 0.1) * np.sin(t * k + g.uniform(0, 6)) for k in (2, 3, 5, 7))
    noise = np.asarray(Image.fromarray((g.random((32, 32)) * 255).astype(np.uint8)).resize((CELL, CELL), Image.BICUBIC)) / 255
    return np.clip((edge - r) / 0.03, 0, 1) * (0.5 + 0.5 * noise)

def sheet(name, cells):
    # Four pictures in a 2x2 sheet, white RGB with the picture as alpha.
    s = np.zeros((CELL * 2, CELL * 2), np.float32)
    for i, c in enumerate(cells):
        s[(i // 2) * CELL:(i // 2 + 1) * CELL, (i % 2) * CELL:(i % 2 + 1) * CELL] = c
    rgba = np.dstack([np.full_like(s, 255), np.full_like(s, 255), np.full_like(s, 255), s * 255]).astype(np.uint8)
    Image.fromarray(rgba, 'RGBA').save(os.path.join(out, name + '.png'))

painted = lambda n, ids, dark: [cut(os.path.join(src, f'vfx_{n}_{i}.png'), dark) for i in ids]
sheet('dab_ink', painted('dab_ink', [31, 32, 34, 32], True))
sheet('splash_ink', painted('splash_ink', [32, 34, 32, 34], True))
sheet('puff_dust', painted('puff_dust', [31, 32, 33, 34], False))
sheet('smoke_incense', painted('smoke_incense', [31, 32, 33, 34], False))
sheet('thread_red', painted('thread_red', [31, 32, 33, 34], False))
sheet('mote_lamp', [glow(2), glow(3), glow(1.5), glow(2.5)])
sheet('ember', [glow(4), glow(5) * 0.9 + glow(1) * 0.2, glow(3), glow(6) + glow(1.2) * 0.15])
sheet('fleck_gold', [star(4, 40), star(6, 60), star(4, 80), star(8, 50)])
sheet('streak', [streak(0.08), streak(0.05), streak(0.12), streak(0.04)])
sheet('bead_gold', [bead(0.3), bead(0.2), bead(0.35), bead(0.25)])
sheet('ash_paper', [flake(1), flake(2), flake(3), flake(4)])
print('ok')

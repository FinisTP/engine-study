# Renders every sprite motion in motions.json to one MP4, to judge them before the engine plays them.
# Usage: python3 preview_motions.py motions.json sprite_dir stage.png specter.png out.mp4
# sprite_dir holds body_<face>.png (facing right, alpha). The specter is one picture, mirrored to face left.
import json, math, subprocess, sys
import numpy as np
from PIL import Image, ImageDraw

W, H, FPS, SPRITE_H, FLOOR = 1280, 720, 30, 500, 680
motions_p, sprite_dir, stage_p, specter_p, out = sys.argv[1:6]
M = {k: v for k, v in json.load(open(motions_p)).items() if not k.startswith('_')}
stage = Image.open(stage_p).convert('RGBA').resize((W, H))

def fit(im):
    im = im.crop(im.getchannel('A').getbbox())
    return im.resize((round(im.width * SPRITE_H / im.height), SPRITE_H), Image.LANCZOS)

faces = {f: fit(Image.open(f'{sprite_dir}/body_{f}.png').convert('RGBA'))
         for f in ['calm', 'angry', 'neardead', 'happy', 'scheming', 'afraid', 'talk']}
spec_im = fit(Image.open(specter_p).convert('RGBA'))
NOISE = np.random.default_rng(3).random((SPRITE_H * 2, SPRITE_H * 2))
FIELDS = ['x', 'y', 'sx', 'sy', 'rot', 'skew', 'alpha', 'shake', 'dissolve']
BASE = dict(x=0, y=0, sx=1, sy=1, rot=0, skew=0, alpha=1, shake=0, dissolve=0, tint=[1, 1, 1, 0])

def smooth(a): return a * a * (3 - 2 * a)

def sample(m, t):
    # 1. Loop or clamp time, 2. for each field, ease between the keys that set it.
    t = t % m['seconds'] if m.get('loop') else min(t, m['seconds'])
    v = dict(BASE)
    for f in FIELDS + ['tint']:
        ks = [k for k in m['keys'] if f in k]
        if not ks: continue
        prev = ks[0]
        val = prev[f]
        for k in ks:
            if k['t'] <= t: prev, val = k, k[f]
            else:
                a = smooth((t - prev['t']) / max(k['t'] - prev['t'], 1e-6))
                if f == 'tint': val = [p + (q - p) * a for p, q in zip(prev[f], k[f])]
                else: val = prev[f] + (k[f] - prev[f]) * a
                break
        v[f] = val
    face = 'calm'
    for ft, fn in m.get('faces', []):
        if ft <= t: face = fn
    return v, face

def draw(frame, im, v, feet_x, facing, t):
    # 1. Tint, fade and burn the picture.
    a = np.array(im).astype(np.float32)
    tr, tg, tb, amt = v['tint']
    a[..., :3] = a[..., :3] * (1 - amt) + np.array([tr, tg, tb]) * 255 * amt
    a[..., 3] *= v['alpha']
    if v['dissolve'] > 0:
        n = NOISE[:a.shape[0], :a.shape[1]]
        a[..., 3] = np.where(n < v['dissolve'], 0, a[..., 3])
        edge = (n >= v['dissolve']) & (n < v['dissolve'] + 0.05) & (a[..., 3] > 0)
        a[edge, :3] = [255, 170, 60]
    src = Image.fromarray(a.clip(0, 255).astype(np.uint8))
    # 2. Picture pixel -> feet-local (y up) -> scale, skew, lean -> screen (Motions.md "The transform").
    w, h = src.size
    r = math.radians(v['rot'])
    to_local = np.array([[1, 0, -w / 2], [0, -1, h], [0, 0, 1]])
    scale = np.diag([v['sx'], v['sy'], 1])
    skew = np.array([[1, v['skew'], 0], [0, 1, 0], [0, 0, 1]])
    lean = np.array([[math.cos(r), math.sin(r), 0], [-math.sin(r), math.cos(r), 0], [0, 0, 1]])
    shake = v['shake'] * SPRITE_H * math.sin(t * 70)
    to_screen = np.array([[facing, 0, feet_x + facing * v['x'] * SPRITE_H + shake],
                          [0, -1, FLOOR - v['y'] * SPRITE_H], [0, 0, 1]])
    fwd = to_screen @ lean @ skew @ scale @ to_local
    inv = np.linalg.inv(fwd)
    layer = src.transform((W, H), Image.AFFINE, tuple(inv[:2].flatten()), Image.BILINEAR)
    frame.alpha_composite(layer)

def actor(frame, im, name, t, feet_x, facing):
    m = M[name]
    if m.get('flip'): facing = -facing
    if m.get('trail') and t > 0.15:
        for g in (3, 2, 1):
            v, face = sample(m, t - 0.05 * g)
            v['alpha'] *= 0.12 * (4 - g)
            draw(frame, im(face), v, feet_x, facing, t)
    v, face = sample(m, t)
    draw(frame, im(face), v, feet_x, facing, t)

hero = lambda f: faces[f]
spec = lambda f: spec_im
HX, SX = 430, 900
# (hero motion, specter motion, specter delay, caption): every game moment that has a motion.
SCENES = [
    ('idle', 'idle', 0, 'idle: any guest waiting'),
    ('walk', 'idle', 0, 'walk: moving between rooms'),
    ('pickup', 'idle', 0, 'pickup: takes an item from the floor'),
    ('drop', 'idle', 0, 'drop: leaves an item'),
    ('use', 'idle', 0, 'use: uses an item, or shows a prop in the duel'),
    ('brace', 'idle', 0, 'brace: an event roll is coming'),
    ('pass', 'idle', 0, 'pass: the roll succeeds'),
    ('fail', 'idle', 0, 'fail: the roll fails'),
    ('talk', 'idle', 0, 'talk: asks, answers, chats'),
    ('nod', 'idle', 0, 'nod: grants a request'),
    ('refuse', 'idle', 0, 'refuse: turns a request down'),
    ('scheme', 'idle', 0, 'scheme: a bot plots (asks someone to hunt)'),
    ('attack', 'hit', 0.42, 'attack: strikes a guest, a monster, or the specter (Strike)'),
    ('guard', 'attack', -0.1, 'guard: Hold in the duel, guarding in a fight'),
    ('hit', 'attack', -0.42, 'hit: takes a blow, a trap or a bad event'),
    ('dodge', 'attack', -0.2, 'dodge: a blow that misses'),
    ('flee', 'idle', 0, 'flee: Leave a fight; runs from the killer'),
    ('power', 'idle', 0, 'power: a rare prop\'s power'),
    ('levelup', 'idle', 0, 'levelup: a level gained'),
    ('fear', 'idle', 0, 'fear: the killer is near'),
    ('down', 'idle', 0, 'down: HP runs out on the board'),
    ('die', 'idle', 0, 'die: killed; becomes a ghost'),
    ('ghost', 'idle', 0, 'ghost: walks to the Dressing room'),
    ('revive', 'idle', 0, 'revive: back to life at the Dressing room'),
    ('bow', 'idle', 0, 'bow: the duel ends standing; Applause; the winner at dawn'),
    ('booed', 'idle', 0, 'booed: HP runs out in the duel; booed off'),
    ('idle', 'loom', 0, 'specter loom: shows a heavy blow is next'),
    ('idle', 'twitch', 0, 'specter twitch: shows a light blow is next'),
    ('idle', 'taunt', 0, 'specter taunt: shows a taunt is next'),
    ('idle', 'die', 0, 'specter die: the specter is beaten (end of the finale)'),
]

def scene_seconds(h, s, d):
    lh = 2.4 if M[h].get('loop') else M[h]['seconds'] + 0.6
    ls = 0 if M[s].get('loop') else M[s]['seconds'] + max(d, 0) + 0.4
    return max(lh, ls, 1.6)

ff = subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{H}',
                       '-r', str(FPS), '-i', '-', '-c:v', 'libx264', '-preset', 'veryfast', '-pix_fmt', 'yuv420p', out],
                      stdin=subprocess.PIPE)
for h, s, d, cap in SCENES:
    secs = scene_seconds(h, s, d)
    hstart = max(-d, 0)  # 1. A negative delay: the specter acts first, the guest answers.
    for i in range(int(secs * FPS)):
        t = i / FPS
        f = stage.copy()
        actor(f, spec, s, max(t - max(d, 0), 0), SX, -1)
        actor(f, hero, h, max(t - hstart, 0), HX, 1)
        dr = ImageDraw.Draw(f)
        dr.rectangle((0, 0, W, 50), fill=(12, 10, 10, 210))
        dr.text((22, 13), cap, fill=(236, 226, 205), font_size=24)
        ff.stdin.write(f.convert('RGB').tobytes())
ff.stdin.close(); ff.wait()
print('done', out)

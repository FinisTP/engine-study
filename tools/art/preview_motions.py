# Renders every sprite motion in motions.json to one MP4, to judge them before the engine plays them.
# Usage: python3 preview_motions.py motions.json sprite_dir stage.png specter out.mp4
# sprite_dir holds body_<face>.png (facing right, alpha). specter: its sprite folder, or one picture; mirrored to face left.
import json, math, os, subprocess, sys
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
FACES = ['calm', 'angry', 'neardead', 'happy', 'scheming', 'afraid', 'talk']
spec_faces = ({f: fit(Image.open(f'{specter_p}/body_{f}.png').convert('RGBA')) for f in FACES} if os.path.isdir(specter_p)
              else {'calm': fit(Image.open(specter_p).convert('RGBA'))})
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
spec = lambda f: spec_faces.get(f, spec_faces['calm'])
HX, SX = 430, 900
# (hero motion, specter motion, specter delay, caption): every game moment that has a motion.
SCENES = [
    ('idle', 'haunt', 0, 'idle: any guest waiting  /  specter haunt: it waits, swaying'),
    ('walk', 'haunt', 0, 'walk: moving between rooms'),
    ('pickup', 'haunt', 0, 'pickup: takes an item from the floor'),
    ('drop', 'haunt', 0, 'drop: leaves an item'),
    ('use', 'haunt', 0, 'use: uses an item, or shows a prop in the duel'),
    ('brace', 'haunt', 0, 'brace: an event roll is coming'),
    ('pass', 'haunt', 0, 'pass: the roll succeeds'),
    ('fail', 'haunt', 0, 'fail: the roll fails'),
    ('talk', 'haunt', 0, 'talk: asks, answers, chats'),
    ('nod', 'haunt', 0, 'nod: grants a request; a turn opens'),
    ('refuse', 'haunt', 0, 'refuse: turns a request down'),
    ('scheme', 'haunt', 0, 'scheme: a bot plots (asks someone to hunt)'),
    ('attack', 'hit', 0.42, 'attack: Strike; the target is hit'),
    ('guard', 'lunge', -0.1, 'guard: Hold against the specter\'s lunge'),
    ('hit', 'lunge', -0.38, 'hit: the specter lunges and lands'),
    ('dodge', 'lunge', -0.2, 'dodge: a lunge that misses'),
    ('flee', 'haunt', 0, 'flee: Leave a fight; runs from the killer'),
    ('power', 'haunt', 0, 'power: a rare prop\'s power'),
    ('levelup', 'haunt', 0, 'levelup: a level gained'),
    ('fear', 'haunt', 0, 'fear: the killer is near'),
    ('down', 'haunt', 0, 'down: HP runs out on the board'),
    ('die', 'haunt', 0, 'die: killed; becomes a ghost'),
    ('ghost', 'haunt', 0, 'ghost: walks to the Dressing room'),
    ('revive', 'haunt', 0, 'revive: back to life at the Dressing room'),
    ('bow', 'haunt', 0, 'bow: the duel ends standing; the winner at dawn'),
    ('booed', 'haunt', 0, 'booed: HP runs out in the duel; booed off'),
    ('afraid_wait', 'loom', 0, 'specter loom: a heavy blow is next (it snarls)'),
    ('afraid_wait', 'twitch', 0, 'specter twitch: a light blow is next (its head cracks aside)'),
    ('idle', 'taunt', 0, 'specter taunt: a taunt is next (the mask grins)'),
    ('idle', 'talk', 0, 'specter talk: its line'),
    ('pass', 'vanish', 0, 'specter vanish: beaten, it screams and burns away'),
]
M['afraid_wait'] = M['fear']

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

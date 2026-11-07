# Rough front view of a slimmed GLB from -Z (Umbra's forward), painter's algorithm, texture
# sampled at each triangle's centroid. Only a sanity check of facing and texture mapping.
import io, json, struct, sys
import numpy as np
from PIL import Image, ImageDraw
def load(path):
    b = open(path, 'rb').read(); l = struct.unpack('<I', b[12:16])[0]; j = json.loads(b[20:20 + l])
    off = 20 + l; bl = struct.unpack('<I', b[off:off + 4])[0]; binb = b[off + 8:off + 8 + bl]
    def acc(i, c, dt):
        a = j['accessors'][i]; v = j['bufferViews'][a['bufferView']]
        return np.frombuffer(binb, dtype=dt, count=a['count'] * c, offset=v['byteOffset']).reshape(-1, c)
    p = acc(0, 3, np.float32); uv = acc(2, 2, np.float32); idx = acc(3, 1, np.uint32).reshape(-1, 3)
    v = j['bufferViews'][j['images'][0]['bufferView']]
    tex = Image.open(io.BytesIO(binb[v['byteOffset']:v['byteOffset'] + v['byteLength']])).convert('RGB')
    return p, uv, idx, np.asarray(tex)
def render(path, size=300, side=False):
    p, uv, idx, tex = load(path)
    img = Image.new('RGB', (size, size), (30, 30, 36)); d = ImageDraw.Draw(img)
    # Camera at -Z looking +Z: screen x = -x (viewer's right is model's -x... keep mirror-free: x_screen = -x), depth = z.
    sx = (-p[:, 0] if not side else p[:, 2]); sy = p[:, 1]; depth = (p[:, 2] if not side else p[:, 0])
    tri_depth = depth[idx].mean(1); order = np.argsort(-tri_depth)  # far first
    light = np.array([0.3, 0.6, -0.7]); light /= np.linalg.norm(light)
    for t in order:
        a, b, c = idx[t]
        n = np.cross(p[b] - p[a], p[c] - p[a]); ln = np.linalg.norm(n)
        if ln == 0: continue
        shade = 0.45 + 0.55 * max(0.0, float(np.dot(n / ln, light if not side else np.array([light[2], light[1], light[0]]))))
        cu = uv[[a, b, c]].mean(0); h, w = tex.shape[:2]
        col = tex[int(np.clip((cu[1]) % 1 * h, 0, h - 1)), int(np.clip(cu[0] % 1 * w, 0, w - 1))]
        pts = [((sx[k] * 0.48 + 0.5) * size, (0.5 - sy[k] * 0.48) * size) for k in (a, b, c)]
        d.polygon(pts, fill=tuple(int(x * shade) for x in col))
    return img
names = sys.argv[1:]
sheet = Image.new('RGB', (300 * len(names), 300))
for i, n in enumerate(names):
    sheet.paste(render(n, side=n.endswith('wolf.glb')), (300 * i, 0))
sheet.save('render.png')  # in the current folder

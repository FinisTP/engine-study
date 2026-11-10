# Packs a rigged character for Umbra (S8): one skinned mesh, its skeleton and named clips in one GLB.
#
#   python pack_animated.py rigged.glb out.glb [--turn 0] [--texture 1024]
#                           [--clip idle=idle.glb] [--clip walk=walk.glb#0] [--keep run=Gallop ...]
#                           [--limit Head=40 ...]
#
# 1. A new root node turns the model (--turn degrees about Y; Meshy rigs already face -Z like
#    Umbra), centers it and scales it so its largest half-extent is 1 (the same convention as slim_glb.py and the -1..1 sample cube).
# 2. All primitives become one mesh. A model without a texture gets a small palette texture made
#    from its material colours, and every vertex points at its material's cell.
# 3. Clips come from the model itself (--keep new=old) and from other GLBs with the same joint names
#    (--clip name=file.glb or file.glb#index); channels are matched by node name.
#    --limit Joint=degrees keeps a joint from turning further than that from its rest pose in any
#    clip (README.md "Joint limits"): library clips on an auto-made rig can twist a head or wrist.
# 4. The base-colour texture becomes a JPEG of at most --texture pixels.
import argparse, io, json, struct
import numpy as np
from PIL import Image

COMPONENT = {5120: np.int8, 5121: np.uint8, 5122: np.int16, 5123: np.uint16, 5125: np.uint32, 5126: np.float32}
WIDTH = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}

def read_glb(path):
    b = open(path, 'rb').read()
    n = struct.unpack('<I', b[12:16])[0]
    j = json.loads(b[20:20 + n])
    off = 20 + n
    bl = struct.unpack('<I', b[off:off + 4])[0]
    return j, b[off + 8:off + 8 + bl]

def accessor(j, binb, index):
    a = j['accessors'][index]
    bv = j['bufferViews'][a['bufferView']]
    dtype, width = COMPONENT[a['componentType']], WIDTH[a['type']]
    stride = bv.get('byteStride', 0) or np.dtype(dtype).itemsize * width
    start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    rows = [np.frombuffer(binb, dtype=dtype, count=width, offset=start + i * stride) for i in range(a['count'])] \
        if stride != np.dtype(dtype).itemsize * width else \
        [np.frombuffer(binb, dtype=dtype, count=a['count'] * width, offset=start)]
    out = np.concatenate(rows).reshape(a['count'], width).copy()
    if a.get('normalized') and dtype != np.float32:
        out = out.astype(np.float32) / np.iinfo(dtype).max
    return out

def trs(node):
    # Node -> 4x4 matrix (glTF: T * R * S, quaternion x, y, z, w).
    if 'matrix' in node:
        return np.array(node['matrix'], dtype=np.float64).reshape(4, 4).T
    t = node.get('translation', [0, 0, 0]); x, y, z, w = node.get('rotation', [0, 0, 0, 1]); s = node.get('scale', [1, 1, 1])
    r = np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                  [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                  [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])
    m = np.eye(4); m[:3, :3] = r * np.array(s); m[:3, 3] = t
    return m

def linear_to_srgb(c):
    c = min(max(c, 0.0), 1.0)
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055

class Writer:
    # Collects arrays into one binary buffer with a bufferView + accessor each.
    def __init__(self): self.bin, self.views, self.accessors = bytearray(), [], []
    def add(self, array, kind, target=None, minmax=False, normalized=False):
        while len(self.bin) % 4: self.bin += b'\0'
        data = np.ascontiguousarray(array)
        view = {'buffer': 0, 'byteOffset': len(self.bin), 'byteLength': data.nbytes}
        if target: view['target'] = target
        self.bin += data.tobytes()
        self.views.append(view)
        ctype = {np.dtype(np.float32): 5126, np.dtype(np.uint16): 5123, np.dtype(np.uint32): 5125, np.dtype(np.uint8): 5121}[data.dtype]
        acc = {'bufferView': len(self.views) - 1, 'componentType': ctype, 'count': len(data), 'type': kind}
        if normalized: acc['normalized'] = True
        if minmax:
            flat = data.reshape(len(data), -1)
            acc['min'] = flat.min(axis=0).astype(float).tolist(); acc['max'] = flat.max(axis=0).astype(float).tolist()
        self.accessors.append(acc)
        return len(self.accessors) - 1
    def add_image(self, jpeg):
        while len(self.bin) % 4: self.bin += b'\0'
        self.views.append({'buffer': 0, 'byteOffset': len(self.bin), 'byteLength': len(jpeg)})
        self.bin += jpeg
        return len(self.views) - 1

def main():
    p = argparse.ArgumentParser()
    p.add_argument('model'); p.add_argument('out')
    p.add_argument('--turn', type=float, default=0.0)
    p.add_argument('--texture', type=int, default=1024)
    p.add_argument('--clip', action='append', default=[])
    p.add_argument('--keep', action='append', default=[])
    p.add_argument('--limit', action='append', default=[])
    a = p.parse_args()
    j, binb = read_glb(a.model)

    # 1. The skinned mesh node and its global (bind) transform.
    mesh_node = next(i for i, n in enumerate(j['nodes']) if 'mesh' in n and 'skin' in n)
    parent = {c: i for i, n in enumerate(j['nodes']) for c in n.get('children', [])}
    def global_of(i):
        m = trs(j['nodes'][i])
        while i in parent: i = parent[i]; m = trs(j['nodes'][i]) @ m
        return m
    mesh = j['meshes'][j['nodes'][mesh_node]['mesh']]

    # 2. Merge the primitives; palette UVs for untextured materials.
    materials = j.get('materials', [])
    textured = any('baseColorTexture' in m.get('pbrMetallicRoughness', {}) for m in materials)
    pos, nrm, uv, jnt, wgt, idx, colours = [], [], [], [], [], [], []
    for prim in mesh['primitives']:
        at = prim['attributes']
        P = accessor(j, binb, at['POSITION']).astype(np.float32)
        base = sum(len(x) for x in pos)
        pos.append(P); nrm.append(accessor(j, binb, at['NORMAL']).astype(np.float32))
        jnt.append(accessor(j, binb, at['JOINTS_0']).astype(np.uint16)); wgt.append(accessor(j, binb, at['WEIGHTS_0']).astype(np.float32))
        if textured:
            uv.append(accessor(j, binb, at['TEXCOORD_0']).astype(np.float32))
        else:
            colour = materials[prim['material']].get('pbrMetallicRoughness', {}).get('baseColorFactor', [1, 1, 1, 1]) if 'material' in prim else [1, 1, 1, 1]
            colours.append(colour)
            uv.append(np.zeros((len(P), 2), np.float32) + len(colours) - 1)  # cell index, turned into UVs below
        idx.append(accessor(j, binb, prim['indices']).reshape(-1).astype(np.uint32) + base)
    pos, nrm, uv, jnt, wgt, idx = map(np.concatenate, (pos, nrm, uv, jnt, wgt, idx))
    wgt /= np.maximum(wgt.sum(axis=1, keepdims=True), 1e-8)

    # 3. Texture: the model's own (scaled down, JPEG) or a palette of material colours (4x4 px cells).
    if textured:
        tex = materials[0]['pbrMetallicRoughness']['baseColorTexture']['index']
        img = j['images'][j['textures'][tex]['source']]
        bv = j['bufferViews'][img['bufferView']]
        image = Image.open(io.BytesIO(binb[bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']])).convert('RGB')
        image.thumbnail((a.texture, a.texture))
    else:
        cells = len(colours); size = 4 * cells
        image = Image.new('RGB', (size, 4))
        for c, col in enumerate(colours):
            rgb = tuple(int(round(linear_to_srgb(v) * 255)) for v in col[:3])
            for x in range(4 * c, 4 * c + 4):
                for y in range(4): image.putpixel((x, y), rgb)
        uv[:, 0] = (uv[:, 0] * 4 + 2) / size; uv[:, 1] = 0.5
    jpeg = io.BytesIO(); image.save(jpeg, 'JPEG', quality=88)

    # 4. The new root: turn, center and scale to half-extent 1. Bounds of the rest pose as the
    #    engine will draw it: each vertex moved by its joints (global joint matrix * inverse bind).
    skin0 = j['skins'][j['nodes'][mesh_node]['skin']]
    ibm = accessor(j, binb, skin0['inverseBindMatrices']).reshape(-1, 4, 4).transpose(0, 2, 1)
    joint_mats = np.array([global_of(n) @ ibm[k] for k, n in enumerate(skin0['joints'])])
    homog = np.c_[pos, np.ones(len(pos))]
    world = np.zeros((len(pos), 4))
    for k in range(4):
        world += wgt[:, k:k + 1] * np.einsum('nij,nj->ni', joint_mats[jnt[:, k]], homog)
    world = world[:, :3]
    lo, hi = world.min(axis=0), world.max(axis=0)
    center, half = (lo + hi) / 2, (hi - lo).max() / 2
    turn = np.radians(a.turn)
    rot = np.array([[np.cos(turn), 0, np.sin(turn)], [0, 1, 0], [-np.sin(turn), 0, np.cos(turn)]])
    translation = -(rot @ center) / half
    quat = [0.0, float(np.sin(turn / 2)), 0.0, float(np.cos(turn / 2))]
    nodes = [dict(n) for n in j['nodes']]
    roots = j['scenes'][j.get('scene', 0)]['nodes']
    nodes.append({'name': 'UmbraRoot', 'translation': translation.tolist(), 'rotation': quat, 'scale': [1 / half] * 3, 'children': roots})
    root = len(nodes) - 1

    w = Writer()
    for n in nodes: n.pop('mesh', None)
    nodes[mesh_node]['mesh'] = 0
    skin = dict(j['skins'][j['nodes'][mesh_node]['skin']])
    skin['inverseBindMatrices'] = w.add(accessor(j, binb, skin['inverseBindMatrices']).astype(np.float32), 'MAT4')
    skin.pop('skeleton', None)
    nodes[mesh_node]['skin'] = 0
    prim = {'attributes': {'POSITION': w.add(pos, 'VEC3', 34962, True), 'NORMAL': w.add(nrm, 'VEC3', 34962),
                           'TEXCOORD_0': w.add(uv, 'VEC2', 34962), 'JOINTS_0': w.add(jnt, 'VEC4', 34962),
                           'WEIGHTS_0': w.add(wgt, 'VEC4', 34962)},
            'indices': w.add(idx, 'SCALAR', 34963), 'material': 0}
    image_view = w.add_image(jpeg.getvalue())

    # 5. Clips: channels matched to our nodes by name.
    names = {n.get('name'): i for i, n in enumerate(j['nodes'])}
    animations = []
    limits = {k: np.radians(float(v)) for k, v in (spec.split('=') for spec in a.limit)}
    def limited(q, rest, most):
        # Rotation away from rest: d = rest^-1 * q, by `angle` about `axis`. The angle is squeezed
        # smoothly under `most` (most * tanh(angle / most)): small turns stay, big ones flatten.
        out = q.copy()
        for i, (x, y, z, w_) in enumerate(q):
            rx, ry, rz, rw = -rest[0], -rest[1], -rest[2], rest[3]
            d = np.array([rw * x + rx * w_ + ry * z - rz * y, rw * y - rx * z + ry * w_ + rz * x,
                          rw * z + rx * y - ry * x + rz * w_, rw * w_ - rx * x - ry * y - rz * z])
            if d[3] < 0: d = -d
            sine = np.linalg.norm(d[:3])
            if sine < 1e-6: continue
            angle = 2 * np.arctan2(sine, d[3])
            half = most * np.tanh(angle / most) / 2
            dx, dy, dz = d[:3] / sine * np.sin(half); dw = np.cos(half)
            x0, y0, z0, w0 = rest
            out[i] = [w0 * dx + x0 * dw + y0 * dz - z0 * dy, w0 * dy - x0 * dz + y0 * dw + z0 * dx,
                      w0 * dz + x0 * dy - y0 * dx + z0 * dw, w0 * dw - x0 * dx - y0 * dy - z0 * dz]
        return out.astype(np.float32)
    def copy_clip(src_j, src_bin, anim, name):
        out = {'name': name, 'channels': [], 'samplers': []}
        for ch in anim['channels']:
            target = src_j['nodes'][ch['target']['node']].get('name')
            if target not in names: continue
            s = anim['samplers'][ch['sampler']]
            values = accessor(src_j, src_bin, s['output']).astype(np.float32)
            if ch['target']['path'] == 'rotation' and target in limits and s.get('interpolation', 'LINEAR') != 'CUBICSPLINE':
                values = limited(values, np.array(j['nodes'][names[target]].get('rotation', [0, 0, 0, 1]), dtype=np.float64), limits[target])
            out['samplers'].append({'input': w.add(accessor(src_j, src_bin, s['input']).astype(np.float32), 'SCALAR', None, True),
                                    'output': w.add(values, src_j['accessors'][s['output']]['type']),
                                    'interpolation': s.get('interpolation', 'LINEAR')})
            out['channels'].append({'sampler': len(out['samplers']) - 1, 'target': {'node': names[target], 'path': ch['target']['path']}})
        animations.append(out)
        print(f'  clip {name}: {len(out["channels"])} channels')
    for spec in a.keep:
        new, old = spec.split('=')
        copy_clip(j, binb, next(x for x in j['animations'] if x.get('name') == old), new)
    for spec in a.clip:
        new, src = spec.split('=')
        path, _, which = src.partition('#')
        sj, sb = read_glb(path)
        copy_clip(sj, sb, sj['animations'][int(which or 0)], new)

    out = {'asset': {'version': '2.0', 'generator': 'Umbra pack_animated.py'}, 'scene': 0, 'scenes': [{'nodes': [root]}],
           'nodes': nodes, 'meshes': [{'name': mesh.get('name', 'mesh'), 'primitives': [prim]}], 'skins': [skin],
           'materials': [{'name': 'Base', 'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0, 'roughnessFactor': 1}}],
           'textures': [{'source': 0, 'sampler': 0}], 'samplers': [{'magFilter': 9729, 'minFilter': 9987}],
           'images': [{'bufferView': image_view, 'mimeType': 'image/jpeg'}],
           'animations': animations, 'accessors': w.accessors, 'bufferViews': w.views, 'buffers': [{'byteLength': len(w.bin)}]}
    js = json.dumps(out, separators=(',', ':')).encode()
    js += b' ' * (-len(js) % 4)
    w.bin += b'\0' * (-len(w.bin) % 4)
    total = 12 + 8 + len(js) + 8 + len(w.bin)
    with open(a.out, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, total))
        f.write(struct.pack('<II', len(js), 0x4E4F534A)); f.write(js)
        f.write(struct.pack('<II', len(w.bin), 0x004E4942)); f.write(w.bin)
    print(f'wrote {a.out}: {len(pos)} vertices, {len(skin["joints"])} joints, {len(animations)} clips, {total // 1024} KB')

if __name__ == '__main__':
    main()

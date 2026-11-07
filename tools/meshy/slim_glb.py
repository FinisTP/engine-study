# Slims a Meshy GLB for Umbra: one mesh, base-color texture only (JPEG, 1024 px),
# turned 180 degrees (Meshy faces +Z, Umbra faces -Z), centered, largest half-extent = 1
# (the same convention as the -1..1 sample cube, so scene scales mean meters of half-size).
import io, json, struct, sys
import numpy as np
from PIL import Image

def read_glb(path):
    b = open(path, 'rb').read()
    l = struct.unpack('<I', b[12:16])[0]
    j = json.loads(b[20:20 + l])
    off = 20 + l
    bl = struct.unpack('<I', b[off:off + 4])[0]
    return j, b[off + 8:off + 8 + bl]

def accessor(j, binb, index, comps, dtype):
    a = j['accessors'][index]
    bv = j['bufferViews'][a['bufferView']]
    start = bv.get('byteOffset', 0) + a.get('byteOffset', 0)
    return np.frombuffer(binb, dtype=dtype, count=a['count'] * comps, offset=start).reshape(-1, comps).copy()

def main(src, dst, size=1024, quality=85):
    j, binb = read_glb(src)
    prim = j['meshes'][0]['primitives'][0]
    pos = accessor(j, binb, prim['attributes']['POSITION'], 3, np.float32)
    nrm = accessor(j, binb, prim['attributes']['NORMAL'], 3, np.float32)
    uv = accessor(j, binb, prim['attributes']['TEXCOORD_0'], 2, np.float32)
    ia = j['accessors'][prim['indices']]
    idt = {5121: np.uint8, 5123: np.uint16, 5125: np.uint32}[ia['componentType']]
    idx = accessor(j, binb, prim['indices'], 1, idt).reshape(-1).astype(np.uint32)

    # 1. Turn around Y (x, z -> -x, -z), center the box, scale the largest half-extent to 1.
    pos[:, [0, 2]] *= -1
    nrm[:, [0, 2]] *= -1
    center = (pos.min(0) + pos.max(0)) / 2
    pos -= center
    pos /= np.abs(pos).max()

    # 2. Base color only, as a smaller JPEG.
    tex = j['materials'][prim.get('material', 0)]['pbrMetallicRoughness']['baseColorTexture']['index']
    img = j['images'][j['textures'][tex]['source']]
    bv = j['bufferViews'][img['bufferView']]
    raw = binb[bv.get('byteOffset', 0):bv.get('byteOffset', 0) + bv['byteLength']]
    picture = Image.open(io.BytesIO(raw)).convert('RGB').resize((size, size), Image.LANCZOS)
    jpeg = io.BytesIO()
    picture.save(jpeg, 'JPEG', quality=quality)
    jpeg = jpeg.getvalue()

    # 3. New binary buffer: positions, normals, uvs, indices, image (4-byte aligned).
    chunks, views = [], []
    def add(data, target=None):
        offset = sum(len(c) for c in chunks)
        chunks.append(data + b'\0' * (-len(data) % 4))
        view = {'buffer': 0, 'byteOffset': offset, 'byteLength': len(data)}
        if target:
            view['target'] = target
        views.append(view)
        return len(views) - 1
    vp = add(pos.astype(np.float32).tobytes(), 34962)
    vn = add(nrm.astype(np.float32).tobytes(), 34962)
    vt = add(uv.astype(np.float32).tobytes(), 34962)
    vi = add(idx.astype(np.uint32).tobytes(), 34963)
    vimg = add(jpeg)
    n = len(pos)
    gltf = {
        'asset': {'version': '2.0', 'generator': 'Meshy + Umbra slim_glb.py'},
        'scene': 0, 'scenes': [{'nodes': [0]}], 'nodes': [{'mesh': 0, 'name': 'model'}],
        'meshes': [{'primitives': [{'attributes': {'POSITION': 0, 'NORMAL': 1, 'TEXCOORD_0': 2}, 'indices': 3, 'material': 0}]}],
        'accessors': [
            {'bufferView': vp, 'componentType': 5126, 'count': n, 'type': 'VEC3', 'min': pos.min(0).tolist(), 'max': pos.max(0).tolist()},
            {'bufferView': vn, 'componentType': 5126, 'count': n, 'type': 'VEC3'},
            {'bufferView': vt, 'componentType': 5126, 'count': n, 'type': 'VEC2'},
            {'bufferView': vi, 'componentType': 5125, 'count': len(idx), 'type': 'SCALAR'},
        ],
        'bufferViews': views,
        'buffers': [{'byteLength': sum(len(c) for c in chunks)}],
        'images': [{'bufferView': vimg, 'mimeType': 'image/jpeg'}],
        'samplers': [{'magFilter': 9729, 'minFilter': 9729}],
        'textures': [{'source': 0, 'sampler': 0}],
        'materials': [{'pbrMetallicRoughness': {'baseColorTexture': {'index': 0}, 'metallicFactor': 0.0, 'roughnessFactor': 1.0}}],
    }
    js = json.dumps(gltf, separators=(',', ':')).encode()
    js += b' ' * (-len(js) % 4)
    bin_ = b''.join(chunks)
    out = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(bin_))
    out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(bin_), 0x004E4942) + bin_
    open(dst, 'wb').write(out)
    print(dst, len(out), 'bytes,', len(idx) // 3, 'triangles, half-extents', (np.abs(pos).max(0)).round(3).tolist())

main(sys.argv[1], sys.argv[2])

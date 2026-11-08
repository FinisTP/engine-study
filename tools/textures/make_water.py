# Writes textures/water.png: a tileable 256x256 dark-blue water texture (sums of whole-period waves).
# usage: python make_water.py <out folder>
import sys, os, math, zlib, struct

def png(path, w, h, rows):
    raw = b''.join(b'\x00' + bytes(r) for r in rows)
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

n, rows = 256, []
for y in range(n):
    row = []
    for x in range(n):
        u, v = 2 * math.pi * x / n, 2 * math.pi * y / n
        w = (math.sin(3 * u + math.sin(2 * v)) + math.sin(5 * v + math.cos(3 * u)) + 0.5 * math.sin(7 * (u + v))) / 2.5  # -1..1
        row += [int(20 + 12 * w), int(55 + 20 * w), int(85 + 25 * w)]
    rows.append(row)
out = sys.argv[1] if len(sys.argv) > 1 else '.'
os.makedirs(out, exist_ok=True)
png(os.path.join(out, 'water.png'), n, n, rows)
print('wrote', os.path.join(out, 'water.png'))

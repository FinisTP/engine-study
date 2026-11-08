# Writes the example color tables (LUT images, 256x16; layout in engine/core/include/Post.md).
# usage: python make_lut.py <out folder>
import sys, zlib, struct, os

def srgb_to_linear(c): return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
def linear_to_srgb(c):
    c = min(max(c, 0.0), 1.0)
    return c * 12.92 if c <= 0.0031308 else 1.055 * c ** (1 / 2.4) - 0.055

def png(path, w, h, rows):
    raw = b''.join(b'\x00' + bytes(r) for r in rows)
    chunk = lambda t, d: struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                           + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))

def lut(path, grade):
    rows = []
    for g in range(16):
        row = []
        for b in range(16):
            for r in range(16):
                rgb = grade([srgb_to_linear(v / 15) for v in (r, g, b)])
                row += [round(linear_to_srgb(v) * 255) for v in rgb]
        rows.append(row)
    png(path, 256, 16, rows)

# 1. Moonlit dungeon: shadows pushed to teal, highlights kept, a little less color.
def moonlight(c):
    luma = 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]
    c = [luma + (v - luma) * 0.75 for v in c]
    shadow = max(0.0, 1.0 - luma * 4)          # 1 in the darks, 0 above 25% gray
    return [c[0] * (1 - 0.15 * shadow), c[1] * (1 + 0.04 * shadow), c[2] * (1 + 0.2 * shadow)]

out = sys.argv[1] if len(sys.argv) > 1 else '.'
os.makedirs(out, exist_ok=True)
lut(os.path.join(out, 'lut_moonlight.png'), moonlight)
print('wrote', os.path.join(out, 'lut_moonlight.png'))

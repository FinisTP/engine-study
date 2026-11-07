"""Draws assets/textures/sample_bricks.png: a 256x256 brick wall that repeats seamlessly.
Hand-made sample for P8C.1 (world-scale texturing); no AI involved. Only the Python standard library."""
import random, struct, sys, zlib

SIZE, ROWS, COLS, MORTAR = 256, 8, 4, 3  # 8 rows of 4 bricks; mortar lines 3 px wide
random.seed(7)
shade = {(r, c): random.uniform(0.82, 1.08) for r in range(ROWS) for c in range(COLS)}

def pixel(x, y):
    row_h, brick_w = SIZE // ROWS, SIZE // COLS
    row = y // row_h
    shifted = (x + (brick_w // 2 if row % 2 else 0)) % SIZE  # every other row offset by half a brick
    if y % row_h < MORTAR or shifted % brick_w < MORTAR:
        g = 168 + random.randint(-8, 8)
        return (g, g - 6, g - 14)
    s = shade[(row, shifted // brick_w)] * random.uniform(0.93, 1.07)
    return tuple(max(0, min(255, int(v * s))) for v in (150, 72, 52))

rows = b"".join(b"\0" + bytes(c for x in range(SIZE) for c in (*pixel(x, y), 255)) for y in range(SIZE))
def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
open(sys.argv[1] if len(sys.argv) > 1 else "sample_bricks.png", "wb").write(png)

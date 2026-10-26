#!/usr/bin/env python3
"""Write the launcher icon bitmaps (no image library needed) in the header layout PilRC 3.2 reads on 64-bit hosts.

A fuel pump: red body with a white window and a dark hose on the right, drawn as
pixel art at 22x22 (plus a 44x44 double-density copy) and 15x9 for the list view.
Magenta (255,0,255) is the transparent colour PilRC is told about.
"""
import struct, os, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "rsc/icons"

T = (255, 0, 255)     # transparent
K = (0, 0, 0)         # outline
W = (255, 255, 255)   # window
R = (204, 0, 0)       # pump body
D = (153, 0, 0)       # body shade
G = (102, 102, 102)   # hose and base
Y = (255, 204, 0)     # drop

PALETTE = [T, K, W, R, D, G, Y]
KEY = {'.': T, '#': K, 'w': W, 'r': R, 'd': D, 'g': G, 'y': Y}

LARGE = [
"......................",
"...#########..........",
"..#rrrrrrrrrrr#.......",
"..#rrwwwwwwwrr#.......",
"..#rrw#####wrr#...##..",
"..#rrw#####wrr#..#gg#.",
"..#rrw#####wrr#..#g##.",
"..#rrwwwwwwwrr#..#g#..",
"..#rrrrrrrrrrr#..#g#..",
"..#rrrrrrrrrrr#..#g#..",
"..#rrdrrrrrrdr#..#g#..",
"..#rrdrrrrrrdr#..#g#..",
"..#rrdrrrrrrdr####g#..",
"..#rrdrrrrrrdr#gggg#..",
"..#rrdrrrrrrdr######..",
"..#rrddddddddr#.......",
"..#rrrrrrrrrrr#.......",
"..#rrrrrrrrrrr#.......",
".###############......",
".#ggggggggggggg#......",
".#ggggggggggggg#......",
".###############......",
]

SMALL = [
"..#######......",
".#rwwwwwr#.##..",
".#rw###wr#.#g#.",
".#rwwwwwr###g#.",
".#rrrrrrr#gg#..",
".#rddddrr####..",
".#rrrrrrr#.....",
".#########.....",
"...............",
]

def scale2(rows):
    out = []
    for r in rows:
        rr = "".join(c * 2 for c in r)
        out += [rr, rr]
    return out

def mono(rows):
    """1-bit version: anything but transparent and white is ink."""
    return ["".join('.' if c in '.w' else '#' for c in r) for r in rows]

def info_header(w, h, bits, image_bytes, colours):
    """PilRC 3.2 declares biWidth/biHeight/biXPelsPerMeter/biYPelsPerMeter as C `long`, which is
    8 bytes on every 64-bit host, so it reads a BMP info header with this padded layout (64 bytes):
      0 biSize  8 biWidth(8)  16 biHeight(8)  24 planes  26 bitCount  28 compression  32 sizeImage
      40 xppm(8)  48 yppm(8)  56 clrUsed  60 clrImportant
    A standard 40-byte header makes it read bitCount = 0 and refuse the file, so these bitmaps are
    written for PilRC only; ordinary image viewers will not open them."""
    hdr = bytearray(64)
    struct.pack_into("<I", hdr, 0, 64)
    struct.pack_into("<qq", hdr, 8, w, h)
    struct.pack_into("<HHII", hdr, 24, 1, bits, 0, image_bytes)
    struct.pack_into("<qq", hdr, 40, 2835, 2835)
    struct.pack_into("<II", hdr, 56, colours, 0)
    return bytes(hdr)

def write_bmp(path, w, h, bits, palette, pix):
    off = 14 + 64 + len(palette) * 4
    palb = b"".join(struct.pack("<BBBB", b, g, r, 0) for (r, g, b) in palette)
    hdr = b"BM" + struct.pack("<IHHI", off + len(pix), 0, 0, off)
    open(path, "wb").write(hdr + info_header(w, h, bits, len(pix), len(palette)) + palb + pix)

def bmp8(path, rows):
    h, w = len(rows), len(rows[0])
    stride = (w + 3) & ~3
    pix = b""
    for r in reversed(rows):
        pix += bytes(PALETTE.index(KEY[c]) for c in r) + b"\0" * (stride - w)
    write_bmp(path, w, h, 8, PALETTE + [(0, 0, 0)] * (256 - len(PALETTE)), pix)

def bmp1(path, rows):
    """1 bit per pixel, index 0 = black ink, 1 = white (left over as the background)."""
    h, w = len(rows), len(rows[0])
    stride = ((w + 31) // 32) * 4
    pix = b""
    for r in reversed(rows):
        line = bytearray(stride)
        for i, c in enumerate(r):
            if c == '.':
                line[i // 8] |= 0x80 >> (i % 8)
        pix += bytes(line)
    write_bmp(path, w, h, 1, [(0, 0, 0), (255, 255, 255)], pix)

os.makedirs(OUT, exist_ok=True)
assert all(len(r) == 22 for r in LARGE) and len(LARGE) == 22
assert all(len(r) == 15 for r in SMALL) and len(SMALL) == 9
bmp1(os.path.join(OUT, "icon22x22x1.bmp"), mono(LARGE))
bmp8(os.path.join(OUT, "icon22x22x8.bmp"), LARGE)
bmp8(os.path.join(OUT, "icon44x44x8.bmp"), scale2(LARGE))
bmp1(os.path.join(OUT, "icon15x9x1.bmp"), mono(SMALL))
bmp8(os.path.join(OUT, "icon15x9x8.bmp"), SMALL)
bmp8(os.path.join(OUT, "icon30x18x8.bmp"), scale2(SMALL))
print("icons written to", OUT)

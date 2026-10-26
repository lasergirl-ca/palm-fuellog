#!/usr/bin/env python3
"""Write the launcher icon bitmaps (no image library needed) in the header layout PilRC 3.2 reads on 64-bit hosts.

A red fuel pump on the Palm launcher's blue oval, lit from the top left with a black
outline, drawn from shapes so the 22x22 and the 44x44 double-density bitmaps (and the
15x9 / 30x18 list icons) come from one description. Magenta (255,0,255) is the
transparent colour PilRC is told about.
"""
import struct, os, sys

OUT = sys.argv[1] if len(sys.argv) > 1 else "rsc/icons"

T = (255, 0, 255)     # transparent
K = (0, 0, 0)         # outline
W = (255, 255, 255)   # highlight / window
R = (204, 0, 0)       # pump body
RL = (255, 102, 102)  # body highlight
RD = (153, 0, 0)      # body shade
G = (102, 102, 102)   # base, hose
GL = (153, 153, 153)  # base highlight
GD = (51, 51, 51)     # hose shade
B = (51, 102, 204)    # the Palm blue oval
BL = (102, 153, 255)  # its lighter rim
LB = (204, 255, 255)  # window glass

PALETTE = [T, K, W, R, RL, RD, G, GL, GD, B, BL, LB]


class Canvas:
    """A tiny rasteriser: the icon is drawn from shapes in 22x22 units at scale s."""
    def __init__(self, w, h, s):
        self.w, self.h, self.s = w * s, h * s, s
        self.px = [[T] * self.w for _ in range(self.h)]

    def put(self, x, y, c):
        if 0 <= x < self.w and 0 <= y < self.h: self.px[y][x] = c

    def rect(self, x0, y0, x1, y1, c):           # inclusive, in 22-unit coords
        s = self.s
        for y in range(int(y0 * s), int((y1 + 1) * s)):
            for x in range(int(x0 * s), int((x1 + 1) * s)):
                self.put(x, y, c)

    def ellipse(self, cx, cy, rx, ry, c):
        s = self.s
        for y in range(self.h):
            for x in range(self.w):
                dx, dy = (x + 0.5) / s - cx, (y + 0.5) / s - cy
                if (dx / rx) ** 2 + (dy / ry) ** 2 <= 1: self.px[y][x] = c

    def outline(self, inside, c):
        """Draw c on every transparent pixel touching a pixel for which inside() is true."""
        new = [row[:] for row in self.px]
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x] != T or not any(0 <= x + dx < self.w and 0 <= y + dy < self.h and inside(self.px[y + dy][x + dx])
                                                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))): continue
                new[y][x] = c
        self.px = new

    def rows(self):
        inv = {v: k for k, v in KEY.items()}
        return ["".join(inv[c] for c in row) for row in self.px]


KEY = {'.': T, '#': K, 'w': W, 'r': R, 'l': RL, 'd': RD, 'g': G, 'h': GL, 'k': GD, 'b': B, 'o': BL, 'c': LB}


def draw_large(s):
    """The launcher icon: a red pump on the Palm blue oval, lit from the top left."""
    cv = Canvas(22, 22, s)
    cv.ellipse(11, 17.5, 10.5, 4.3, B)                 # the pedestal
    cv.ellipse(11, 17.0, 9.0, 3.0, BL)
    cv.ellipse(11, 17.3, 7.5, 2.4, B)
    # base
    cv.rect(3, 16, 14, 18, G); cv.rect(3, 16, 14, 16, GL)
    # body with highlight and shade
    cv.rect(4, 3, 13, 15, R); cv.rect(5, 3, 6, 15, RL); cv.rect(12, 4, 13, 15, RD); cv.rect(4, 15, 13, 15, RD)
    # window
    cv.rect(6, 5, 11, 8, LB); cv.rect(6, 5, 11, 5, W); cv.rect(6, 5, 6, 8, W)
    # door line
    cv.rect(8, 11, 10, 11, RD)
    # hose: up the right side, over the top, to a nozzle
    cv.rect(14, 10, 15, 10, G); cv.rect(16, 5, 16, 10, G); cv.rect(16, 4, 18, 4, G); cv.rect(18, 5, 18, 7, GD); cv.rect(17, 7, 19, 8, GD)
    for x in (14, 15): cv.rect(x, 11, x, 11, GD)
    # outline everything but the oval
    cv.outline(lambda c: c not in (T, B, BL), K)
    return cv.rows()


def draw_small(s):
    """The list icon, 15x9: the pump alone."""
    cv = Canvas(15, 9, s)
    cv.rect(2, 7, 9, 8, G); cv.rect(2, 7, 9, 7, GL)
    cv.rect(3, 1, 8, 6, R); cv.rect(3, 1, 3, 6, RL); cv.rect(8, 1, 8, 6, RD)
    cv.rect(4, 2, 7, 3, LB); cv.rect(4, 2, 7, 2, W)
    cv.rect(9, 4, 10, 4, G); cv.rect(11, 1, 11, 4, G); cv.rect(11, 1, 12, 1, G); cv.rect(12, 2, 12, 3, GD)
    cv.outline(lambda c: c != T, K)
    return cv.rows()


LARGE = draw_large(1)
LARGE2 = draw_large(2)
SMALL = draw_small(1)
SMALL2 = draw_small(2)

def mono(rows):
    """1-bit version: the outline and dark colours are ink; the oval and light colours drop out."""
    return ["".join('#' if c in '#rdgkh' else '.' for c in r) for r in rows]


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
bmp8(os.path.join(OUT, "icon44x44x8.bmp"), LARGE2)
bmp1(os.path.join(OUT, "icon15x9x1.bmp"), mono(SMALL))
bmp8(os.path.join(OUT, "icon15x9x8.bmp"), SMALL)
bmp8(os.path.join(OUT, "icon30x18x8.bmp"), SMALL2)
print("icons written to", OUT)

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

    def outline(self, inside, c, over=(T,)):
        """Draw c on every pixel of a colour in `over` that touches a pixel for which inside() is true."""
        new = [row[:] for row in self.px]
        for y in range(self.h):
            for x in range(self.w):
                if self.px[y][x] not in over or not any(0 <= x + dx < self.w and 0 <= y + dy < self.h and inside(self.px[y + dy][x + dx])
                                                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))): continue
                new[y][x] = c
        self.px = new

    def rows(self):
        inv = {v: k for k, v in KEY.items()}
        return ["".join(inv[c] for c in row) for row in self.px]


KEY = {'.': T, '#': K, 'w': W, 'r': R, 'l': RL, 'd': RD, 'g': G, 'h': GL, 'k': GD, 'b': B, 'o': BL, 'c': LB}


import math

ANGLE = 16   # degrees: the stock launcher icons are turned about this much (top edge rising to the right)


def pump_scene(hi):
    """The pump drawn flat on a fine grid (hi pixels per 22-unit), as a box seen from the
    front-left: front face, a darker right side, a top, the base and the hose."""
    cv = Canvas(22, 22, hi)
    # base: front and right side
    cv.rect(3, 16, 13, 18, G); cv.rect(3, 16, 13, 16, GL)
    for i in range(hi * 2):                        # side of the base, one unit deep, slanting up-right
        x = 14 + i / hi
        cv.rect(x, 16 - (i + 1) / hi, x + 1 / hi - 1e-6, 18 - (i + 1) / hi, GD)
    # body front
    cv.rect(4, 3, 12, 15, R); cv.rect(4, 3, 5, 15, RL); cv.rect(11, 4, 12, 15, RD)
    # body right side and top (oblique, two units deep)
    for i in range(hi * 2):
        x = 13 + i / hi
        cv.rect(x, 3 - (i + 1) / hi, x + 1 / hi - 1e-6, 15 - (i + 1) / hi, RD)
        cv.rect(4 + (i + 1) / hi, 3 - (i + 1) / hi, 12 + (i + 1) / hi, 3 - i / hi - 1e-6, RL)
    # window and door line on the front
    cv.rect(5.5, 5, 10.5, 8, LB); cv.rect(5.5, 5, 10.5, 5.5, W); cv.rect(5.5, 5, 6, 8, W)
    cv.rect(7, 11, 9.5, 11.5, RD)
    # hose: out of the side, up, and a nozzle hanging to the right
    cv.rect(15, 9, 16.5, 9.8, G); cv.rect(16.5, 4, 17.3, 9.8, G); cv.rect(16.5, 3.2, 19.2, 4, G)
    cv.rect(18.4, 4, 19.2, 6.5, GD); cv.rect(17.8, 6.5, 20, 7.5, GD)
    return cv


def rotated(scene, s, angle, cx, cy):
    """Sample the fine scene into a 22*s grid, turned `angle` degrees about (cx, cy),
    taking the most common colour of a 4x4 sample grid so edges stay hard."""
    cv = Canvas(22, 22, s)
    hi = scene.s
    th = math.radians(angle)
    c, sn = math.cos(th), math.sin(th)
    for v in range(cv.h):
        for u in range(cv.w):
            votes = {}
            for j in range(4):
                for i in range(4):
                    ux, vy = (u + (i + 0.5) / 4) / s, (v + (j + 0.5) / 4) / s
                    x = cx + (ux - cx) * c - (vy - cy) * sn
                    y = cy + (ux - cx) * sn + (vy - cy) * c
                    xi, yi = int(x * hi), int(y * hi)
                    col = scene.px[yi][xi] if 0 <= xi < scene.w and 0 <= yi < scene.h else T
                    votes[col] = votes.get(col, 0) + 1
            best = max(votes.items(), key=lambda kv: kv[1])[0]
            if best != T: cv.px[v][u] = best
    return cv


def draw_large(s):
    """The launcher icon: a red pump on the Palm blue oval, turned like the stock icons."""
    cv = Canvas(22, 22, s)
    cv.ellipse(11, 17.5, 10.5, 4.3, B)                 # the pedestal
    cv.ellipse(11, 17.0, 9.0, 3.0, BL)
    cv.ellipse(11, 17.3, 7.5, 2.4, B)
    pump = rotated(pump_scene(8), s, -ANGLE, 9.5, 11)
    for y in range(cv.h):
        for x in range(cv.w):
            if pump.px[y][x] != T: cv.px[y][x] = pump.px[y][x]
    cv.outline(lambda col: col not in (T, B, BL), K, over=(T, B, BL))
    return cv.rows()


def draw_small(s):
    """The list icon, 15x9: the pump alone, upright (too small to lean)."""
    cv = Canvas(15, 9, s)
    cv.rect(2, 7, 9, 8, G); cv.rect(2, 7, 9, 7, GL)
    cv.rect(3, 1, 8, 6, R); cv.rect(3, 1, 3, 6, RL); cv.rect(8, 1, 8, 6, RD)
    cv.rect(4, 2, 7, 3, LB); cv.rect(4, 2, 7, 2, W)
    cv.rect(9, 4, 10, 4, G); cv.rect(11, 1, 11, 4, G); cv.rect(11, 1, 12, 1, G); cv.rect(12, 2, 12, 3, GD)
    cv.outline(lambda col: col != T, K)
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

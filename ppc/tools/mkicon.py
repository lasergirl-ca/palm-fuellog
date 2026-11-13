#!/usr/bin/env python3
"""Builds FuelLog.ico for the Pocket PC from the Palm's 44x44 8-bit icon
(rsc/icons/icon44x44x8.bmp): 16x16, 32x32 and 44x44 images, each 8-bit with
the Palm's palette and a transparency mask where the Palm art is magenta
(255, 0, 255). Smaller sizes average the opaque source pixels each covers.
Standard library only.

Usage: mkicon.py icon44x44x8.bmp FuelLog.ico
"""
import struct
import sys

MAGENTA = (255, 0, 255)


def read_bmp(path):
    d = open(path, "rb").read()
    assert d[:2] == b"BM", "not a BMP"
    off = struct.unpack("<I", d[10:14])[0]
    hsize = struct.unpack("<I", d[14:18])[0]
    w, h, _planes, bpp = struct.unpack("<iiHH", d[18:30])
    if w == 0 and hsize == 64:
        # the Palm icons' header: each 32-bit field followed by four zero bytes
        w, h = struct.unpack("<ii", d[22:26] + d[30:34])
        _planes, bpp = struct.unpack("<HH", d[38:42])
    assert bpp == 8, "an 8-bit BMP is expected"
    pal_at = 14 + hsize
    ncol = (off - pal_at) // 4
    pal = [tuple(d[pal_at + 4 * i + k] for k in (2, 1, 0)) for i in range(ncol)]
    stride = (w + 3) & ~3
    rows = []
    for y in range(abs(h)):
        src = (abs(h) - 1 - y) if h > 0 else y          # bottom-up unless the height is negative
        rows.append(list(d[off + src * stride: off + src * stride + w]))
    return w, abs(h), pal, rows


def resample(w, h, pal, rows, size):
    """size x size RGB pixels and a transparent flag each, averaging what each covers."""
    out = []
    for y in range(size):
        line = []
        y0, y1 = y * h / size, (y + 1) * h / size
        for x in range(size):
            x0, x1 = x * w / size, (x + 1) * w / size
            r = g = b = cov = tot = 0.0
            sy = int(y0)
            while sy < y1 and sy < h:
                fy = min(y1, sy + 1) - max(y0, sy)
                sx = int(x0)
                while sx < x1 and sx < w:
                    fx = min(x1, sx + 1) - max(x0, sx)
                    a = fx * fy
                    tot += a
                    c = pal[rows[sy][sx]]
                    if c != MAGENTA:
                        r += c[0] * a; g += c[1] * a; b += c[2] * a; cov += a
                    sx += 1
                sy += 1
            if cov < tot / 2:
                line.append(None)
            else:
                line.append((round(r / cov), round(g / cov), round(b / cov)))
        out.append(line)
    return out


def nearest(pal, c):
    best, bi = None, 0
    for i, p in enumerate(pal):
        if p == MAGENTA:
            continue
        dd = (p[0] - c[0]) ** 2 + (p[1] - c[1]) ** 2 + (p[2] - c[2]) ** 2
        if best is None or dd < best:
            best, bi = dd, i
    return bi


def ico_image(pal, px):
    size = len(px)
    pal = (pal + [(0, 0, 0)] * 256)[:256]
    hdr = struct.pack("<IiiHHIIiiII", 40, size, size * 2, 1, 8, 0, 0, 0, 0, 256, 0)
    palette = b"".join(struct.pack("BBBB", c[2], c[1], c[0], 0) for c in pal)
    stride = (size + 3) & ~3
    mstride = ((size + 31) // 32) * 4
    xor = bytearray()
    andm = bytearray()
    cache = {}
    for y in range(size - 1, -1, -1):                 # bottom-up
        row = bytearray(stride)
        mrow = bytearray(mstride)
        for x in range(size):
            c = px[y][x]
            if c is None:
                mrow[x // 8] |= 0x80 >> (x % 8)
                row[x] = 0
            else:
                if c not in cache:
                    cache[c] = nearest(pal, c)
                row[x] = cache[c]
        xor += row
        andm += mrow
    return hdr + palette + bytes(xor) + bytes(andm)


def main(argv):
    w, h, pal, rows = read_bmp(argv[1])
    sizes = [16, 32, 44]
    images = [ico_image(pal, resample(w, h, pal, rows, s)) for s in sizes]
    out = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for s, img in zip(sizes, images):
        out += struct.pack("<BBBBHHII", s, s, 0, 0, 1, 8, len(img), offset)
        offset += len(img)
    out += b"".join(images)
    open(argv[2], "wb").write(out)
    print(f"wrote {argv[2]}: {', '.join(f'{s}x{s}' for s in sizes)}, {len(out)} bytes")


if __name__ == "__main__":
    main(sys.argv)

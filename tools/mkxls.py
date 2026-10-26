#!/usr/bin/env python3
"""Builds BIFF8 .xls fixtures for tests/test_xlsread.c with nothing but struct.

Two workbooks, each with a Vehicles sheet first and the Fills sheet second, the
shared strings split across a CONTINUE record, and every cell record kind the
reader understands. The small one lands in the compound file's mini stream;
the padded one uses regular sectors and a multi-sector chain.

usage: mkxls.py out-mini.xls out-fat.xls
"""
import struct, sys

ENDOFCHAIN = 0xFFFFFFFE
FREESECT = 0xFFFFFFFF
FATSECT = 0xFFFFFFFD


def rec(t, body):
    return struct.pack('<HH', t, len(body)) + body


def uni(s, short=False):
    """A BIFF8 unicode string: compressed when it fits Latin-1."""
    try:
        b = s.encode('latin-1'); flags = 0
    except UnicodeEncodeError:
        b = s.encode('utf-16-le'); flags = 1
    head = struct.pack('<B', len(s)) if short else struct.pack('<H', len(s))
    return head + bytes([flags]) + b


def bof(dt): return rec(0x0809, struct.pack('<HHHHII', 0x0600, dt, 0x0DBB, 0x07CC, 0, 0))
def eof(): return rec(0x000A, b'')
def labelsst(r, c, i): return rec(0x00FD, struct.pack('<HHHI', r, c, 0, i))
def number(r, c, v): return rec(0x0203, struct.pack('<HHHd', r, c, 0, v))
def rk_int(r, c, v, x100=False): return rec(0x027E, struct.pack('<HHHI', r, c, 0, ((v << 2) & 0xFFFFFFFF) | 2 | (1 if x100 else 0)))
def rk_dbl(r, c, v, x100=False):
    hi = struct.unpack('<II', struct.pack('<d', v))[1]
    assert struct.unpack('<d', struct.pack('<II', 0, hi & ~3))[0] == v, "not a 30-bit double"
    return rec(0x027E, struct.pack('<HHHI', r, c, 0, (hi & ~3) | (1 if x100 else 0)))
def mulrk(r, c0, vals):
    body = struct.pack('<HH', r, c0) + b''.join(struct.pack('<HI', 0, ((v << 2) & 0xFFFFFFFF) | 2) for v in vals)
    return rec(0x00BD, body + struct.pack('<H', c0 + len(vals) - 1))
def label(r, c, s): return rec(0x0204, struct.pack('<HHH', r, c, 0) + uni(s))
def boolerr(r, c, v): return rec(0x0205, struct.pack('<HHHBB', r, c, 0, 1 if v else 0, 0))
def blank(r, c): return rec(0x0201, struct.pack('<HHH', r, c, 0))
def formula_num(r, c, v): return rec(0x0006, struct.pack('<HHHd', r, c, 0, v) + struct.pack('<HIH', 0, 0, 0))
def formula_str(r, c, s):
    f = rec(0x0006, struct.pack('<HHH', r, c, 0) + bytes([0, 0, 0, 0, 0, 0, 0xFF, 0xFF]) + struct.pack('<HIH', 0, 0, 0))
    return f + rec(0x0207, uni(s))


def sst(strings, split=None):
    """SST; split=(index, chars) breaks that string into a CONTINUE after so many characters."""
    out1 = struct.pack('<II', len(strings), len(strings)); out2 = None
    for k, s in enumerate(strings):
        u = uni(s); hdr, chars, flags = u[:3], u[3:], u[2]
        if split and split[0] == k:
            per = 2 if flags & 1 else 1
            out1 += hdr + chars[:split[1] * per]
            out2 = bytes([flags]) + chars[split[1] * per:]
        elif out2 is None:
            out1 += u
        else:
            out2 += u
    data = rec(0x00FC, out1)
    if out2 is not None:
        data += rec(0x003C, out2)
    return data


LONG_NOTE = "Filled up before the long drive north; " * 4 + "end"   # 159 chars, split mid-way
STRINGS = ["Equinox", LONG_NOTE, "Zoë’s car", "Bolt", "Equinox 2019"]


def workbook(padRows):
    fills = bof(0x0010)
    header = ["Date", "Time", "Vehicle", "Odometer (km)", "Volume (L)", "Price (per L)", "Total", "Currency",
              "Payment", "Full", "Notes", "A", "B", "C", "Neg", "Tiny", "Big", "Half"]
    for c, h in enumerate(header):
        fills += label(0, c, h)
    r = 1
    fills += number(r, 0, 45000.0) + number(r, 1, 0.5875) + labelsst(r, 2, 0) + rk_int(r, 3, 123456)
    fills += number(r, 4, 45.123) + rk_int(r, 5, 159, x100=True) + rk_dbl(r, 6, 71.75) + label(r, 7, "CAD")
    fills += formula_str(r, 8, "Card") + boolerr(r, 9, True) + labelsst(r, 10, 1) + mulrk(r, 11, [1, 2, 3])
    fills += rk_int(r, 14, -5) + number(r, 15, 0.1) + number(r, 16, 1e10) + number(r, 17, 2.675)
    fills += blank(2, 0)                        # a row with nothing in it is not returned
    r = 3
    fills += rk_int(r, 0, 45001) + labelsst(r, 2, 2) + formula_num(r, 4, 7.5) + rk_dbl(r, 5, -1.5, x100=True)
    fills += boolerr(r, 9, False) + number(r, 1, 1.25)   # out-of-order cell: ignored, the row keeps its shape
    for i in range(padRows):
        fills += rk_int(10 + i, 0, 45002 + i) + rk_int(10 + i, 3, 130000 + i * 10)
    fills += eof()
    vehicles = bof(0x0010) + labelsst(0, 0, 3) + labelsst(1, 0, 4) + eof()

    def globals_(posV, posF):
        g = bof(0x0005) + rec(0x0022, struct.pack('<H', 0))
        g += rec(0x0085, struct.pack('<IBB', posV, 0, 0) + uni("Vehicles", short=True))
        g += rec(0x0085, struct.pack('<IBB', posF, 0, 0) + uni("Fills", short=True))
        g += sst(STRINGS, split=(1, 70)) + eof()
        return g
    glen = len(globals_(0, 0))
    return globals_(glen, glen + len(vehicles)) + vehicles + fills


def ole(stream):
    """A compound file holding one stream named Workbook."""
    SEC, MINI = 512, 64
    mini = len(stream) < 4096
    sectors = []     # payloads, sid order
    fat = []

    def add(data):
        sectors.append(data.ljust(SEC, b'\0')); fat.append(FREESECT); return len(sectors) - 1

    def chain(data):
        sids = [add(data[i:i + SEC]) for i in range(0, max(len(data), 1), SEC)]
        for a, b in zip(sids, sids[1:]): fat[a] = b
        fat[sids[-1]] = ENDOFCHAIN
        return sids[0]
    fatSid = add(b'')                     # patched below
    dirSid = add(b'')
    if mini:
        container = stream + b'\0' * (-len(stream) % MINI)
        nMini = len(container) // MINI
        miniFat = b''.join(struct.pack('<I', i + 1 if i + 1 < nMini else ENDOFCHAIN) for i in range(nMini))
        miniFatSid = chain(miniFat.ljust(SEC, b'\xff'))
        rootStart, rootSize, wbStart = chain(container), len(container), 0
    else:
        miniFatSid, rootStart, rootSize = ENDOFCHAIN, ENDOFCHAIN, 0
        wbStart = chain(stream)
    fat[fatSid] = FATSECT; fat[dirSid] = ENDOFCHAIN
    assert len(sectors) <= SEC // 4, "fixture needs a second FAT sector"

    def entry(name, typ, child, start, size, color=1):
        n = name.encode('utf-16-le') + b'\0\0'
        return n.ljust(64, b'\0') + struct.pack('<HBBIII', len(n), typ, color, FREESECT, FREESECT, child) \
            + b'\0' * 16 + struct.pack('<I', 0) + b'\0' * 16 + struct.pack('<III', start, size, 0)
    empty = b'\0' * 64 + struct.pack('<HBBIII', 0, 0, 0, FREESECT, FREESECT, FREESECT) + b'\0' * 16 + b'\0' * 4 + b'\0' * 16 + struct.pack('<III', 0, 0, 0)
    directory = entry("Root Entry", 5, 1, rootStart, rootSize) + entry("Workbook", 2, FREESECT, wbStart, len(stream)) + empty + empty
    sectors[dirSid] = directory
    sectors[fatSid] = b''.join(struct.pack('<I', f) for f in fat).ljust(SEC, b'\xff')
    header = b'\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1' + b'\0' * 16 + struct.pack('<HHHHH', 0x3E, 3, 0xFFFE, 9, 6) + b'\0' * 6
    header += struct.pack('<IIIIIIIII', 0, 1, dirSid, 0, 4096, miniFatSid, 1 if mini else 0, ENDOFCHAIN, 0)
    header += struct.pack('<I', fatSid) + struct.pack('<I', FREESECT) * 108
    assert len(header) == SEC
    return header + b''.join(sectors)


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    small = workbook(0); big = workbook(300)
    assert len(small) < 4096 <= len(big)
    open(sys.argv[1], 'wb').write(ole(small))
    open(sys.argv[2], 'wb').write(ole(big))
    print("xls fixtures: %d and %d bytes" % (len(small), len(big)))

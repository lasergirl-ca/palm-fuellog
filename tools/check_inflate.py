#!/usr/bin/env python3
"""Build zips with every kind of deflate block, inflate each entry with
build/test_inflate, and compare the bytes with Python's zipfile.

Usage: check_inflate.py <test_inflate binary> <work dir> [extra zips...]
"""
import os
import random
import subprocess
import sys
import zipfile
import zlib


def make_zips(work):
    random.seed(7)
    text = ("".join(random.choice("abcdefghij klmnop<row r=\"1\"><c r=\"A1\"><v>84.62</v></c></row>\n") for _ in range(300_000))).encode()
    noise = bytes(random.getrandbits(8) for _ in range(60_000))
    tiny = b"FuelLog"
    repeat = b"ab" * 40_000                       # long matches, max distance well inside 32 KB
    far = (noise[:40_000] + noise[:40_000])        # a 40,000-byte distance, beyond the window... (limited by deflate to 32 KB)
    cases = {
        "text-l1.zip": [("sheet.xml", text, zipfile.ZIP_DEFLATED, 1)],
        "text-l6.zip": [("sheet.xml", text, zipfile.ZIP_DEFLATED, 6)],
        "text-l9.zip": [("sheet.xml", text, zipfile.ZIP_DEFLATED, 9)],
        "noise.zip":   [("noise.bin", noise, zipfile.ZIP_DEFLATED, 6)],
        "tiny.zip":    [("t.txt", tiny, zipfile.ZIP_DEFLATED, 6)],
        "repeat.zip":  [("r.bin", repeat, zipfile.ZIP_DEFLATED, 9)],
        "far.zip":     [("f.bin", far, zipfile.ZIP_DEFLATED, 9)],
        "stored.zip":  [("s.bin", noise[:5000], zipfile.ZIP_STORED, 0)],
        "multi.zip":   [("a.txt", text[:1000], zipfile.ZIP_DEFLATED, 6), ("b.bin", noise[:1000], zipfile.ZIP_STORED, 0), ("xl/worksheets/sheet1.xml", text, zipfile.ZIP_DEFLATED, 6)],
    }
    paths = []
    for name, entries in cases.items():
        p = os.path.join(work, name)
        with zipfile.ZipFile(p, "w") as z:
            for ename, data, method, level in entries:
                z.writestr(ename, data, compress_type=method, compresslevel=level if method else None)
        paths.append(p)
    return paths


def main(argv):
    binary, work = argv[1], argv[2]
    os.makedirs(work, exist_ok=True)
    paths = make_zips(work) + argv[3:]
    failures = 0
    for p in paths:
        with zipfile.ZipFile(p) as z:
            for info in z.infolist():
                out = os.path.join(work, "out.bin")
                r = subprocess.run([binary, p, info.filename, out], capture_output=True, text=True)
                got = open(out, "rb").read() if r.returncode == 0 else b""
                want = z.read(info.filename)
                ok = r.returncode == 0 and got == want
                print(f"{'ok  ' if ok else 'FAIL'} {os.path.basename(p):14s} {info.filename:28s} {info.compress_size:7d} -> {len(want):7d}  {r.stdout.strip().split(' ')[-1] if r.stdout else r.stderr.strip()}")
                failures += not ok
    print(f"{failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

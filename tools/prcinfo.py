#!/usr/bin/env python3
"""List the resources in a Palm .prc so the build can be checked on the host.

Usage: prcinfo.py FuelLog.prc [--expect-code N]
Exits 1 when --expect-code is given and the number of 'code' resources differs.
"""
import struct
import sys


def read_prc(path):
    with open(path, "rb") as f:
        data = f.read()
    (name, attrs, version, ctime, mtime, btime, modnum, appinfo, sortinfo,
     dbtype, creator, seed, nextlist, count) = struct.unpack(">32sHHIIIIII4s4sIIH", data[:78])
    name = name.split(b"\0", 1)[0].decode("latin-1")
    entries = []
    pos = 78
    for _ in range(count):
        rtype, rid, offset = struct.unpack(">4sHI", data[pos:pos + 10])
        entries.append([rtype.decode("latin-1"), rid, offset])
        pos += 10
    for i, e in enumerate(entries):
        end = entries[i + 1][2] if i + 1 < len(entries) else len(data)
        e.append(end - e[2])
    return name, dbtype.decode("latin-1"), creator.decode("latin-1"), attrs, entries


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    expect = None
    if "--expect-code" in argv:
        expect = int(argv[argv.index("--expect-code") + 1])
    name, dbtype, creator, attrs, entries = read_prc(argv[1])
    print(f"{name}  type={dbtype} creator={creator} attrs=0x{attrs:04x} resources={len(entries)}")
    for rtype, rid, offset, size in entries:
        print(f"  {rtype} {rid:5d}  {size:6d} bytes")
    ncode = sum(1 for e in entries if e[0] == "code")
    if expect is not None and ncode != expect:
        print(f"expected {expect} code resources, found {ncode}")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

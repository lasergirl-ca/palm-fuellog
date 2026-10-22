#!/usr/bin/env python3
"""Validate the XLSX samples: zip integrity, part layout, XML well-formedness,
and that the seek and stream layouts carry identical parts.

Usage: check_xlsx.py seek.xlsx stream.xlsx
"""
import sys
import struct
import zipfile
import xml.etree.ElementTree as ET
import datetime

NS = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}


def check_one(path):
    print(f"== {path}")
    with zipfile.ZipFile(path) as z:
        bad = z.testzip()
        if bad:
            print(f"  CRC mismatch in {bad}")
            return None
        parts = {}
        for info in z.infolist():
            flag = "descriptor" if info.flag_bits & 8 else "header"
            assert info.compress_type == zipfile.ZIP_STORED, info.filename
            data = z.read(info.filename)
            parts[info.filename] = data
            print(f"  {info.filename:32s} {info.file_size:6d} bytes  stored, sizes in {flag}")
            if info.filename.endswith(".xml") or info.filename.endswith(".rels"):
                ET.fromstring(data)
    with open(path, "rb") as f:
        raw = f.read()
    # Every local header must agree with the central directory in the seek layout.
    sig = struct.unpack("<I", raw[:4])[0]
    assert sig == 0x04034b50, "no local header at offset 0"
    wb = ET.fromstring(parts["xl/workbook.xml"])
    names = [s.get("name") for s in wb.find("m:sheets", NS)]
    sheet1 = ET.fromstring(parts["xl/worksheets/sheet1.xml"])
    rows = sheet1.find("m:sheetData", NS).findall("m:row", NS)
    first = [c.find("m:is/m:t", NS).text for c in rows[0].findall("m:c", NS)]
    r2 = rows[1].findall("m:c", NS)
    vals = [(c.get("r"), c.get("s"), (c.find("m:v", NS).text if c.find("m:v", NS) is not None else c.find("m:is/m:t", NS).text)) for c in r2]
    print(f"  sheets: {names}; sheet1 rows: {len(rows)}; header: {first[:4]}...")
    print(f"  row 2: {vals[:5]}")
    assert names == ["Fills", "Vehicles"]
    assert len(rows) == 9
    serial = (datetime.date(2026, 7, 29) - datetime.date(1899, 12, 30)).days
    assert vals[0] == ("A2", "1", str(serial)), vals[0]   # 2026-07-29 as an Excel serial
    assert vals[4] == ("E2", "4", "79.37")
    return parts


def main(argv):
    seek = check_one(argv[1])
    stream = check_one(argv[2])
    if seek is None or stream is None:
        return 1
    same = seek == stream
    print("parts identical across layouts:", same)
    return 0 if same else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))

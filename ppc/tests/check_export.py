#!/usr/bin/env python3
"""Check an XLSX the Pocket PC exporter wrote from the mkpdb fixture: the zip's
CRCs, every XML part well-formed, the Palm's five sheets, the three fills, and
the accented note arriving as UTF-8.

Usage: check_export.py FuelLog-YYYYMMDD.xlsx
"""
import sys
import zipfile
import xml.etree.ElementTree as ET

NS = {"m": "http://schemas.openxmlformats.org/spreadsheetml/2006/main"}


def main(argv):
    path = argv[1]
    with zipfile.ZipFile(path) as z:
        bad = z.testzip()
        assert bad is None, f"CRC mismatch in {bad}"
        for name in z.namelist():
            if name.endswith(".xml") or name.endswith(".rels"):
                ET.fromstring(z.read(name))
        book = ET.fromstring(z.read("xl/workbook.xml"))
        sheets = [s.get("name") for s in book.find("m:sheets", NS)]
        assert sheets == ["Fills", "Monthly", "Vehicles", "Programmes", "Services"], sheets
        fills = ET.fromstring(z.read("xl/worksheets/sheet1.xml"))
        rows = fills.findall(".//m:row", NS)
        assert len(rows) == 4, len(rows)          # the header and three fills
        text = "".join(t.text or "" for t in fills.iter("{%s}t" % NS["m"]))
        assert "Petro-Canada on 152 St., café" in text, "the accented note"
        vehicles = ET.fromstring(z.read("xl/worksheets/sheet3.xml"))
        assert len(vehicles.findall(".//m:row", NS)) == 3
    print(f"ok   {path}: five sheets, three fills, the accented note in UTF-8")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))

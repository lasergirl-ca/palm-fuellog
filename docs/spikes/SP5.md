# SP5: Does a stored (uncompressed) zip XLSX made by our writer open clean in Excel and Numbers?

**Status (Oct 22, 2026): Numbers passes for both layouts; Excel is pending.** The samples
are in `docs/spikes/samples/`: `fuellog-seek.xlsx` (local headers patched through a seek)
and `fuellog-stream.xlsx` (data descriptors, the layout used over Bluetooth).

## The writer

`src/xlsx.c` over `src/outstream.c`, plain C with no Palm headers, compiled for both the host
and the 68k (section `xport`).

- Zip with STORED entries only: a table-driven CRC-32 (1 KB table, filled at run time) and
  no deflate. Each part streams straight to the sink in small writes.
- Two layouts from one code path. With a seekable sink the local header is written with
  zero CRC and sizes, then patched once the part is done. Without a seek (the Exchange
  Manager) general-purpose bit 3 is set and a 16-byte data descriptor follows each part. The
  central directory carries the real values in both cases.
- Seven parts: `[Content_Types].xml`, `_rels/.rels`, `xl/workbook.xml`,
  `xl/_rels/workbook.xml.rels`, `xl/styles.xml` and one `xl/worksheets/sheetN.xml` per sheet.
  Cells are inline strings (`t="inlineStr"`), typed numbers (`<v>`), and dates as serial
  numbers in the 1900 system (days since 1904 plus 1,462) with a date style.
- `styles.xml` defines a date format, `0.0`, `0.00`, `[$CAD] #,##0.00`, `[$USD] #,##0.00`
  and a bold header font, as cell styles 1 to 6.
- The sample has a Fills sheet (the eight mock-up fills, ten columns, one note with `<`, `&`
  and `>` to exercise escaping) and a Vehicles sheet.

## Host checks (all pass)

| Check | Result |
| --- | --- |
| `unzip -t` on both files | no errors |
| Python `zipfile.testzip` (CRC of every part) | clean |
| Every part parses as XML | yes |
| Workbook lists `Fills`, `Vehicles`; sheet 1 has 9 rows; A2 is serial 46232 (2026-07-29) with the date style; E2 is 79.37 with the CAD style | yes |
| The seven parts are byte-identical between the seek and stream layouts | yes |

Three writer bugs were found by these checks and fixed before the files ever reached a
viewer: the end-of-central-directory size was computed after 12 bytes of the record had
been written, sheet part names shared one static buffer, and the DOS date constant said 2034.

## Viewer results

| Reader | Seek layout | Data-descriptor layout |
| --- | --- | --- |
| Numbers 15.4 (macOS), opened through Launch Services | Fills 10x10 and Vehicles 10x5 tables; A2 is a real date shown as 2026-07-29; E2 shows as currency ($ 79.37); the note with `<`, `&` and `>` round-trips; export to CSV works | identical result, and the CSV exports of the two files are byte-identical |
| Quick Look (macOS) | renders the Fills sheet with dates, currency and bold headers | identical |
| Go excelize 2.9 (strict OOXML reader) | both sheets, 9 rows, date and CAD styles resolved | identical |
| Excel | pending | pending |

No repair prompt could be observed by script (System Events is not available to osascript
here), but Numbers created full tables and exported them, which it does not do for a file it
had to repair or reject.

Two notes for anyone repeating this. First, Numbers' AppleScript `open` command imports an
XLSX into an empty document and never fills it, even for a workbook Numbers itself wrote; the
file must be opened through Launch Services (`open -a Numbers`), after which AppleScript can
read every table. Second, a Numbers reference workbook showed that Numbers writes deflated
entries, shared strings, `docProps` and a theme, none of which the importer requires.

If Excel rejects only the data-descriptor layout, the Bluetooth path falls back to computing
each part's CRC and size in a first pass before writing it, which costs time but no memory;
the SD path is unaffected.

## Cost on the 68k

The three export sources (`outstream.c`, `xlsx.c`, `pdf.c`) compile into their own code
section, `xport`, of about 9.4 KB. Nothing in the app calls them yet.

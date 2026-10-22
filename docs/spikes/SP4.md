# SP4: VFS on the T|X: write speed, folder creation, `VFSExportDatabaseToFile` round trip

**Answer (Oct 22, 2026): yes on all three counts.** 200 KB writes at 86 KB/s in 4 KB chunks,
the folders create cleanly, and a 1,000-record database exported with
`VFSExportDatabaseToFile` re-imports intact. The test is the SP4 button at the bottom left
of the hello-world form; it needs an SD card in the T|X.

## What the test does

`src/vfstest.c`, run from the SP4 button, in this order:

1. Finds the first mounted volume with `VFSVolumeEnumerate` and prints its label.
2. Creates `/PALM/Programs/FuelLog/Export`, `Backup` and `Import`, one level at a time,
   treating "already exists" as success.
3. Writes 200 KB to `Export/sp4-200kb.bin` in 4 KB chunks from a fixed pseudo-random
   stream and times it; then reads it back in 4 KB chunks, checks the size and a byte sum,
   times that, and deletes the file.
4. Creates `FuelSP4-ZFlg` (type `SP4t`, creator `ZFlg`, backup bit set) with 1,000 records
   of 64 bytes, timed, and takes a checksum over every record byte.
5. Exports it with `VFSExportDatabaseToFile` to `Backup/FuelSP4-ZFlg.pdb`, timed, and
   prints the file size.
6. Deletes the live database, imports the `.pdb` with `VFSImportDatabaseFromFile`, timed,
   re-opens it, and compares the record count and checksum with step 4.
7. Deletes the imported database and leaves the `.pdb` on the card, which later serves as a
   fixture for the Go PDB reader.

Every step prints one line on the results form; an error prints its code and stops.

## What to check on the T|X

With a card inserted, tap SP4 and wait a few seconds. Expect something like:

```
Card: <label>
Dirs: /PALM/Programs/FuelLog/* ok
Write 200 KB: NNN ms
  = NNN KB/s in 4 KB chunks
Read 200 KB: NNN ms, verified
Create 1000 recs: NNN ms
Export .pdb: NNN ms
  .pdb size: NNNNN bytes
Import .pdb: NNN ms
Round trip: 1000 recs, intact
Left on card: Backup/FuelSP4-ZFlg.pdb
```

A screenshot of that form settles the spike. "MISMATCH" or an error code is the finding to
send. The exit criterion is 200 KB written and timed, and the exported `.pdb` re-imported
intact.

## Device result

T|X with its SD card (label CARD), Oct 22, 2026:

| Step | Result |
| --- | --- |
| Folders `/PALM/Programs/FuelLog/{Export,Backup,Import}` | created |
| Write 200 KB in 4 KB chunks | 2,310 ms (86 KB/s) |
| Read 200 KB back in 4 KB chunks | 370 ms, byte sum and size verified |
| Create 1,000 records of 64 bytes | 870 ms |
| `VFSExportDatabaseToFile`, 72,080-byte `.pdb` | 150 ms |
| `VFSImportDatabaseFromFile` | 360 ms |
| Round trip | 1,000 records, checksum intact |

The `.pdb` size is exactly the 78-byte header, 1,000 eight-byte record entries, two bytes of
padding and 64,000 bytes of records, so the exporter writes a plain PDB with no extras.

## What this means for the design

- Backups are cheap: exporting a 72 KB database took 150 ms, so a five-database set of a
  few hundred KB will take about a second. Auto-backup on exit is fine.
- Card writes through `VFSFileWrite` are the slow path at 86 KB/s. A CSV of a thousand
  fills (about 100 KB) takes a little over a second; an XLSX or PDF of a few hundred KB takes
  several seconds, which is why those exports get the cancellable progress dialog the spec
  already calls for. Larger chunks may help and are worth one measurement when the real
  writers land in M5; the fallback of smaller chunks is not needed.
- Reads are six times faster than writes, so import of a 200 KB spreadsheet spends its time
  in parsing, not in the card.
- Creating 1,000 records took 870 ms, under one millisecond each, so the import's second
  pass is bounded by the card read and the parser.

A 16-bit `int` slip showed up on the results form: `err ? err : -1` widened to 65535 under
prc-tools and printed as a stray "6" at the screen edge. Conditionals that mix an unsigned
16-bit value with a negative literal must be cast to `Int32` explicitly.

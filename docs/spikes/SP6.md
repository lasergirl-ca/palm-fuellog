# SP6: Inflate on 68k under PACE: speed and heap for a 200 KB .xlsx

**Answer (Oct 22, 2026): yes. On the T|X the decoder inflates at 94 KB/s of output with 42 KB
of heap, so the 1.6 MB worksheet of a 218 KB Numbers-saved workbook takes 17 s, and a
paper log of a few hundred rows takes a second or two.** The first build managed 21 KB/s;
a 9-bit lookup table, a table CRC and direct copies made it 4.4 times faster.
The sample is `docs/spikes/samples/sp6.xlsx`, a 217,819-byte workbook saved by Numbers
15.4 from a 4,000-row import of our own export, whose Fills sheet is 210,529 bytes deflated
and 1,647,437 bytes inflated.

## The decoder

`src/inflate.c` is an RFC 1951 inflate written for the 68k: no malloc, no recursion, streaming
input through a read callback and output through a write callback, with a caller-owned 32 KB
window, 4 KB input buffer and 1 KB CRC table. Codes of up to 9 bits decode through a
512-entry lookup table built with each Huffman table; longer codes fall back to canonical
bit-by-bit decoding in the manner of zlib's `puff`. Matches that stay inside the window copy
directly. It handles stored, fixed and dynamic blocks, keeps a running CRC-32 of the
output, and refuses back-references before the start of output. `src/zipread.c` finds an
entry by name through the central directory using positioned reads, so the zip can live on
the card. Both compile into the `xport` code section; together they are about 3 KB.

Heap while decoding: the `Inflate` state (about 3.3 KB of Huffman and lookup tables), the
32 KB window, the 4 KB input buffer, a 1 KB scratch buffer and the 1 KB CRC table, about
41 KB in all, allocated at run time and freed after. The T|X reported 5.7 MB free.

## Host tests (all pass)

`make test` builds `tests/test_inflate.c` and runs `tools/check_inflate.py`, which zips text
at levels 1, 6 and 9, incompressible noise (stored blocks inside deflate), a seven-byte entry
(fixed Huffman), 80 KB of a two-byte repeat (long matches), a stored entry and a multi-entry
zip, then inflates every entry and compares the bytes with Python's `zipfile`. It also runs
every entry of Numbers' own small reference workbook, our `fuellog-seek.xlsx`, and all twelve
entries of the SP6 sample. The host decodes the 1.6 MB sheet in about 20 ms on this Mac, which gives the device number a scale.

## What to check on the T|X

1. Copy `docs/spikes/samples/sp6.xlsx` to the card as `/PALM/Programs/FuelLog/Import/sp6.xlsx`
   (the folder exists after SP4; create it if not).
2. Launch FuelLog and tap SP6. The first pass inflates the sheet and discards the output; the
   second writes it to `Export/sp6-sheet1.xml`, which at 86 KB/s takes about 20 seconds on
   its own, so expect the form to take up to a minute.
3. Expect lines like:

```
sp6.xlsx: 217819 bytes
sheet2.xml: 210529 deflated
  -> 1647437 bytes, method 8
Discard: NNNN ms
  = NNN KB/s, CRC ok
To card: NNNN ms
  = NNN KB/s, CRC ok
Heap free: NNNN KB before, NNNN KB in
Used: 39236 bytes of heap
```

The discard rate is the decoder's own speed under PACE; the to-card rate includes the SD
write. "CRC BAD" or an error code is the finding to send.

## Device results

**First build (bit-by-bit decoding, bitwise CRC), Oct 22:**

| Pass | Time | Rate | CRC |
| --- | --- | --- | --- |
| Decode and discard, 1,647,437 bytes | 74,840 ms | 21 KB/s | ok |
| Decode to `Export/sp6-sheet1.xml` | 75,850 ms | 21 KB/s | ok |

Heap: 5,786 KB free before, 5,748 KB during (39 KB used). The first attempt looked frozen
because nothing was drawn for 75 seconds; the progress line was added after that, and the
run above is from the build with it.

Two findings beyond the decoder. Writing 1.6 MB to the card in 32 KB pieces added only one
second to the pass, against the 86 KB/s measured in SP4 with 4 KB writes; the export writers
should buffer 32 KB. And 21 KB/s is the decoder's own speed under PACE, so the per-bit
function calls were the cost, not the card.

**Second build (lookup table, table CRC, direct copies), Oct 22:**

| Pass | Time | Rate | CRC |
| --- | --- | --- | --- |
| Decode and discard, 1,647,437 bytes | 17,030 ms | 94 KB/s | ok |
| Decode to `Export/sp6-sheet1.xml` | 18,040 ms | 89 KB/s | ok |

Heap: 5,786 KB free before, 5,745 KB during (42 KB used, 3 KB more than the first build for
the two 512-entry lookup tables and the CRC table). On the host the same change halved the
time; under PACE it gave 4.4x, which confirms that the per-bit function calls were the cost.

## What this means for import

Paper logs run to a few hundred rows, which is a worksheet of 50 to 150 KB inflated, so an
import decodes in one or two seconds, and the fallback of decompressing to the card in
passes is not needed. The 4,000-row sample is the extreme case, and its 1.6 MB sheet is
also why the XLSX reader in M6 must scan the XML as a stream rather than hold it.

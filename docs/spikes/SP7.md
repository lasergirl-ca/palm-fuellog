# SP7: Base-14 PDF from our writer: renders in Preview, Acrobat and iOS?

**Status (Oct 22, 2026): Preview's engine passes with the euro sign correct; Acrobat and iOS
are pending.** The sample is `docs/spikes/samples/fuellog.pdf`, three pages, 5,293 bytes.

## The writer

`src/pdf.c` over `src/outstream.c`, plain C, compiled for the host and the 68k.

- PDF 1.4, uncompressed, US Letter landscape (792 x 612 pt), Helvetica and Helvetica-Bold
  with `/Encoding /WinAnsiEncoding` and no embedded fonts.
- One pass, no buffering: each page's content stream takes its `/Length` from an object
  written right after the stream, and the page tree (object 2) is written last once every
  page is known. The xref table is built from recorded offsets.
- Strings escape `(`, `)` and `\`; bytes outside printable ASCII go out as octal, so Palm
  Latin text reaches the font untouched. The euro is 0x80 in both Palm Latin and WinAnsi.
- Rules and the graph are path operators only (`m`, `l`, `re`, `S`, `w`, `d`, `G`).

Page 1 is a summary with a euro line, accented words and symbols; page 2 is the log laid out
like the paper form; page 3 is the economy graph with a dashed mean.

## Results

| Viewer | Result |
| --- | --- |
| Apple's PDF engine (ImageIO via `sips`, the same renderer as Preview and iOS) | page 1 renders; the euro, ¢, ©, °, ½, é and ï all correct |
| Quick Look thumbnail | renders |
| poppler (`pdfinfo`, `pdftotext`, `pdftoppm`) | 3 pages, Letter; text extracts with the euro and accents intact; all pages render |
| Preview.app | same engine as above; open the file to confirm the page view |
| Acrobat | pending, not installed here |
| iOS (Files or Mail on an iPhone) | pending |

## What to check on iOS and in Acrobat

Open `fuellog.pdf`, go to page 1, and confirm the line "Euro test: a fill in Lyon at 1,789
€/L" shows a euro sign and not a box or a blank. Then flip to pages 2 and 3 to confirm the
table rules and the graph draw. If the euro fails anywhere, the fallback stays as written in
the backlog: print "EUR" instead of the glyph.

#include "sp6test.h"
#include "inflate.h"
#include "zipread.h"
#include "crc32.h"
#include <VFSMgr.h>

#define kZipPath  "/PALM/Programs/FuelLog/Import/sp6.xlsx"
#define kOutPath  "/PALM/Programs/FuelLog/Export/sp6-sheet1.xml"
#define kInBuf    4096

typedef struct {
    FileRef in;
    UInt32 pos, left;
    FileRef out;      /* 0 = discard */
    UInt32 start;     /* ticks when the pass began */
    UInt32 done;      /* inflated bytes so far */
    const char *label;
} Ctx;

/* Called from the window flush every 32 KB: a one-line status on the form,
 * so a long decode under PACE is visibly alive and can be timed by eye. */
/* Section placement: see sections.h. */
static void Progress(Ctx *x) SEC2;
static Int32 ReadAt(void *c, UInt32 pos, UInt8 *buf, UInt32 len) SEC2;
static Int32 ReadNext(void *c, UInt8 *buf, UInt32 len) SEC2;
static Int32 WriteOut(void *c, const UInt8 *buf, UInt32 len) SEC2;
static void Line(char lines[][vfsTestLineLen], UInt16 *n, const char *a, Int32 num, const char *b) SEC2;
static Int32 Ms(UInt32 start) SEC2;
static UInt32 FreeHeap(void) SEC2;

static void Progress(Ctx *x)
{
    RectangleType r;
    char text[48];
    r.topLeft.x = 2; r.topLeft.y = 52; r.extent.x = 156; r.extent.y = 12;
    WinEraseRectangle(&r, 0);
    StrCopy(text, x->label);
    StrIToA(text + StrLen(text), (Int32)(x->done / 1024UL));
    StrCat(text, " KB, ");
    StrIToA(text + StrLen(text), (Int32)((TimGetTicks() - x->start) * 1000UL / SysTicksPerSecond()));
    StrCat(text, " ms");
    FntSetFont(stdFont);
    WinDrawChars(text, StrLen(text), 4, 53);
}

static Int32 ReadAt(void *c, UInt32 pos, UInt8 *buf, UInt32 len)
{
    Ctx *x = c;
    UInt32 got = 0;
    if (VFSFileSeek(x->in, vfsOriginBeginning, (Int32)pos) != errNone) return -1;
    if (VFSFileRead(x->in, len, buf, &got) != errNone && got == 0) return -1;
    return (Int32)got;
}

static Int32 ReadNext(void *c, UInt8 *buf, UInt32 len)
{
    Ctx *x = c;
    UInt32 got = 0;
    if (x->left == 0) return 0;
    if (len > x->left) len = x->left;
    if (VFSFileSeek(x->in, vfsOriginBeginning, (Int32)x->pos) != errNone) return -1;
    if (VFSFileRead(x->in, len, buf, &got) != errNone && got == 0) return -1;
    x->pos += got; x->left -= got;
    return (Int32)got;
}

static Int32 WriteOut(void *c, const UInt8 *buf, UInt32 len)
{
    Ctx *x = c;
    UInt32 wrote = 0;
    x->done += len;
    Progress(x);
    if (!x->out) return 0;
    if (VFSFileWrite(x->out, len, buf, &wrote) != errNone || wrote != len) return -1;
    return 0;
}

static void Line(char lines[][vfsTestLineLen], UInt16 *n, const char *a, Int32 num, const char *b)
{
    char *s;
    if (*n >= vfsTestMaxLines) return;
    s = lines[(*n)++];
    StrNCopy(s, a, vfsTestLineLen - 1); s[vfsTestLineLen - 1] = 0;
    if (num >= 0) StrIToA(s + StrLen(s), num);
    if (b) StrNCat(s, b, vfsTestLineLen - 1);
}

static Int32 Ms(UInt32 start) { return (Int32)((TimGetTicks() - start) * 1000UL / SysTicksPerSecond()); }

static UInt32 FreeHeap(void)
{
    UInt32 freeBytes = 0, maxChunk = 0;
    MemHeapFreeBytes(0, &freeBytes, &maxChunk);
    return freeBytes;
}

UInt16 Sp6TestRun(char lines[vfsTestMaxLines][vfsTestLineLen])
{
    UInt16 n = 0, vol = 0, pass;
    UInt32 iterator = vfsIteratorStart, size = 0, heapBefore, heapDuring = 0;
    Ctx x;
    ZipEntry e;
    Inflate *z;
    UInt8 *window, *inBuf, *scratch;
    UInt32 *crcTable;
    Int32 r;
    Err err;

    heapBefore = FreeHeap();
    if (VFSVolumeEnumerate(&vol, &iterator) != errNone) { Line(lines, &n, "No card mounted", -1, NULL); return n; }
    err = VFSFileOpen(vol, kZipPath, vfsModeRead, &x.in);
    if (err) { Line(lines, &n, "Import/sp6.xlsx: open err ", err, NULL); return n; }
    VFSFileSize(x.in, &size);

    z = MemPtrNew(sizeof *z);
    window = MemPtrNew(inflateWindowSize);
    inBuf = MemPtrNew(kInBuf);
    scratch = MemPtrNew(1024);
    crcTable = MemPtrNew(256 * sizeof(UInt32));
    if (crcTable) Crc32Init(crcTable);
    if (!z || !window || !inBuf || !scratch || !crcTable) { Line(lines, &n, "Out of heap", -1, NULL); VFSFileClose(x.in); return n; }

    /* Numbers and Excel number their sheets differently, so take the largest
     * worksheet among the first four. */
    {
        char entry[32];
        ZipEntry best;
        UInt16 k, found = 0;
        best.usize = 0;
        for (k = 1; k <= 4; k++) {
            StrCopy(entry, "xl/worksheets/sheet");
            StrIToA(entry + StrLen(entry), k);
            StrCat(entry, ".xml");
            if (ZipFind(ReadAt, &x, size, entry, scratch, 1024, &e) == 0 && e.usize > best.usize) {
                best = e;
                found = k;
            }
        }
        if (!found) { Line(lines, &n, "No worksheet found in the zip", -1, NULL); goto done; }
        e = best;
        Line(lines, &n, "sp6.xlsx: ", (Int32)size, " bytes");
        Line(lines, &n, "sheet", found, ".xml: ");
        StrIToA(lines[n - 1] + StrLen(lines[n - 1]), (Int32)e.csize);
        StrNCat(lines[n - 1], " deflated", vfsTestLineLen - 1);
    }
    Line(lines, &n, "  -> ", (Int32)e.usize, " bytes, method ");
    StrIToA(lines[n - 1] + StrLen(lines[n - 1]), e.method);
    if (e.method != 8) { Line(lines, &n, "not deflated; nothing to time", -1, NULL); goto done; }

    for (pass = 0; pass < 2; pass++) {
        UInt32 start;
        Int32 ms;
        x.pos = e.dataOffset; x.left = e.csize; x.out = 0;
        x.done = 0; x.label = pass ? "To card: " : "Decoding: ";
        if (pass == 1) {
            VFSFileDelete(vol, kOutPath);
            if (VFSFileOpen(vol, kOutPath, vfsModeWrite | vfsModeCreate | vfsModeTruncate, &x.out) != errNone) {
                Line(lines, &n, "Export/sp6-sheet1.xml: open failed", -1, NULL);
                break;
            }
        }
        InflateInit(z, ReadNext, WriteOut, &x, inBuf, kInBuf, window, crcTable);
        start = TimGetTicks();
        x.start = start;
        Progress(&x);
        r = InflateRun(z);
        ms = Ms(start);
        heapDuring = FreeHeap();
        if (x.out) VFSFileClose(x.out);
        if (r) { Line(lines, &n, pass ? "To card: err " : "Discard: err ", -r, NULL); break; }
        Line(lines, &n, pass ? "To card: " : "Discard: ", ms, " ms");
        if (ms > 0) Line(lines, &n, "  = ", (Int32)(e.usize / 1024UL * 1000UL / (UInt32)ms), (z->crc == e.crc) ? " KB/s, CRC ok" : " KB/s, CRC BAD");
    }
    Line(lines, &n, "Heap free: ", (Int32)(heapBefore / 1024UL), " KB before, ");
    StrIToA(lines[n - 1] + StrLen(lines[n - 1]), (Int32)(heapDuring / 1024UL));
    StrNCat(lines[n - 1], " KB in", vfsTestLineLen - 1);
    Line(lines, &n, "Used: ", (Int32)(sizeof *z + inflateWindowSize + kInBuf + 1024 + 1024), " bytes of heap");

done:
    VFSFileClose(x.in);
    if (z) MemPtrFree(z);
    if (window) MemPtrFree(window);
    if (inBuf) MemPtrFree(inBuf);
    if (scratch) MemPtrFree(scratch);
    if (crcTable) MemPtrFree(crcTable);
    return n;
}

#include "xlsread.h"
#include "csv.h"
#include "datecalc.h"

typedef unsigned long long U64;

#define ENDOFCHAIN 0xFFFFFFFEUL
#define FREESECT   0xFFFFFFFFUL
#define MAXREGSECT 0xFFFFFFFAUL

/* record types */
#define rtBof        0x0809
#define rtEof        0x000A
#define rtContinue   0x003C
#define rtDate1904   0x0022
#define rtBoundSheet 0x0085
#define rtSst        0x00FC
#define rtLabelSst   0x00FD
#define rtNumber     0x0203
#define rtRk         0x027E
#define rtMulRk      0x00BD
#define rtLabel      0x0204
#define rtBoolErr    0x0205
#define rtFormula    0x0006
#define rtString     0x0207
#define rtShrFmla    0x04BC
#define rtArray      0x0221
#define rtTable      0x0236

/* Section placement: see sections.h. */
static UInt16 Le16(const UInt8 *p) XLS_SEC;
static UInt32 Le32(const UInt8 *p) XLS_SEC;
static UInt16 Cp(char *d, const char *s, UInt16 cap) XLS_SEC;
static UInt16 Len(const char *s) XLS_SEC;
static UInt8 EqI(const char *a, const char *b) XLS_SEC;
static Int32 ReadFile(XlsReader *r, UInt32 off, UInt8 *buf, UInt16 len) XLS_SEC;
static UInt32 FatNext(XlsReader *r, UInt32 sid) XLS_SEC;
static UInt32 MiniNext(XlsReader *r, UInt32 msid) XLS_SEC;
static UInt32 ChainAt(XlsReader *r, XsChain *c, UInt32 index, UInt8 mini) XLS_SEC;
static Int32 StreamRead(XlsReader *r, XsChain *c, UInt8 mini, UInt32 pos, UInt8 *buf, UInt16 len) XLS_SEC;
static Int32 WbRead(XlsReader *r, UInt32 pos, UInt8 *buf, UInt16 len) XLS_SEC;
static Int32 OpenOle(XlsReader *r) XLS_SEC;
static Int32 NextRecord(XlsReader *r) XLS_SEC;
static UInt8 Continue(XlsReader *r) XLS_SEC;
static UInt8 U8(XlsReader *r) XLS_SEC;
static UInt16 U16(XlsReader *r) XLS_SEC;
static UInt32 U32(XlsReader *r) XLS_SEC;
static void Bytes(XlsReader *r, UInt8 *buf, UInt16 n) XLS_SEC;
static UInt8 C8(XlsReader *r) XLS_SEC;
static UInt16 C16(XlsReader *r) XLS_SEC;
static UInt32 C32(XlsReader *r) XLS_SEC;
static void CSkip(XlsReader *r, UInt32 n) XLS_SEC;
static UInt16 ReadUni(XlsReader *r, UInt16 cch, char *out, UInt16 cap) XLS_SEC;
static Int32 ParseSst(XlsReader *r) XLS_SEC;
static void SstGet(XlsReader *r, UInt32 index, char *out, UInt16 cap) XLS_SEC;
static void SetNumber(XlsReader *r, UInt8 neg, U64 m, Int16 e) XLS_SEC;
static void DoubleToNumber(XlsReader *r, const UInt8 *d) XLS_SEC;
static void RkToNumber(XlsReader *r, UInt32 rk) XLS_SEC;
static void CellHead(XlsReader *r) XLS_SEC;
static UInt8 FetchCell(XlsReader *r) XLS_SEC;
static void NumberText(XlsReader *r, Int16 col, char *out, UInt16 cap) XLS_SEC;

static UInt16 Le16(const UInt8 *p) { return (UInt16)(p[0] | (p[1] << 8)); }
static UInt32 Le32(const UInt8 *p) { return (UInt32)p[0] | ((UInt32)p[1] << 8) | ((UInt32)p[2] << 16) | ((UInt32)p[3] << 24); }
static UInt16 Len(const char *s) { UInt16 n = 0; while (s[n]) n++; return n; }
static UInt16 Cp(char *d, const char *s, UInt16 cap) { UInt16 n = 0; while (s[n] && n + 1 < cap) { d[n] = s[n]; n++; } d[n] = 0; return n; }
static UInt8 EqI(const char *a, const char *b)
{
    while (*a && *b) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* ---- the compound file ---- */

/* Bytes at a file offset through the one-sector cache; returns the count read. */
static Int32 ReadFile(XlsReader *r, UInt32 off, UInt8 *buf, UInt16 len)
{
    Int32 total = 0;
    while (len) {
        UInt32 block = off & ~511UL;
        UInt16 in = (UInt16)(off - block), n;
        if (r->cacheLen == 0 || r->cacheOff != block) {
            Int32 got = r->readAt(r->ctx, block, r->cache, 512);
            if (got <= 0) break;
            r->cacheOff = block; r->cacheLen = (UInt16)got;
        }
        if (in >= r->cacheLen) break;
        n = (UInt16)(r->cacheLen - in);
        if (n > len) n = len;
        { UInt16 i; for (i = 0; i < n; i++) buf[i] = r->cache[in + i]; }
        buf += n; off += n; len = (UInt16)(len - n); total += n;
    }
    return total;
}

static UInt32 FatNext(XlsReader *r, UInt32 sid)
{
    UInt8 b[4];
    UInt16 perSector = (UInt16)(1U << (r->secShift - 2));
    UInt32 idx = sid / perSector, fatSid;
    if (idx >= r->difatCount) return ENDOFCHAIN;
    fatSid = r->difat[idx];
    if (ReadFile(r, ((fatSid + 1) << r->secShift) + (sid % perSector) * 4, b, 4) != 4) return ENDOFCHAIN;
    return Le32(b);
}

static UInt32 MiniNext(XlsReader *r, UInt32 msid)
{
    UInt8 b[4];
    if (StreamRead(r, &r->miniFat, 0, msid * 4, b, 4) != 4) return ENDOFCHAIN;
    return Le32(b);
}

/* The sector id of the index-th sector in the chain, or ENDOFCHAIN. */
static UInt32 ChainAt(XlsReader *r, XsChain *c, UInt32 index, UInt8 mini)
{
    UInt32 guard = 0;
    if (c->start >= MAXREGSECT) return ENDOFCHAIN;
    if (index < c->index || c->sid >= MAXREGSECT) { c->index = 0; c->sid = c->start; }
    while (c->index < index && c->sid < MAXREGSECT) {
        c->sid = mini ? MiniNext(r, c->sid) : FatNext(r, c->sid);
        c->index++;
        if (++guard > 0x100000UL) return ENDOFCHAIN;   /* a looped chain */
    }
    return c->sid;
}

static Int32 StreamRead(XlsReader *r, XsChain *c, UInt8 mini, UInt32 pos, UInt8 *buf, UInt16 len)
{
    Int32 total = 0;
    UInt8 shift = mini ? r->miniShift : r->secShift;
    UInt32 unit = 1UL << shift;
    while (len) {
        UInt32 sid = ChainAt(r, c, pos >> shift, mini), in = pos & (unit - 1);
        UInt16 chunk = (UInt16)(unit - in);
        Int32 got;
        if (sid >= MAXREGSECT) break;
        if (chunk > len) chunk = len;
        if (mini) got = StreamRead(r, &r->root, 0, (sid << r->miniShift) + in, buf, chunk);
        else got = ReadFile(r, ((sid + 1) << r->secShift) + in, buf, chunk);
        if (got <= 0) break;
        buf += got; pos += (UInt32)got; len = (UInt16)(len - got); total += got;
        if (got < chunk) break;
    }
    return total;
}

static Int32 WbRead(XlsReader *r, UInt32 pos, UInt8 *buf, UInt16 len)
{
    if (pos >= r->wbSize) return 0;
    if (pos + len > r->wbSize) len = (UInt16)(r->wbSize - pos);
    return StreamRead(r, &r->wb, r->wbMini, pos, buf, len);
}

/* Header, DIFAT and directory: finds the Workbook (or Book) stream. */
static Int32 OpenOle(XlsReader *r)
{
    UInt8 h[128];
    UInt32 dirStart, miniFatStart, difatStart, k, i;
    UInt16 n;
    if (ReadFile(r, 0, h, 128) != 128) return xsErrNotOle;
    if (h[0] != 0xD0 || h[1] != 0xCF || h[2] != 0x11 || h[3] != 0xE0 || h[4] != 0xA1 || h[5] != 0xB1 || h[6] != 0x1A || h[7] != 0xE1) return xsErrNotOle;
    r->secShift = (UInt8)Le16(h + 0x1E); r->miniShift = (UInt8)Le16(h + 0x20);
    if (r->secShift < 7 || r->secShift > 12 || r->miniShift < 2 || r->miniShift > r->secShift) return xsErrNotOle;
    dirStart = Le32(h + 0x30); r->miniCutoff = Le32(h + 0x38); miniFatStart = Le32(h + 0x3C); difatStart = Le32(h + 0x44);
    /* the first 109 FAT sector ids sit in the header, the rest in DIFAT sectors */
    r->difatCount = 0;
    for (k = 0; k < 109; k++) {
        UInt8 e[4];
        if (ReadFile(r, 0x4C + k * 4, e, 4) != 4) break;
        if (Le32(e) >= MAXREGSECT) break;
        r->difat[r->difatCount++] = Le32(e);
    }
    k = difatStart;
    while (k < MAXREGSECT && r->difatCount < xsDifatMax) {
        UInt32 base = (k + 1) << r->secShift, per = (1UL << (r->secShift - 2)) - 1;
        UInt8 e[4];
        for (i = 0; i < per && r->difatCount < xsDifatMax; i++) {
            if (ReadFile(r, base + i * 4, e, 4) != 4 || Le32(e) >= MAXREGSECT) break;
            r->difat[r->difatCount++] = Le32(e);
        }
        if (ReadFile(r, base + per * 4, e, 4) != 4) break;
        k = Le32(e);
    }
    r->miniFat.start = miniFatStart; r->miniFat.sid = miniFatStart; r->miniFat.index = 0;
    /* directory entries, 128 bytes each; entry 0 is the root (the mini stream container) */
    {
        XsChain dir;
        UInt8 found = 0;
        dir.start = dirStart; dir.sid = dirStart; dir.index = 0;
        for (n = 0; n < 2048; n++) {
            UInt8 type;
            if (StreamRead(r, &dir, 0, (UInt32)n * 128, h, 128) != 128) break;
            type = h[66];
            if (n == 0) { r->root.start = Le32(h + 116); r->root.sid = r->root.start; r->root.index = 0; continue; }
            if (type == 2 && !found) {
                UInt16 len = Le16(h + 64) / 2;   /* UTF-16 units including the terminator */
                char name[16];
                UInt16 q;
                for (q = 0; q + 1 < len && q < 15; q++) name[q] = (char)(h[q * 2] | (h[q * 2 + 1] ? '?' : 0));
                name[q] = 0;
                if (EqI(name, "Workbook") || EqI(name, "Book")) {
                    r->wb.start = Le32(h + 116); r->wb.sid = r->wb.start; r->wb.index = 0;
                    r->wbSize = Le32(h + 120);
                    r->wbMini = r->wbSize < r->miniCutoff;
                    found = 1;
                }
            }
        }
        if (!found) return xsErrNoBook;
    }
    return 0;
}

/* ---- BIFF records ---- */

static Int32 NextRecord(XlsReader *r)
{
    UInt8 h[4];
    r->pos = r->recEnd;
    if (r->pos + 4 > r->wbSize) return -1;
    if (WbRead(r, r->pos, h, 4) != 4) return -1;
    r->recType = Le16(h); r->recLen = Le16(h + 2);
    r->pos += 4;
    r->recEnd = r->pos + r->recLen;
    if (r->recEnd > r->wbSize) r->recEnd = r->wbSize;
    return r->recType;
}

/* Steps into a CONTINUE record if one follows the current record. */
static UInt8 Continue(XlsReader *r)
{
    UInt8 h[2];
    if (r->recEnd + 4 > r->wbSize) return 0;
    if (WbRead(r, r->recEnd, h, 2) != 2 || Le16(h) != rtContinue) return 0;
    NextRecord(r);
    return 1;
}

static UInt8 U8(XlsReader *r)
{
    UInt8 b = 0;
    if (r->pos < r->recEnd && WbRead(r, r->pos, &b, 1) == 1) r->pos++;
    else r->pos = r->recEnd;
    return b;
}
static UInt16 U16(XlsReader *r) { UInt16 v = U8(r); return (UInt16)(v | ((UInt16)U8(r) << 8)); }
static UInt32 U32(XlsReader *r) { UInt32 v = U16(r); return v | ((UInt32)U16(r) << 16); }
static void Bytes(XlsReader *r, UInt8 *buf, UInt16 n)
{
    UInt16 i;
    UInt16 have = r->pos < r->recEnd ? (UInt16)(r->recEnd - r->pos) : 0;
    if (have > n) have = n;
    if (have && WbRead(r, r->pos, buf, have) != have) have = 0;
    r->pos += have;
    for (i = have; i < n; i++) buf[i] = 0;
}
/* the same, carrying on into CONTINUE records */
static UInt8 C8(XlsReader *r) { if (r->pos >= r->recEnd && !Continue(r)) return 0; return U8(r); }
static UInt16 C16(XlsReader *r) { UInt16 v = C8(r); return (UInt16)(v | ((UInt16)C8(r) << 8)); }
static UInt32 C32(XlsReader *r) { UInt32 v = C16(r); return v | ((UInt32)C16(r) << 16); }
static void CSkip(XlsReader *r, UInt32 n)
{
    while (n) {
        UInt32 chunk;
        if (r->pos >= r->recEnd && !Continue(r)) return;
        chunk = r->recEnd - r->pos;
        if (chunk > n) chunk = n;
        r->pos += chunk; n -= chunk;
    }
}

/* A BIFF8 unicode string whose character count was already read: the flags byte,
 * optional rich-text and extension sizes, the characters (a split string restarts
 * with its own flags byte in the CONTINUE record), then the skipped extras. */
static UInt16 ReadUni(XlsReader *r, UInt16 cch, char *out, UInt16 cap)
{
    UInt8 flags = C8(r), high = flags & 1;
    UInt16 cRun = 0, i, n = 0;
    UInt32 cbExt = 0;
    if (flags & 8) cRun = C16(r);
    if (flags & 4) cbExt = C32(r);
    for (i = 0; i < cch; i++) {
        UInt16 u;
        if (r->pos >= r->recEnd) { if (!Continue(r)) break; high = U8(r) & 1; }
        if (high) { u = U8(r); u |= (UInt16)U8(r) << 8; } else u = U8(r);
        if (n + 1 < cap) out[n++] = (char)CsvUnicodeToPalm(u);
    }
    out[n] = 0;
    CSkip(r, (UInt32)cRun * 4);
    CSkip(r, cbExt);
    return n;
}

/* The shared string table goes to the temporary part as NUL-terminated Palm text. */
static Int32 ParseSst(XlsReader *r)
{
    UInt32 unique, i, off = 0;
    char *s = r->nextText;
    U32(r);                                  /* total references */
    unique = U32(r);
    if (r->tmpBegin(r->ctx, r->tmpPart)) return xsErrTmp;
    r->sstCount = 0;
    for (i = 0; i < unique; i++) {
        UInt16 cch, n;
        if (r->pos >= r->recEnd && !Continue(r)) break;
        cch = C16(r);
        n = ReadUni(r, cch, s, xsCellText);
        if (r->sstCount < r->sstCap) r->sstOffsets[r->sstCount++] = off;
        if (r->tmpWrite(r->ctx, (UInt8 *)s, (UInt32)n + 1)) { r->tmpEnd(r->ctx); return xsErrTmp; }
        off += (UInt32)n + 1;
    }
    r->tmpEnd(r->ctx);
    return 0;
}

static void SstGet(XlsReader *r, UInt32 index, char *out, UInt16 cap)
{
    Int32 got = 0;
    if (index < r->sstCount) got = r->tmpReadAt(r->ctx, r->tmpPart, r->sstOffsets[index], (UInt8 *)out, (UInt32)(cap - 1));
    if (got < 0) got = 0;
    out[got] = 0;
}

/* ---- numbers: value = m * 2^e, decoded to an integer part and six fraction digits ---- */

static void SetNumber(XlsReader *r, UInt8 neg, U64 m, Int16 e)
{
    U64 ip, f, mask;
    UInt32 frac = 0;
    Int16 s;
    UInt8 k;
    r->nextKind = 1; r->nextNeg = neg; r->nextBig = 0; r->nextInt = 0; r->nextFrac = 0;
    if (m == 0) return;
    if (e >= 0) {
        if (e > 10 || (m << e) > 0xFFFFFFFFULL) { r->nextBig = 1; return; }
        r->nextInt = (UInt32)(m << e);
        return;
    }
    s = (Int16)-e;
    if (s > 60) { if (s - 60 >= 64) m = 0; else m >>= (s - 60); s = 60; }
    mask = (1ULL << s) - 1;
    ip = m >> s; f = m & mask;
    for (k = 0; k < 7; k++) { f *= 10; frac = frac * 10 + (UInt32)(f >> s); f &= mask; }
    frac = (frac + 5) / 10;                   /* seven digits rounded to six */
    if (frac >= 1000000UL) { frac -= 1000000UL; ip++; }
    if (ip > 0xFFFFFFFFULL) { r->nextBig = 1; return; }
    r->nextInt = (UInt32)ip; r->nextFrac = frac;
}

static void DoubleToNumber(XlsReader *r, const UInt8 *d)
{
    UInt32 lo = Le32(d), hi = Le32(d + 4);
    UInt8 neg = (UInt8)((hi >> 31) & 1);
    Int16 ex = (Int16)((hi >> 20) & 0x7FF);
    U64 m = ((U64)(hi & 0xFFFFFUL) << 32) | lo;
    if (ex == 0x7FF) { SetNumber(r, neg, 0, 0); r->nextBig = 1; return; }   /* infinity or not a number */
    if (ex == 0) { SetNumber(r, neg, 0, 0); return; }                      /* zero or too small to matter */
    m |= 1ULL << 52;
    SetNumber(r, neg, m, (Int16)(ex - 1075));
}

static void RkToNumber(XlsReader *r, UInt32 rk)
{
    UInt8 x100 = (UInt8)(rk & 1);
    if (rk & 2) {
        Int32 v = (Int32)(rk & ~3UL) / 4;
        UInt32 mag = (UInt32)(v < 0 ? -v : v);
        r->nextKind = 1; r->nextNeg = v < 0; r->nextBig = 0;
        if (x100) { r->nextInt = mag / 100; r->nextFrac = (mag % 100) * 10000UL; }
        else { r->nextInt = mag; r->nextFrac = 0; }
    } else {
        UInt8 d[8];
        UInt32 hi = rk & ~3UL;
        d[0] = d[1] = d[2] = d[3] = 0;
        d[4] = (UInt8)hi; d[5] = (UInt8)(hi >> 8); d[6] = (UInt8)(hi >> 16); d[7] = (UInt8)(hi >> 24);
        DoubleToNumber(r, d);
        if (x100 && !r->nextBig) {
            UInt32 ip = r->nextInt, fr = r->nextFrac;
            r->nextInt = ip / 100; r->nextFrac = (ip % 100) * 10000UL + fr / 100;
        }
    }
}

/* ---- cells ---- */

static void CellHead(XlsReader *r) { r->nextRow = U16(r); r->nextCol = (Int16)U16(r); U16(r); }

/* The next cell in sheet order into the lookahead slot; 0 at the end of the sheet. */
static UInt8 FetchCell(XlsReader *r)
{
    for (;;) {
        Int32 t;
        if (r->mulLeft) {
            UInt32 rk;
            U16(r); rk = U32(r);
            r->nextCol = r->mulCol++; r->mulLeft--;
            RkToNumber(r, rk);
            return 1;
        }
        if (r->pushed) { r->pushed = 0; t = r->recType; r->pos = r->recEnd - r->recLen; }
        else t = NextRecord(r);
        if (t < 0 || t == rtEof) return 0;
        switch (t) {
        case rtLabelSst: { UInt32 isst; CellHead(r); isst = U32(r); r->nextKind = 0; SstGet(r, isst, r->nextText, xsCellText); return 1; }
        case rtNumber: { UInt8 d[8]; CellHead(r); Bytes(r, d, 8); DoubleToNumber(r, d); return 1; }
        case rtRk: { UInt32 rk; CellHead(r); rk = U32(r); RkToNumber(r, rk); return 1; }
        case rtMulRk:
            r->nextRow = U16(r); r->mulCol = (Int16)U16(r);
            r->mulLeft = r->recLen >= 6 ? (UInt16)((r->recLen - 6) / 6) : 0;
            continue;
        case rtLabel: { UInt16 cch; CellHead(r); cch = U16(r); r->nextKind = 0; ReadUni(r, cch, r->nextText, xsCellText); return 1; }
        case rtBoolErr: {
            UInt8 v, isErr;
            CellHead(r); v = U8(r); isErr = U8(r);
            r->nextKind = 0;
            Cp(r->nextText, isErr ? "" : (v ? "TRUE" : "FALSE"), xsCellText);
            return 1;
        }
        case rtFormula: {
            UInt8 d[8];
            CellHead(r); Bytes(r, d, 8);
            if (d[6] == 0xFF && d[7] == 0xFF) {
                r->nextKind = 0; r->nextText[0] = 0;
                if (d[0] == 1) Cp(r->nextText, d[2] ? "TRUE" : "FALSE", xsCellText);
                else if (d[0] == 0) {
                    /* the text is in the STRING record that follows (after any shared formula) */
                    UInt16 row = r->nextRow; Int16 col = r->nextCol;
                    for (;;) {
                        Int32 u = NextRecord(r);
                        if (u < 0) break;
                        if (u == rtString) { UInt16 cch = U16(r); ReadUni(r, cch, r->nextText, xsCellText); break; }
                        if (u != rtShrFmla && u != rtArray && u != rtTable) { r->pushed = 1; break; }
                    }
                    r->nextRow = row; r->nextCol = col;
                }
            } else DoubleToNumber(r, d);
            return 1;
        }
        default: continue;
        }
    }
}

/* Number text: ISO date or HH:MM in the date and time columns, else plain decimal. */
static void NumberText(XlsReader *r, Int16 col, char *out, UInt16 cap)
{
    UInt16 n = 0;
    char digits[12];
    UInt8 k, len;
    if (cap < 20) { out[0] = 0; return; }   /* "-4294967295.123456" and the NUL */
    if (r->nextBig) { Cp(out, "#", cap); return; }
    if (col == r->dateCol && col >= 0 && !r->nextNeg) {
        Int32 days = (Int32)r->nextInt - (r->date1904 ? 0 : 1462);
        UInt16 y; UInt8 m, d;
        if (days >= 0 && days <= 65535) {
            DateDaysToYmd((UInt16)days, &y, &m, &d);
            out[0] = (char)('0' + y / 1000); out[1] = (char)('0' + (y / 100) % 10); out[2] = (char)('0' + (y / 10) % 10); out[3] = (char)('0' + y % 10);
            out[4] = '-'; out[5] = (char)('0' + m / 10); out[6] = (char)('0' + m % 10); out[7] = '-'; out[8] = (char)('0' + d / 10); out[9] = (char)('0' + d % 10); out[10] = 0;
            return;
        }
    }
    if (col == r->timeCol && col >= 0 && !r->nextNeg) {
        UInt32 mins = (r->nextFrac * 1440UL + 500000UL) / 1000000UL;
        if (mins >= 1440) mins = 1439;
        out[0] = (char)('0' + mins / 600); out[1] = (char)('0' + (mins / 60) % 10); out[2] = ':';
        out[3] = (char)('0' + (mins % 60) / 10); out[4] = (char)('0' + mins % 10); out[5] = 0;
        return;
    }
    if (r->nextNeg && (r->nextInt || r->nextFrac)) out[n++] = '-';
    { UInt32 v = r->nextInt; len = 0; do { digits[len++] = (char)('0' + v % 10); v /= 10; } while (v); }
    while (len) out[n++] = digits[--len];
    if (r->nextFrac) {
        UInt32 f = r->nextFrac;
        out[n++] = '.';
        for (k = 0; k < 6; k++) { digits[k] = (char)('0' + f / 100000UL); f = (f % 100000UL) * 10; }
        len = 6;
        while (len && digits[len - 1] == '0') len--;
        for (k = 0; k < len; k++) out[n++] = digits[k];
    }
    out[n] = 0;
}

/* ---- the reader ---- */

Int32 XlsrOpen(XlsReader *r)
{
    UInt32 firstSheet = 0, fills = 0;
    Int32 e = OpenOle(r);
    if (e) return e;
    r->pos = 0; r->recEnd = 0; r->pushed = 0; r->sstCount = 0; r->date1904 = 0;
    r->dateCol = r->timeCol = -1;
    if (NextRecord(r) != rtBof || U16(r) != 0x0600) return xsErrNotBiff8;
    for (;;) {
        Int32 t = NextRecord(r);
        if (t < 0 || t == rtEof) break;
        if (t == rtDate1904) r->date1904 = U16(r) != 0;
        else if (t == rtBoundSheet) {
            UInt32 p = U32(r);
            UInt8 cch;
            char name[32];
            U16(r);                              /* visibility and type */
            cch = U8(r);
            ReadUni(r, cch, name, sizeof name);
            if (!firstSheet) firstSheet = p;
            if (!fills && EqI(name, "Fills")) fills = p;
        } else if (t == rtSst) { e = ParseSst(r); if (e) return e; }
    }
    if (!fills) fills = firstSheet;
    if (!fills) return xsErrNoSheet;
    r->pos = r->recEnd = fills;
    if (NextRecord(r) != rtBof) return xsErrNoSheet;
    r->hasNext = 0; r->eof = 0; r->mulLeft = 0; r->rowNo = 0;
    return 0;
}

void XlsrSetDateColumns(XlsReader *r, Int16 dateCol, Int16 timeCol) { r->dateCol = dateCol; r->timeCol = timeCol; }

Int32 XlsrNextRow(XlsReader *r, char *fields[], Int16 maxFields, char *buf, UInt16 cap)
{
    UInt16 used = 1, row;
    Int16 n = 0;
    if (cap < 2) return -1;
    buf[0] = 0;                                  /* the shared empty field */
    if (!r->hasNext) {
        if (r->eof || !FetchCell(r)) { r->eof = 1; return -1; }
        r->hasNext = 1;
    }
    row = r->nextRow;
    r->rowNo = (UInt32)row + 1;
    while (r->hasNext && r->nextRow == row) {
        Int16 c = r->nextCol;
        if (c >= 0 && c < maxFields && c >= n) {
            char *dst = buf + used;
            UInt16 room = (UInt16)(cap - used);
            while (n < c) fields[n++] = buf;     /* cells the row skipped */
            if (room > 1) {
                if (r->nextKind) NumberText(r, c, dst, room); else Cp(dst, r->nextText, room);
                fields[n++] = dst;
                used = (UInt16)(used + Len(dst) + 1);
            } else fields[n++] = buf;
        }
        if (!FetchCell(r)) { r->hasNext = 0; r->eof = 1; }
    }
    return n;
}

#include "xlsxread.h"
#include "csv.h"
#include "crc32.h"
#include "datecalc.h"
#ifdef XR_DEBUG
#include <stdio.h>
#endif

/* ---- byte reader over a temporary part ---- */

typedef struct XrCursor Cur;
static void TrSeek(Cur *c, UInt8 which, UInt32 pos) XP_SEC;
static Int16 TrGetc(XlsxReader *r, Cur *c) XP_SEC;
static Int32 Extract(XlsxReader *r, const char *name, UInt8 which, UInt32 *sizeOut) XP_SEC;
static Int32 ZipIn(void *ctx, UInt8 *buf, UInt32 len) XP_SEC;
static Int32 TmpOut(void *ctx, const UInt8 *buf, UInt32 len) XP_SEC;
static UInt16 ReadTag(XlsxReader *r, Cur *cu, char *name, UInt16 nameCap, char *attrs, UInt16 attrCap, UInt8 *selfClose, UInt8 *isEnd) XP_SEC;
static UInt16 ReadText(XlsxReader *r, Cur *cu, char *out, UInt16 cap) XP_SEC;
static void PutPalm(char *out, UInt16 *n, UInt16 cap, UInt16 u) XP_SEC;
static UInt16 Attr(const char *attrs, const char *key, char *out, UInt16 cap) XP_SEC;
static Int16 ColumnIndex(const char *ref) XP_SEC;
static UInt16 SstGet(XlsxReader *r, UInt32 index, char *out, UInt16 cap) XP_SEC;
static void SerialToText(XlsxReader *r, const char *v, UInt8 asTime, char *out, UInt16 cap) XP_SEC;
static UInt16 Cp(char *d, const char *s, UInt16 cap) XP_SEC;
static UInt8 StrEq(const char *a, const char *b) XP_SEC;

static UInt8 StrEq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }
static UInt16 Cp(char *d, const char *s, UInt16 cap) { UInt16 n = 0; while (s[n] && n + 1 < cap) { d[n] = s[n]; n++; } d[n] = 0; return n; }

static void TrSeek(Cur *c, UInt8 which, UInt32 pos)
{
    c->which = which; c->base = pos; c->len = 0; c->pos = 0;
}

static Int16 TrGetc(XlsxReader *r, Cur *c)
{
    if (c->pos >= c->len) {
        Int32 got = r->tmpReadAt(r->ctx, c->which, c->base, c->buf, sizeof c->buf);
        if (got <= 0) return -1;
        c->base += (UInt32)got; c->len = (UInt16)got; c->pos = 0;
    }
    return c->buf[c->pos++];
}

/* ---- extracting a zip entry to a temporary part ---- */

typedef struct { XlsxReader *r; UInt32 pos, left; } ZipCtx;
static ZipCtx gZc;

static Int32 ZipIn(void *ctx, UInt8 *buf, UInt32 len)
{
    ZipCtx *z = (ZipCtx *)ctx;
    Int32 got;
    if (z->left == 0) return 0;
    if (len > z->left) len = z->left;
    got = z->r->readAt(z->r->ctx, z->pos, buf, len);
    if (got <= 0) return got < 0 ? got : 0;
    z->pos += (UInt32)got; z->left -= (UInt32)got;
    return got;
}

static Int32 TmpOut(void *ctx, const UInt8 *buf, UInt32 len) { return ((ZipCtx *)ctx)->r->tmpWrite(((ZipCtx *)ctx)->r->ctx, buf, len); }

static Int32 Extract(XlsxReader *r, const char *name, UInt8 which, UInt32 *sizeOut)
{
    ZipEntry e;
    Int32 rc = ZipFind(r->readAt, r->ctx, r->fileSize, name, r->inBuf, r->inSize, &e);
    if (rc) return rc;
    if (r->tmpBegin(r->ctx, which)) return xrErrTmp;
    gZc.r = r; gZc.pos = e.dataOffset; gZc.left = e.csize;
    if (e.method == 0) {
        while (gZc.left) {
            Int32 got = ZipIn(&gZc, r->inBuf, r->inSize);
            if (got <= 0) { r->tmpEnd(r->ctx); return xrErrInflate; }
            if (r->tmpWrite(r->ctx, r->inBuf, (UInt32)got)) { r->tmpEnd(r->ctx); return xrErrTmp; }
        }
    } else {
        Inflate z;
        InflateInit(&z, ZipIn, TmpOut, &gZc, r->inBuf, r->inSize, r->window, r->crcTable);
        if (InflateRun(&z)) { r->tmpEnd(r->ctx); return xrErrInflate; }
    }
    if (r->tmpEnd(r->ctx)) return xrErrTmp;
    *sizeOut = e.usize;
    return 0;
}

/* ---- a small XML tokenizer: tags with their attribute text, and text runs ---- */

static void PutPalm(char *out, UInt16 *n, UInt16 cap, UInt16 u)
{
    if (*n + 1 < cap) out[(*n)++] = (char)CsvUnicodeToPalm(u);
}

/* Reads from the current position up to and including the next '>' once a '<' has been
 * consumed. Returns the attribute text length. */
static UInt16 ReadTag(XlsxReader *r, Cur *cu, char *name, UInt16 nameCap, char *attrs, UInt16 attrCap, UInt8 *selfClose, UInt8 *isEnd)
{
    Int16 c = TrGetc(r, cu);
    UInt16 n = 0, a = 0;
    *selfClose = 0; *isEnd = 0;
    if (c == '/') { *isEnd = 1; c = TrGetc(r, cu); }
    if (c == '?' || c == '!') {          /* declaration or comment: skip to '>' */
        while (c >= 0 && c != '>') c = TrGetc(r, cu);
        name[0] = 0; attrs[0] = 0; return 0;
    }
    while (c >= 0 && c != ' ' && c != '>' && c != '/' && c != '\n' && c != '\r' && c != '\t') { if (n + 1 < nameCap) name[n++] = (char)c; c = TrGetc(r, cu); }
    name[n] = 0;
    {
        char quote = 0;
        while (c >= 0 && (quote || c != '>')) {
            if (quote) { if (c == quote) quote = 0; if (a + 1 < attrCap) attrs[a++] = (char)c; }
            else if (c == '"' || c == '\'') { quote = (char)c; if (a + 1 < attrCap) attrs[a++] = (char)c; }
            else if (c == '/') { *selfClose = 1; }          /* only outside quotes: "/>" */
            else if (a + 1 < attrCap) attrs[a++] = (char)c;
            c = TrGetc(r, cu);
        }
    }
    attrs[a] = 0;
    return a;
}

/* Text up to the next '<' (consumed), entities decoded, UTF-8 folded to Palm Latin. */
static UInt16 ReadText(XlsxReader *r, Cur *cu, char *out, UInt16 cap)
{
    UInt16 n = 0;
    Int16 c;
    for (;;) {
        c = TrGetc(r, cu);
        if (c < 0 || c == '<') break;
        if (c == '&') {
            char ent[12]; UInt16 k = 0;
            while ((c = TrGetc(r, cu)) >= 0 && c != ';' && k < 11) ent[k++] = (char)c;
            ent[k] = 0;
            if (StrEq(ent, "amp")) PutPalm(out, &n, cap, '&');
            else if (StrEq(ent, "lt")) PutPalm(out, &n, cap, '<');
            else if (StrEq(ent, "gt")) PutPalm(out, &n, cap, '>');
            else if (StrEq(ent, "quot")) PutPalm(out, &n, cap, '"');
            else if (StrEq(ent, "apos")) PutPalm(out, &n, cap, '\'');
            else if (ent[0] == '#') {
                UInt32 v = 0; const char *p = ent + 1; UInt8 hex = 0;
                if (*p == 'x' || *p == 'X') { hex = 1; p++; }
                for (; *p; p++) v = hex ? v * 16 + (UInt32)(*p >= 'a' ? *p - 'a' + 10 : *p >= 'A' ? *p - 'A' + 10 : *p - '0') : v * 10 + (UInt32)(*p - '0');
                PutPalm(out, &n, cap, v > 0xFFFF ? '?' : (UInt16)v);
            }
            continue;
        }
        if (c >= 0x80) {
            UInt16 u = 0; Int16 extra = 0, k;
            if ((c & 0xE0) == 0xC0) { u = (UInt16)(c & 0x1F); extra = 1; }
            else if ((c & 0xF0) == 0xE0) { u = (UInt16)(c & 0x0F); extra = 2; }
            else if ((c & 0xF8) == 0xF0) { extra = 3; }
            for (k = 0; k < extra; k++) { Int16 cc = TrGetc(r, cu); if (cc < 0) break; u = (UInt16)((u << 6) | (cc & 0x3F)); }
            PutPalm(out, &n, cap, extra == 3 ? '?' : u);
            continue;
        }
        PutPalm(out, &n, cap, (UInt16)c);
    }
    out[n] = 0;
    return n;
}

/* The value of key="..." inside attrs; returns its length (0 when absent). */
static UInt16 Attr(const char *attrs, const char *key, char *out, UInt16 cap)
{
    const char *p = attrs;
    UInt16 kl = 0, n = 0;
    while (key[kl]) kl++;
    while (*p) {
        const char *k = key; const char *q = p;
        while (*k && *q == *k) { q++; k++; }
        if (!*k && *q == '=' && (p == attrs || p[-1] == ' ')) {
            char quote = q[1];
            q += 2;
            while (*q && *q != quote && n + 1 < cap) out[n++] = *q++;
            out[n] = 0;
            return n;
        }
        p++;
    }
    out[0] = 0;
    return 0;
}

static Int16 ColumnIndex(const char *ref)
{
    Int16 col = 0;
    while (*ref >= 'A' && *ref <= 'Z') { col = (Int16)(col * 26 + (*ref - 'A' + 1)); ref++; }
    return (Int16)(col - 1);
}

/* ---- shared strings ---- */

static UInt16 SstGet(XlsxReader *r, UInt32 index, char *out, UInt16 cap)
{
    char name[16], attrs[64], piece[256];
    UInt8 sc, end;
    UInt16 n = 0;
    out[0] = 0;
    if (!r->hasSst || index >= r->sstCount) return 0;
    TrSeek(&r->sst, xrTmpSst, r->sstOffsets[index]);
    /* at "<si>": collect every <t> text until </si> */
    for (;;) {
        Int16 c = TrGetc(r, &r->sst);
        if (c < 0) break;
        if (c != '<') continue;
        ReadTag(r, &r->sst, name, sizeof name, attrs, sizeof attrs, &sc, &end);
        if (end && StrEq(name, "si")) break;
        if (!end && !sc && StrEq(name, "t")) {
            UInt16 k = ReadText(r, &r->sst, piece, sizeof piece), i;
            for (i = 0; i < k && n + 1 < cap; i++) out[n++] = piece[i];
            out[n] = 0;
            /* ReadText consumed the '<' of </t>; skip that tag */
            ReadTag(r, &r->sst, name, sizeof name, attrs, sizeof attrs, &sc, &end);
        }
    }
    return n;
}

/* ---- open ---- */

Int32 XlsxrOpen(XlsxReader *r)
{
    char name[24], attrs[200], val[64], target[64], rid[16], sheetPath[80];
    UInt8 sc, end;
    Int32 rc;
    UInt32 size;
    Crc32Init(r->crcTable);
    r->sstCount = 0; r->hasSst = 0; r->date1904 = 0; r->dateCol = r->timeCol = -1; r->rowNo = 0;
    rid[0] = 0; sheetPath[0] = 0;
    /* workbook: date system and the Fills sheet's relationship id (else the first sheet) */
    rc = Extract(r, "xl/workbook.xml", xrTmpAux, &r->auxSize);
    if (rc) return xrErrNoSheet;
    TrSeek(&r->aux, xrTmpAux, 0);
    for (;;) {
        Int16 c = TrGetc(r, &r->aux);
        if (c < 0) break;
        if (c != '<') continue;
        ReadTag(r, &r->aux, name, sizeof name, attrs, sizeof attrs, &sc, &end);
        if (StrEq(name, "workbookPr")) { if (Attr(attrs, "date1904", val, sizeof val) && (val[0] == '1' || val[0] == 't')) r->date1904 = 1; }
        else if (StrEq(name, "sheet") && !end) {
            Attr(attrs, "name", val, sizeof val);
            if (!rid[0] || StrEq(val, "Fills")) { if (!Attr(attrs, "r:id", rid, sizeof rid)) Attr(attrs, "id", rid, sizeof rid); }
        }
    }
    if (!rid[0]) return xrErrNoSheet;
    rc = Extract(r, "xl/_rels/workbook.xml.rels", xrTmpAux, &r->auxSize);
    if (rc) return xrErrNoSheet;
    TrSeek(&r->aux, xrTmpAux, 0);
    for (;;) {
        Int16 c = TrGetc(r, &r->aux);
        if (c < 0) break;
        if (c != '<') continue;
        ReadTag(r, &r->aux, name, sizeof name, attrs, sizeof attrs, &sc, &end);
        if (StrEq(name, "Relationship") && Attr(attrs, "Id", val, sizeof val) && StrEq(val, rid)) {
            Attr(attrs, "Target", target, sizeof target);
            if (target[0] == '/') Cp(sheetPath, target + 1, sizeof sheetPath);
            else { Cp(sheetPath, "xl/", sizeof sheetPath); Cp(sheetPath + 3, target, sizeof sheetPath - 3); }
            break;
        }
    }
#ifdef XR_DEBUG
    fprintf(stderr, "rid=[%s] sheetPath=[%s]\n", rid, sheetPath);
#endif
    if (!sheetPath[0]) return xrErrNoSheet;
    rc = Extract(r, sheetPath, xrTmpSheet, &r->sheetSize);
    if (rc) return rc;
    if (Extract(r, "xl/sharedStrings.xml", xrTmpSst, &r->sstSize) == 0) {
        /* index: the position of each <si> */
        UInt32 pos = 0;
        Int16 c, prev = 0;
        r->hasSst = 1;
        TrSeek(&r->sst, xrTmpSst, 0);
        while ((c = TrGetc(r, &r->sst)) >= 0) {
            if (prev == '<' && c == 's') {
                Int16 c2 = TrGetc(r, &r->sst), c3 = TrGetc(r, &r->sst);
                if (c2 == 'i' && c3 == '>' && r->sstCount < r->sstCap) r->sstOffsets[r->sstCount++] = pos - 1;
                pos += 3; prev = c3; continue;
            }
            prev = c; pos++;
        }
    }
    TrSeek(&r->sheet, xrTmpSheet, 0);
    (void)size;
    return 0;
}

void XlsxrSetDateColumns(XlsxReader *r, Int16 dateCol, Int16 timeCol) { r->dateCol = dateCol; r->timeCol = timeCol; }

/* Excel serial text ("46232" or "0.5875") to "YYYY-MM-DD" or "HH:MM". */
static void SerialToText(XlsxReader *r, const char *v, UInt8 asTime, char *out, UInt16 cap)
{
    UInt32 whole = 0, frac = 0, scale = 1;
    UInt8 inFrac = 0;
    const char *start = v;
    for (; *v; v++) {
        if (*v >= '0' && *v <= '9') { if (!inFrac) whole = whole * 10 + (UInt32)(*v - '0'); else if (scale < 100000UL) { frac = frac * 10 + (UInt32)(*v - '0'); scale *= 10; } }
        else if (*v == '.') inFrac = 1;
        else { Cp(out, start, cap); return; }   /* not a plain number: leave it whole */
    }
    if (asTime) {
        UInt32 mins = (frac * 1440UL + scale / 2) / scale;
        UInt16 n = 0;
        if (mins >= 1440) mins = 1439;
        out[n++] = (char)('0' + mins / 600); out[n++] = (char)('0' + (mins / 60) % 10); out[n++] = ':';
        out[n++] = (char)('0' + (mins % 60) / 10); out[n++] = (char)('0' + mins % 10); out[n] = 0;
        return;
    }
    {
        Int32 days = (Int32)whole - (r->date1904 ? 0 : 1462);
        UInt16 y; UInt8 m, d;
        if (days < 0 || days > 65535) { Cp(out, v, cap); return; }
        DateDaysToYmd((UInt16)days, &y, &m, &d);
        out[0] = (char)('0' + y / 1000); out[1] = (char)('0' + (y / 100) % 10); out[2] = (char)('0' + (y / 10) % 10); out[3] = (char)('0' + y % 10);
        out[4] = '-'; out[5] = (char)('0' + m / 10); out[6] = (char)('0' + m % 10); out[7] = '-'; out[8] = (char)('0' + d / 10); out[9] = (char)('0' + d % 10); out[10] = 0;
    }
}

Int32 XlsxrNextRow(XlsxReader *r, char *fields[], Int16 maxFields, char *buf, UInt16 cap)
{
    char name[24], attrs[200], val[48], cellType[16], text[256], conv[16];
    UInt8 sc, end, inRow = 0, haveCell = 0, numeric = 0;
    Int16 col = -1, maxCol = -1, i;
    UInt16 used = 0;
    for (i = 0; i < maxFields; i++) fields[i] = buf;   /* "" by default: buf[0] is the empty string */
    if (cap) buf[0] = 0;
    used = 1;
    for (;;) {
        Int16 c = TrGetc(r, &r->sheet);
        if (c < 0) return inRow ? (Int32)(maxCol + 1) : -1;
        if (c != '<') continue;
        ReadTag(r, &r->sheet, name, sizeof name, attrs, sizeof attrs, &sc, &end);
        if (StrEq(name, "row")) {
            if (end) return maxCol + 1;
            if (Attr(attrs, "r", val, sizeof val)) { UInt32 rn = 0; const char *p = val; while (*p >= '0' && *p <= '9') rn = rn * 10 + (UInt32)(*p++ - '0'); r->rowNo = rn; }
            inRow = 1; maxCol = -1;
            if (sc) return 0;
            continue;
        }
        if (!inRow) continue;
        if (StrEq(name, "c")) {
            if (end) { haveCell = 0; continue; }
            if (Attr(attrs, "r", val, sizeof val)) col = ColumnIndex(val);
            else col = (Int16)(col + 1);                 /* no reference: the next column */
            cellType[0] = 0; Attr(attrs, "t", cellType, sizeof cellType);
            haveCell = !sc; numeric = cellType[0] == 0 || StrEq(cellType, "n");
            text[0] = 0;
            continue;
        }
        if (haveCell && !end && !sc && (StrEq(name, "v") || StrEq(name, "t"))) {
            UInt16 n = ReadText(r, &r->sheet, text, sizeof text);
            ReadTag(r, &r->sheet, name, sizeof name, attrs, sizeof attrs, &sc, &end);   /* the closing tag ReadText stopped at */
            if (col >= 0 && col < maxFields) {
                const char *out = text;
                if (StrEq(cellType, "s")) {
                    UInt32 idx = 0; const char *p = text;
                    while (*p >= '0' && *p <= '9') idx = idx * 10 + (UInt32)(*p++ - '0');
                    SstGet(r, idx, text, sizeof text); out = text;
                }
                if (numeric && (col == r->dateCol || col == r->timeCol)) { SerialToText(r, text, col == r->timeCol, conv, sizeof conv); out = conv; }
                n = 0; while (out[n]) n++;
                if (used + n + 1 <= cap) { fields[col] = buf + used; Cp(buf + used, out, (UInt16)(cap - used)); used += n + 1; }
                if (col > maxCol) maxCol = col;
            }
            continue;
        }
    }
}

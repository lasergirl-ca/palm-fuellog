/* Palm database files (pdbfile.h). Layout: a 78-byte header, an 8-byte
 * entry per record (offset, attribute, 24-bit id), two pad bytes, the
 * AppInfo block, then the records in order. */
#include "pdbfile.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       /* unlink: Windows CE's C library has no remove() */

#define kHeader 78

static UInt16 rd16(const UInt8 *p) { return (UInt16)((p[0] << 8) | p[1]); }
static UInt32 rd32(const UInt8 *p) { return ((UInt32)p[0] << 24) | ((UInt32)p[1] << 16) | ((UInt32)p[2] << 8) | p[3]; }
static void wr16(UInt8 *p, UInt16 v) { p[0] = (UInt8)(v >> 8); p[1] = (UInt8)v; }
static void wr32(UInt8 *p, UInt32 v) { p[0] = (UInt8)(v >> 24); p[1] = (UInt8)(v >> 16); p[2] = (UInt8)(v >> 8); p[3] = (UInt8)v; }

void PdbInit(Pdb *d, const char *name, UInt32 type, UInt32 creator)
{
    memset(d, 0, sizeof *d);
    strncpy(d->name, name, sizeof d->name - 1);
    d->attrs = pdbAttrBackup;
    d->version = 1;
    d->type = type;
    d->creator = creator;
}

void PdbFree(Pdb *d)
{
    UInt16 i;
    for (i = 0; i < d->count; i++) free(d->recs[i].data);
    free(d->recs);
    free(d->appInfo);
    d->recs = NULL; d->appInfo = NULL; d->count = d->cap = 0; d->appInfoLen = 0;
}

static Int32 Grow(Pdb *d)
{
    PdbRecord *r;
    UInt16 cap;
    if (d->count < d->cap) return pdbOk;
    if (d->count >= pdbMaxRecords) return pdbErrRange;
    cap = (UInt16)(d->cap ? d->cap * 2 : 32);
    r = (PdbRecord *)realloc(d->recs, (size_t)cap * sizeof *r);
    if (!r) return pdbErrMemory;
    d->recs = r; d->cap = cap;
    return pdbOk;
}

static UInt8 *Copy(const UInt8 *p, UInt32 len)
{
    UInt8 *c = (UInt8 *)malloc(len ? len : 1);
    if (c && len) memcpy(c, p, len);
    return c;
}

Int32 PdbSetAppInfo(Pdb *d, const UInt8 *p, UInt32 len)
{
    UInt8 *c = Copy(p, len);
    if (!c) return pdbErrMemory;
    free(d->appInfo);
    d->appInfo = c; d->appInfoLen = len;
    return pdbOk;
}

Int32 PdbInsert(Pdb *d, UInt16 index, const UInt8 *p, UInt32 len)
{
    UInt8 *c;
    Int32 e = Grow(d);
    if (e) return e;
    c = Copy(p, len);
    if (!c) return pdbErrMemory;
    if (index > d->count) index = d->count;
    memmove(d->recs + index + 1, d->recs + index, (size_t)(d->count - index) * sizeof *d->recs);
    d->recs[index].data = c; d->recs[index].len = len; d->recs[index].attr = 0x40;   /* dirty, as a new Palm record is */
    {
        /* One more than any id in use: the header's seed is not always kept current. */
        UInt32 top = d->idSeed & 0xFFFFFFUL;
        UInt16 i;
        for (i = 0; i < d->count + 1; i++) if (i != index && d->recs[i].id > top) top = d->recs[i].id;
        top = (top + 1) & 0xFFFFFFUL;
        if (top == 0) top = 1;
        d->recs[index].id = top;
        d->idSeed = top;
    }
    d->count++;
    return pdbOk;
}

Int32 PdbReplace(Pdb *d, UInt16 index, const UInt8 *p, UInt32 len)
{
    UInt8 *c;
    if (index >= d->count) return pdbErrRange;
    c = Copy(p, len);
    if (!c) return pdbErrMemory;
    free(d->recs[index].data);
    d->recs[index].data = c; d->recs[index].len = len; d->recs[index].attr |= 0x40;
    return pdbOk;
}

Int32 PdbRemove(Pdb *d, UInt16 index)
{
    if (index >= d->count) return pdbErrRange;
    free(d->recs[index].data);
    memmove(d->recs + index, d->recs + index + 1, (size_t)(d->count - index - 1) * sizeof *d->recs);
    d->count--;
    return pdbOk;
}

Int32 PdbLoad(Pdb *d, const char *path)
{
    FILE *f = fopen(path, "rb");
    UInt8 *buf = NULL;
    long size;
    UInt32 len, appOff, sortOff, first, end;
    UInt16 n, i;
    Int32 err = pdbErrMalformed;

    memset(d, 0, sizeof *d);
    if (!f) return pdbErrMissing;
    if (fseek(f, 0, SEEK_END) != 0 || (size = ftell(f)) < kHeader || size > 0x1000000L || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return pdbErrMalformed; }
    len = (UInt32)size;
    buf = (UInt8 *)malloc(len);
    if (!buf) { fclose(f); return pdbErrMemory; }
    if (fread(buf, 1, len, f) != len) { fclose(f); free(buf); return pdbErrMalformed; }
    fclose(f);

    memcpy(d->name, buf, 31);
    d->attrs = rd16(buf + 32); d->version = rd16(buf + 34);
    d->created = rd32(buf + 36); d->modified = rd32(buf + 40); d->backedUp = rd32(buf + 44); d->modNumber = rd32(buf + 48);
    appOff = rd32(buf + 52); sortOff = rd32(buf + 56);
    d->type = rd32(buf + 60); d->creator = rd32(buf + 64); d->idSeed = rd32(buf + 68);
    n = rd16(buf + 76);
    if (rd32(buf + 72) != 0 || n > pdbMaxRecords || (UInt32)kHeader + 8UL * n > len) goto bad;   /* one record list only */
    first = n ? rd32(buf + kHeader) : len;
    if (first > len) goto bad;
    if (appOff) {
        end = sortOff ? sortOff : first;
        if (appOff > end || end > len) goto bad;
        if ((err = PdbSetAppInfo(d, buf + appOff, end - appOff)) != pdbOk) goto bad;
        err = pdbErrMalformed;
    }
    if (n) {
        d->recs = (PdbRecord *)calloc(n, sizeof *d->recs);
        if (!d->recs) { err = pdbErrMemory; goto bad; }
        d->cap = n;
    }
    for (i = 0; i < n; i++) {
        const UInt8 *e = buf + kHeader + 8UL * i;
        UInt32 off = rd32(e), next = i + 1 < n ? rd32(e + 8) : len;
        if (off > next || next > len) goto bad;
        d->recs[i].data = Copy(buf + off, next - off);
        if (!d->recs[i].data) { err = pdbErrMemory; goto bad; }
        d->recs[i].len = next - off;
        d->recs[i].attr = e[4];
        d->recs[i].id = ((UInt32)e[5] << 16) | ((UInt32)e[6] << 8) | e[7];
        d->count++;
    }
    free(buf);
    return pdbOk;
bad:
    free(buf);
    PdbFree(d);
    return err;
}

Int32 PdbSave(const Pdb *d, const char *path)
{
    char tmp[300];
    UInt8 h[kHeader], e[8];
    UInt32 off;
    UInt16 i;
    FILE *f;
    size_t pl = strlen(path);
    int ok = 1;

    if (pl + 2 > sizeof tmp) return pdbErrWrite;
    memcpy(tmp, path, pl); tmp[pl] = '~'; tmp[pl + 1] = 0;
    f = fopen(tmp, "wb");
    if (!f) return pdbErrWrite;
    memset(h, 0, sizeof h);
    memcpy(h, d->name, 31);
    wr16(h + 32, d->attrs); wr16(h + 34, d->version);
    wr32(h + 36, d->created); wr32(h + 40, d->modified); wr32(h + 44, d->backedUp); wr32(h + 48, d->modNumber);
    off = (UInt32)kHeader + 8UL * d->count + 2;
    wr32(h + 52, d->appInfoLen ? off : 0);
    wr32(h + 60, d->type); wr32(h + 64, d->creator); wr32(h + 68, d->idSeed);
    wr16(h + 76, d->count);
    ok = ok && fwrite(h, 1, sizeof h, f) == sizeof h;
    off += d->appInfoLen;
    for (i = 0; i < d->count && ok; i++) {
        wr32(e, off);
        e[4] = d->recs[i].attr; e[5] = (UInt8)(d->recs[i].id >> 16); e[6] = (UInt8)(d->recs[i].id >> 8); e[7] = (UInt8)d->recs[i].id;
        ok = fwrite(e, 1, 8, f) == 8;
        off += d->recs[i].len;
    }
    e[0] = e[1] = 0;
    ok = ok && fwrite(e, 1, 2, f) == 2;
    if (d->appInfoLen) ok = ok && fwrite(d->appInfo, 1, d->appInfoLen, f) == d->appInfoLen;
    for (i = 0; i < d->count && ok; i++)
        if (d->recs[i].len) ok = fwrite(d->recs[i].data, 1, d->recs[i].len, f) == d->recs[i].len;
    if (fclose(f) != 0) ok = 0;
    if (!ok) { unlink(tmp); return pdbErrWrite; }
    unlink(path);                       /* rename does not replace a file on Windows CE */
    if (rename(tmp, path) != 0) return pdbErrWrite;
    return pdbOk;
}

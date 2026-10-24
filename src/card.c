#include "card.h"
#include "app.h"
#include "datecalc.h"

Err CardFind(UInt16 *vol)
{
    UInt32 iter = vfsIteratorStart;
    while (iter != vfsIteratorStop) {
        if (VFSVolumeEnumerate(vol, &iter) == errNone) return errNone;
    }
    return vfsErrVolumeBadRef;
}

Err CardEnsureDirs(UInt16 vol)
{
    static const char *dirs[] = { "/PALM", "/PALM/Programs", cardRoot, cardExport, cardBackup, cardImport };
    UInt16 i;
    for (i = 0; i < 6; i++) {
        Err err = VFSDirCreate(vol, dirs[i]);
        if (err != errNone && err != vfsErrFileAlreadyExists) return err;
    }
    return errNone;
}

void CardExportName(char *buf, const char *ext)
{
    UInt16 y; UInt8 m, d;
    DateDaysToYmd(AppToday(), &y, &m, &d);
    StrCopy(buf, "FuelLog-");
    StrIToA(buf + StrLen(buf), y);
    if (m < 10) StrCat(buf, "0"); StrIToA(buf + StrLen(buf), m);
    if (d < 10) StrCat(buf, "0"); StrIToA(buf + StrLen(buf), d);
    StrCat(buf, ext);
}

/* Section placement: see sections.h. */
static Err Flush(CardOut *c) IO_SEC;
static Int32 CardWrite(OutStream *s, const void *buf, UInt32 len) IO_SEC;
static Int32 CardSeek(OutStream *s, UInt32 pos) IO_SEC;

static Err Flush(CardOut *c)
{
    UInt32 wrote = 0;
    Err err;
    if (c->len == 0) return errNone;
    err = VFSFileWrite(c->f, c->len, c->buf, &wrote);
    if (!err && wrote != c->len) err = vfsErrFileGeneric;
    c->len = 0;
    if (err && !c->err) c->err = err;
    return err;
}

static Int32 CardWrite(OutStream *s, const void *buf, UInt32 len)
{
    CardOut *c = (CardOut *)s->ctx;
    const UInt8 *p = (const UInt8 *)buf;
    while (len) {
        UInt32 room = cardBufSize - c->len, n = len < room ? len : room;
        MemMove(c->buf + c->len, p, n);
        c->len += n; p += n; len -= n;
        if (c->len == cardBufSize && Flush(c)) return -1;
    }
    return 0;
}

static Int32 CardSeek(OutStream *s, UInt32 pos)
{
    CardOut *c = (CardOut *)s->ctx;
    if (Flush(c)) return -1;
    return VFSFileSeek(c->f, vfsOriginBeginning, (Int32)pos) == errNone ? 0 : -1;
}

Err CardOutOpen(CardOut *c, UInt16 vol, const char *path)
{
    Err err;
    MemSet(c, sizeof *c, 0);
    c->buf = MemPtrNew(cardBufSize);
    if (!c->buf) return memErrNotEnoughSpace;
    VFSFileDelete(vol, path);
    err = VFSFileOpen(vol, path, vfsModeWrite | vfsModeCreate | vfsModeTruncate, &c->f);
    if (err) { MemPtrFree(c->buf); c->buf = NULL; return err; }
    c->out.write = CardWrite; c->out.seek = CardSeek; c->out.ctx = c;
    return errNone;
}

Err CardOutClose(CardOut *c)
{
    Err err = Flush(c);
    if (c->f) VFSFileClose(c->f);
    if (c->buf) MemPtrFree(c->buf);
    c->f = 0; c->buf = NULL;
    if (c->out.err && !c->err) c->err = (Err)c->out.err;
    return c->err ? c->err : err;
}

Err CardInOpen(CardIn *c, UInt16 vol, const char *path)
{
    Err err;
    MemSet(c, sizeof *c, 0);
    c->buf = MemPtrNew(4096);
    if (!c->buf) return memErrNotEnoughSpace;
    err = VFSFileOpen(vol, path, vfsModeRead, &c->f);
    if (err) { MemPtrFree(c->buf); c->buf = NULL; }
    return err;
}

Int16 CardInGetc(CardIn *c)
{
    if (c->pos >= c->len) {
        UInt32 got = 0;
        if (c->eof) return -1;
        if (VFSFileRead(c->f, 4096, c->buf, &got) != errNone && got == 0) { c->eof = true; return -1; }
        if (got == 0) { c->eof = true; return -1; }
        c->len = got; c->pos = 0;
    }
    return c->buf[c->pos++];
}

void CardInClose(CardIn *c)
{
    if (c->f) VFSFileClose(c->f);
    if (c->buf) MemPtrFree(c->buf);
    c->f = 0; c->buf = NULL;
}

UInt16 CardList(UInt16 vol, const char *dir, Boolean folders, char names[][cardNameLen], UInt16 max)
{
    FileRef d;
    UInt32 iter = vfsIteratorStart;
    FileInfoType info;
    char name[cardNameLen];
    UInt16 n = 0, i, j;
    if (VFSFileOpen(vol, dir, vfsModeRead, &d) != errNone) return 0;
    info.nameP = name; info.nameBufLen = cardNameLen;
    while (iter != vfsIteratorStop && n < max) {
        if (VFSDirEntryEnumerate(d, &iter, &info) != errNone) break;
        if (((info.attributes & vfsFileAttrDirectory) != 0) != folders) continue;
        if (name[0] == '.') continue;
        /* insertion sort, descending, so dated names come newest first */
        for (i = n; i > 0 && StrCompare(names[i - 1], name) < 0; i--) StrCopy(names[i], names[i - 1]);
        StrCopy(names[i], name);
        n++;
    }
    VFSFileClose(d);
    (void)j;
    return n;
}

Err CardDeleteFolder(UInt16 vol, const char *dir)
{
    static char names[8][cardNameLen];
    char path[80];
    UInt16 n, i;
    do {
        n = CardList(vol, dir, false, names, 8);
        for (i = 0; i < n; i++) {
            StrCopy(path, dir); StrCat(path, "/"); StrCat(path, names[i]);
            VFSFileDelete(vol, path);
        }
    } while (n == 8);
    return VFSFileDelete(vol, dir);
}

UInt32 CardFileSize(UInt16 vol, const char *path)
{
    FileRef f;
    UInt32 size = 0;
    if (VFSFileOpen(vol, path, vfsModeRead, &f) == errNone) { VFSFileSize(f, &size); VFSFileClose(f); }
    return size;
}

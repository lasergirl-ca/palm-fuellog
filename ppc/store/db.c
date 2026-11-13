/* The FuelLog databases over .pdb files (db.h). Mirrors src/db.c: sorted
 * adds for fills and visits, the sync trailer stamped on add and replace,
 * tombstones for user deletions. */
#include "db.h"
#include "pdbfile.h"
#include <string.h>

#ifdef _WIN32_WCE
#define kSep '\\'
#else
#define kSep '/'
#endif

typedef struct {
    const char *name;
    UInt32 type;
    Int32 (*compare)(const UInt8 *a, const UInt8 *b);   /* NULL: kept in the order added */
} DbSpec;

static const DbSpec kSpec[dbAll] = {
    { "FuelLog-ZFlg", PdbTag('D', 'a', 't', 'a'), FillCompare },
    { "FuelVeh-ZFlg", PdbTag('V', 'e', 'h', 'i'), 0 },
    { "FuelPgm-ZFlg", PdbTag('P', 'g', 'm', 's'), 0 },
    { "FuelSvT-ZFlg", PdbTag('S', 'v', 'c', 'T'), 0 },
    { "FuelSvL-ZFlg", PdbTag('S', 'v', 'c', 'L'), ServiceVisitCompare },
    { "FuelDel-ZFlg", PdbTag('D', 'e', 'l', 'e'), 0 },
};
#define kCreator PdbTag('Z', 'F', 'l', 'g')

static Pdb gDb[dbAll];
static UInt8 gOpen, gChanged[dbAll], gNoStamp;
static char gDir[260];
static DbClock gClock;
static UInt32 gSeed;
static UInt8 gPacked[dbMaxRecord + 16];

static Int32 Err(Int32 pdbErr)
{
    switch (pdbErr) {
    case pdbOk: return dbOk;
    case pdbErrMemory: return dbErrMemory;
    case pdbErrRange: return dbErrRange;
    case pdbErrMalformed: return dbErrCorrupt;
    default: return dbErrIo;
    }
}

const char *DbName(UInt8 which) { return which < dbAll ? kSpec[which].name : ""; }

void DbPath(const char *dir, UInt8 which, char *path)
{
    size_t n = strlen(dir);
    if (n > 250) n = 250;
    memcpy(path, dir, n);
    path[n++] = kSep;
    strcpy(path + n, DbName(which));
    strcat(path, ".pdb");
}

static Int32 SetSchema(Pdb *d)
{
    AppInfoBlock a;
    UInt8 buf[appInfoSize];
    a.schemaVersion = recSchemaVersion; a.reserved = 0;
    AppInfoPack(&a, buf);
    return Err(PdbSetAppInfo(d, buf, appInfoSize));
}

Int32 DbOpenAll(const char *dir, const DbClock *clock)
{
    UInt8 i;
    char path[300];
    DbCloseAll();
    strncpy(gDir, dir, sizeof gDir - 1); gDir[sizeof gDir - 1] = 0;
    gClock = *clock;
    for (i = 0; i < dbAll; i++) {
        Int32 e;
        DbPath(gDir, i, path);
        e = PdbLoad(&gDb[i], path);
        gChanged[i] = 0;
        if (e == pdbErrMissing) {
            PdbInit(&gDb[i], kSpec[i].name, kSpec[i].type, kCreator);
            e = SetSchema(&gDb[i]);
            if (e) { gOpen = 1; DbCloseAll(); return e; }
            continue;                   /* written when something is first saved */
        }
        if (e) { gOpen = 1; DbCloseAll(); return Err(e); }
        if (gDb[i].appInfoLen) {
            AppInfoBlock a;
            if (AppInfoUnpack(gDb[i].appInfo, gDb[i].appInfoLen, &a) != 0) { gOpen = 1; DbCloseAll(); return dbErrCorrupt; }
            if (a.schemaVersion > recSchemaVersion) { gOpen = 1; DbCloseAll(); return dbErrNewerSchema; }
            if (a.schemaVersion != recSchemaVersion) { SetSchema(&gDb[i]); gChanged[i] = 1; }
        } else {
            /* Schema 0, no block: an empty or pre-release database with nothing to convert. */
            SetSchema(&gDb[i]); gChanged[i] = 1;
        }
    }
    gOpen = 1;
    return dbOk;
}

void DbCloseAll(void)
{
    UInt8 i;
    if (!gOpen) return;
    for (i = 0; i < dbAll; i++) PdbFree(&gDb[i]);
    gOpen = 0;
}

int DbDirty(void)
{
    UInt8 i;
    for (i = 0; i < dbAll; i++) if (gChanged[i]) return 1;
    return 0;
}

Int32 DbSave(void)
{
    UInt8 i;
    char path[300];
    Int32 first = dbOk;
    if (!gOpen) return dbErrIo;
    for (i = 0; i < dbAll; i++) {
        Int32 e;
        if (!gChanged[i]) continue;
        gDb[i].modified = DbNow();
        if (!gDb[i].created) gDb[i].created = gDb[i].modified;
        gDb[i].modNumber++;
        DbPath(gDir, i, path);
        e = PdbSave(&gDb[i], path);
        if (e) { if (!first) first = Err(e); continue; }
        gChanged[i] = 0;
    }
    return first;
}

UInt16 DbCount(UInt8 which) { return gOpen && which < dbAll ? gDb[which].count : 0; }

void DbSetStamping(int on) { gNoStamp = (UInt8)!on; }
UInt32 DbNow(void) { return gClock.now ? gClock.now() : 0; }

UInt32 DbNewUid(void)
{
    /* xorshift over the time and what came before; never 0 */
    UInt32 x = gSeed ^ DbNow() ^ 0x9E3779B9UL;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    gSeed = x + 0x6D2B79F5UL;
    return x ? x : 1;
}

/* The trailer written in place, as src/db.c does: a uid when there is none, modified = now. */
static void Stamp(UInt8 which, UInt8 *packed, UInt32 len)
{
    UInt32 uid, mod;
    if (gNoStamp || which >= dbCount) return;
    if (!RecTrailerRead(which, packed, len, &uid, &mod)) return;
    RecTrailerWrite(which, packed, len, uid ? uid : DbNewUid(), DbNow());
}

static Int32 Take(const UInt8 *packed, UInt32 len)
{
    if (len > dbMaxRecord) return dbErrRange;
    memcpy(gPacked, packed, len);
    return dbOk;
}

Int32 DbAdd(UInt8 which, const UInt8 *packed, UInt32 len, UInt16 *indexOut)
{
    Pdb *d;
    UInt16 pos;
    Int32 e;
    if (!gOpen || which >= dbAll) return dbErrRange;
    if ((e = Take(packed, len)) != dbOk) return e;
    Stamp(which, gPacked, len);
    d = &gDb[which];
    pos = d->count;
    if (kSpec[which].compare)          /* after the last record that does not sort later */
        while (pos > 0 && kSpec[which].compare(gPacked, d->recs[pos - 1].data) < 0) pos--;
    e = Err(PdbInsert(d, pos, gPacked, len));
    if (e) return e;
    gChanged[which] = 1;
    if (indexOut) *indexOut = pos;
    return dbOk;
}

Int32 DbAppend(UInt8 which, const UInt8 *packed, UInt32 len)
{
    Int32 e;
    if (!gOpen || which >= dbAll) return dbErrRange;
    if ((e = Take(packed, len)) != dbOk) return e;
    Stamp(which, gPacked, len);
    e = Err(PdbInsert(&gDb[which], gDb[which].count, gPacked, len));
    if (!e) gChanged[which] = 1;
    return e;
}

Int32 DbReplace(UInt8 which, UInt16 index, const UInt8 *packed, UInt32 len)
{
    Int32 e;
    if (!gOpen || which >= dbAll) return dbErrRange;
    if ((e = Take(packed, len)) != dbOk) return e;
    Stamp(which, gPacked, len);
    e = Err(PdbReplace(&gDb[which], index, gPacked, len));
    if (!e) gChanged[which] = 1;
    return e;
}

Int32 DbGet(UInt8 which, UInt16 index, UInt8 *buf, UInt32 bufLen, UInt32 *lenOut)
{
    const PdbRecord *r;
    if (!gOpen || which >= dbAll || index >= gDb[which].count) return dbErrRange;
    r = &gDb[which].recs[index];
    if (r->len > bufLen) return dbErrRange;
    memcpy(buf, r->data, r->len);
    if (lenOut) *lenOut = r->len;
    return dbOk;
}

Int32 DbRemove(UInt8 which, UInt16 index)
{
    Int32 e;
    if (!gOpen || which >= dbAll) return dbErrRange;
    e = Err(PdbRemove(&gDb[which], index));
    if (!e) gChanged[which] = 1;
    return e;
}

void DbSort(UInt8 which)
{
    Pdb *d;
    UInt16 i, j;
    if (!gOpen || which >= dbAll || !kSpec[which].compare) return;
    d = &gDb[which];
    for (i = 1; i < d->count; i++) {       /* insertion sort: stable, and the databases are nearly sorted */
        PdbRecord r = d->recs[i];
        for (j = i; j > 0 && kSpec[which].compare(r.data, d->recs[j - 1].data) < 0; j--) d->recs[j] = d->recs[j - 1];
        if (j != i) { d->recs[j] = r; gChanged[which] = 1; }
    }
}

Int32 DbTombstoneAdd(UInt8 which, UInt32 uid, UInt32 when)
{
    UInt8 t[dbTombstoneSize];
    Int32 e;
    if (!gOpen || !uid) return dbOk;
    t[0] = which;
    t[1] = (UInt8)(uid >> 24); t[2] = (UInt8)(uid >> 16); t[3] = (UInt8)(uid >> 8); t[4] = (UInt8)uid;
    t[5] = (UInt8)(when >> 24); t[6] = (UInt8)(when >> 16); t[7] = (UInt8)(when >> 8); t[8] = (UInt8)when;
    while (gDb[dbDeleted].count >= dbTombstoneMax) PdbRemove(&gDb[dbDeleted], 0);   /* the oldest go first */
    e = Err(PdbInsert(&gDb[dbDeleted], gDb[dbDeleted].count, t, sizeof t));
    gChanged[dbDeleted] = 1;
    return e;
}

Int32 DbTombstoneGet(UInt16 index, UInt8 *which, UInt32 *uid, UInt32 *when)
{
    const PdbRecord *r;
    if (!gOpen || index >= gDb[dbDeleted].count) return dbErrRange;
    r = &gDb[dbDeleted].recs[index];
    if (r->len < dbTombstoneSize) return dbErrCorrupt;
    *which = r->data[0];
    *uid = ((UInt32)r->data[1] << 24) | ((UInt32)r->data[2] << 16) | ((UInt32)r->data[3] << 8) | r->data[4];
    *when = ((UInt32)r->data[5] << 24) | ((UInt32)r->data[6] << 16) | ((UInt32)r->data[7] << 8) | r->data[8];
    return dbOk;
}

Int32 DbDelete(UInt8 which, UInt16 index)
{
    UInt32 uid = 0, mod = 0;
    if (!gOpen || which >= dbCount || index >= gDb[which].count) return dbErrRange;
    RecTrailerRead(which, gDb[which].recs[index].data, gDb[which].recs[index].len, &uid, &mod);
    DbTombstoneAdd(which, uid, DbNow());
    return DbRemove(which, index);
}

UInt16 DbEnsureUids(void)
{
    UInt8 w;
    UInt16 i, n = 0;
    if (!gOpen) return 0;
    for (w = 0; w < dbCount; w++) {
        for (i = 0; i < gDb[w].count; i++) {
            UInt32 uid, mod, len = gDb[w].recs[i].len;
            if (len > dbMaxRecord) continue;
            RecTrailerRead(w, gDb[w].recs[i].data, len, &uid, &mod);
            if (uid) continue;
            memcpy(gPacked, gDb[w].recs[i].data, len);
            len = RecTrailerWrite(w, gPacked, len, DbNewUid(), mod);
            if (PdbReplace(&gDb[w], i, gPacked, len) == pdbOk) { gChanged[w] = 1; n++; }
        }
    }
    return n;
}

Int32 DbAddFill(const FillRecord *r, UInt16 *indexOut)
{
    static UInt8 buf[dbMaxRecord + 16];
    return DbAdd(dbFills, buf, FillPack(r, buf), indexOut);
}

Int32 DbGetFill(UInt16 index, FillRecord *r)
{
    if (!gOpen || index >= gDb[dbFills].count) return dbErrRange;
    return FillUnpack(gDb[dbFills].recs[index].data, gDb[dbFills].recs[index].len, r) == 0 ? dbOk : dbErrCorrupt;
}

Int32 DbGetVehicle(UInt16 index, VehicleRecord *r)
{
    if (!gOpen || index >= gDb[dbVehicles].count) return dbErrRange;
    return VehicleUnpack(gDb[dbVehicles].recs[index].data, gDb[dbVehicles].recs[index].len, r) == 0 ? dbOk : dbErrCorrupt;
}

int DbVehicleById(UInt8 id, VehicleRecord *r)
{
    UInt16 i, n = DbCount(dbVehicles);
    for (i = 0; i < n; i++) if (DbGetVehicle(i, r) == dbOk && r->id == id) return 1;
    return 0;
}

Int32 DbGetProgramme(UInt16 index, ProgrammeRecord *r)
{
    if (!gOpen || index >= gDb[dbProgrammes].count) return dbErrRange;
    return ProgrammeUnpack(gDb[dbProgrammes].recs[index].data, gDb[dbProgrammes].recs[index].len, r) == 0 ? dbOk : dbErrCorrupt;
}

int DbProgrammeById(UInt8 id, ProgrammeRecord *r)
{
    UInt16 i, n = DbCount(dbProgrammes);
    for (i = 0; i < n; i++) if (DbGetProgramme(i, r) == dbOk && r->id == id) return 1;
    return 0;
}

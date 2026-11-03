#include "db.h"
#include "datecalc.h"
#include "app.h"

#define kCreator 'ZFlg'

typedef struct {
    const char *name;
    UInt32 type;
    Int32 (*compare)(const UInt8 *, const UInt8 *);
} DbSpec;

static const DbSpec kSpec[dbAll] = {
    { "FuelLog-ZFlg", 'Data', FillCompare },
    { "FuelVeh-ZFlg", 'Vehi', NULL },
    { "FuelPgm-ZFlg", 'Pgms', NULL },
    { "FuelSvT-ZFlg", 'SvcT', NULL },
    { "FuelSvL-ZFlg", 'SvcL', ServiceVisitCompare },
    { "FuelDel-ZFlg", 'Dele', NULL },
};

static DmOpenRef gRef[dbAll];
static UInt16 gVersion[dbAll];
static Boolean gNoStamp;

static Err WriteAppInfo(DmOpenRef db, UInt16 card, LocalID id, UInt16 version)
{
    LocalID infoId;
    MemHandle h;
    UInt8 packed[appInfoSize];
    AppInfoBlock a;
    Err err;

    a.schemaVersion = version;
    a.reserved = 0;
    AppInfoPack(&a, packed);
    err = DmDatabaseInfo(card, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &infoId, NULL, NULL, NULL);
    if (err) return err;
    if (infoId == 0) {
        /* First time: allocate the block in the database's storage heap and
         * attach it through the database header. */
        h = DmNewHandle(db, appInfoSize);
        if (!h) return memErrNotEnoughSpace;
        infoId = MemHandleToLocalID(h);
        err = DmSetDatabaseInfo(card, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &infoId, NULL, NULL, NULL);
        if (err) return err;
    } else {
        h = (MemHandle)MemLocalIDToGlobal(infoId, card);
    }
    err = DmWrite(MemHandleLock(h), 0, packed, appInfoSize);
    MemHandleUnlock(h);
    return err;
}

static Err ReadAppInfo(UInt16 card, LocalID id, UInt16 *version)
{
    LocalID infoId;
    MemHandle h;
    AppInfoBlock a;
    Err err = DmDatabaseInfo(card, id, NULL, NULL, NULL, NULL, NULL, NULL, NULL, &infoId, NULL, NULL, NULL);
    if (err) return err;
    if (infoId == 0) { *version = 0; return errNone; }
    h = (MemHandle)MemLocalIDToGlobal(infoId, card);
    if (AppInfoUnpack(MemHandleLock(h), MemHandleSize(h), &a) != 0) { MemHandleUnlock(h); return dbErrCorrupt; }
    MemHandleUnlock(h);
    *version = a.schemaVersion;
    return errNone;
}

static Err OpenOne(UInt8 which)
{
    const DbSpec *s = &kSpec[which];
    LocalID id = DmFindDatabase(0, s->name);
    Err err;
    UInt16 attrs, version;
    Boolean created = false;

    if (id == 0) {
        err = DmCreateDatabase(0, s->name, kCreator, s->type, false);
        if (err) return err;
        id = DmFindDatabase(0, s->name);
        DmDatabaseInfo(0, id, NULL, &attrs, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        attrs |= dmHdrAttrBackup;
        DmSetDatabaseInfo(0, id, NULL, &attrs, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
        version = recSchemaVersion;
        created = true;
    } else {
        err = ReadAppInfo(0, id, &version);
        if (err) return err;
        if (version > recSchemaVersion) return dbErrNewerSchema;
    }
    gVersion[which] = version;
    gRef[which] = DmOpenDatabase(0, id, dmModeReadWrite);
    if (!gRef[which]) return DmGetLastErr();
    if (created || version != recSchemaVersion) {
        /* A new database gets its AppInfo block now. Schema 0 (no block) is
         * an empty or pre-release database with nothing to convert; later
         * versions migrate records here before the version is raised. */
        err = WriteAppInfo(gRef[which], 0, id, recSchemaVersion);
        if (err) return err;
    }
    return errNone;
}

Err DbOpenAll(void)
{
    UInt8 i;
    Err err;
    for (i = 0; i < dbAll; i++) {
        err = OpenOne(i);
        if (err) { DbCloseAll(); return err; }
    }
    return errNone;
}

void DbCloseAll(void)
{
    UInt8 i;
    for (i = 0; i < dbAll; i++)
        if (gRef[i]) { DmCloseDatabase(gRef[i]); gRef[i] = 0; }
}

DmOpenRef DbRef(UInt8 which) { return gRef[which]; }
const char *DbName(UInt8 which) { return kSpec[which].name; }
UInt16 DbSchemaVersion(UInt8 which) { return gVersion[which]; }
UInt16 DbCount(UInt8 which) { return gRef[which] ? DmNumRecords(gRef[which]) : 0; }

static Int16 CompareRecords(void *a, void *b, Int16 other, SortRecordInfoPtr ia, SortRecordInfoPtr ib, MemHandle appInfo)
{
    /* The comparator is chosen through `other`, which carries the database index. */
    return (Int16)kSpec[other].compare((const UInt8 *)a, (const UInt8 *)b);
}

void DbSetStamping(Boolean on) { gNoStamp = !on; }

UInt32 DbNow(void) { return TimGetSeconds() + (UInt32)AppClockShift() * 86400UL; }

UInt32 DbNewUid(void)
{
    UInt32 u = ((UInt32)SysRandom(0) << 17) ^ ((UInt32)SysRandom(0) << 3) ^ TimGetSeconds() ^ (TimGetTicks() << 11);
    return u ? u : 1;
}

/* Stamp the trailer in place: Pack always wrote one, so the length holds. */
static void Stamp(UInt8 which, const UInt8 *packed, UInt32 len)
{
    UInt32 uid, mod;
    if (gNoStamp || which >= dbCount) return;
    if (!RecTrailerRead(which, packed, len, &uid, &mod)) return;
    RecTrailerWrite(which, (UInt8 *)packed, len, uid ? uid : DbNewUid(), DbNow());
}

Err DbAdd(UInt8 which, const UInt8 *packed, UInt32 len, UInt16 *indexOut)
{
    DmOpenRef db = gRef[which];
    UInt16 index;
    MemHandle h;
    Err err;

    if (!db) return dmErrNoOpenDatabase;
    Stamp(which, packed, len);
    if (kSpec[which].compare)
        index = DmFindSortPosition(db, (void *)packed, NULL, CompareRecords, which);
    else
        index = dmMaxRecordIndex;
    h = DmNewRecord(db, &index, len);
    if (!h) return DmGetLastErr();
    err = DmWrite(MemHandleLock(h), 0, packed, len);
    MemHandleUnlock(h);
    DmReleaseRecord(db, index, true);
    if (indexOut) *indexOut = index;
    return err;
}

Err DbReplace(UInt8 which, UInt16 index, const UInt8 *packed, UInt32 len)
{
    DmOpenRef db = gRef[which];
    MemHandle h;
    Err err;
    if (!db) return dmErrNoOpenDatabase;
    Stamp(which, packed, len);
    h = DmResizeRecord(db, index, len);
    if (!h) return DmGetLastErr();
    h = DmGetRecord(db, index);
    err = DmWrite(MemHandleLock(h), 0, packed, len);
    MemHandleUnlock(h);
    DmReleaseRecord(db, index, true);
    return err;
}

Err DbGet(UInt8 which, UInt16 index, UInt8 *buf, UInt32 bufLen, UInt32 *lenOut)
{
    DmOpenRef db = gRef[which];
    MemHandle h;
    UInt32 len;
    if (!db) return dmErrNoOpenDatabase;
    h = DmQueryRecord(db, index);
    if (!h) return dmErrIndexOutOfRange;
    len = MemHandleSize(h);
    if (len > bufLen) return dmErrMemError;
    MemMove(buf, MemHandleLock(h), len);
    MemHandleUnlock(h);
    if (lenOut) *lenOut = len;
    return errNone;
}

Err DbRemove(UInt8 which, UInt16 index)
{
    if (!gRef[which]) return dmErrNoOpenDatabase;
    return DmRemoveRecord(gRef[which], index);
}

/* Packed-record scratch kept off the 4 KB 68k stack. */
static UInt8 gPacked[dbMaxRecord];

Err DbAppend(UInt8 which, const UInt8 *packed, UInt32 len)
{
    DmOpenRef db = gRef[which];
    UInt16 index = dmMaxRecordIndex;
    MemHandle h;
    Err err;
    if (!db) return dmErrNoOpenDatabase;
    Stamp(which, packed, len);
    h = DmNewRecord(db, &index, len);
    if (!h) return DmGetLastErr();
    err = DmWrite(MemHandleLock(h), 0, packed, len);
    MemHandleUnlock(h);
    DmReleaseRecord(db, index, true);
    return err;
}

void DbSort(UInt8 which)
{
    if (gRef[which] && kSpec[which].compare) DmQuickSort(gRef[which], CompareRecords, which);
}

Err DbDelete(UInt8 which, UInt16 index)
{
    UInt32 len, uid, mod;
    if (!gRef[which]) return dmErrNoOpenDatabase;
    if (DbGet(which, index, gPacked, sizeof gPacked, &len) == errNone && RecTrailerRead(which, gPacked, len, &uid, &mod) && uid)
        DbTombstoneAdd(which, uid, DbNow());
    return DbRemove(which, index);
}

Err DbTombstoneAdd(UInt8 which, UInt32 uid, UInt32 when)
{
    UInt8 t[dbTombstoneSize];
    DmOpenRef db = gRef[dbDeleted];
    if (!db) return dmErrNoOpenDatabase;
    while (DmNumRecords(db) >= dbTombstoneMax) DmRemoveRecord(db, 0);
    t[0] = which;
    t[1] = (UInt8)(uid >> 24); t[2] = (UInt8)(uid >> 16); t[3] = (UInt8)(uid >> 8); t[4] = (UInt8)uid;
    t[5] = (UInt8)(when >> 24); t[6] = (UInt8)(when >> 16); t[7] = (UInt8)(when >> 8); t[8] = (UInt8)when;
    return DbAdd(dbDeleted, t, sizeof t, NULL);
}

Err DbTombstoneGet(UInt16 index, UInt8 *which, UInt32 *uid, UInt32 *when)
{
    UInt8 t[dbTombstoneSize];
    UInt32 len;
    Err err = DbGet(dbDeleted, index, t, sizeof t, &len);
    if (err) return err;
    if (len < dbTombstoneSize) return dbErrCorrupt;
    *which = t[0];
    *uid = ((UInt32)t[1] << 24) | ((UInt32)t[2] << 16) | ((UInt32)t[3] << 8) | t[4];
    *when = ((UInt32)t[5] << 24) | ((UInt32)t[6] << 16) | ((UInt32)t[7] << 8) | t[8];
    return errNone;
}

UInt16 DbEnsureUids(void)
{
    UInt8 db;
    UInt16 i, n, fixed = 0;
    UInt8 *buf = MemPtrNew(dbMaxRecord);
    if (!buf) return 0;
    DbSetStamping(false);
    for (db = 0; db < dbCount; db++) {
        n = DbCount(db);
        for (i = 0; i < n; i++) {
            UInt32 len, uid, mod;
            if (DbGet(db, i, buf, dbMaxRecord, &len) != errNone) continue;
            if (RecTrailerRead(db, buf, len, &uid, &mod) && uid) continue;
            len = RecTrailerWrite(db, buf, len, DbNewUid(), mod);
            if (DbReplace(db, i, buf, len) == errNone) fixed++;
        }
    }
    DbSetStamping(true);
    MemPtrFree(buf);
    return fixed;
}

Err DbAddFill(const FillRecord *r, UInt16 *indexOut)
{
    UInt32 len = FillPack(r, gPacked);
    return DbAdd(dbFills, gPacked, len, indexOut);
}

Err DbGetFill(UInt16 index, FillRecord *r)
{
    UInt32 len;
    Err err = DbGet(dbFills, index, gPacked, sizeof gPacked, &len);
    if (err) return err;
    return FillUnpack(gPacked, len, r) == 0 ? errNone : dbErrCorrupt;
}

Err DbAddVehicle(const VehicleRecord *r, UInt16 *indexOut)
{
    UInt8 buf[vehRecordSize + recTrailerSize];
    UInt32 n = VehiclePack(r, buf);
    return DbAdd(dbVehicles, buf, n, indexOut);
}

Err DbGetVehicle(UInt16 index, VehicleRecord *r)
{
    UInt8 buf[vehRecordSize + recTrailerSize];
    UInt32 len;
    Err err = DbGet(dbVehicles, index, buf, sizeof buf, &len);
    if (err) return err;
    return VehicleUnpack(buf, len, r) == 0 ? errNone : dbErrCorrupt;
}

UInt16 DbShiftClockDates(UInt16 deviceToday, UInt16 shift, UInt16 plausibleFrom)
{
    UInt16 n, i, moved = 0, fillsMoved = 0, visitsMoved = 0;
    UInt32 len;
    FillRecord *f;
    ServiceVisitRecord *v;
    UInt8 *buf;
    if (!gRef[dbFills] || !gRef[dbServiceVisits]) return 0;
    f = MemPtrNew(sizeof *f); v = MemPtrNew(sizeof *v); buf = MemPtrNew(svlFixedSize + fillMaxNotes + 16);
    if (!f || !v || !buf) { if (f) MemPtrFree(f); if (v) MemPtrFree(v); if (buf) MemPtrFree(buf); return 0; }
    DbSetStamping(false);          /* a moved or marked date is not an edit */
    n = DmNumRecords(gRef[dbFills]);
    for (i = 0; i < n; i++) {
        UInt16 d;
        if (DbGetFill(i, f) != errNone) continue;
        d = f->date;
        if (!DateEstablish(f->clockShift, &d, deviceToday, shift, plausibleFrom, &f->clockShift)) continue;
        if (d != f->date) fillsMoved++;
        f->date = d;
        len = FillPack(f, gPacked);
        DbReplace(dbFills, i, gPacked, len);
    }
    n = DmNumRecords(gRef[dbServiceVisits]);
    for (i = 0; i < n; i++) {
        UInt16 d;
        if (DbGet(dbServiceVisits, i, buf, svlFixedSize + fillMaxNotes + 16, &len) != errNone) continue;
        if (ServiceVisitUnpack(buf, len, v) != 0) continue;
        d = v->date;
        if (!DateEstablish(v->clockShift, &d, deviceToday, shift, plausibleFrom, &v->clockShift)) continue;
        if (d != v->date) visitsMoved++;
        v->date = d;
        len = ServiceVisitPack(v, buf);
        DbReplace(dbServiceVisits, i, buf, len);
    }
    if (fillsMoved) DmQuickSort(gRef[dbFills], CompareRecords, dbFills);
    if (visitsMoved) DmQuickSort(gRef[dbServiceVisits], CompareRecords, dbServiceVisits);
    moved = (UInt16)(fillsMoved + visitsMoved);
    DbSetStamping(true);
    MemPtrFree(f); MemPtrFree(v); MemPtrFree(buf);
    return moved;
}

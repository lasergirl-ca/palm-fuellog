#include "beam.h"
#include "export.h"
#include "card.h"
#include "csv.h"
#include "xlsx.h"
#include "pdf.h"
#include "app.h"
#include "db.h"
#include "datecalc.h"
#include "vehicles.h"
#include "programmes.h"
#include "resources.h"
#include <ExgMgr.h>

#define kCreator 'ZFlg'

/* ---- an OutStream that sends ---- */
typedef struct { OutStream out; ExgSocketType *sock; Err err; } ExgOut;
static ExgOut gEo;
static UInt8 gBuf[600];
static FillRecord gBFill;
static VehicleRecord gBVeh;
static ProgrammeRecord gBPgm;
static ServiceTypeRecord gBType;
static ServiceVisitRecord gBVisit;

/* Section placement: see sections.h. */
static Int32 ExgWrite(OutStream *s, const void *buf, UInt32 len) XP_SEC;
static Err OpenSocket(ExgSocketType *s, const char *name, const char *desc) XP_SEC;
static void Put16(UInt8 *p, UInt16 v) XP_SEC;
static UInt16 Get16(const UInt8 *p) XP_SEC;
static void Header(OutStream *out, UInt16 kind) XP_SEC;
static void PutRecord(OutStream *out, UInt8 db, const UInt8 *packed, UInt32 len) XP_SEC;
static UInt8 VehIdByName(const char *name) XP_SEC;
static UInt8 PgmIdByName(const char *name) XP_SEC;
static UInt8 TypeIdByName(UInt8 vehicle, const char *name, UInt32 *usedOut) XP_SEC;
static Boolean FillExists(const FillRecord *f) XP_SEC;
static Boolean VisitExists(const ServiceVisitRecord *v) XP_SEC;
static void ApplyRecord(UInt8 db, const UInt8 *packed, UInt32 len, Boolean merge, UInt16 *added) XP_SEC;
static void ClearLog(void) XP_SEC;

static Int32 ExgWrite(OutStream *s, const void *buf, UInt32 len)
{
    ExgOut *e = (ExgOut *)s->ctx;
    const UInt8 *p = (const UInt8 *)buf;
    Err err = errNone;
    while (len && !err) {
        UInt32 sent = ExgSend(e->sock, p, len, &err);
        if (err || sent == 0) { e->err = err ? err : exgErrAppError; return -1; }
        p += sent; len -= sent;
    }
    return 0;
}

static Err OpenSocket(ExgSocketType *s, const char *name, const char *desc)
{
    MemSet(s, sizeof *s, 0);
    s->description = (Char *)desc;
    s->type = (Char *)"application/x-fuellog";
    s->name = (Char *)name;        /* "?_send:..." lets the user pick IR or Bluetooth */
    s->target = kCreator;
    return ExgPut(s);
}

static void Put16(UInt8 *p, UInt16 v) { p[0] = (UInt8)(v >> 8); p[1] = (UInt8)v; }
static UInt16 Get16(const UInt8 *p) { return (UInt16)((p[0] << 8) | p[1]); }

static void Header(OutStream *out, UInt16 kind)
{
    UInt8 h[8];
    h[0] = 'F'; h[1] = 'U'; h[2] = 'E'; h[3] = 'L';
    Put16(h + 4, recSchemaVersion); Put16(h + 6, kind);
    OutWrite(out, h, 8);
}

/* One packed record: db, length, bytes. */
static void PutRecord(OutStream *out, UInt8 db, const UInt8 *packed, UInt32 len)
{
    UInt8 h[3];
    h[0] = db; Put16(h + 1, (UInt16)len);
    OutWrite(out, h, 3);
    OutWrite(out, packed, len);
}

Err BeamSendLog(UInt8 vehicle)
{
    ExgSocketType s;
    UInt16 n, i, count = 0;
    UInt8 db;
    Err err = OpenSocket(&s, "?_send:fuellog.fuel", "FuelLog log");
    if (err) return err;
    MemSet(&gEo, sizeof gEo, 0);
    gEo.out.write = ExgWrite; gEo.out.ctx = &gEo; gEo.sock = &s;
    Header(&gEo.out, beamKindRecords);
    /* count first so the receiver can size its buffer */
    for (db = 0; db < dbCount; db++) count += DbCount(db);
    { UInt8 c[2]; Put16(c, count); OutWrite(&gEo.out, c, 2); }
    for (db = dbVehicles; db <= dbServiceVisits && !gEo.err; db++) {
        n = DbCount(db);
        for (i = 0; i < n && !gEo.err; i++) {
            UInt32 len;
            if (DbGet(db, i, gBuf, sizeof gBuf, &len) != errNone) continue;
            if (vehicle != 255) {
                if (db == dbVehicles && gBuf[0] != vehicle) continue;
                if (db == dbServiceTypes && gBuf[1] != vehicle) continue;
                if (db == dbServiceVisits && gBuf[2] != vehicle) continue;
            }
            PutRecord(&gEo.out, db, gBuf, len);
        }
    }
    n = DbCount(dbFills);
    for (i = 0; i < n && !gEo.err; i++) {
        UInt32 len;
        if (DbGet(dbFills, i, gBuf, sizeof gBuf, &len) != errNone) continue;
        if (vehicle != 255 && gBuf[6] != vehicle) continue;
        PutRecord(&gEo.out, dbFills, gBuf, len);
    }
    err = gEo.err;
    ExgDisconnect(&s, err);
    return err;
}

Err BeamSendExport(UInt8 fmt, UInt8 vehicle, UInt16 year)
{
    ExgSocketType s;
    char name[40];
    UInt8 h[2];
    Err err;
    CardExportName(name, fmt == 1 ? ".xlsx" : fmt == 2 ? ".pdf" : ".csv");
    err = OpenSocket(&s, "?_send:fuellog.fuel", name);
    if (err) return err;
    MemSet(&gEo, sizeof gEo, 0);
    gEo.out.write = ExgWrite; gEo.out.ctx = &gEo; gEo.sock = &s;
    Header(&gEo.out, beamKindFile);
    h[0] = (UInt8)StrLen(name); h[1] = fmt;
    OutWrite(&gEo.out, h, 2);
    OutWrite(&gEo.out, name, StrLen(name));
    ExportToStream(&gEo.out, fmt, vehicle, year);
    err = gEo.err ? gEo.err : (Err)gEo.out.err;
    ExgDisconnect(&s, err);
    return err;
}

void BeamRegister(void)
{
    ExgRegisterDatatype(kCreator, exgRegExtensionID, "fuel", "FuelLog", 0);
    ExgRegisterDatatype(kCreator, exgRegTypeID, "application/x-fuellog", "FuelLog", 0);
}

/* ---- processing the inbox: merge or replace ---- */

static UInt8 gVehMap[17], gPgmMap[17];
static UInt8 gTypeMap[17][32];

static UInt8 VehIdByName(const char *name)
{
    UInt16 n = DbCount(dbVehicles), i;
    for (i = 0; i < n; i++) if (DbGetVehicle(i, &gBVeh) == errNone && StrCaselessCompare(gBVeh.name, name) == 0) return gBVeh.id;
    return 0;
}

static UInt8 PgmIdByName(const char *name)
{
    UInt16 n = DbCount(dbProgrammes), i;
    for (i = 0; i < n; i++) if (PgmGetAt(i, &gBPgm) == errNone && StrCaselessCompare(gBPgm.name, name) == 0) return gBPgm.id;
    return 0;
}

static UInt8 TypeIdByName(UInt8 vehicle, const char *name, UInt32 *usedOut)
{
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt32 used = 0, len;
    UInt8 found = 255;
    for (i = 0; i < n; i++) {
        if (DbGet(dbServiceTypes, i, gBuf, sizeof gBuf, &len) != errNone || ServiceTypeUnpack(gBuf, len, &gBType) != 0) continue;
        if (gBType.vehicle != vehicle) continue;
        if (gBType.id < 31) used |= 1UL << gBType.id;
        if (StrCaselessCompare(gBType.name, name) == 0) found = gBType.id;
    }
    *usedOut = used;
    return found;
}

static Boolean FillExists(const FillRecord *f)
{
    UInt16 n = DbCount(dbFills), i;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gBFill) != errNone) continue;
        if (gBFill.date > f->date) break;
        if (gBFill.date == f->date && gBFill.vehicle == f->vehicle && gBFill.odometer == f->odometer) return true;
    }
    return false;
}

static Boolean VisitExists(const ServiceVisitRecord *v)
{
    UInt16 n = DbCount(dbServiceVisits), i;
    UInt32 len;
    for (i = 0; i < n; i++) {
        if (DbGet(dbServiceVisits, i, gBuf, sizeof gBuf, &len) != errNone || ServiceVisitUnpack(gBuf, len, &gBVisit) != 0) continue;
        if (gBVisit.date == v->date && gBVisit.vehicle == v->vehicle && gBVisit.odometer == v->odometer) return true;
    }
    return false;
}

/* Apply one bundle record. merge = true remaps ids and skips duplicates. */
static void ApplyRecord(UInt8 db, const UInt8 *packed, UInt32 len, Boolean merge, UInt16 *added)
{
    static UInt8 out[600];
    UInt32 n;
    switch (db) {
    case dbVehicles:
        if (VehicleUnpack(packed, len, &gBVeh) != 0) return;
        if (merge) {
            UInt8 id = VehIdByName(gBVeh.name);
            if (id) { gVehMap[gBVeh.id & 15] = id; return; }
            id = VehNextId();
            if (!id) return;
            gVehMap[gBVeh.id & 15] = id; gBVeh.id = id;
        }
        n = VehiclePack(&gBVeh, out);
        if (DbAdd(dbVehicles, out, n, NULL) == errNone) (*added)++;
        break;
    case dbProgrammes:
        if (ProgrammeUnpack(packed, len, &gBPgm) != 0) return;
        if (merge) {
            UInt8 id = PgmIdByName(gBPgm.name);
            if (id) { gPgmMap[gBPgm.id & 15] = id; return; }
            id = PgmNextId();
            if (!id) return;
            gPgmMap[gBPgm.id & 15] = id; gBPgm.id = id;
        }
        n = ProgrammePack(&gBPgm, out);
        if (DbAdd(dbProgrammes, out, n, NULL) == errNone) (*added)++;
        break;
    case dbServiceTypes:
        if (ServiceTypeUnpack(packed, len, &gBType) != 0) return;
        if (merge) {
            UInt32 used; UInt8 id, old = gBType.id, v = gVehMap[gBType.vehicle & 15];
            if (!v) return;
            gBType.vehicle = v;
            id = TypeIdByName(v, gBType.name, &used);
            if (id != 255) { gTypeMap[v][old & 31] = id; return; }
            for (id = 0; id < 31 && (used & (1UL << id)); id++) ;
            if (id == 31) return;
            gTypeMap[v][old & 31] = id; gBType.id = id;
        }
        n = ServiceTypePack(&gBType, out);
        if (DbAdd(dbServiceTypes, out, n, NULL) == errNone) (*added)++;
        break;
    case dbServiceVisits:
        if (ServiceVisitUnpack(packed, len, &gBVisit) != 0) return;
        if (merge) {
            UInt32 mask = 0; UInt8 b, v = gVehMap[gBVisit.vehicle & 15];
            if (!v) return;
            gBVisit.vehicle = v;
            for (b = 0; b < 31; b++) if (gBVisit.services & (1UL << b)) mask |= 1UL << gTypeMap[v][b];
            gBVisit.services = mask | (gBVisit.services & svlOtherBit);
            if (VisitExists(&gBVisit)) return;
        }
        n = ServiceVisitPack(&gBVisit, out);
        if (DbAdd(dbServiceVisits, out, n, NULL) == errNone) (*added)++;
        break;
    case dbFills:
        if (FillUnpack(packed, len, &gBFill) != 0) return;
        if (merge) {
            UInt8 k, v = gVehMap[gBFill.vehicle & 15];
            if (!v) return;
            gBFill.vehicle = v;
            for (k = 0; k < gBFill.pointCount; k++) gBFill.points[k].programme = gPgmMap[gBFill.points[k].programme & 15];
            if (FillExists(&gBFill)) return;
        }
        if (DbAddFill(&gBFill, NULL) == errNone) (*added)++;
        break;
    default: break;
    }
}

static void ClearLog(void)
{
    UInt8 db;
    for (db = 0; db < dbCount; db++) while (DbCount(db)) DbRemove(db, 0);
}

Boolean BeamProcessInbox(void)
{
    LocalID id = DmFindDatabase(0, beamInboxName);
    DmOpenRef inbox;
    UInt16 n, i, added = 0;
    Boolean changed = false;
    if (!id) return false;
    inbox = DmOpenDatabase(0, id, dmModeReadWrite);
    if (!inbox) return false;
    n = DmNumRecords(inbox);
    for (i = 0; i < n; i++) {
        MemHandle h = DmQueryRecord(inbox, i);
        const UInt8 *p, *end;
        UInt16 count, kind, choice;
        char what[40];
        if (!h) continue;
        p = MemHandleLock(h); end = p + MemHandleSize(h);
        if (end - p < 10 || p[0] != 'F' || p[1] != 'U' || p[2] != 'E' || p[3] != 'L') { MemHandleUnlock(h); continue; }
        if (Get16(p + 4) > recSchemaVersion) { MemHandleUnlock(h); FrmAlert(NewerBackupAlert); continue; }
        kind = Get16(p + 6);
        if (kind != beamKindRecords) { MemHandleUnlock(h); continue; }
        count = Get16(p + 8);
        StrIToA(what, count); StrCat(what, count == 1 ? " record" : " records");
        choice = FrmCustomAlert(ReceivedBundleAlert, what, "", "");
        if (choice == 2) { MemHandleUnlock(h); continue; }
        if (choice == 1) ClearLog();
        {
            Boolean merge = choice == 0;
            UInt8 k;
            for (k = 0; k < 17; k++) { gVehMap[k] = merge ? 0 : k; gPgmMap[k] = merge ? 0 : k; MemSet(gTypeMap[k], 32, 0); }
            if (!merge) { UInt8 v, b; for (v = 0; v < 17; v++) for (b = 0; b < 32; b++) gTypeMap[v][b] = b; }
            p += 10;
            while (p + 3 <= end) {
                UInt8 db = p[0]; UInt16 len = Get16(p + 1);
                p += 3;
                if (p + len > end) break;
                ApplyRecord(db, p, len, merge, &added);
                p += len;
            }
        }
        MemHandleUnlock(h);
        changed = true;
    }
    DmCloseDatabase(inbox);
    DmDeleteDatabase(0, id);
    if (changed) {
        char a[12];
        StrIToA(a, added);
        FrmCustomAlert(BeamDoneAlert, a[0] ? "Received data: records added" : "Received data", "", "");
        (void)a;
    }
    return changed;
}

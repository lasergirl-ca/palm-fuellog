#include "sync.h"
#include "records.h"

/* ---- rules ---- */

Int32 SyncNewer(UInt32 localModified, UInt32 inModified) { return inModified > localModified ? 1 : 0; }
Int32 SyncDeletes(UInt32 localModified, UInt32 deletedAt) { return localModified <= deletedAt ? 1 : 0; }
Int32 SyncRefused(UInt32 inModified, UInt32 deletedAt) { return deletedAt >= inModified ? 1 : 0; }

/* ---- framing ---- */

static UInt16 Rd16(const UInt8 *p) SYNC_SEC;
static UInt16 Rd16(const UInt8 *p) { return (UInt16)((p[0] << 8) | p[1]); }

Int32 SyncOpen(SyncReader *r, const UInt8 *bundle, UInt32 len, UInt16 *kind, UInt16 *count)
{
    if (len < syncHeaderSize || bundle[0] != 'F' || bundle[1] != 'U' || bundle[2] != 'E' || bundle[3] != 'L') return -1;
    if (Rd16(bundle + 4) > recSchemaVersion) return -2;
    *kind = Rd16(bundle + 6);
    *count = Rd16(bundle + 8);
    r->p = bundle + syncHeaderSize;
    r->end = bundle + len;
    return 0;
}

Int32 SyncNext(SyncReader *r, UInt8 *db, const UInt8 **rec, UInt16 *len)
{
    if (r->end - r->p < 3) return 0;
    *db = r->p[0];
    *len = Rd16(r->p + 1);
    if ((UInt32)(r->end - r->p - 3) < *len) return -1;
    *rec = r->p + 3;
    r->p += 3 + (UInt32)*len;
    return 1;
}

/* ---- device: apply ---- */
#ifdef __palmos__
#include "db.h"
#include "vehicles.h"
#include "programmes.h"

typedef struct { UInt32 uid, mod, odo; UInt16 index, date; UInt8 veh, gone; } Entry;
typedef struct { Entry *e; UInt16 n; } Table;
typedef struct { UInt32 uid, when; } Tomb;

/* Everything lives on the heap: the data segment is full. */
typedef struct {
    Table  t[dbCount];
    UInt8  vehMap[17], pgmMap[17];
    UInt8  typeMap[17][32];
    UInt32 typeUsed[17];               /* type ids handed out in this apply, per local vehicle */
    Tomb  *tomb;
    UInt16 tombN;
    UInt8 *buf;
    FillRecord fill;
    VehicleRecord veh, veh2;
    ProgrammeRecord pgm, pgm2;
    ServiceTypeRecord svt, svt2;
    ServiceVisitRecord svl;
    Boolean sortFills, sortVisits;
    SyncStats st;
} Ctx;

static Err    BuildTable(Ctx *c, UInt8 db) SYNC_SEC;
static Entry *FindUid(Table *t, UInt32 uid) SYNC_SEC;
static Entry *FindKey(Table *t, UInt8 veh, UInt16 date, UInt32 odo) SYNC_SEC;
static Boolean Refused(Ctx *c, UInt32 uid, UInt32 mod) SYNC_SEC;
static void   AdoptUid(Ctx *c, UInt8 db, Entry *e, UInt32 uid) SYNC_SEC;
static Boolean VehUsed(Ctx *c, UInt8 id) SYNC_SEC;
static Boolean PgmUsed(Ctx *c, UInt8 id) SYNC_SEC;
static void   ApplyVehicle(Ctx *c, const UInt8 *rec, UInt16 len) SYNC_SEC;
static void   ApplyProgramme(Ctx *c, const UInt8 *rec, UInt16 len) SYNC_SEC;
static void   ApplyType(Ctx *c, const UInt8 *rec, UInt16 len) SYNC_SEC;
static void   ApplyVisit(Ctx *c, const UInt8 *rec, UInt16 len) SYNC_SEC;
static void   ApplyFill(Ctx *c, const UInt8 *rec, UInt16 len) SYNC_SEC;
static void   ApplyTombstone(Ctx *c, const UInt8 *rec, UInt16 len) SYNC_SEC;
static void   Finish(Ctx *c) SYNC_SEC;

static Err BuildTable(Ctx *c, UInt8 db)
{
    UInt16 n = DbCount(db), i;
    Entry *e = NULL;
    if (n) { e = MemPtrNew((UInt32)n * sizeof(Entry)); if (!e) return memErrNotEnoughSpace; }
    for (i = 0; i < n; i++) {
        UInt32 len;
        Entry *x = &e[i];
        x->index = i; x->gone = 0; x->uid = 0; x->mod = 0; x->date = 0; x->odo = 0; x->veh = 0;
        if (DbGet(db, i, c->buf, dbMaxRecord, &len) != errNone) continue;
        RecTrailerRead(db, c->buf, len, &x->uid, &x->mod);
        if (db == dbFills && len >= fillHeaderSize) {
            x->date = Rd16(c->buf); x->veh = c->buf[6];
            x->odo = ((UInt32)c->buf[8] << 24) | ((UInt32)c->buf[9] << 16) | ((UInt32)c->buf[10] << 8) | c->buf[11];
        } else if (db == dbServiceVisits && len >= svlFixedSize) {
            x->date = Rd16(c->buf); x->veh = c->buf[2];
            x->odo = ((UInt32)c->buf[3] << 24) | ((UInt32)c->buf[4] << 16) | ((UInt32)c->buf[5] << 8) | c->buf[6];
        }
    }
    c->t[db].e = e; c->t[db].n = n;
    return errNone;
}

static Entry *FindUid(Table *t, UInt32 uid)
{
    UInt16 i;
    if (!uid) return NULL;
    for (i = 0; i < t->n; i++) if (t->e[i].uid == uid) return &t->e[i];
    return NULL;
}

static Entry *FindKey(Table *t, UInt8 veh, UInt16 date, UInt32 odo)
{
    UInt16 i;
    for (i = 0; i < t->n; i++) if (t->e[i].veh == veh && t->e[i].date == date && t->e[i].odo == odo) return &t->e[i];
    return NULL;
}

static Boolean Refused(Ctx *c, UInt32 uid, UInt32 mod)
{
    UInt16 i;
    for (i = 0; i < c->tombN; i++) if (c->tomb[i].uid == uid && SyncRefused(mod, c->tomb[i].when)) return true;
    return false;
}

/* The local record matched by its natural key takes the incoming uid. */
static void AdoptUid(Ctx *c, UInt8 db, Entry *e, UInt32 uid)
{
    UInt32 len;
    if (!uid || e->uid == uid) return;
    if (DbGet(db, e->index, c->buf, dbMaxRecord, &len) != errNone) return;
    len = RecTrailerWrite(db, c->buf, len, uid, e->mod);
    if (DbReplace(db, e->index, c->buf, len) == errNone) e->uid = uid;
}

static Boolean VehUsed(Ctx *c, UInt8 id) { return VehGet(id, &c->veh2); }

static Boolean PgmUsed(Ctx *c, UInt8 id)
{
    UInt16 n = DbCount(dbProgrammes), i;
    for (i = 0; i < n; i++) if (PgmGetAt(i, &c->pgm2) == errNone && c->pgm2.id == id) return true;
    return false;
}

static void ApplyVehicle(Ctx *c, const UInt8 *rec, UInt16 len)
{
    VehicleRecord *v = &c->veh;
    Entry *e;
    UInt8 inId;
    UInt32 n;
    if (VehicleUnpack(rec, len, v) != 0) return;
    inId = v->id;
    e = FindUid(&c->t[dbVehicles], v->uid);
    if (!e) {
        UInt16 i;
        for (i = 0; i < c->t[dbVehicles].n && !e; i++)
            if (DbGetVehicle(c->t[dbVehicles].e[i].index, &c->veh2) == errNone && StrCaselessCompare(c->veh2.name, v->name) == 0) e = &c->t[dbVehicles].e[i];
    }
    if (e) {
        if (DbGetVehicle(e->index, &c->veh2) != errNone) return;
        c->vehMap[inId & 15] = c->veh2.id;
        if (SyncNewer(e->mod, v->modified)) {
            v->id = c->veh2.id;
            n = VehiclePack(v, c->buf);
            if (DbReplace(dbVehicles, e->index, c->buf, n) == errNone) { e->mod = v->modified; e->uid = v->uid; c->st.updated++; }
        } else AdoptUid(c, dbVehicles, e, v->uid);
        return;
    }
    if (Refused(c, v->uid, v->modified)) return;
    v->id = (inId == 0 || inId > vehMaxVehicles || VehUsed(c, inId)) ? VehNextId() : inId;
    if (!v->id) return;
    c->vehMap[inId & 15] = v->id;
    n = VehiclePack(v, c->buf);
    if (DbAdd(dbVehicles, c->buf, n, NULL) == errNone) c->st.added++;
}

static void ApplyProgramme(Ctx *c, const UInt8 *rec, UInt16 len)
{
    ProgrammeRecord *p = &c->pgm;
    Entry *e;
    UInt8 inId;
    UInt32 n;
    if (ProgrammeUnpack(rec, len, p) != 0) return;
    inId = p->id;
    e = FindUid(&c->t[dbProgrammes], p->uid);
    if (!e) {
        UInt16 i;
        for (i = 0; i < c->t[dbProgrammes].n && !e; i++)
            if (PgmGetAt(c->t[dbProgrammes].e[i].index, &c->pgm2) == errNone && StrCaselessCompare(c->pgm2.name, p->name) == 0) e = &c->t[dbProgrammes].e[i];
    }
    if (e) {
        if (PgmGetAt(e->index, &c->pgm2) != errNone) return;
        c->pgmMap[inId & 15] = c->pgm2.id;
        if (SyncNewer(e->mod, p->modified)) {
            p->id = c->pgm2.id;
            n = ProgrammePack(p, c->buf);
            if (DbReplace(dbProgrammes, e->index, c->buf, n) == errNone) { e->mod = p->modified; e->uid = p->uid; c->st.updated++; }
        } else AdoptUid(c, dbProgrammes, e, p->uid);
        return;
    }
    if (Refused(c, p->uid, p->modified)) return;
    p->id = (inId == 0 || inId > pgmMaxProgrammes || PgmUsed(c, inId)) ? PgmNextId() : inId;
    if (!p->id) return;
    c->pgmMap[inId & 15] = p->id;
    n = ProgrammePack(p, c->buf);
    if (DbAdd(dbProgrammes, c->buf, n, NULL) == errNone) c->st.added++;
}

static void ApplyType(Ctx *c, const UInt8 *rec, UInt16 len)
{
    ServiceTypeRecord *t = &c->svt;
    Entry *e;
    UInt8 inId, v;
    UInt32 used = 0, n;
    UInt16 i;
    if (ServiceTypeUnpack(rec, len, t) != 0) return;
    inId = t->id;
    v = c->vehMap[t->vehicle & 15];
    if (!v) return;
    t->vehicle = v;
    e = FindUid(&c->t[dbServiceTypes], t->uid);
    /* ids in use for this vehicle, and the name match when the uid is unknown */
    for (i = 0; i < c->t[dbServiceTypes].n; i++) {
        UInt32 l;
        Entry *x = &c->t[dbServiceTypes].e[i];
        if (DbGet(dbServiceTypes, x->index, c->buf, dbMaxRecord, &l) != errNone || ServiceTypeUnpack(c->buf, l, &c->svt2) != 0) continue;
        if (c->svt2.vehicle != v) continue;
        if (c->svt2.id < 31) used |= 1UL << c->svt2.id;
        if (!e && StrCaselessCompare(c->svt2.name, t->name) == 0) e = x;
    }
    used |= c->typeUsed[v];            /* types this apply already added are not in the table */
    if (e) {
        UInt32 l;
        if (DbGet(dbServiceTypes, e->index, c->buf, dbMaxRecord, &l) != errNone || ServiceTypeUnpack(c->buf, l, &c->svt2) != 0) return;
        c->typeMap[v][inId & 31] = c->svt2.id;
        if (SyncNewer(e->mod, t->modified)) {
            t->id = c->svt2.id; t->vehicle = c->svt2.vehicle;
            n = ServiceTypePack(t, c->buf);
            if (DbReplace(dbServiceTypes, e->index, c->buf, n) == errNone) { e->mod = t->modified; e->uid = t->uid; c->st.updated++; }
        } else AdoptUid(c, dbServiceTypes, e, t->uid);
        return;
    }
    if (Refused(c, t->uid, t->modified)) return;
    if (inId >= 31 || (used & (1UL << inId))) { for (inId = 0; inId < 31 && (used & (1UL << inId)); inId++) ; if (inId == 31) return; }
    c->typeMap[v][t->id & 31] = inId;
    t->id = inId;
    n = ServiceTypePack(t, c->buf);
    if (DbAdd(dbServiceTypes, c->buf, n, NULL) == errNone) { c->st.added++; c->typeUsed[v] |= 1UL << inId; }
}

static void ApplyVisit(Ctx *c, const UInt8 *rec, UInt16 len)
{
    ServiceVisitRecord *s = &c->svl;
    Entry *e;
    UInt8 v, b;
    UInt32 mask = 0, n;
    if (ServiceVisitUnpack(rec, len, s) != 0) return;
    v = c->vehMap[s->vehicle & 15];
    if (!v) return;
    s->vehicle = v;
    for (b = 0; b < 31; b++) if ((s->services & (1UL << b)) && c->typeMap[v][b] != 0xFF) mask |= 1UL << c->typeMap[v][b];
    s->services = mask | (s->services & svlOtherBit);
    e = FindUid(&c->t[dbServiceVisits], s->uid);
    if (!e) e = FindKey(&c->t[dbServiceVisits], v, s->date, s->odometer);
    if (e) {
        if (SyncNewer(e->mod, s->modified)) {
            n = ServiceVisitPack(s, c->buf);
            if (DbReplace(dbServiceVisits, e->index, c->buf, n) == errNone) { e->mod = s->modified; e->uid = s->uid; e->date = s->date; e->odo = s->odometer; c->st.updated++; c->sortVisits = true; }
        } else AdoptUid(c, dbServiceVisits, e, s->uid);
        return;
    }
    if (Refused(c, s->uid, s->modified)) return;
    n = ServiceVisitPack(s, c->buf);
    if (DbAppend(dbServiceVisits, c->buf, n) == errNone) { c->st.added++; c->sortVisits = true; }
}

static void ApplyFill(Ctx *c, const UInt8 *rec, UInt16 len)
{
    FillRecord *f = &c->fill;
    Entry *e;
    UInt8 v, k, kept = 0;
    UInt32 n;
    if (FillUnpack(rec, len, f) != 0) return;
    v = c->vehMap[f->vehicle & 15];
    if (!v) return;
    f->vehicle = v;
    for (k = 0; k < f->pointCount; k++) {
        UInt8 id = c->pgmMap[f->points[k].programme & 15];
        if (!id) continue;
        f->points[kept] = f->points[k]; f->points[kept].programme = id; kept++;
    }
    f->pointCount = kept;
    e = FindUid(&c->t[dbFills], f->uid);
    if (!e) e = FindKey(&c->t[dbFills], v, f->date, f->odometer);
    if (e) {
        if (SyncNewer(e->mod, f->modified)) {
            n = FillPack(f, c->buf);
            if (DbReplace(dbFills, e->index, c->buf, n) == errNone) { e->mod = f->modified; e->uid = f->uid; e->date = f->date; e->odo = f->odometer; c->st.updated++; c->sortFills = true; }
        } else AdoptUid(c, dbFills, e, f->uid);
        return;
    }
    if (Refused(c, f->uid, f->modified)) return;
    n = FillPack(f, c->buf);
    if (DbAppend(dbFills, c->buf, n) == errNone) { c->st.added++; c->sortFills = true; }
}

static void ApplyTombstone(Ctx *c, const UInt8 *rec, UInt16 len)
{
    UInt8 db;
    UInt32 uid, when;
    UInt16 i;
    Entry *e;
    if (len < dbTombstoneSize) return;
    db = rec[0];
    uid = ((UInt32)rec[1] << 24) | ((UInt32)rec[2] << 16) | ((UInt32)rec[3] << 8) | rec[4];
    when = ((UInt32)rec[5] << 24) | ((UInt32)rec[6] << 16) | ((UInt32)rec[7] << 8) | rec[8];
    if (db >= dbCount || !uid) return;
    e = FindUid(&c->t[db], uid);
    if (e && !e->gone && SyncDeletes(e->mod, when)) { e->gone = 1; c->st.deleted++; }
    for (i = 0; i < c->tombN; i++) if (c->tomb[i].uid == uid) return;
    if (c->tombN < dbTombstoneMax && DbTombstoneAdd(db, uid, when) == errNone) { c->tomb[c->tombN].uid = uid; c->tomb[c->tombN].when = when; c->tombN++; }
}

/* Deletions last, highest index first, so the table's indices stay true; then the sorts. */
static void Finish(Ctx *c)
{
    UInt8 db;
    for (db = 0; db < dbCount; db++) {
        UInt16 i = c->t[db].n;
        while (i--) if (c->t[db].e[i].gone) DbRemove(db, c->t[db].e[i].index);
    }
    if (c->sortFills) DbSort(dbFills);
    if (c->sortVisits) DbSort(dbServiceVisits);
}

Err SyncApply(const UInt8 *bundle, UInt32 len, SyncStats *st)
{
    Ctx *c;
    SyncReader r;
    UInt16 kind, count, i, n;
    UInt8 db, k;
    Err err = errNone;
    Int32 rc;
    rc = SyncOpen(&r, bundle, len, &kind, &count);
    if (rc == -2) return dbErrNewerSchema;
    if (rc != 0 || kind != syncKindSync) return dbErrCorrupt;
    c = MemPtrNew(sizeof *c);
    if (!c) return memErrNotEnoughSpace;
    MemSet(c, sizeof *c, 0);
    c->buf = MemPtrNew(dbMaxRecord);
    c->tomb = MemPtrNew(dbTombstoneMax * sizeof(Tomb));
    if (!c->buf || !c->tomb) err = memErrNotEnoughSpace;
    for (k = 0; k < 17 && !err; k++) MemSet(c->typeMap[k], 32, 0xFF);
    for (db = 0; db < dbCount && !err; db++) err = BuildTable(c, db);
    n = DbCount(dbDeleted);
    for (i = 0; i < n && !err && c->tombN < dbTombstoneMax; i++) {
        UInt8 which;
        if (DbTombstoneGet(i, &which, &c->tomb[c->tombN].uid, &c->tomb[c->tombN].when) == errNone) c->tombN++;
    }
    if (!err) {
        const UInt8 *rec;
        UInt16 rlen;
        DbSetStamping(false);
        while (SyncNext(&r, &db, &rec, &rlen) == 1) {
            switch (db) {
            case dbVehicles:      ApplyVehicle(c, rec, rlen); break;
            case dbProgrammes:    ApplyProgramme(c, rec, rlen); break;
            case dbServiceTypes:  ApplyType(c, rec, rlen); break;
            case dbServiceVisits: ApplyVisit(c, rec, rlen); break;
            case dbFills:         ApplyFill(c, rec, rlen); break;
            case dbDeleted:       ApplyTombstone(c, rec, rlen); break;
            default: break;
            }
        }
        Finish(c);
        DbSetStamping(true);
    }
    *st = c->st;
    for (db = 0; db < dbCount; db++) if (c->t[db].e) MemPtrFree(c->t[db].e);
    if (c->buf) MemPtrFree(c->buf);
    if (c->tomb) MemPtrFree(c->tomb);
    MemPtrFree(c);
    return err;
}
#endif

/* The FuelLog databases off the Palm: the same calls the Palm build's
 * src/db.h declares, over .pdb files in one folder (pdbfile.h) instead of
 * the Palm's data manager. Opened whole at start, written back with
 * DbSave when something changed. Plain C, host-tested. */
#ifndef PPC_DB_H
#define PPC_DB_H
#include "../../src/ptypes.h"
#include "../../src/records.h"

#define dbFills     0
#define dbVehicles  1
#define dbProgrammes 2
#define dbServiceTypes 3
#define dbServiceVisits 4
#define dbCount 5            /* the record databases; the sync trailer kinds of records.h */
#define dbDeleted 5          /* tombstones for sync: db, uid, when deleted */
#define dbAll 6
#define dbTombstoneSize 9
#define dbTombstoneMax 500
#define dbMaxRecord (svlFixedSize + fillMaxNotes + recTrailerSize)

#define dbOk             0
#define dbErrNewerSchema 1   /* the data was written by a newer FuelLog */
#define dbErrCorrupt     2
#define dbErrIo          3
#define dbErrMemory      4
#define dbErrRange       5

/* What the store needs from the device: the time (real seconds since 1904)
 * and the days its clock is behind real time, for the records' trailers. */
typedef struct {
    UInt32 (*now)(void);
    UInt16 clockShift;
} DbClock;

/* Opens the six databases in dir (no trailing separator); a missing file
 * is an empty database, created on the first save. */
Int32  DbOpenAll(const char *dir, const DbClock *clock);
void   DbCloseAll(void);
Int32  DbSave(void);                 /* writes every database changed since it was loaded or saved */
int    DbDirty(void);
UInt16 DbCount(UInt8 which);
const char *DbName(UInt8 which);     /* "FuelLog-ZFlg" */
/* The file of one database in dir, into path (at least 300 bytes). */
void   DbPath(const char *dir, UInt8 which, char *path);

Int32 DbAdd(UInt8 which, const UInt8 *packed, UInt32 len, UInt16 *indexOut);
Int32 DbReplace(UInt8 which, UInt16 index, const UInt8 *packed, UInt32 len);
Int32 DbGet(UInt8 which, UInt16 index, UInt8 *buf, UInt32 bufLen, UInt32 *lenOut);
Int32 DbRemove(UInt8 which, UInt16 index);
Int32 DbAppend(UInt8 which, const UInt8 *packed, UInt32 len);
void  DbSort(UInt8 which);
Int32 DbDelete(UInt8 which, UInt16 index);         /* a user deletion: tombstone, then remove */
void  DbSetStamping(int on);
UInt32 DbNow(void);
UInt32 DbNewUid(void);
Int32 DbTombstoneAdd(UInt8 which, UInt32 uid, UInt32 when);
Int32 DbTombstoneGet(UInt16 index, UInt8 *which, UInt32 *uid, UInt32 *when);
UInt16 DbEnsureUids(void);

Int32 DbAddFill(const FillRecord *r, UInt16 *indexOut);
Int32 DbGetFill(UInt16 index, FillRecord *r);
Int32 DbGetVehicle(UInt16 index, VehicleRecord *r);
/* The vehicle with this id; 0 when there is none. */
int   DbVehicleById(UInt8 id, VehicleRecord *r);

#endif

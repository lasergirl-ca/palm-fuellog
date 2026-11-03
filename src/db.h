/* The FuelLog databases on the device: the five record databases plus the
 * deletion log, created, opened, version-checked, and accessed through the
 * packed layouts in records.h. Device only. */
#ifndef DB_H
#define DB_H
#include <PalmOS.h>
#include "records.h"

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
#define dbMaxRecord (svlFixedSize + fillMaxNotes + recTrailerSize)   /* the largest record, a visit */

#define dbErrNewerSchema  (appErrorClass | 1)   /* the data was written by a newer FuelLog */
#define dbErrCorrupt      (appErrorClass | 2)

/* Opens every database, creating any that is missing with the current
 * schema version and the backup bit, and migrating older schemas. */
Err  DbOpenAll(void);
void DbCloseAll(void);
DmOpenRef DbRef(UInt8 which);
UInt16 DbSchemaVersion(UInt8 which);   /* as read at open, before migration */
UInt16 DbCount(UInt8 which);
const char *DbName(UInt8 which);     /* "FuelLog-ZFlg" */

/* Packed record access. Add keeps the fill and visit databases sorted.
 * While stamping is on (the default), Add and Replace write the sync
 * trailer in place: a uid when the record has none, and modified = now.
 * Sync, the clock-shift migration and the uid pass turn it off so that
 * records keep the stamps they came with. */
Err DbAdd(UInt8 which, const UInt8 *packed, UInt32 len, UInt16 *indexOut);
Err DbReplace(UInt8 which, UInt16 index, const UInt8 *packed, UInt32 len);
Err DbGet(UInt8 which, UInt16 index, UInt8 *buf, UInt32 bufLen, UInt32 *lenOut);
Err DbRemove(UInt8 which, UInt16 index);           /* no tombstone: edits remove and re-add */
Err DbAppend(UInt8 which, const UInt8 *packed, UInt32 len);   /* at the end, unsorted: sync adds, then DbSort */
void DbSort(UInt8 which);
Err DbDelete(UInt8 which, UInt16 index);           /* a user deletion: tombstone, then remove */
void DbSetStamping(Boolean on);
UInt32 DbNow(void);                                /* real seconds since 1904, clock shift applied */
UInt32 DbNewUid(void);
/* Tombstones, oldest first; Add drops the oldest past dbTombstoneMax. */
Err DbTombstoneAdd(UInt8 which, UInt32 uid, UInt32 when);
Err DbTombstoneGet(UInt16 index, UInt8 *which, UInt32 *uid, UInt32 *when);
/* Gives a uid to every record that has none (modified stays 0). Returns how many. */
UInt16 DbEnsureUids(void);

/* Typed convenience wrappers. */
Err DbAddFill(const FillRecord *r, UInt16 *indexOut);
Err DbGetFill(UInt16 index, FillRecord *r);
Err DbAddVehicle(const VehicleRecord *r, UInt16 *indexOut);
Err DbGetVehicle(UInt16 index, VehicleRecord *r);

/* Fills and service visits dated by a clock set 28 years back (see
 * DateUnshift) are moved to their real dates and the databases re-sorted.
 * A no-op when shift is 0. Returns how many records moved. */
UInt16 DbShiftClockDates(UInt16 deviceToday, UInt16 shift);

#endif

/* The five FuelLog databases on the device: create, open, version-check,
 * and record access through the packed layouts in records.h. Device only. */
#ifndef DB_H
#define DB_H
#include <PalmOS.h>
#include "records.h"

#define dbFills     0
#define dbVehicles  1
#define dbProgrammes 2
#define dbServiceTypes 3
#define dbServiceVisits 4
#define dbCount 5

#define dbErrNewerSchema  (appErrorClass | 1)   /* the data was written by a newer FuelLog */
#define dbErrCorrupt      (appErrorClass | 2)

/* Opens every database, creating any that is missing with the current
 * schema version and the backup bit, and migrating older schemas. */
Err  DbOpenAll(void);
void DbCloseAll(void);
DmOpenRef DbRef(UInt8 which);
UInt16 DbSchemaVersion(UInt8 which);   /* as read at open, before migration */
UInt16 DbCount(UInt8 which);

/* Packed record access. Add keeps the fill and visit databases sorted. */
Err DbAdd(UInt8 which, const UInt8 *packed, UInt32 len, UInt16 *indexOut);
Err DbReplace(UInt8 which, UInt16 index, const UInt8 *packed, UInt32 len);
Err DbGet(UInt8 which, UInt16 index, UInt8 *buf, UInt32 bufLen, UInt32 *lenOut);
Err DbRemove(UInt8 which, UInt16 index);

/* Typed convenience wrappers. */
Err DbAddFill(const FillRecord *r, UInt16 *indexOut);
Err DbGetFill(UInt16 index, FillRecord *r);
Err DbAddVehicle(const VehicleRecord *r, UInt16 *indexOut);
Err DbGetVehicle(UInt16 index, VehicleRecord *r);

#endif

/* Sync between two Palms: a bundle of every record with its sync trailer
 * (uid, modified) plus the deletion log, applied so that both logs end up
 * the same. Identity is the uid; a record without a known uid is matched by
 * the natural key the Merge uses (name, or vehicle + date + odometer) and
 * adopts the incoming uid. The newer edit wins. The rules and the framing
 * are plain C and host-tested; SyncApply is device only. */
#ifndef SYNC_H
#define SYNC_H
#include "ptypes.h"
#include "sections.h"

#define syncKindRecords 1      /* a Log bundle: Merge or Replace */
#define syncKindFile    2      /* an export inside a file envelope */
#define syncKindSync    3      /* this: records with trailers, then tombstones as db 5 */
#define syncHeaderSize 10      /* "FUEL", schema, kind, count */

/* Replace the local record? Newer wins; equal, older, and unknown against unknown keep local. */
Int32 SyncNewer(UInt32 localModified, UInt32 inModified) SYNC_SEC;
/* A deletion applies unless the local copy was edited after it. */
Int32 SyncDeletes(UInt32 localModified, UInt32 deletedAt) SYNC_SEC;
/* An incoming record is refused when it was deleted here at least as recently as it was edited. */
Int32 SyncRefused(UInt32 inModified, UInt32 deletedAt) SYNC_SEC;

typedef struct { const UInt8 *p, *end; } SyncReader;
/* Checks the header. 0 = ok with kind and count filled in, -1 = not a bundle, -2 = a newer schema. */
Int32 SyncOpen(SyncReader *r, const UInt8 *bundle, UInt32 len, UInt16 *kind, UInt16 *count) SYNC_SEC;
/* The next framed record: 1 with db, rec and len; 0 at the end; -1 when a length runs past the end. */
Int32 SyncNext(SyncReader *r, UInt8 *db, const UInt8 **rec, UInt16 *len) SYNC_SEC;

#ifdef __palmos__
#include <PalmOS.h>
typedef struct { UInt16 added, updated, deleted; } SyncStats;
/* Applies a kind 3 bundle (header included) to the open databases. */
Err SyncApply(const UInt8 *bundle, UInt32 len, SyncStats *st) SYNC_SEC;
#endif

#endif

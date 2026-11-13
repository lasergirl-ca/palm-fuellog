/* Sync through a .fuel file (syncfile.c): the Palm build's sync bundle and
 * its apply, ported over the Pocket PC store. A Palm writes the same file
 * to Sync/ on its card with Export, Sync, SD card, and reads one with
 * Choose a file. Plain C, host-tested. */
#ifndef SYNCFILE_H
#define SYNCFILE_H
#include "../../src/ptypes.h"
#include "../../src/outstream.h"

#define syncOk            0
#define syncErrFolder     1    /* the Sync folder could not be made or the file opened */
#define syncErrWrite      2
#define syncErrRead       3
#define syncErrNotBundle  4    /* not a FuelLog sync bundle */
#define syncErrNewer      5    /* written by a newer FuelLog */
#define syncErrMemory     6

typedef struct { UInt16 added, updated, deleted; } SyncStats;

/* Every record with its trailer, and the deletion log, as FuelLog-YYYYMMDD.fuel in dir. */
Int32 SyncWriteFile(const char *dir, UInt16 today, char *nameOut);
/* Applies the bundle at path to the open log: the newer edit wins, deletions travel. */
Int32 SyncApplyFile(const char *path, SyncStats *st);

/* The Palm's own, ported: the bundle to any stream (vehicle 255 = all), and the apply. */
void  WriteBundle(OutStream *out, UInt8 vehicle, UInt16 kind);
Int32 SyncApply(const UInt8 *bundle, UInt32 len, SyncStats *st);

#endif

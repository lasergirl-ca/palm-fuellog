/* Backup sets and restore, after the Palm build's src/backup.c: a set is a
 * folder named for the time ("20261112-0730") holding the six databases as
 * .pdb files, the same as the Palm writes to its card, so a set restores on
 * either. Plain C over store/db.h and store/dirs.h, host-tested. */
#ifndef BACKUPFILE_H
#define BACKUPFILE_H
#include "../../src/ptypes.h"
#include "../../src/prefs.h"
#include "../store/db.h"
#include "../store/dirs.h"

#define backupOk         0
#define backupErrFolder  1     /* the folder could not be made */
#define backupErrWrite   2
#define backupErrMissing 3     /* the set lacks one of the five record databases */
#define backupErrNewer   4     /* written by a newer FuelLog */
#define backupErrOpen    5     /* copied, but the log does not open */

void   BackupSetName(char *set, UInt16 today, UInt16 minutes);
/* Writes the log as it stands into a new set under dir, then keeps only the newest `keep` sets. */
Int32  BackupNow(const char *dir, UInt16 today, UInt16 minutes, UInt8 keep, char *setOut);
UInt16 BackupList(const char *dir, char names[][dirNameLen], UInt16 max);
/* Whether the set at setDir can be restored, before anything is touched. */
Int32  BackupCheck(const char *setDir);
/* Replaces the log in dataDir with the set and opens it. A set from before sync has no
 * deletion log: the one here is kept, as on the Palm. */
Int32  BackupRestore(const char *setDir, const char *dataDir, const DbClock *clock);
/* Auto backup is on and a week has passed since the last. */
int    BackupDue(const FuelPrefs *p, UInt16 today);

#endif

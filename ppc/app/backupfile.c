/* Backup sets and restore (backupfile.h), after src/backup.c. */
#include "backupfile.h"
#include "../store/pdbfile.h"
#include "../../src/records.h"
#include "../../src/datecalc.h"
#include "../../src/text.h"
#include <string.h>

static void Two(char *b, UInt16 v)
{
    if (v < 10) strcat(b, "0");
    TextFixed(b + strlen(b), v, 0, '.', 0);
}

void BackupSetName(char *set, UInt16 today, UInt16 minutes)
{
    UInt16 y; UInt8 m, d;
    DateDaysToYmd(today, &y, &m, &d);
    TextFixed(set, y, 0, '.', 0);
    Two(set, m); Two(set, d);
    strcat(set, "-");
    Two(set, (UInt16)(minutes / 60)); Two(set, (UInt16)(minutes % 60));
}

Int32 BackupNow(const char *dir, UInt16 today, UInt16 minutes, UInt8 keep, char *setOut)
{
    char setDir[300], names[24][dirNameLen], path[300];
    UInt16 n, i;
    if (!DirMake(dir)) return backupErrFolder;
    BackupSetName(setOut, today, minutes);
    PathJoin(setDir, dir, setOut);
    if (!DirMake(setDir)) return backupErrFolder;
    if (DbWriteCopy(setDir) != dbOk) return backupErrWrite;
    /* Keep the newest sets; the oldest go. */
    n = DirList(dir, 1, 0, names, 24);
    for (i = keep ? keep : 5; i < n; i++) {
        PathJoin(path, dir, names[i]);
        DirRemoveTree(path);
    }
    return backupOk;
}

UInt16 BackupList(const char *dir, char names[][dirNameLen], UInt16 max)
{
    return DirList(dir, 1, 0, names, max);
}

/* The schema version inside a .pdb: its AppInfo block's first two bytes. 0xFFFF when unreadable. */
static UInt16 Schema(const char *path)
{
    Pdb d;
    AppInfoBlock a;
    UInt16 v = 0xFFFF;
    if (PdbLoad(&d, path) != pdbOk) return v;
    if (!d.appInfoLen) v = 0;
    else if (AppInfoUnpack(d.appInfo, d.appInfoLen, &a) == 0) v = a.schemaVersion;
    PdbFree(&d);
    return v;
}

Int32 BackupCheck(const char *setDir)
{
    char path[300];
    UInt8 i;
    for (i = 0; i < dbAll; i++) {
        UInt16 v;
        DbPath(setDir, i, path);
        v = Schema(path);
        if (v == 0xFFFF) { if (i == dbDeleted) continue; return backupErrMissing; }
        if (v > recSchemaVersion) return backupErrNewer;
    }
    return backupOk;
}

Int32 BackupRestore(const char *setDir, const char *dataDir, const DbClock *clock)
{
    char from[300], to[300];
    UInt8 i;
    Int32 e = BackupCheck(setDir);
    if (e != backupOk) return e;
    DbCloseAll();
    for (i = 0; i < dbAll; i++) {
        DbPath(setDir, i, from);
        DbPath(dataDir, i, to);
        if (i == dbDeleted && !FileExists(from)) continue;
        if (!FileCopy(from, to)) { e = backupErrWrite; break; }
    }
    if (DbOpenAll(dataDir, clock) != dbOk) return e ? e : backupErrOpen;
    if (DbEnsureUids() || DbDirty()) DbSave();
    return e;
}

int BackupDue(const FuelPrefs *p, UInt16 today)
{
    if (!p->autoBackup) return 0;
    return p->lastBackupDate == 0 || today > p->lastBackupDate + 7;
}

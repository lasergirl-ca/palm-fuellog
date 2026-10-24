#include "backup.h"
#include "app.h"
#include "db.h"
#include "datecalc.h"

#define kCreator 'ZFlg'

/* Section placement: see sections.h. */
static void SetName(char *set) IO_SEC;
static void Prune(UInt16 vol) IO_SEC;
static UInt16 PdbSchema(UInt16 vol, const char *path) IO_SEC;

static void SetName(char *set)
{
    UInt16 y; UInt8 m, d; UInt16 mins = AppNowMinutes();
    DateDaysToYmd(AppToday(), &y, &m, &d);
    StrIToA(set, y);
    if (m < 10) StrCat(set, "0"); StrIToA(set + StrLen(set), m);
    if (d < 10) StrCat(set, "0"); StrIToA(set + StrLen(set), d);
    StrCat(set, "-");
    if (mins / 60 < 10) StrCat(set, "0"); StrIToA(set + StrLen(set), mins / 60);
    if (mins % 60 < 10) StrCat(set, "0"); StrIToA(set + StrLen(set), mins % 60);
}

static void Prune(UInt16 vol)
{
    static char names[16][cardNameLen];
    char path[80];
    UInt16 n = CardList(vol, cardBackup, true, names, 16), i, keep = gPrefs.backupSetsKept ? gPrefs.backupSetsKept : 5;
    for (i = keep; i < n; i++) {
        StrCopy(path, cardBackup); StrCat(path, "/"); StrCat(path, names[i]);
        CardDeleteFolder(vol, path);
    }
}

Err BackupNow(UInt16 vol, char *setOut)
{
    char dir[80], path[120];
    UInt8 i;
    Err err = CardEnsureDirs(vol);
    if (err) return err;
    SetName(setOut);
    StrCopy(dir, cardBackup); StrCat(dir, "/"); StrCat(dir, setOut);
    err = VFSDirCreate(vol, dir);
    if (err && err != vfsErrFileAlreadyExists) return err;
    for (i = 0; i < dbCount; i++) {
        LocalID id = DmFindDatabase(0, DbName(i));
        if (!id) continue;
        StrCopy(path, dir); StrCat(path, "/"); StrCat(path, DbName(i)); StrCat(path, ".pdb");
        VFSFileDelete(vol, path);
        err = VFSExportDatabaseToFile(vol, path, 0, id);
        if (err) return err;
    }
    gPrefs.lastBackupDate = AppToday();
    PrefsSave(&gPrefs);
    Prune(vol);
    return errNone;
}

UInt16 BackupList(UInt16 vol, char names[][cardNameLen], UInt16 max)
{
    return CardList(vol, cardBackup, true, names, max);
}

/* The schema version inside a .pdb: the AppInfo block's first two bytes. */
static UInt16 PdbSchema(UInt16 vol, const char *path)
{
    FileRef f;
    UInt8 hdr[78], ver[2];
    UInt32 got = 0, appInfo;
    UInt16 v = 0xFFFF;
    if (VFSFileOpen(vol, path, vfsModeRead, &f) != errNone) return v;
    if (VFSFileRead(f, 78, hdr, &got) == errNone && got == 78) {
        appInfo = ((UInt32)hdr[52] << 24) | ((UInt32)hdr[53] << 16) | ((UInt32)hdr[54] << 8) | hdr[55];
        if (appInfo && VFSFileSeek(f, vfsOriginBeginning, (Int32)appInfo) == errNone &&
            VFSFileRead(f, 2, ver, &got) == errNone && got == 2) v = (UInt16)((ver[0] << 8) | ver[1]);
        else if (appInfo == 0) v = 0;
    }
    VFSFileClose(f);
    return v;
}

Err BackupRestore(UInt16 vol, const char *set)
{
    char dir[80], path[120];
    UInt8 i;
    Err err;
    StrCopy(dir, cardBackup); StrCat(dir, "/"); StrCat(dir, set);
    /* Refuse a newer schema before anything is touched. */
    for (i = 0; i < dbCount; i++) {
        UInt16 v;
        StrCopy(path, dir); StrCat(path, "/"); StrCat(path, DbName(i)); StrCat(path, ".pdb");
        v = PdbSchema(vol, path);
        if (v == 0xFFFF) return vfsErrFileNotFound;
        if (v > recSchemaVersion) return dmErrAlreadyExists;
    }
    DbCloseAll();
    for (i = 0; i < dbCount; i++) {
        LocalID id = DmFindDatabase(0, DbName(i));
        UInt16 card = 0;
        if (id) DmDeleteDatabase(0, id);
        StrCopy(path, dir); StrCat(path, "/"); StrCat(path, DbName(i)); StrCat(path, ".pdb");
        err = VFSImportDatabaseFromFile(vol, path, &card, &id);
        if (err) break;
    }
    gPrefs.currentVehicle = 255;
    PrefsSave(&gPrefs);
    {
        Err open = DbOpenAll();   /* also migrates an older schema */
        if (!err) err = open;
    }
    return err;
}

Boolean BackupDue(void)
{
    UInt16 today = AppToday();
    if (!gPrefs.autoBackup) return false;
    return gPrefs.lastBackupDate == 0 || today > gPrefs.lastBackupDate + 7;
}

void BackupAuto(void)
{
    UInt16 vol;
    char set[cardNameLen];
    if (!BackupDue()) return;
    if (CardFind(&vol) != errNone) return;
    BackupNow(vol, set);
}

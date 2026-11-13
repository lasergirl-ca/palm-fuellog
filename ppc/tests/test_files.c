/* Host tests for the Pocket PC build's export (app/exportfile.c), backup and
 * restore (app/backupfile.c) and folders (store/dirs.c), against a working
 * copy of the fixture set tools/mkpdb writes (argv[1]) and a scratch folder
 * (argv[2]). The XLSX it writes is checked by tools/check_xlsx.py after. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../store/dirs.h"
#include "../store/pdbfile.h"
#include "../app/exportfile.h"
#include "../app/backupfile.h"
#include "../../src/datecalc.h"
#include "../../src/units.h"

static int failures;

static void check(const char *name, long got, long want)
{
    if (got == want) printf("ok   %s = %ld\n", name, got);
    else { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
}
static void checkStr(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) printf("ok   %s = \"%s\"\n", name, got);
    else { printf("FAIL %s: got \"%s\" want \"%s\"\n", name, got, want); failures++; }
}

static UInt32 Now(void) { return 3877000000UL; }

/* The file's first n bytes and its line count. */
static long ReadHead(const char *path, char *buf, long n, int *lines)
{
    FILE *f = fopen(path, "rb");
    long got;
    int c;
    *lines = 0;
    if (!f) return -1;
    got = (long)fread(buf, 1, (size_t)n - 1, f);
    buf[got < 0 ? 0 : got] = 0;
    fseek(f, 0, SEEK_SET);
    while ((c = fgetc(f)) != EOF) if (c == '\n') (*lines)++;
    fclose(f);
    return got;
}

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : "build/host/work7", *scratch = argc > 2 ? argv[2] : "build/host/files";
    DbClock clock;
    FuelPrefs p;
    UInt16 today = DateYmdToDays(2026, 11, 12);
    char name[40], path[300], dir[300], set[40], names[8][dirNameLen], head[600];
    UInt32 bytes;
    int lines;

    clock.now = Now; clock.clockShift = 0;
    check("open", DbOpenAll(work, &clock), dbOk);
    DbEnsureUids();
    PrefsSetDefaults(&p);
    ExportSetup(&p, '.', ',', 1);

    /* Folders. */
    PathJoin(dir, scratch, "Export");
    check("make a folder", DirMake(dir), 1);
    check("  again", DirMake(dir), 1);
    PathJoin(path, "a", "b");
    checkStr("join", path, "a" dirSep "b");

    /* CSV. */
    check("CSV export", ExportFile(dir, exportCsv, 255, 0, 255, today, name, &bytes), exportOk);
    checkStr("  named for the day", name, "FuelLog-20261112.csv");
    check("  has bytes", bytes > 200, 1);
    PathJoin(path, dir, name);
    check("  reads back", ReadHead(path, head, sizeof head, &lines) > 0, 1);
    check("  UTF-8 BOM", (unsigned char)head[0] == 0xEF && (unsigned char)head[1] == 0xBB && (unsigned char)head[2] == 0xBF, 1);
    check("  header, then three fills", lines, 4);
    check("  the header names the units", strstr(head, "Odometer (km)") != NULL, 1);
    check("  Petro-Points columns", strstr(head, "Petro-Points earned") != NULL, 1);
    check("  the first fill", strstr(head, "Outback") != NULL, 1);
    check("CSV of the Ioniq alone", ExportFile(dir, exportCsv, 2, 0, 255, today, name, &bytes), exportOk);
    ReadHead(path, head, sizeof head, &lines);
    check("  header and one charge", lines, 2);
    check("CSV of Petro-Points fills only", ExportFile(dir, exportCsv, 255, 0, 2, today, name, &bytes), exportOk);
    ReadHead(path, head, sizeof head, &lines);
    check("  header and the one fill", lines, 2);
    check("  the filter is cleared after", gExportPgm, 255);
    check("CSV of 2025: no fills", ExportFile(dir, exportCsv, 255, 2025, 255, today, name, &bytes), exportOk);
    ReadHead(path, head, sizeof head, &lines);
    check("  header only", lines, 1);

    /* XLSX and PDF. */
    check("XLSX export", ExportFile(dir, exportXlsx, 255, 0, 255, today, name, &bytes), exportOk);
    checkStr("  name", name, "FuelLog-20261112.xlsx");
    PathJoin(path, dir, name);
    ReadHead(path, head, sizeof head, &lines);
    check("  a zip", head[0] == 'P' && head[1] == 'K', 1);
    check("  size", bytes > 2000, 1);
    check("PDF export", ExportFile(dir, exportPdf, 255, 0, 255, today, name, &bytes), exportOk);
    PathJoin(path, dir, name);
    ReadHead(path, head, sizeof head, &lines);
    check("  a PDF", strncmp(head, "%PDF-", 5), 0);
    check("  size", bytes > 1000, 1);
    {
        FILE *f = fopen(path, "rb");
        char tail[16];
        memset(tail, 0, sizeof tail);
        if (f) { fseek(f, -6, SEEK_END); fread(tail, 1, 6, f); fclose(f); }
        check("  ends with %%EOF", strstr(tail, "%%EOF") != NULL, 1);
    }
    check("a folder that cannot be made", ExportFile("/no/such/place/at/all", exportCsv, 255, 0, 255, today, name, &bytes), exportErrFolder);

    /* Backup sets. */
    PathJoin(dir, scratch, "Backup");
    BackupSetName(set, today, 7 * 60 + 30);
    checkStr("set name", set, "20261112-0730");
    check("back up", BackupNow(dir, today, 7 * 60 + 30, 2, set), backupOk);
    check("  listed", BackupList(dir, names, 8), 1);
    PathJoin(path, dir, set);
    check("  can be restored", BackupCheck(path), backupOk);
    check("two more sets, two kept", BackupNow(dir, today, 8 * 60, 2, set) == backupOk && BackupNow(dir, today, 9 * 60, 2, set) == backupOk, 1);
    check("  two listed", BackupList(dir, names, 8), 2);
    checkStr("  newest first", names[0], "20261112-0900");
    checkStr("  then", names[1], "20261112-0800");

    /* Change the log, then restore the set over it. */
    {
        UInt16 fills = DbCount(dbFills);
        char data[300];
        DbDelete(dbFills, 0);
        check("a fill deleted", DbCount(dbFills), fills - 1);
        DbSave();
        PathJoin(path, dir, names[0]);
        PathJoin(data, scratch, "data");
        DirMake(data);
        DbWriteCopy(data);                          /* the log, in its own folder, as the device keeps it */
        DbCloseAll();
        check("  open that copy", DbOpenAll(data, &clock), dbOk);
        check("  restore", BackupRestore(path, data, &clock), backupOk);
        check("  the fill is back", DbCount(dbFills), fills);
        check("  vehicles", DbCount(dbVehicles), 2);
        DbCloseAll();
    }

    /* A set from a newer FuelLog, and one with a database missing. */
    {
        char bad[300], f[300];
        Pdb d;
        UInt8 ai[4] = { 0, 2, 0, 0 };
        PathJoin(bad, scratch, "Backup" dirSep "20261112-0800");
        DbPath(bad, dbVehicles, f);
        PdbLoad(&d, f);
        PdbSetAppInfo(&d, ai, 4);
        PdbSave(&d, f);
        PdbFree(&d);
        check("newer schema refused", BackupCheck(bad), backupErrNewer);
        DbPath(bad, dbFills, f);
        remove(f);
        check("missing database refused", BackupCheck(bad), backupErrMissing);
    }

    /* Auto backup. */
    PrefsSetDefaults(&p);
    check("due: never backed up", BackupDue(&p, today), 1);
    p.lastBackupDate = (UInt16)(today - 3);
    check("  not three days on", BackupDue(&p, today), 0);
    p.lastBackupDate = (UInt16)(today - 8);
    check("  due eight days on", BackupDue(&p, today), 1);
    p.autoBackup = 0;
    check("  never when off", BackupDue(&p, today), 0);

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

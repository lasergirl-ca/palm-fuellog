/* Host tests for the Pocket PC build's sync files (app/syncfile.c): two logs
 * that share uids (B starts as a copy of A, as after a restore), changes on
 * A carried to B and back, a sync into an empty log, and files that are not
 * bundles or come from a newer FuelLog. argv[1] is a working copy of the
 * fixture set tools/mkpdb writes, argv[2] a scratch folder. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../store/dirs.h"
#include "../app/syncfile.h"
#include "../../src/datecalc.h"

static int failures;

static void check(const char *name, long got, long want)
{
    if (got == want) printf("ok   %s = %ld\n", name, got);
    else { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
}

static UInt32 gNow = 3877000000UL;
static UInt32 Now(void) { return gNow; }

int main(int argc, char **argv)
{
    const char *a = argc > 1 ? argv[1] : "build/host/work9", *scratch = argc > 2 ? argv[2] : "build/host/sync";
    DbClock clock;
    char b[300], c[300], sync[300], name[40], path[300];
    UInt16 today = DateYmdToDays(2026, 11, 12);
    SyncStats st;
    FillRecord f;
    UInt16 fillsB;

    clock.now = Now; clock.clockShift = 0;
    PathJoin(b, scratch, "b");
    PathJoin(c, scratch, "c");
    PathJoin(sync, scratch, "Sync");
    DirMake(b);
    DirMake(c);

    /* A gets its uids, and B starts as a copy of it. */
    check("open A", DbOpenAll(a, &clock), dbOk);
    DbEnsureUids();
    DbSave();
    check("B is a copy of A", DbWriteCopy(b), dbOk);

    /* An hour later on A: a fill added, one edited, one deleted. */
    gNow += 3600;
    memset(&f, 0, sizeof f);
    f.date = DateYmdToDays(2026, 8, 20); f.time = fillTimeUnknown; f.flags = fillFlagFullTank; f.vehicle = 1;
    f.odometer = 817000; f.volume = 41000; f.price = 17000; f.total = 6970; f.currency = 1; f.fxRate = 1000000UL;
    check("A: add a fill", DbAddFill(&f, NULL), dbOk);
    DbGetFill(1, &f);
    f.volume = 47000;
    {
        UInt8 buf[dbMaxRecord];
        check("A: edit the Aug 10 fill", DbReplace(dbFills, 1, buf, FillPack(&f, buf)), dbOk);
    }
    check("A: delete the charge", DbDelete(dbFills, 2), dbOk);
    check("  A has three fills", DbCount(dbFills), 3);
    check("A writes its sync file", SyncWriteFile(sync, today, name), syncOk);
    check("  named for the day", strcmp(name, "FuelLog-20261112.fuel"), 0);
    PathJoin(path, sync, name);
    check("  has bytes", FileSize(path) > 100, 1);
    DbSave();
    DbCloseAll();

    /* B takes A's file. */
    check("open B", DbOpenAll(b, &clock), dbOk);
    fillsB = DbCount(dbFills);
    check("  B has the three original fills", fillsB, 3);
    check("B applies A's file", SyncApplyFile(path, &st), syncOk);
    check("  one added", st.added, 1);
    check("  one updated", st.updated, 1);
    check("  one deleted", st.deleted, 1);
    check("  B has three fills now", DbCount(dbFills), 3);
    DbGetFill(1, &f);
    check("  the edit arrived", (long)f.volume, 47000);
    DbGetFill(2, &f);
    check("  the new fill, in date order", f.date, DateYmdToDays(2026, 8, 20));
    check("  the deletion is kept to pass on", DbCount(dbDeleted), 1);
    check("B writes its side back", SyncWriteFile(sync, today, name), syncOk);
    DbSave();
    DbCloseAll();

    /* A takes B's file: nothing left to change. */
    check("open A", DbOpenAll(a, &clock), dbOk);
    check("A applies B's file", SyncApplyFile(path, &st), syncOk);
    check("  none added", st.added, 0);
    check("  none updated", st.updated, 0);
    check("  none deleted", st.deleted, 0);
    check("  A still has three fills", DbCount(dbFills), 3);
    DbCloseAll();

    /* An empty log takes the whole of it. */
    check("open an empty log", DbOpenAll(c, &clock), dbOk);
    check("apply into it", SyncApplyFile(path, &st), syncOk);
    check("  three fills", DbCount(dbFills), 3);
    check("  two vehicles", DbCount(dbVehicles), 2);
    check("  the programme", DbCount(dbProgrammes), 1);
    check("  the service type", DbCount(dbServiceTypes), 1);
    check("  the visit", DbCount(dbServiceVisits), 1);
    check("  added counts them all", st.added, 8);

    /* Files that are not bundles, and a bundle from a newer FuelLog. */
    {
        FILE *fp;
        PathJoin(path, scratch, "not.fuel");
        fp = fopen(path, "wb"); fputs("this is not a bundle at all", fp); fclose(fp);
        check("not a bundle", SyncApplyFile(path, &st), syncErrNotBundle);
        PathJoin(path, scratch, "newer.fuel");
        fp = fopen(path, "wb"); fwrite("FUEL\0\x02\0\x03\0\0", 1, 10, fp); fclose(fp);
        check("a newer FuelLog", SyncApplyFile(path, &st), syncErrNewer);
        check("a missing file", SyncApplyFile("no-such.fuel", &st), syncErrRead);
        check("  still three fills", DbCount(dbFills), 3);
    }
    DbSave();
    DbCloseAll();

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

/* Host tests for the Pocket PC build's import (app/importfile.c): the CSV
 * and XLSX the exporter writes from the mkpdb fixture (argv[1]) read back
 * into empty logs under a scratch folder (argv[2]), the merge rule, row
 * errors, a KB Points column, and the Palm build's XLS sample (argv[3]). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../store/dirs.h"
#include "../app/exportfile.h"
#include "../app/importfile.h"
#include "../../src/datecalc.h"

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

static void Write(const char *path, const char *text)
{
    FILE *f = fopen(path, "wb");
    if (f) { fputs(text, f); fclose(f); }
}

/* A fresh, empty log in a folder of its own. */
static void Fresh(const char *scratch, const char *name, const DbClock *clock)
{
    char dir[300];
    PathJoin(dir, scratch, name);
    DirMake(dir);
    DbCloseAll();
    DbOpenAll(dir, clock);
}

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : "build/host/work8", *scratch = argc > 2 ? argv[2] : "build/host/import";
    const char *xls = argc > 3 ? argv[3] : "../build/fills-mini.xls";
    DbClock clock;
    FuelPrefs p;
    char csv[300], xlsx[300], name[40], path[300];
    UInt32 bytes;
    FillRecord f;
    ProgrammeRecord pg;
    VehicleRecord v;
    UInt16 today = DateYmdToDays(2026, 11, 12);

    clock.now = Now; clock.clockShift = 0;
    PrefsSetDefaults(&p);
    ImportSetup(&p);
    ExportSetup(&p, '.', ',', 1);

    /* Export the fixture as CSV and XLSX. */
    check("open the fixture", DbOpenAll(work, &clock), dbOk);
    DbEnsureUids();
    check("export CSV", ExportFile(scratch, exportCsv, 255, 0, 255, today, name, &bytes), exportOk);
    PathJoin(csv, scratch, name);
    check("export XLSX", ExportFile(scratch, exportXlsx, 255, 0, 255, today, name, &bytes), exportOk);
    PathJoin(xlsx, scratch, name);

    /* CSV into an empty log. */
    Fresh(scratch, "a", &clock);
    check("scan the CSV", ImportScan(csv, scratch), 0);
    checkStr("  the file", gImport.file, "FuelLog-20261112.csv");
    check("  three rows", (long)gImport.rows, 3);
    check("  three ready", (long)gImport.ready, 3);
    check("  no errors", (long)gImport.errors, 0);
    check("  two new vehicles", gImport.newVehicles, 2);
    checkStr("  the first", gImport.newVehicleNames[0], "Outback");
    check("  one new programme", gImport.newProgrammes, 1);
    check("  nothing written yet", DbCount(dbFills), 0);
    check("write", ImportWrite(), 0);
    check("  three written", (long)gImport.written, 3);
    check("  three fills", DbCount(dbFills), 3);
    check("  two vehicles", DbCount(dbVehicles), 2);
    check("  Petro-Points made", DbCount(dbProgrammes), 1);
    DbGetFill(0, &f);
    check("  first fill: odometer", (long)f.odometer, 806500);
    check("  volume", (long)f.volume, 45200);
    check("  total", (long)f.total, 7937);
    check("  card", f.payment, 1);
    check("  its points", f.pointCount, 1);
    check("  452 earned", (long)f.points[0].earned, 452);
    checkStr("  the accented note", f.notes, "Petro-Canada on 152 St., caf\xE9");
    check("  a uid", f.uid != 0, 1);
    DbGetProgramme(0, &pg);
    checkStr("  the programme", pg.name, "Petro-Points");
    check("  points on the right programme", f.points[0].programme, pg.id);
    DbGetFill(2, &f);
    check("  the charge is a charge", (f.flags & 8) != 0, 1);
    check("  38.6 kWh", (long)f.volume, 38600);
    DbGetVehicle(1, &v);
    checkStr("  second vehicle", v.name, "Ioniq 5");

    /* The same file again: every row is already there. */
    check("scan again", ImportScan(csv, scratch), 0);
    check("  three duplicates", (long)gImport.duplicates, 3);
    check("  none ready", (long)gImport.ready, 0);
    check("  no new vehicles", gImport.newVehicles, 0);
    ImportWrite();
    check("  skipped", (long)gImport.skipped, 3);
    check("  still three fills", DbCount(dbFills), 3);

    /* XLSX into another empty log. */
    Fresh(scratch, "b", &clock);
    check("scan the XLSX", ImportScan(xlsx, scratch), 0);
    check("  three ready", (long)gImport.ready, 3);
    check("  no errors", (long)gImport.errors, 0);
    check("write", ImportWrite(), 0);
    check("  three fills", DbCount(dbFills), 3);
    DbGetFill(1, &f);
    check("  Aug 10", f.date, DateYmdToDays(2026, 8, 10));
    check("  81,190 km", (long)f.odometer, 811900);
    check("  the temporary parts are gone", FileExists(".part0") || 0, 0);
    PathJoin(path, scratch, ".part0");
    check("  .part0 removed", FileExists(path), 0);

    /* Errors, by line. */
    Fresh(scratch, "c", &clock);
    PathJoin(path, scratch, "bad.csv");
    Write(path, "Date,Vehicle,Odometer (km),Volume (L),Total\n"
                "2026-08-01,Civic,1000,40,70.00\n"
                "2026-08-09,Civic,1x00,40,70.00\n"
                "2026-08-15,Civic,900,40,70.00\n"
                "2026-13-01,Civic,1500,40,70.00\n"
                "\n"
                "2026-08-20,Civic,1600,40,70.00\n");
    check("scan a file with errors", ImportScan(path, scratch), 0);
    check("  five rows", (long)gImport.rows, 5);
    check("  three errors", (long)gImport.errors, 3);
    checkStr("  line 3", gImport.errorLines[0], "Line 3: odometer is not a number");
    checkStr("  line 4", gImport.errorLines[1], "Line 4: odometer lower than line 2");
    checkStr("  line 5", gImport.errorLines[2], "Line 5: date is not valid");
    check("  two ready", (long)gImport.ready, 2);
    ImportWrite();
    check("  two written", (long)gImport.written, 2);
    DbGetVehicle(0, &v);
    checkStr("  Civic made", v.name, "Civic");

    /* Not a fills file. */
    PathJoin(path, scratch, "other.csv");
    Write(path, "Name,Phone\nPat,555-0100\n");
    check("a header that is not fills", ImportScan(path, scratch), 0);
    check("  says so", (long)gImport.badHeader, 1);
    check("a file that is not there", ImportScan("no-such-file.csv", scratch) != 0, 1);

    /* The paper form's KB Points column, with no programme for it yet. */
    Fresh(scratch, "d", &clock);
    PathJoin(path, scratch, "kb.csv");
    Write(path, "Date,Odometer,Volume,Total,KB Points\n2026-08-01,1000,40,70,120\n");
    check("scan KB Points", ImportScan(path, scratch), 0);
    check("  the column is seen", gImport.kbPoints, 1);
    checkStr("  goes to a new programme", gImport.kbTarget, "new KB Points programme");
    check("  counted as new", gImport.newProgrammes, 1);
    ImportWrite();
    check("  written", (long)gImport.written, 1);
    check("  KB Points made", DbCount(dbProgrammes), 1);
    DbGetProgramme(0, &pg);
    checkStr("  its name", pg.name, "KB Points");
    DbGetFill(0, &f);
    check("  120 points on it", (long)f.points[0].earned, 120);
    check("  a vehicle made for the unnamed", DbCount(dbVehicles), 0);

    /* The Palm build's XLS sample. */
    if (FileExists(xls)) {
        Fresh(scratch, "e", &clock);
        check("scan the XLS sample", ImportScan(xls, scratch), 0);
        check("  one row ready", (long)gImport.ready, 1);
        check("  its edge-case row (no odometer) is an error", (long)gImport.errors, 1);
        ImportWrite();
        check("  all written", (long)gImport.written, (long)gImport.ready);
    } else printf("skip XLS sample (%s not built)\n", xls);
    DbSave();
    DbCloseAll();

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

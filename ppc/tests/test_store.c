/* Host tests for the Pocket PC build's storage and fill list model: the
 * .pdb reader and writer, the database store over it, and the rows the
 * list shows. Runs against the fixture set tools/mkpdb writes (argv[1],
 * read only) and a working copy of it (argv[2]). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/pdbfile.h"
#include "../store/db.h"
#include "../app/fillview.h"
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

static long FileBytes(const char *path, unsigned char *buf, long cap)
{
    FILE *f = fopen(path, "rb");
    long n;
    if (!f) return -1;
    n = (long)fread(buf, 1, (size_t)cap, f);
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    const char *fixture = argc > 1 ? argv[1] : "build/host/pdb", *work = argc > 2 ? argv[2] : "build/host/work";
    static unsigned char a[65536], b[65536];
    char path[300], out[300];
    DbClock clock;
    FuelPrefs prefs;
    UInt8 w;

    /* A database read and written back unchanged is the same bytes. */
    for (w = 0; w < dbCount; w++) {
        Pdb d;
        long na, nb;
        char name[64];
        DbPath(fixture, w, path);
        DbPath(work, w, out);
        strcat(out, ".copy");
        sprintf(name, "%s loads", DbName(w));
        check(name, PdbLoad(&d, path), pdbOk);
        check("  saves", PdbSave(&d, out), pdbOk);
        PdbFree(&d);
        na = FileBytes(path, a, sizeof a); nb = FileBytes(out, b, sizeof b);
        check("  same length", nb, na);
        check("  same bytes", na > 0 && na == nb && memcmp(a, b, (size_t)na) == 0, 1);
        remove(out);
    }
    {
        Pdb d;
        FILE *f;
        check("a missing file", PdbLoad(&d, "build/host/no-such.pdb"), pdbErrMissing);
        DbPath(fixture, dbFills, path);
        FileBytes(path, a, sizeof a);
        sprintf(out, "%s/short.pdb", work);
        f = fopen(out, "wb"); fwrite(a, 1, 100, f); fclose(f);        /* cut inside the record list */
        check("a truncated file", PdbLoad(&d, out), pdbErrMalformed);
        remove(out);
    }

    /* The store over the working copy. */
    clock.now = Now; clock.clockShift = 0;
    check("open", DbOpenAll(work, &clock), dbOk);
    check("fills", DbCount(dbFills), 3);
    check("vehicles", DbCount(dbVehicles), 2);
    check("programmes", DbCount(dbProgrammes), 1);
    check("service types", DbCount(dbServiceTypes), 1);
    check("service visits", DbCount(dbServiceVisits), 1);
    check("tombstones (no file yet)", DbCount(dbDeleted), 0);
    check("nothing to save after a plain open", DbDirty(), 0);
    {
        FillRecord f;
        VehicleRecord v;
        check("first fill reads", DbGetFill(0, &f), dbOk);
        check("  date", f.date, DateYmdToDays(2026, 7, 29));
        check("  odometer", (long)f.odometer, 806500);
        checkStr("  notes", f.notes, "Petro-Canada on 152 St., caf\xE9");
        check("vehicle 2 by id", DbVehicleById(2, &v), 1);
        checkStr("  name", v.name, "Ioniq 5");
        check("no vehicle 9", DbVehicleById(9, &v), 0);
    }

    /* The fill list. */
    PrefsSetDefaults(&prefs);
    {
        UInt16 idx[16];
        char col[fillViewCols][fillViewText], avg[40], month[40];
        FillRecord f;
        check("all vehicles: three rows", FillViewCollect(255, idx, 16), 3);
        check("  newest first", idx[0], 2);
        check("Outback: two rows", FillViewCollect(1, idx, 16), 2);
        check("  the August fill first", idx[0], 1);
        DbGetFill(1, &f);
        check("Aug 10 economy 46.0 L over 540 km", (long)FillViewRowEconomy(1, &f), 85);
        check("  as km/L x 10", (long)FillViewEcoDisplay(85, 0, &prefs), 118);
        check("row reads", FillViewRow(1, &prefs, '.', ',', col), 1);
        checkStr("  date", col[0], "Aug 10");
        checkStr("  odometer", col[1], "81,190");
        checkStr("  volume", col[2], "46.0");
        checkStr("  total", col[3], "81.37");
        checkStr("  economy", col[4], "11.8");
        FillViewRow(0, &prefs, '.', ',', col);
        checkStr("first fill has no economy", col[4], "");
        FillViewRow(2, &prefs, '.', ',', col);
        checkStr("charge volume in kWh", col[2], "38.6");
        checkStr("economy title", FillViewEcoHeader(0, &prefs), "KPL");
        prefs.economyUnit = unitEcoMpgUS;
        checkStr("  in MPG", FillViewEcoHeader(0, &prefs), "MPG");
        FillViewRow(1, &prefs, '.', ',', col);
        checkStr("  the row follows", col[4], "27.7");
        prefs.economyUnit = unitEcoL100km;
        prefs.currentVehicle = 1;
        FillViewFooter(&prefs, DateYmdToDays(2026, 8, 20), '.', ',', avg, month);
        checkStr("footer average", avg, "Avg 11.8 KPL");
        checkStr("footer month", month, "Aug $81.37");
        prefs.currentVehicle = 2;
        check("the Ioniq is electric", FillViewElectric(&prefs), 1);
        FillViewFooter(&prefs, DateYmdToDays(2026, 8, 20), '.', ',', avg, month);
        checkStr("one charge: no average", avg, "");
        checkStr("  its month", month, "Aug $0.69");
        prefs.currentVehicle = 255;
    }

    /* Writing: a sorted add, the trailer, uids, a deletion, save and reopen. */
    {
        FillRecord f, g;
        UInt16 at = 99;
        UInt8 which; UInt32 uid, when;
        memset(&f, 0, sizeof f);
        f.date = DateYmdToDays(2026, 8, 5); f.time = fillTimeUnknown; f.vehicle = 1; f.odometer = 809000; f.volume = 20000;
        f.price = 17500; f.total = 3500; f.currency = 1; f.fxRate = 1000000UL; f.clockShift = 0;
        check("add a fill dated between two others", DbAddFill(&f, &at), dbOk);
        check("  lands in date order", at, 1);
        check("  four fills", DbCount(dbFills), 4);
        DbGetFill(1, &g);
        check("  given a uid", g.uid != 0, 1);
        check("  modified now", (long)(g.modified == Now()), 1);
        check("  clock shift kept", g.clockShift, 0);
        check("something to save", DbDirty(), 1);
        check("the fixture's records get uids", DbEnsureUids(), 8);
        check("  once", DbEnsureUids(), 0);
        check("delete the added fill", DbDelete(dbFills, 1), dbOk);
        check("  three fills", DbCount(dbFills), 3);
        check("  one tombstone", DbCount(dbDeleted), 1);
        check("  reads", DbTombstoneGet(0, &which, &uid, &when), dbOk);
        check("  for the fills", which, dbFills);
        check("  its uid", (long)(uid == g.uid), 1);
        check("  when", (long)(when == Now()), 1);
        check("save", DbSave(), dbOk);
        check("nothing left to save", DbDirty(), 0);
        DbCloseAll();
        check("reopen", DbOpenAll(work, &clock), dbOk);
        check("  three fills", DbCount(dbFills), 3);
        check("  the tombstone is on disk", DbCount(dbDeleted), 1);
        DbGetFill(0, &g);
        check("  uids were saved", g.uid != 0, 1);
        checkStr("  notes intact", g.notes, "Petro-Canada on 152 St., caf\xE9");
        DbCloseAll();
    }
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

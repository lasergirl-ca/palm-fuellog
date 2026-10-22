#include "m1test.h"
#include "db.h"
#include "prefs.h"
#include "units.h"
#include "currency.h"

static void Line(char lines[][vfsTestLineLen], UInt16 *n, const char *a, Int32 num, const char *b)
{
    char *s;
    if (*n >= vfsTestMaxLines) return;
    s = lines[(*n)++];
    StrNCopy(s, a, vfsTestLineLen - 1); s[vfsTestLineLen - 1] = 0;
    if (num >= 0) StrIToA(s + StrLen(s), num);
    if (b) StrNCat(s, b, vfsTestLineLen - 1);
}

UInt16 M1TestRun(char lines[vfsTestMaxLines][vfsTestLineLen])
{
    UInt16 n = 0, i, index, before;
    Err err;
    FillRecord f, g;
    FuelPrefs p;
    UInt32 start = TimGetTicks();

    err = DbOpenAll();
    if (err) { Line(lines, &n, "DbOpenAll err ", err, NULL); return n; }
    for (i = 0; i < dbCount; i++) {
        static const char *const names[dbCount] = { "Fills", "Vehicles", "Programmes", "SvcTypes", "SvcVisits" };
        Line(lines, &n, names[i], -1, ": v");
        StrIToA(lines[n - 1] + StrLen(lines[n - 1]), DbSchemaVersion(i));
        StrNCat(lines[n - 1], ", ", vfsTestLineLen - 1);
        StrIToA(lines[n - 1] + StrLen(lines[n - 1]), DbCount(i));
        StrNCat(lines[n - 1], " recs", vfsTestLineLen - 1);
    }

    /* A fill through pack, sorted insert and unpack. */
    MemSet(&f, sizeof f, 0);
    f.date = 44852; f.flags = fillFlagFullTank; f.vehicle = 1; f.odometer = 842120; f.trip = 5720;
    f.volume = 48200; f.price = 1749; f.total = 8430; f.currency = curCAD; f.fxRate = fxOne;
    f.pointCount = 1; f.points[0].programme = 2; f.points[0].earned = 482;
    StrCopy(f.notes, "M1 check");
    before = DbCount(dbFills);
    err = DbAddFill(&f, &index);
    if (err) { Line(lines, &n, "DbAddFill err ", err, NULL); DbCloseAll(); return n; }
    err = DbGetFill(index, &g);
    if (err) { Line(lines, &n, "DbGetFill err ", err, NULL); DbCloseAll(); return n; }
    Line(lines, &n, "Fill round trip: ", (g.odometer == f.odometer && g.points[0].earned == 482 && StrCompare(g.notes, f.notes) == 0) ? 1 : 0, " (1 = intact)");
    /* An earlier fill must sort before it. */
    f.date = 44840; f.odometer = 836400;
    err = DbAddFill(&f, &index);
    Line(lines, &n, "Earlier fill index: ", index, " (expect 0 of 2)");
    DbRemove(dbFills, 0);
    DbRemove(dbFills, 0);
    Line(lines, &n, "Fills after cleanup: ", DbCount(dbFills), before == DbCount(dbFills) ? " (restored)" : " (CHANGED)");

    /* Prefs: load, bump a field, save, reload. */
    PrefsLoad(&p);
    Line(lines, &n, "Prefs v", p.version, p.homeCurrency == curCAD ? ", home CAD" : ", home other");
    p.lastScrollRow++;
    PrefsSave(&p);
    i = p.lastScrollRow;
    MemSet(&p, sizeof p, 0);
    PrefsLoad(&p);
    Line(lines, &n, "Prefs persist: ", p.lastScrollRow == i ? 1 : 0, " (1 = yes), row ");
    StrIToA(lines[n - 1] + StrLen(lines[n - 1]), p.lastScrollRow);

    /* A conversion on the device, so the 64-bit path is seen here too. */
    Line(lines, &n, "47300 mL = ", UnitVolToDisplay(47300, unitVolUSGal), " US gal x1000");
    DbCloseAll();
    Line(lines, &n, "Total: ", (Int32)((TimGetTicks() - start) * 1000UL / SysTicksPerSecond()), " ms");
    return n;
}

/* Host tests for the Pocket PC build's New and Edit Fill rules
 * (app/filledit.c), against a working copy of the fixture set tools/mkpdb
 * writes (argv[1]) and an empty folder (argv[2]). The fixture: an Outback
 * (fuel, last fill Aug 10 at 81,190 km) and an Ioniq 5 (electric, home rate
 * 18.0 c/kWh), and Petro-Points (CAD, 10 points a litre, auto-apply,
 * 1,000 points take $1 off). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../app/filledit.h"
#include "../../src/datecalc.h"
#include "../../src/units.h"
#include "../../src/currency.h"

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

static FillEdit e;
static FillEditText t;
static FillEditInput in;
static char hint[48], ask[3][24], buf[64];
static FuelPrefs prefs;

static void New(void) { FillEditBegin(&e, fillEditNew, &prefs, DateYmdToDays(2026, 11, 12), 10 * 60, '.', ',', &t); }

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : "build/host/work2", *empty = argc > 2 ? argv[2] : "build/host/empty";
    DbClock clock;
    UInt16 at = 99;
    FillRecord f;
    UInt8 map[7];
    UInt32 uid;

    clock.now = Now; clock.clockShift = 0;
    check("open", DbOpenAll(work, &clock), dbOk);
    DbEnsureUids();
    PrefsSetDefaults(&prefs);
    memset(&in, 0, sizeof in);
    in.date = DateYmdToDays(2026, 11, 12); in.time = 10 * 60; in.full = 1; in.note = "";

    /* A new fill: defaults, any two give the third, points follow. */
    check("a log with vehicles needs none made", FillEditEnsureVehicle(), 0);
    New();
    checkStr("title", FillEditTitle(&e), "New Fill");
    check("  on the first vehicle", e.fill.vehicle, 1);
    check("  in the home currency", e.fill.currency, curCAD);
    check("  no rate field", FillEditForeign(&e), 0);
    FillEditOdoHint(&e, buf);
    checkStr("  previous reading", buf, "prev 81,190");
    FillEditPriceLabel(&e, buf);
    checkStr("  price label", buf, "Price c/L:");
    check("  four fuel grades", FillEditGrades(&e, map), 4);
    FillEditPointsText(&e, buf);
    checkStr("  Petro-Points applied", buf, "Petro-Points 0 pts");
    strcpy(t.volume, "40");
    check("volume alone works nothing out", FillEditTyped(&e, feVolume, &t, hint), 0);
    strcpy(t.price, "175.9");
    check("volume and price give the total", FillEditTyped(&e, fePrice, &t, hint), feTotal);
    checkStr("  40 L at 175.9", t.total, "70.36");
    checkStr("  hint", hint, "total calculated");
    FillEditPointsText(&e, buf);
    checkStr("  10 points a litre", buf, "Petro-Points 400 pts");
    strcpy(t.total, "35.18");
    check("typing the total after the price gives the volume", FillEditTyped(&e, feTotal, &t, hint), feVolume);
    checkStr("  35.18 at 175.9", t.volume, "20.00");
    checkStr("  hint", hint, "volume calculated");
    strcpy(t.volume, "25");
    check("then the volume: the price follows", FillEditTyped(&e, feVolume, &t, hint), fePrice);
    checkStr("  35.18 over 25 L", t.price, "140.7");

    /* The trip: a blank odometer comes from it. */
    strcpy(t.trip, "500");
    FillEditTripHint(&e, &t, buf);
    checkStr("blank odometer, trip 500", buf, "odo 81,690");
    strcpy(t.odo, "81690");
    FillEditTripHint(&e, &t, buf);
    checkStr("odometer agrees", buf, "= odo");
    strcpy(t.odo, "82000");
    FillEditTripHint(&e, &t, buf);
    checkStr("odometer 310 km out", buf, "check odo");
    t.odo[0] = 0;
    check("save with the odometer blank", FillEditSave(&e, &t, &in, ask, &at), feSaved);
    check("  lands last", at, 3);
    check("  four fills", DbCount(dbFills), 4);
    DbGetFill(3, &f);
    check("  odometer from the trip", (long)f.odometer, 816900);
    check("  trip kept", (long)f.trip, 5000);
    check("  trip flag", (f.flags & fillFlagTripEntered) != 0, 1);
    check("  full tank", (f.flags & fillFlagFullTank) != 0, 1);
    check("  volume mL", (long)f.volume, 25000);
    check("  price", (long)f.price, 14070);
    check("  total", (long)f.total, 3518);
    check("  points stored", f.pointCount, 1);
    check("  250 earned", (long)f.points[0].earned, 250);
    check("  dated by an unshifted clock", f.clockShift, 0);
    check("  a uid", f.uid != 0, 1);
    check("  rate of one", (long)f.fxRate, 1000000);

    /* Trip and odometer disagree. */
    New();
    strcpy(t.odo, "82500"); strcpy(t.trip, "500"); strcpy(t.volume, "30");
    in.odoChoice = feOdoAsk;
    check("a mismatch stops the save", FillEditSave(&e, &t, &in, ask, &at), feOdoMismatch);
    checkStr("  the trip", ask[0], "500.0 km");
    checkStr("  the odometer moved", ask[1], "810.0 km");
    checkStr("  the reading offered", ask[2], "82,190");
    check("  nothing stored", DbCount(dbFills), 4);
    in.odoChoice = feOdoUseTrip;
    check("use the trip", FillEditSave(&e, &t, &in, ask, &at), feSaved);
    DbGetFill(at, &f);
    check("  odometer corrected", (long)f.odometer, 821900);
    in.odoChoice = feOdoAsk;

    /* What cannot be saved. */
    New();
    strcpy(t.odo, "8x1"); strcpy(t.volume, "30");
    check("a stray character", FillEditSave(&e, &t, &in, ask, &at), feBadNumber);
    strcpy(t.odo, ""); strcpy(t.volume, "30");
    check("no odometer and no trip", FillEditSave(&e, &t, &in, ask, &at), feBadNumber);
    strcpy(t.odo, "83000"); t.volume[0] = 0;
    check("no volume and no total", FillEditSave(&e, &t, &in, ask, &at), feNeedVolume);
    check("  still five fills", DbCount(dbFills), 5);

    /* Editing: the fields, and a changed date moves the record. */
    FillEditBegin(&e, 1, &prefs, in.date, in.time, '.', ',', &t);
    checkStr("edit title", FillEditTitle(&e), "Edit Fill");
    checkStr("  odometer in whole km", t.odo, "81190");
    checkStr("  trip", t.trip, "540.0");
    checkStr("  volume", t.volume, "46.00");
    checkStr("  price in cents", t.price, "176.9");
    checkStr("  total", t.total, "81.37");
    FillEditOdoHint(&e, buf);
    checkStr("  the fill before it", buf, "prev 80,650");
    FillEditPointsText(&e, buf);
    checkStr("  no points stored, none added", buf, "none");
    uid = e.fill.uid;
    t.volume[0] = 0;
    check("emptying the volume to retype it", FillEditTyped(&e, feVolume, &t, hint), 0);
    checkStr("  leaves it empty", t.volume, "");
    strcpy(t.volume, "46.00");
    check("  typed back: three known, nothing to work out", FillEditTyped(&e, feVolume, &t, hint), 0);
    checkStr("  total as it was", t.total, "81.37");
    FillEditBegin(&e, 1, &prefs, in.date, in.time, '.', ',', &t);
    in.date = DateYmdToDays(2026, 7, 20);
    check("save with an earlier date", FillEditSave(&e, &t, &in, ask, &at), feSaved);
    check("  moves to the front", at, 0);
    check("  still five fills", DbCount(dbFills), 5);
    DbGetFill(0, &f);
    check("  the new date", f.date, DateYmdToDays(2026, 7, 20));
    check("  the same uid", (long)(f.uid == uid), 1);
    check("  volume unchanged", (long)f.volume, 46000);
    check("  price unchanged", (long)f.price, 17690);
    check("  total unchanged", (long)f.total, 8137);
    DbGetFill(1, &f);
    check("  July 29 follows", f.date, DateYmdToDays(2026, 7, 29));
    DbGetFill(2, &f);
    check("  then the charge: the old copy is gone", f.vehicle, 2);
    in.date = DateYmdToDays(2026, 11, 12);

    /* A charge. */
    FillEditBegin(&e, 2, &prefs, in.date, in.time, '.', ',', &t);
    checkStr("charge title", FillEditTitle(&e), "Edit Charge");
    checkStr("  energy label", FillEditVolumeLabel(&e), "Energy kWh:");
    checkStr("  kWh", t.volume, "38.6");
    checkStr("  price per kWh", t.price, "18.0");
    FillEditPriceLabel(&e, buf);
    checkStr("  price label", buf, "Price c/kWh:");
    checkStr("  paid on the bill", FillEditCashLabel(&e), "Bill");
    check("  three places to charge", FillEditGrades(&e, map), 3);
    check("  home first", map[0], fillGradeHomeAC);
    FillEditOdoHint(&e, buf);
    checkStr("  no reading before the first", buf, "");

    /* A new fill moved to the electric car becomes a charge. */
    New();
    strcpy(t.volume, "40"); strcpy(t.price, "175.9");
    check("to the Ioniq: fuel to charge", FillEditSetVehicle(&e, 2, &t), 1);
    checkStr("  quantities cleared", t.volume, "");
    check("  a home charge", e.fill.grade, fillGradeHomeAC);
    check("picking Home again", FillEditSetGrade(&e, fillGradeHomeAC, &t), 0);
    checkStr("  offers the home rate", t.price, "18.0");
    strcpy(t.volume, "20");
    check("20 kWh", FillEditTyped(&e, feVolume, &t, hint), feTotal);
    checkStr("  at 18 c", t.total, "3.60");
    check("back to the Outback: charge to fuel", FillEditSetVehicle(&e, 1, &t), 1);
    check("  regular", e.fill.grade, fillGradeRegular);

    /* Currency: a US fill is typed in gallons and earns no Petro-Points. */
    New();
    FillEditSetCurrency(&e, curUSD, &t);
    check("USD: US gallons", e.entryVol, unitVolUSGal);
    check("  the rate shows", FillEditForeign(&e), 1);
    check("  with a rate in it", t.rate[0] != 0, 1);
    FillEditRateLabel(&e, buf);
    checkStr("  rate label", buf, "CAD per USD");
    FillEditPriceLabel(&e, buf);
    checkStr("  price label", buf, "Price c/gal:");
    FillEditPointsText(&e, buf);
    checkStr("  no Canadian points", buf, "none");
    strcpy(t.volume, "10");
    FillEditSetEntryUnit(&e, unitVolLitres, &t);
    checkStr("10 US gallons shown in litres", t.volume, "37.85");
    FillEditSetCurrency(&e, curCAD, &t);
    checkStr("back to CAD: no rate", t.rate, "");
    FillEditPointsText(&e, buf);
    check("  Petro-Points again", strncmp(buf, "Petro-Points", 12), 0);
    FillEditSetPayment(&e, fillPaymentCard);
    check("payment kept", e.fill.payment, fillPaymentCard);

    /* A stored redemption comes off the total. */
    memset(&f, 0, sizeof f);
    f.date = DateYmdToDays(2026, 11, 13); f.time = fillTimeUnknown; f.flags = fillFlagFullTank; f.vehicle = 1; f.odometer = 830000;
    f.volume = 40000; f.price = 17500; f.total = 6800; f.discount = 200; f.currency = curCAD; f.fxRate = 1000000UL;
    f.pointCount = 1; f.points[0].programme = 2; f.points[0].earned = 400; f.points[0].redeemed = 2000;
    check("a fill with 2,000 points redeemed", DbAddFill(&f, &at), dbOk);
    FillEditBegin(&e, at, &prefs, in.date, in.time, '.', ',', &t);
    check("  two dollars off", (long)e.discount, 200);
    FillEditPointsText(&e, buf);
    checkStr("  net points", buf, "Petro-Points -1,600 pts");
    FillEditTyped(&e, feVolume, &t, hint);
    check("retype volume and price", FillEditTyped(&e, fePrice, &t, hint), feTotal);
    checkStr("  70.00 less 2.00", t.total, "68.00");
    checkStr("  hint", hint, "total less points");
    in.date = f.date; in.time = fillTimeUnknown;
    check("  saves", FillEditSave(&e, &t, &in, ask, &at), feSaved);
    DbGetFill(at, &f);
    check("  redemption kept", (long)f.points[0].redeemed, 2000);
    check("  earned kept as stored", (long)f.points[0].earned, 400);
    check("  discount kept", (long)f.discount, 200);
    in.date = DateYmdToDays(2026, 11, 12); in.time = 10 * 60;

    /* Delete. */
    {
        UInt16 fills = DbCount(dbFills), stones = DbCount(dbDeleted);
        check("delete the fill being edited", FillEditDelete(&e), dbOk);
        check("  one fill fewer", DbCount(dbFills), fills - 1);
        check("  one tombstone more", DbCount(dbDeleted), stones + 1);
        check("  a new fill has nothing to delete", FillEditDelete(&e), dbErrRange);
    }
    check("save the set", DbSave(), dbOk);
    DbCloseAll();
    check("reopen", DbOpenAll(work, &clock), dbOk);
    check("  five fills", DbCount(dbFills), 5);
    DbCloseAll();

    /* An empty log: a first vehicle, then a first fill. */
    check("open an empty folder", DbOpenAll(empty, &clock), dbOk);
    check("a first vehicle is made", FillEditEnsureVehicle(), 1);
    check("  once", FillEditEnsureVehicle(), 0);
    New();
    check("  the fill is on it", e.fill.vehicle, 1);
    checkStr("  no previous reading", (FillEditOdoHint(&e, buf), buf), "");
    strcpy(t.odo, "1000"); strcpy(t.volume, "30");
    FillEditTyped(&e, feVolume, &t, hint);
    strcpy(t.total, "50");
    check("volume and total give the price", FillEditTyped(&e, feTotal, &t, hint), fePrice);
    checkStr("  50.00 over 30 L", t.price, "166.7");
    check("  saves", FillEditSave(&e, &t, &in, ask, &at), feSaved);
    check("  one fill", DbCount(dbFills), 1);
    check("  written", DbSave(), dbOk);
    DbCloseAll();
    check("  and read back", DbOpenAll(empty, &clock), dbOk);
    check("  one fill, one vehicle", DbCount(dbFills) + DbCount(dbVehicles), 2);
    DbCloseAll();

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

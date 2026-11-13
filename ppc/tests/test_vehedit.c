/* Host tests for the Pocket PC build's Prefs bytes (app/settings.c) and
 * vehicle rules (app/vehedit.c), against a working copy of the fixture set
 * tools/mkpdb writes (argv[1]): an Outback (fuel, two fills, last at
 * 81,190 km) and an Ioniq 5 (electric, one charge at 18,420 km). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../app/settings.h"
#include "../app/vehedit.h"
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

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : "build/host/work3";
    DbClock clock;
    FuelPrefs p, q;
    UInt8 blob[settingsSize + 8];
    VehEdit e;
    VehEditForm f;
    VehicleRecord v;
    UInt16 idx[vehMaxVehicles], n;
    char name[40], kind[40], fills[40], odo[40], status[48];

    /* Prefs to bytes and back. */
    PrefsSetDefaults(&p);
    p.distanceUnit = unitDistMiles; p.volumeUnit = unitVolUSGal; p.economyUnit = unitEcoMpgUS; p.priceUnit = unitPricePerUSGal;
    p.priceFormat = unitPriceFormatDollars; p.homeCurrency = curUSD; p.defaultCurrency = curCAD; p.lastCurrency = curEUR;
    p.lastPayment = fillPaymentCard; p.currentVehicle = 2; p.fxRate[curCAD] = 731234UL; p.fxRate[7] = 0xFEDCBA98UL;
    p.autoBackup = 0; p.backupSetsKept = 10; p.lastBackupDate = 44876;
    check("prefs pack", (long)SettingsPack(&p, blob), settingsSize);
    PrefsSetDefaults(&q);
    check("  unpack", SettingsUnpack(blob, settingsSize, &q), 0);
    check("  distance", q.distanceUnit, unitDistMiles);
    check("  volume", q.volumeUnit, unitVolUSGal);
    check("  economy", q.economyUnit, unitEcoMpgUS);
    check("  price unit", q.priceUnit, unitPricePerUSGal);
    check("  price format", q.priceFormat, unitPriceFormatDollars);
    check("  home", q.homeCurrency, curUSD);
    check("  default currency", q.defaultCurrency, curCAD);
    check("  last currency", q.lastCurrency, curEUR);
    check("  last payment", q.lastPayment, fillPaymentCard);
    check("  current vehicle", q.currentVehicle, 2);
    check("  a kept rate", (long)(q.fxRate[curCAD] == 731234UL), 1);
    check("  the last rate slot", (long)(q.fxRate[7] == 0xFEDCBA98UL), 1);
    check("  auto backup off", q.autoBackup, 0);
    check("  sets kept", q.backupSetsKept, 10);
    check("  last backup", q.lastBackupDate, 44876);
    PrefsSetDefaults(&q);
    check("too short is refused", SettingsUnpack(blob, settingsSize - 1, &q), -1);
    check("  defaults untouched", q.distanceUnit, unitDistKm);
    blob[2] = 9; blob[7] = 77; blob[8] = curLastUsed;
    PrefsSetDefaults(&q);
    check("a later version's longer value reads", SettingsUnpack(blob, settingsSize + 8, &q), 0);
    check("  a unit out of range keeps its default", q.distanceUnit, unitDistKm);
    check("  so does a currency", q.homeCurrency, curCAD);
    check("  last used is allowed", q.defaultCurrency, curLastUsed);
    check("  the rest is read", q.volumeUnit, unitVolUSGal);

    clock.now = Now; clock.clockShift = 0;
    check("open", DbOpenAll(work, &clock), dbOk);
    DbEnsureUids();
    PrefsSetDefaults(&p);

    /* The list. */
    check("two vehicles listed", VehListCollect(0, idx, vehMaxVehicles), 2);
    check("row reads", VehListRow(idx[0], &p, '.', ',', name, kind, fills, odo), 1);
    checkStr("  name", name, "Outback");
    checkStr("  kind", kind, "Fuel");
    checkStr("  fills", fills, "2");
    checkStr("  last odometer", odo, "81,190");
    VehListRow(idx[1], &p, '.', ',', name, kind, fills, odo);
    checkStr("  the Ioniq", kind, "Electric");
    checkStr("  its odometer", odo, "18,420");
    p.distanceUnit = unitDistMiles;
    VehListRow(idx[0], &p, '.', ',', name, kind, fills, odo);
    checkStr("  in miles", odo, "50,449");
    p.distanceUnit = unitDistKm;
    check("next free id", VehNextId(), 3);
    check("Outback fills", VehFillCount(1), 2);
    check("Ioniq last odometer", (long)VehLastOdometer(2), 184200);

    /* Edit the Outback. */
    check("begin on the Outback", VehEditBegin(&e, 0, '.', &f), 1);
    checkStr("  name", f.name, "Outback");
    checkStr("  start", f.start, "80650");
    checkStr("  tank in litres", f.tank, "60");
    checkStr("  no home rate", f.homeRate, "");
    check("  currency: use Prefs", f.currency, 0);
    checkStr("  tank label", VehEditTankLabel(f.powertrain), "Tank (L):");
    check("  a grade", VehEditShowsGrade(f.powertrain), 1);
    check("  no home rate row", VehEditShowsHomeRate(f.powertrain), 0);
    check("  cannot be deleted", VehEditCanDelete(&e), 0);
    VehEditStatus(&e, status);
    checkStr("  why", status, "2 fills: archive it instead of deleting");
    check("  delete refused", VehEditDelete(&e, &p), dbErrRange);
    strcpy(f.name, "Outback XT"); strcpy(f.tank, "63"); f.grade = fillGradePremium; f.currency = curUSD + 1; f.tripMode = vehTripNever;
    check("save", VehEditSave(&e, &f, '.', ',', &p), veSaved);
    check("  still two vehicles", DbCount(dbVehicles), 2);
    DbGetVehicle(0, &v);
    checkStr("  renamed", v.name, "Outback XT");
    check("  tank", (long)v.tankSize, 63000);
    check("  premium", v.fuelDefault, fillGradePremium);
    check("  USD", v.defaultCurrency, curUSD);
    check("  trip never reset", v.tripMode, vehTripNever);
    check("  start unchanged", (long)v.startOdo, 806500);
    check("  same id", v.id, 1);
    check("  uid kept", (long)(v.uid == e.veh.uid && v.uid != 0), 1);
    check("  not taken as the only vehicle", p.currentVehicle, 255);
    strcpy(f.name, "  ");
    check("a blank name", VehEditSave(&e, &f, '.', ',', &p), veNeedName);
    strcpy(f.name, "Outback"); strcpy(f.start, "8o650");
    check("a stray letter", VehEditSave(&e, &f, '.', ',', &p), veBadNumber);

    /* The electric car. */
    VehEditBegin(&e, 1, '.', &f);
    checkStr("Ioniq battery label", VehEditTankLabel(f.powertrain), "Battery kWh:");
    checkStr("  kWh", f.tank, "77.0");
    checkStr("  home rate in cents", f.homeRate, "18.0");
    check("  no grade row", VehEditShowsGrade(f.powertrain), 0);
    check("  home rate row", VehEditShowsHomeRate(f.powertrain), 1);
    strcpy(f.tank, "77.4"); strcpy(f.homeRate, "12.5");
    check("  saves", VehEditSave(&e, &f, '.', ',', &p), veSaved);
    DbGetVehicle(1, &v);
    check("  battery Wh", (long)v.batteryWh, 77400);
    check("  home rate", (long)v.homeRate, 1250);

    /* A new one, in miles, then archived, then deleted. */
    check("begin a new vehicle", VehEditBegin(&e, vehEditNew, '.', &f), 1);
    check("  id 3", e.veh.id, 3);
    checkStr("  tank 60", f.tank, "60");
    VehEditStatus(&e, status);
    checkStr("  no status", status, "");
    strcpy(f.name, "F-150"); f.odoUnit = vehOdoMiles; strcpy(f.start, "12,500");
    check("  saves", VehEditSave(&e, &f, '.', ',', &p), veSaved);
    check("  three vehicles", DbCount(dbVehicles), 3);
    DbGetVehicle(2, &v);
    check("  12,500 miles in km x 10", (long)v.startOdo, 201168);
    check("  a uid", v.uid != 0, 1);
    VehListRow(2, &p, '.', ',', name, kind, fills, odo);
    checkStr("  no fills", fills, "0");
    checkStr("  odometer from the start", odo, "20,116");
    VehEditBegin(&e, 2, '.', &f);
    checkStr("  start shown in miles", f.start, "12500");
    f.archived = 1;
    VehEditSave(&e, &f, '.', ',', &p);
    check("archived: two listed", VehListCollect(0, idx, vehMaxVehicles), 2);
    check("  three with archived shown", VehListCollect(1, idx, vehMaxVehicles), 3);
    VehListRow(2, &p, '.', ',', name, kind, fills, odo);
    checkStr("  marked", name, "F-150 (archived)");
    p.currentVehicle = 3;
    n = DbCount(dbDeleted);
    check("  can be deleted", VehEditCanDelete(&e), 1);
    check("  delete", VehEditDelete(&e, &p), dbOk);
    check("  two vehicles", DbCount(dbVehicles), 2);
    check("  a tombstone", DbCount(dbDeleted), n + 1);
    check("  the list goes back to all", p.currentVehicle, 255);

    /* Sixteen is the limit. */
    while (VehNextId()) {
        VehEditBegin(&e, vehEditNew, '.', &f);
        strcpy(f.name, "Fleet");
        if (VehEditSave(&e, &f, '.', ',', &p) != veSaved) break;
    }
    check("sixteen vehicles", DbCount(dbVehicles), 16);
    check("  a seventeenth is refused", VehEditBegin(&e, vehEditNew, '.', &f), 0);
    check("save the set", DbSave(), dbOk);
    DbCloseAll();

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

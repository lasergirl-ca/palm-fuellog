/* Vehicles without the window (vehedit.h), after src/vehicles.c. */
#include "vehedit.h"
#include "../store/db.h"
#include "../../src/units.h"
#include "../../src/text.h"
#include <string.h>

static VehicleRecord gScanVeh;
static FillRecord gScanFill;

UInt8 VehNextId(void)
{
    UInt16 used = 0, n = DbCount(dbVehicles), i;
    UInt8 id;
    for (i = 0; i < n; i++)
        if (DbGetVehicle(i, &gScanVeh) == dbOk && gScanVeh.id >= 1 && gScanVeh.id <= 16) used |= (UInt16)(1 << (gScanVeh.id - 1));
    for (id = 1; id <= vehMaxVehicles; id++) if (!(used & (1 << (id - 1)))) return id;
    return 0;
}

UInt16 VehFillCount(UInt8 id)
{
    UInt16 n = DbCount(dbFills), i, count = 0;
    for (i = 0; i < n; i++) if (DbGetFill(i, &gScanFill) == dbOk && gScanFill.vehicle == id) count++;
    return count;
}

UInt32 VehLastOdometer(UInt8 id)
{
    UInt16 n = DbCount(dbFills);
    while (n--) if (DbGetFill(n, &gScanFill) == dbOk && gScanFill.vehicle == id) return gScanFill.odometer;
    /* No fills yet: the reading tracking began at is the previous reading. */
    return DbVehicleById(id, &gScanVeh) ? gScanVeh.startOdo : 0;
}

/* ---- the list ---- */

UInt16 VehListCollect(int showArchived, UInt16 *index, UInt16 max)
{
    UInt16 n = DbCount(dbVehicles), i, count = 0;
    for (i = 0; i < n && count < max; i++) {
        if (DbGetVehicle(i, &gScanVeh) != dbOk) continue;
        if (gScanVeh.archived && !showArchived) continue;
        index[count++] = i;
    }
    return count;
}

int VehListRow(UInt16 dbIndex, const FuelPrefs *p, char point, char thousands, char *name, char *kind, char *fills, char *odo)
{
    static const char *const power[] = { "Fuel", "Hybrid", "Plug-in", "Electric" };
    VehicleRecord v;
    UInt32 last;
    if (DbGetVehicle(dbIndex, &v) != dbOk) return 0;
    strcpy(name, v.name);
    if (v.archived) strcat(name, " (archived)");
    strcpy(kind, power[v.powertrain < 4 ? v.powertrain : 0]);
    TextFixed(fills, VehFillCount(v.id), 0, point, thousands);
    last = VehLastOdometer(v.id);
    odo[0] = 0;
    if (last) TextFixed(odo, (Int32)(UnitDistToDisplay(last, p->distanceUnit) / 10), 0, point, thousands);
    return 1;
}

/* ---- the form ---- */

int VehEditBegin(VehEdit *e, UInt16 index, char point, VehEditForm *f)
{
    memset(e, 0, sizeof *e);
    memset(f, 0, sizeof *f);
    e->index = vehEditNew;
    if (index != vehEditNew && DbGetVehicle(index, &e->veh) == dbOk) {
        e->index = index;
    } else {
        memset(&e->veh, 0, sizeof e->veh);
        e->veh.id = VehNextId();
        if (!e->veh.id) return 0;
        e->veh.defaultCurrency = vehCurrencyUsePrefs;
        e->veh.tripMode = vehTripReset;
        e->veh.tankSize = 60000;
    }
    strcpy(f->name, e->veh.name);
    f->powertrain = e->veh.powertrain < 4 ? e->veh.powertrain : 0;
    f->odoUnit = e->veh.odoUnit == vehOdoMiles ? vehOdoMiles : vehOdoKm;
    if (e->veh.startOdo) TextFixed(f->start, (Int32)(UnitDistToDisplay(e->veh.startOdo, f->odoUnit) / 10), 0, point, 0);
    if (f->powertrain == vehPowerElectric) TextFixed(f->tank, (Int32)(e->veh.batteryWh / 100), 1, point, 0);
    else TextFixed(f->tank, (Int32)(e->veh.tankSize / 1000), 0, point, 0);
    if (e->veh.homeRate) TextPrice(f->homeRate, e->veh.homeRate, 1, point);
    f->grade = e->veh.fuelDefault < 4 ? e->veh.fuelDefault : 0;
    f->currency = e->veh.defaultCurrency == vehCurrencyUsePrefs ? 0 : (UInt8)(e->veh.defaultCurrency + 1);
    f->tripMode = e->veh.tripMode < 3 ? e->veh.tripMode : 0;
    f->archived = e->veh.archived ? 1 : 0;
    return 1;
}

const char *VehEditTankLabel(UInt8 powertrain) { return powertrain == vehPowerElectric ? "Battery kWh:" : "Tank (L):"; }
int VehEditShowsGrade(UInt8 powertrain) { return powertrain != vehPowerElectric; }
int VehEditShowsHomeRate(UInt8 powertrain) { return powertrain == vehPowerElectric || powertrain == vehPowerPlugIn; }

int VehEditCanDelete(const VehEdit *e)
{
    return e->index != vehEditNew && VehFillCount(e->veh.id) == 0;
}

void VehEditStatus(const VehEdit *e, char *buf)
{
    UInt16 fills = e->index == vehEditNew ? 0 : VehFillCount(e->veh.id);
    buf[0] = 0;
    if (!fills) return;
    /* Delete gives way to the reason it is unavailable. */
    TextFixed(buf, fills, 0, '.', 0);
    strcat(buf, fills == 1 ? " fill: archive it instead of deleting" : " fills: archive it instead of deleting");
}

Int32 VehEditSave(VehEdit *e, const VehEditForm *f, char point, char thousands, FuelPrefs *prefs)
{
    UInt8 buf[vehRecordSize + recTrailerSize + 8];
    UInt32 v = 0, n;
    Int32 r, err;
    const char *name = f->name;
    while (*name == ' ') name++;
    if (!name[0]) return veNeedName;
    r = TextParseFixed(f->start, 1, point, thousands, &v);
    if (r < 0) return veBadNumber;
    e->veh.startOdo = r == 1 ? UnitDistFromDisplay(v, f->odoUnit) : 0;
    if (f->powertrain == vehPowerElectric) {
        r = TextParseFixed(f->tank, 1, point, thousands, &v);
        if (r < 0) return veBadNumber;
        e->veh.batteryWh = r == 1 ? v * 100 : 0;
    } else {
        r = TextParseFixed(f->tank, 0, point, thousands, &v);
        if (r < 0) return veBadNumber;
        e->veh.tankSize = r == 1 ? v * 1000 : 0;
    }
    r = TextParsePrice(f->homeRate, 1, point, thousands, &v);
    if (r < 0) return veBadNumber;
    e->veh.homeRate = r == 1 ? v : 0;
    strncpy(e->veh.name, name, vehNameLen - 1); e->veh.name[vehNameLen - 1] = 0;
    e->veh.powertrain = f->powertrain < 4 ? f->powertrain : 0;
    e->veh.odoUnit = f->odoUnit == vehOdoMiles ? vehOdoMiles : vehOdoKm;
    e->veh.fuelDefault = f->grade < 4 ? f->grade : 0;
    e->veh.defaultCurrency = f->currency == 0 ? vehCurrencyUsePrefs : (UInt8)(f->currency - 1);
    e->veh.tripMode = f->tripMode < 3 ? f->tripMode : 0;
    e->veh.archived = f->archived ? 1 : 0;
    n = VehiclePack(&e->veh, buf);
    if (e->index == vehEditNew) {
        err = DbAppend(dbVehicles, buf, n);
        if (err == dbOk) e->index = (UInt16)(DbCount(dbVehicles) - 1);
    } else {
        err = DbReplace(dbVehicles, e->index, buf, n);
    }
    if (err != dbOk) return veSaveFailed;
    /* The only vehicle is the one the list shows. */
    if (prefs->currentVehicle == 255 && DbCount(dbVehicles) == 1) prefs->currentVehicle = e->veh.id;
    return veSaved;
}

Int32 VehEditDelete(VehEdit *e, FuelPrefs *prefs)
{
    Int32 err;
    if (!VehEditCanDelete(e)) return dbErrRange;
    err = DbDelete(dbVehicles, e->index);
    if (err != dbOk) return err;
    if (prefs->currentVehicle == e->veh.id) prefs->currentVehicle = 255;
    e->index = vehEditNew;
    return dbOk;
}

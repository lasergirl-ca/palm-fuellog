/* The fill list's rows and footer (fillview.h). */
#include "fillview.h"
#include "../store/db.h"
#include "../../src/calc.h"
#include "../../src/units.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <string.h>

static FillRecord gScan, gRow;
static VehicleRecord gVeh;

UInt16 FillViewCollect(UInt8 vehicle, UInt16 *index, UInt16 max)
{
    UInt16 n = DbCount(dbFills), i, count = 0;
    for (i = n; i > 0 && count < max; i--) {
        if (vehicle != 255) {
            if (DbGetFill((UInt16)(i - 1), &gScan) != dbOk || gScan.vehicle != vehicle) continue;
        }
        index[count++] = (UInt16)(i - 1);
    }
    return count;
}

UInt32 FillViewRowEconomy(UInt16 k, const FillRecord *f)
{
    UInt32 volume = f->volume;
    Int32 j;
    if (!(f->flags & fillFlagFullTank) || (f->flags & fillFlagMissedPrev)) return 0;
    for (j = (Int32)k - 1; j >= 0; j--) {
        if (DbGetFill((UInt16)j, &gScan) != dbOk) return 0;
        if (gScan.vehicle != f->vehicle) continue;
        if (gScan.flags & fillFlagFullTank) {
            if (gScan.odometer >= f->odometer) return 0;
            if (f->flags & fillFlagCharge) return CalcKwh100kmX10(volume, f->odometer - gScan.odometer);
            return CalcL100kmX10(volume, f->odometer - gScan.odometer);
        }
        if (gScan.flags & fillFlagMissedPrev) return 0;
        volume += gScan.volume;
    }
    return 0;
}

static int Miles(const FuelPrefs *p) { return p->economyUnit == unitEcoMpgUS || p->economyUnit == unitEcoMpgImp; }

UInt32 FillViewEcoDisplay(UInt32 eco, int charge, const FuelPrefs *p)
{
    if (eco == 0) return 0;
    if (charge) {
        /* km per kWh x 10 = 10000 / (kWh/100 km x 10); miles divide by 1.609 */
        if (p->distanceUnit == unitDistMiles) return CalcMulDiv(10000UL, 1000UL, eco * 1609UL);
        return (10000UL + eco / 2) / eco;
    }
    return UnitEcoFuelToDisplay(eco, p->economyUnit == unitEcoL100km ? unitEcoKmPerL : p->economyUnit);
}

const char *FillViewEcoHeader(int electric, const FuelPrefs *p)
{
    if (electric) return p->distanceUnit == unitDistMiles ? "MPE" : "KPE";
    return Miles(p) ? "MPG" : "KPL";
}

int FillViewElectric(const FuelPrefs *p)
{
    return p->currentVehicle != 255 && DbVehicleById(p->currentVehicle, &gVeh) && gVeh.powertrain == vehPowerElectric;
}

int FillViewRow(UInt16 k, const FuelPrefs *p, char point, char thousands, char col[fillViewCols][fillViewText])
{
    FillRecord *f = &gRow;
    UInt32 eco;
    if (DbGetFill(k, f) != dbOk) return 0;
    DateShortText(col[0], f->date);
    TextFixed(col[1], (Int32)(UnitDistToDisplay(f->odometer, p->distanceUnit) / 10), 0, point, thousands);
    if (f->flags & fillFlagCharge) {
        TextFixed(col[2], (Int32)((f->volume + 50) / 100), 1, point, 0);        /* Wh to kWh, one decimal */
    } else {
        /* One decimal of volume in the list; the form keeps them all. */
        UInt32 shown = UnitVolToDisplay(f->volume, p->volumeUnit), div = 1;
        UInt8 d = UnitVolDecimals(p->volumeUnit);
        while (d-- > 1) div *= 10;
        TextFixed(col[2], (Int32)((shown + div / 2) / div), 1, point, 0);
    }
    TextFixed(col[3], (Int32)f->total, 2, point, 0);
    eco = FillViewRowEconomy(k, f);
    if (eco) TextFixed(col[4], (Int32)FillViewEcoDisplay(eco, (f->flags & fillFlagCharge) != 0, p), 1, point, 0);
    else strcpy(col[4], (f->flags & fillFlagFullTank) ? "" : "partial");
    return 1;
}

void FillViewFooter(const FuelPrefs *p, UInt16 today, char point, char thousands, char *avg, char *month)
{
    UInt16 count = DbCount(dbFills), i, y, fy;
    UInt32 fullVol = 0, firstFullOdo = 0, lastFullOdo = 0, monthCents = 0, eco;
    UInt8 m, d, tm, td;
    FillRecord *f = &gRow;
    char num[16];
    /* The average covers fuel, or charges when the selected car is electric; never both. */
    int electric = FillViewElectric(p);

    DateDaysToYmd(today, &y, &tm, &td);
    for (i = 0; i < count; i++) {
        if (DbGetFill(i, f) != dbOk) continue;
        if (p->currentVehicle != 255 && f->vehicle != p->currentVehicle) continue;
        DateDaysToYmd(f->date, &fy, &m, &d);
        if (fy == y && m == tm) monthCents += CalcHomeCents(f->total, f->fxRate);   /* fuel and electricity */
        if (((f->flags & fillFlagCharge) != 0) != electric) continue;
        if (f->flags & fillFlagFullTank) {
            if (firstFullOdo == 0) firstFullOdo = f->odometer; else fullVol += f->volume;
            lastFullOdo = f->odometer;
        } else if (firstFullOdo) {
            fullVol += f->volume;
        }
    }
    avg[0] = 0;
    /* CalcL100kmX10 and CalcKwh100kmX10 scale alike: Wh over 0.1 km */
    eco = (lastFullOdo > firstFullOdo) ? CalcL100kmX10(fullVol, lastFullOdo - firstFullOdo) : 0;
    if (eco) {
        strcpy(avg, "Avg ");
        TextFixed(num, (Int32)FillViewEcoDisplay(eco, electric, p), 1, point, 0);
        strcat(avg, num); strcat(avg, " "); strcat(avg, FillViewEcoHeader(electric, p));
    }
    strcpy(month, DateMonthName(tm));
    strcat(month, " $");
    TextFixed(num, (Int32)monthCents, 2, point, thousands);
    strcat(month, num);
}

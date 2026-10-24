#include "service.h"
#include "datecalc.h"
#include "calc.h"

static void Cp(char *d, const char *s, UInt16 n) EXPORT_SEC;
static void Cp(char *d, const char *s, UInt16 n)
{
    UInt16 i = 0;
    while (s[i] && i + 1 < n) { d[i] = s[i]; i++; }
    d[i] = 0;
}

UInt32 SvcPaceX10(UInt32 odoFirst, UInt32 odoLast, UInt16 dateFirst, UInt16 dateLast)
{
    UInt32 days = dateLast > dateFirst ? (UInt32)(dateLast - dateFirst) : 0;
    if (days == 0 || odoLast <= odoFirst) return 0;
    return (odoLast - odoFirst + days / 2) / days;
}

UInt32 SvcDefaultWarnDist(UInt32 intervalDist) { return (intervalDist + 5) / 10; }

/* A month is 30.4375 days (487/16): months x 10 = days x 160 / 487. */
UInt16 SvcMonthsX10(UInt16 days) { return (UInt16)(((UInt32)days * 160 + 243) / 487); }

void SvcCompute(const ServiceTypeRecord *t, UInt8 haveLast, UInt16 lastDate, UInt32 lastOdo,
                UInt32 odoNow, UInt16 today, UInt32 paceX10, SvcDue *d)
{
    UInt8 *p = (UInt8 *)d;
    UInt32 i;
    for (i = 0; i < sizeof *d; i++) p[i] = 0;
    if (t->intervalDist == 0 && t->intervalMonths == 0) { d->status = svcStatusNoInterval; return; }
    if (!haveLast) { d->status = svcStatusNoVisit; return; }
    if (t->intervalDist) {
        UInt32 dueOdo = lastOdo + t->intervalDist;
        d->hasDist = 1;
        d->kmLeftX10 = dueOdo >= odoNow ? (Int32)(dueOdo - odoNow) : -(Int32)(odoNow - dueOdo);
        if (paceX10) {
            UInt32 daysAhead = d->kmLeftX10 > 0 ? ((UInt32)d->kmLeftX10 + paceX10 / 2) / paceX10 : 0;
            d->distDue = (UInt16)(today + daysAhead);
        }
    }
    if (t->intervalMonths) {
        d->hasTime = 1;
        d->timeDue = DateAddMonths(lastDate, t->intervalMonths);
        d->daysLeft = (Int32)d->timeDue - (Int32)today;
    }
    if (d->hasTime && d->distDue) d->due = d->distDue < d->timeDue ? d->distDue : d->timeDue;
    else if (d->hasTime) d->due = d->timeDue;
    else d->due = d->distDue;
    if ((d->hasDist && d->kmLeftX10 < 0) || (d->hasTime && d->daysLeft < 0)) d->status = svcStatusOverdue;
    else if ((d->hasDist && (UInt32)d->kmLeftX10 <= t->warnDist)
          || (d->hasTime && d->daysLeft <= (Int32)t->warnDays)
          || (d->distDue && (Int32)d->distDue - (Int32)today <= (Int32)t->warnDays)) d->status = svcStatusSoon;
    else d->status = svcStatusOK;
}

/* Fuel cars get all nine; electric cars skip oil, plugs and transmission fluid. */
static const char *const kFuelPresets[9] = {
    "Oil and filter", "Tyre rotation", "Engine air filter", "Cabin air filter", "Brake fluid",
    "Coolant", "Spark plugs", "Transmission fluid", "Wiper blades" };
static const char *const kElectricPresets[5] = {
    "Tyre rotation", "Cabin air filter", "Brake fluid", "Coolant", "Wiper blades" };

UInt8 SvcPresetCount(UInt8 powertrain) { return powertrain == vehPowerElectric ? 5 : 9; }

void SvcPreset(UInt8 powertrain, UInt8 i, ServiceTypeRecord *t)
{
    UInt8 *p = (UInt8 *)t;
    UInt32 k;
    for (k = 0; k < sizeof *t; k++) p[k] = 0;
    t->id = i;
    t->warnDays = 14;
    Cp(t->name, powertrain == vehPowerElectric ? kElectricPresets[i % 5] : kFuelPresets[i % 9], svtNameLen);
}

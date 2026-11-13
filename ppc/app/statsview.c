/* Stats as lines (textview.h), after src/stats_form.c. */
#include "textview.h"
#include "pgmedit.h"
#include "../store/db.h"
#include "../../src/stats.h"
#include "../../src/calc.h"
#include "../../src/units.h"
#include "../../src/currency.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <string.h>

static Stats gStats;
static FillRecord gFill;
static ServiceVisitRecord gVisit;
static UInt8 gBuf[dbMaxRecord + 16];

UInt32 StatsViewServiceCents(UInt8 vehicle)
{
    UInt16 n = DbCount(dbServiceVisits), i;
    UInt32 cents = 0, len;
    for (i = 0; i < n; i++) {
        if (DbGet(dbServiceVisits, i, gBuf, sizeof gBuf, &len) != dbOk || ServiceVisitUnpack(gBuf, len, &gVisit) != 0) continue;
        if (vehicle != 255 && gVisit.vehicle != vehicle) continue;
        cents += CalcHomeCents(gVisit.cost, gVisit.fxRate ? gVisit.fxRate : fxOne);
    }
    return cents;
}

/* ---- building lines ---- */

typedef struct {
    TextLine *lines;
    UInt16 count, max;
} Out;

static TextLine *Line(Out *o, UInt8 style)
{
    static TextLine spare;
    TextLine *l = o->count < o->max ? &o->lines[o->count++] : &spare;
    memset(l, 0, sizeof *l);
    l->style = style;
    return l;
}

/* A cell's text, cut to fit. */
static void Put(char *cell, const char *s)
{
    int i;
    for (i = 0; i < tvText - 1 && s[i]; i++) cell[i] = s[i];
    cell[i] = 0;
}

static void Row(Out *o, const char *label, const char *value)
{
    TextLine *l = Line(o, tvRow);
    Put(l->cell[0], label);
    Put(l->cell[1], value);
}

static void Money(char *b, UInt32 cents, char point, char thousands)
{
    strcpy(b, "$");
    TextFixed(b + 1, (Int32)cents, 2, point, thousands);
}

/* Volume to one decimal in the display unit. */
static void VolText(char *b, UInt32 ml, const FuelPrefs *p, char point, char thousands)
{
    UInt32 shown = UnitVolToDisplay(ml, p->volumeUnit), div = 1;
    UInt8 d = UnitVolDecimals(p->volumeUnit);
    while (d-- > 1) div *= 10;
    TextFixed(b, (Int32)((shown + div / 2) / div), 1, point, thousands);
}

static void EcoText(char *b, UInt32 l100x10, const FuelPrefs *p, char point)
{
    TextFixed(b, (Int32)UnitEcoFuelToDisplay(l100x10, p->economyUnit < 4 ? p->economyUnit : unitEcoL100km), 1, point, 0);
}

UInt16 StatsViewBuild(const FuelPrefs *p, UInt16 today, char point, char thousands, TextLine *lines, UInt16 max)
{
    Out o;
    const Stats *s = &gStats;
    char b[64], n1[24], n2[24];
    UInt16 n = DbCount(dbFills), i;
    UInt32 service, eco;
    UInt8 id, eu = p->economyUnit < 4 ? p->economyUnit : unitEcoL100km;
    int electricOnly, any = 0;
    TextLine *l;

    o.lines = lines; o.count = 0; o.max = max;
    StatsBegin(&gStats);
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gFill) != dbOk) continue;
        if (p->currentVehicle != 255 && gFill.vehicle != p->currentVehicle) continue;
        StatsAdd(&gStats, &gFill);
    }
    service = StatsViewServiceCents(p->currentVehicle);
    if (s->count == 0) {
        l = Line(&o, tvNote);
        Put(l->cell[0], "No fills yet for this choice.");
        return o.count;
    }
    electricOnly = s->fuelCount == 0 && s->chargeCount > 0;

    /* "Since Jul 29, 8 fills" */
    l = Line(&o, tvNote);
    strcpy(b, "Since "); DateShortText(b + 6, s->firstDate);
    strcat(b, ", "); TextFixed(n1, s->count, 0, point, thousands); strcat(b, n1); strcat(b, s->count == 1 ? " fill" : " fills");
    Put(l->cell[0], b);

    /* Headlines: economy and spend in large type, captions under. */
    l = Line(&o, tvHeadline);
    eco = electricOnly ? StatsAvgEcoElectric(s) : StatsAvgEco(s);
    if (!eco) strcpy(n1, "--");
    else if (electricOnly) TextFixed(n1, (Int32)eco, 1, point, 0);
    else EcoText(n1, eco, p, point);
    Put(l->cell[0], n1);
    Money(n2, s->homeCents, point, thousands);
    Put(l->cell[1], n2);
    l = Line(&o, tvCaption);
    strcpy(b, electricOnly ? "kWh/100 km" : UnitEcoLabel(eu)); strcat(b, " average");
    Put(l->cell[0], b);
    strcpy(b, electricOnly ? "energy spend, " : (s->chargeCount ? "fuel and power, " : "fuel spend, ")); strcat(b, CurrencyCode(p->homeCurrency));
    Put(l->cell[1], b);
    Line(&o, tvRule);

    TextFixed(n1, (Int32)(UnitDistToDisplay(s->distance, p->distanceUnit) / 10), 0, point, thousands);
    strcpy(b, n1); strcat(b, " "); strcat(b, UnitDistLabel(p->distanceUnit));
    Row(&o, "Distance", b);
    if (s->fuelCount) {
        VolText(n1, s->fuelMl, p, point, thousands);
        strcpy(b, n1); strcat(b, " "); strcat(b, UnitVolLabel(p->volumeUnit));
        Row(&o, "Fuel used", b);
    }
    if (s->chargeCount) {
        TextFixed(n1, (Int32)((s->wh + 50) / 100), 1, point, thousands);
        strcpy(b, n1); strcat(b, " kWh, "); Money(b + strlen(b), s->elecHomeCents, point, thousands);
        Row(&o, s->fuelCount ? "Electricity" : "Energy used", b);
    }
    if (electricOnly ? s->bestEcoE : s->bestEco) {
        if (electricOnly) { TextFixed(n1, (Int32)s->bestEcoE, 1, point, 0); TextFixed(n2, (Int32)s->worstEcoE, 1, point, 0); }
        else { EcoText(n1, s->bestEco, p, point); EcoText(n2, s->worstEco, p, point); }
        strcpy(b, n1); strcat(b, " / "); strcat(b, n2);
        Row(&o, "Best / worst", b);
    }
    if (s->fuelCount && StatsAvgPrice(s)) {
        UInt8 cents = p->priceFormat == unitPriceFormatCents;
        TextPrice(n1, UnitPriceToDisplay(StatsAvgPrice(s), p->priceUnit), cents, point);
        strcpy(b, n1); strcat(b, cents ? " c" : " $"); strcat(b, UnitPriceLabel(p->priceUnit));
        Row(&o, "Average price", b);
        TextPrice(n1, UnitPriceToDisplay(s->lowPrice, p->priceUnit), cents, point);
        TextPrice(n2, UnitPriceToDisplay(s->highPrice, p->priceUnit), cents, point);
        strcpy(b, n1); strcat(b, " / "); strcat(b, n2);
        Row(&o, "Low / high price", b);
    } else if (electricOnly && StatsAvgPriceElectric(s)) {
        TextPrice(n1, StatsAvgPriceElectric(s), p->priceFormat == unitPriceFormatCents, point);
        strcpy(b, n1); strcat(b, p->priceFormat == unitPriceFormatCents ? " c/kWh" : " $/kWh");
        Row(&o, "Average price", b);
    }
    if (s->distance) {
        Money(b, StatsCostPer100km(s), point, 0);
        strcpy(n1, "Cost per 100 "); strcat(n1, UnitDistLabel(p->distanceUnit));
        Row(&o, n1, b);
        if (service) {
            /* Running cost: fuel and electricity plus service over the same distance. */
            Money(b, CalcCostPer100kmCents(s->homeCents + service, s->distance), point, 0);
            strcpy(n1, "With service /100 "); strcat(n1, UnitDistLabel(p->distanceUnit));
            Row(&o, n1, b);
        }
    }
    if (service) { Money(b, service, point, thousands); Row(&o, "Service spend", b); }
    if (s->fuelCount) {
        VolText(n1, StatsAvgFillMl(s), p, point, 0);
        strcpy(b, n1); strcat(b, " "); strcat(b, UnitVolLabel(p->volumeUnit));
        Row(&o, "Average fill", b);
    }
    if (s->count > 1) { TextFixed(b, (Int32)StatsDaysBetweenX10(s), 1, point, 0); Row(&o, "Days between fills", b); }
    Money(b, StatsSpendPerMonth(s), point, thousands);
    Row(&o, "Spend per month", b);
    if (s->cardCents && s->cashCents) {
        Money(b, s->cashCents, point, thousands); strcat(b, " / "); Money(b + strlen(b), s->cardCents, point, thousands);
        Row(&o, "Cash / card", b);
    }
    if (s->discountCents) { Money(b, s->discountCents, point, thousands); Row(&o, "Saved by points", b); }

    /* Points: one line per programme that has ever appeared on a fill. */
    for (id = 1; id < statsMaxId; id++) if (s->pgmUsed[id]) any = 1;
    if (any) {
        Line(&o, tvRule);
        l = Line(&o, tvHeading);
        Put(l->cell[0], "Points");
        for (id = 1; id < statsMaxId; id++) {
            ProgrammeRecord pg;
            Int32 bal, exp;
            UInt16 when;
            if (!s->pgmUsed[id] || !DbProgrammeById(id, &pg)) continue;
            bal = PgmBalance(id, pg.openingBalance);
            PgmValueText(b, &pg, bal, point, thousands);
            exp = PgmExpiring(&pg, today, &when);
            if (exp > 0) {
                strcat(b, " ("); TextFixed(b + strlen(b), exp, 0, point, thousands);
                strcat(b, " expire "); DateShortText(b + strlen(b), when); strcat(b, ")");
            } else if (!PgmIsMoney(&pg) && pg.valuePerPoint && bal > 0) {
                strcat(b, " (about ");
                Money(b + strlen(b), CalcMulDiv((UInt32)bal, pg.valuePerPoint, 1000), point, thousands);
                strcat(b, ")");
            }
            Row(&o, pg.name, b);
        }
    }
    return o.count;
}

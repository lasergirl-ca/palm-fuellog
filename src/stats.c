#include "stats.h"
#include "calc.h"
#include "datecalc.h"

void StatsBegin(Stats *s)
{
    UInt8 *p = (UInt8 *)s;
    UInt32 i;
    for (i = 0; i < sizeof *s; i++) p[i] = 0;
}

void StatsAdd(Stats *s, const FillRecord *f)
{
    UInt32 home = CalcHomeCents(f->total, f->fxRate ? f->fxRate : 1000000UL);
    UInt8 charge = (f->flags & fillFlagCharge) ? 1 : 0, k;
    StatsChain *c;

    if (s->count == 0) s->firstDate = f->date;
    s->lastDate = f->date;
    s->count++;
    s->homeCents += home;
    s->discountCents += f->discount;
    if (f->payment == fillPaymentCard) s->cardCents += home; else s->cashCents += home;
    if (f->currency < 4) s->curCents[f->currency] += f->total;
    if (charge) { s->chargeCount++; s->wh += f->volume; s->elecHomeCents += home; }
    else {
        s->fuelCount++; s->fuelMl += f->volume; s->fuelHomeCents += home;
        if (f->price && f->volume) {
            if (!s->lowPrice || f->price < s->lowPrice) { s->lowPrice = f->price; s->lowDate = f->date; }
            if (f->price > s->highPrice) { s->highPrice = f->price; s->highDate = f->date; }
        }
    }
    for (k = 0; k < f->pointCount && k < fillMaxPoints; k++) {
        UInt8 id = f->points[k].programme;
        if (id >= statsMaxId) continue;
        s->earned[id] += f->points[k].earned;
        s->redeemed[id] += f->points[k].redeemed;
        s->pgmUsed[id] = 1;
        if (f->points[k].redeemed > 0) s->saved[id] += f->discount;   /* one redeeming programme per fill in practice */
    }
    if (f->vehicle >= statsMaxId || f->odometer == 0) return;
    {
        UInt8 v = f->vehicle;
        if (!s->veh[v].seen) { s->veh[v].seen = 1; s->veh[v].firstOdo = s->veh[v].lastOdo = f->odometer; }
        else if (f->odometer > s->veh[v].lastOdo) { s->distance += f->odometer - s->veh[v].lastOdo; s->veh[v].lastOdo = f->odometer; }
        c = &s->veh[v].chain[charge];
    }
    if (f->flags & fillFlagMissedPrev) { c->haveFull = 0; c->pending = 0; }
    c->pending += f->volume;
    if (f->flags & fillFlagFullTank) {
        if (c->haveFull && f->odometer > c->lastFullOdo) {
            UInt32 dist = f->odometer - c->lastFullOdo;
            if (charge) {
                UInt32 eco = CalcKwh100kmX10(c->pending, dist);
                s->ftfWh += c->pending; s->ftfDkmE += dist;
                if (eco) {
                    if (!s->bestEcoE || eco < s->bestEcoE) s->bestEcoE = eco;
                    if (eco > s->worstEcoE) s->worstEcoE = eco;
                }
            } else {
                UInt32 eco = CalcL100kmX10(c->pending, dist);
                s->ftfMl += c->pending; s->ftfDkm += dist;
                if (eco) {
                    if (!s->bestEco || eco < s->bestEco) { s->bestEco = eco; s->bestDate = f->date; }
                    if (eco > s->worstEco) { s->worstEco = eco; s->worstDate = f->date; }
                }
            }
        }
        c->lastFullOdo = f->odometer; c->haveFull = 1; c->pending = 0;
    }
}

UInt32 StatsAvgEco(const Stats *s) { return s->ftfDkm ? CalcL100kmX10(s->ftfMl, s->ftfDkm) : 0; }
UInt32 StatsAvgEcoElectric(const Stats *s) { return s->ftfDkmE ? CalcKwh100kmX10(s->ftfWh, s->ftfDkmE) : 0; }
UInt32 StatsAvgPrice(const Stats *s) { return s->fuelMl ? CalcMulDiv(s->fuelHomeCents, 100000UL, s->fuelMl) : 0; }
UInt32 StatsAvgPriceElectric(const Stats *s) { return s->wh ? CalcMulDiv(s->elecHomeCents, 100000UL, s->wh) : 0; }
UInt32 StatsCostPer100km(const Stats *s) { return CalcCostPer100kmCents(s->homeCents, s->distance); }
UInt32 StatsAvgFillMl(const Stats *s) { return s->fuelCount ? (s->fuelMl + s->fuelCount / 2) / s->fuelCount : 0; }

UInt32 StatsDaysBetweenX10(const Stats *s)
{
    if (s->count < 2 || s->lastDate <= s->firstDate) return 0;
    return CalcMulDiv(s->lastDate - s->firstDate, 10, s->count - 1);
}

UInt32 StatsSpendPerMonth(const Stats *s)
{
    UInt16 y1, y2;
    UInt8 m1, d1, m2, d2;
    UInt32 months;
    if (s->count == 0) return 0;
    DateDaysToYmd(s->firstDate, &y1, &m1, &d1);
    DateDaysToYmd(s->lastDate, &y2, &m2, &d2);
    months = (UInt32)(y2 * 12 + m2) - (UInt32)(y1 * 12 + m1) + 1;
    return (s->homeCents + months / 2) / months;
}

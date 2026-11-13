/* Graphs without the window (graphview.h), after src/graphs_form.c. */
#include "graphview.h"
#include "../store/db.h"
#include "../../src/stats.h"
#include "../../src/calc.h"
#include "../../src/units.h"
#include "../../src/currency.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <string.h>

static FillRecord gFill;
static struct { UInt32 lastFullOdo, pending; UInt8 haveFull; } gChain[statsMaxId][2];
static UInt32 gMonthCents[gvMax];

static UInt8 SeriesFor(GraphData *g, UInt8 vehicle)
{
    VehicleRecord v;
    UInt8 i;
    for (i = 0; i < g->seriesCount; i++) if (g->seriesId[i] == vehicle) return i;
    if (g->seriesCount >= gvSeries) return 0;
    g->seriesId[g->seriesCount] = vehicle;
    if (DbVehicleById(vehicle, &v)) strcpy(g->seriesName[g->seriesCount], v.name);
    else strcpy(g->seriesName[g->seriesCount], "Vehicle");
    return g->seriesCount++;
}

static void AddPoint(GraphData *g, Int32 x, Int32 y, UInt8 s)
{
    if (g->n >= gvMax) return;
    g->x[g->n] = x; g->y[g->n] = y; g->s[g->n] = s; g->n++;
}

void GraphViewCollect(GraphData *g, UInt8 kind, const FuelPrefs *p)
{
    UInt16 n = DbCount(dbFills), i, months = 0;
    int all = p->currentVehicle == 255;
    UInt8 eu = p->economyUnit < 4 ? p->economyUnit : unitEcoL100km;
    VehicleRecord veh;

    memset(g, 0, sizeof *g);
    memset(gChain, 0, sizeof gChain);
    memset(gMonthCents, 0, sizeof gMonthCents);
    g->kind = kind;
    g->electric = (UInt8)(kind == gvEconomy && !all && DbVehicleById(p->currentVehicle, &veh) && veh.powertrain == vehPowerElectric);
    for (i = 0; i < n; i++) {
        UInt8 v, c, s;
        if (DbGetFill(i, &gFill) != dbOk) continue;
        if (!all && gFill.vehicle != p->currentVehicle) continue;
        v = gFill.vehicle < statsMaxId ? gFill.vehicle : 0;
        c = (gFill.flags & fillFlagCharge) ? 1 : 0;
        s = (all && kind != gvSpend) ? SeriesFor(g, gFill.vehicle) : 0;   /* spend pools the fleet */
        if (kind == gvSpend) {
            UInt16 y, idx; UInt8 m, d;
            DateDaysToYmd(gFill.date, &y, &m, &d);
            idx = (UInt16)(y * 12 + m - 1);
            if (g->firstMonth == 0) g->firstMonth = idx;
            if (idx >= g->firstMonth && idx - g->firstMonth < gvMax) {
                gMonthCents[idx - g->firstMonth] += CalcHomeCents(gFill.total, gFill.fxRate ? gFill.fxRate : fxOne);
                if (idx - g->firstMonth + 1 > months) months = (UInt16)(idx - g->firstMonth + 1);
            }
            continue;
        }
        if (kind == gvPrice) {
            if (!c && gFill.price) AddPoint(g, gFill.date, (Int32)UnitPriceToDisplay(gFill.price, p->priceUnit), s);
            continue;
        }
        /* economy: segments closing at a full fill */
        if (gFill.flags & fillFlagMissedPrev) { gChain[v][c].haveFull = 0; gChain[v][c].pending = 0; }
        gChain[v][c].pending += gFill.volume;
        if (gFill.flags & fillFlagFullTank) {
            if (gChain[v][c].haveFull && gFill.odometer > gChain[v][c].lastFullOdo) {
                UInt32 dist = gFill.odometer - gChain[v][c].lastFullOdo;
                UInt32 eco = c ? CalcKwh100kmX10(gChain[v][c].pending, dist) : CalcL100kmX10(gChain[v][c].pending, dist);
                if (eco) AddPoint(g, gFill.date, (Int32)(c ? eco : UnitEcoFuelToDisplay(eco, eu)), s);
            }
            gChain[v][c].lastFullOdo = gFill.odometer; gChain[v][c].haveFull = 1; gChain[v][c].pending = 0;
        }
    }
    if (kind == gvSpend) for (i = 0; i < months; i++) AddPoint(g, i, (Int32)gMonthCents[i], 0);
    if (g->n) {
        Int32 sum = 0;
        g->min = g->max = g->y[0]; g->xmin = g->xmax = g->x[0];
        for (i = 0; i < g->n; i++) {
            if (g->y[i] < g->min) g->min = g->y[i];
            if (g->y[i] > g->max) g->max = g->y[i];
            if (g->x[i] < g->xmin) g->xmin = g->x[i];
            if (g->x[i] > g->xmax) g->xmax = g->x[i];
            sum += g->y[i];
        }
        g->mean = (sum + (Int32)g->n / 2) / (Int32)g->n;
        if (g->max == g->min) { g->max += 1; g->min -= 1; }
        if (g->xmax == g->xmin) g->xmax++;
    }
}

void GraphViewValueText(const GraphData *g, const FuelPrefs *p, Int32 v, char point, char *buf)
{
    if (g->kind == gvSpend) { strcpy(buf, "$"); TextFixed(buf + 1, v, 2, point, 0); return; }
    if (g->kind == gvPrice) { TextPrice(buf, (UInt32)(v < 0 ? 0 : v), p->priceFormat == unitPriceFormatCents, point); return; }
    TextFixed(buf, v, 1, point, 0);
}

void GraphViewPointText(const GraphData *g, const FuelPrefs *p, UInt16 k, char point, char *buf)
{
    buf[0] = 0;
    if (k >= g->n) return;
    if (g->kind == gvSpend) {
        UInt16 abs = (UInt16)(g->firstMonth + g->x[k]);
        strcpy(buf, DateMonthName((UInt8)(abs % 12 + 1)));
        strcat(buf, " ");
        TextFixed(buf + strlen(buf), abs / 12, 0, point, 0);
    } else DateShortText(buf, (UInt16)g->x[k]);
    strcat(buf, ": ");
    GraphViewValueText(g, p, g->y[k], point, buf + strlen(buf));
}

void GraphViewSummary(const GraphData *g, const FuelPrefs *p, char point, char *buf)
{
    char lo[20], hi[20], me[20];
    buf[0] = 0;
    if (!g->n) return;
    GraphViewValueText(g, p, g->min, point, lo);
    GraphViewValueText(g, p, g->max, point, hi);
    GraphViewValueText(g, p, g->mean, point, me);
    strcpy(buf, "Low "); strcat(buf, lo); strcat(buf, ", high "); strcat(buf, hi); strcat(buf, ", mean "); strcat(buf, me);
}

const char *GraphViewUnit(const GraphData *g, const FuelPrefs *p)
{
    if (g->electric) return "kWh/100 km";
    return UnitEcoLabel(p->economyUnit < 4 ? p->economyUnit : unitEcoL100km);
}

UInt8 GraphViewXLabels(const GraphData *g, Int32 at[gvLabels], char text[gvLabels][12])
{
    UInt8 count = 0;
    if (!g->n) return 0;
    if (g->kind == gvSpend) {
        UInt16 m, step = (UInt16)((g->xmax - g->xmin) / 5 + 1);
        for (m = 0; (Int32)m <= g->xmax && count < gvLabels; m += step) {
            UInt16 abs = (UInt16)(g->firstMonth + m);
            at[count] = m;
            strcpy(text[count], DateMonthName((UInt8)(abs % 12 + 1)));
            count++;
        }
        return count;
    } else {
        UInt16 y, yy, k = 0, months, step;
        UInt8 m, mm, d;
        DateDaysToYmd((UInt16)g->xmin, &y, &m, &d);
        DateDaysToYmd((UInt16)g->xmax, &yy, &mm, &d);
        months = (UInt16)((yy * 12 + mm) - (y * 12 + m) + 1);
        step = (UInt16)(months / 5 + 1);
        for (; (UInt32)(y * 12 + m) <= (UInt32)(yy * 12 + mm) && count < gvLabels; m++, k++) {
            Int32 days;
            if (m > 12) { m = 1; y++; }
            days = DateYmdToDays(y, m, 1);
            if (days < g->xmin || days > g->xmax || (k % step)) continue;
            at[count] = days;
            strcpy(text[count], DateMonthName(m));
            count++;
        }
        if (count < 2) {
            /* Under a month of data: name the first and last dates instead. */
            at[0] = g->xmin; DateShortText(text[0], (UInt16)g->xmin);
            at[1] = g->xmax; DateShortText(text[1], (UInt16)g->xmax);
            count = 2;
        }
        return count;
    }
}

Int16 GraphViewNearest(const Int32 *px, const Int32 *py, UInt16 n, Int32 tx, Int32 ty, Int32 maxDist2)
{
    UInt16 k;
    Int32 best = maxDist2 + 1;
    Int16 hit = -1;
    for (k = 0; k < n; k++) {
        Int32 dx = px[k] - tx, dy = py[k] - ty, d2 = dx * dx + dy * dy;
        if (d2 < best) { best = d2; hit = (Int16)k; }
    }
    return hit;
}

/* Reports as lines (textview.h), after src/reports_form.c. */
#include "textview.h"
#include "../store/db.h"
#include "../../src/report.h"
#include "../../src/units.h"
#include "../../src/currency.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <string.h>

static Report gRpt;
static FillRecord gFill;

void ReportViewSetMode(ReportPeriod *rp, UInt8 mode, UInt16 today)
{
    UInt16 y; UInt8 m, d;
    DateDaysToYmd(today, &y, &m, &d);
    rp->mode = mode <= rptRange ? mode : rptMonthly;
    if (rp->mode == rptMonthly) rp->year = y;
    else if (rp->mode == rptYearly) rp->year = (UInt16)(y - rptMaxRows + 1);
    if (!rp->to) { rp->to = today; rp->from = (UInt16)(today > 90 ? today - 90 : 0); }
}

void ReportViewStep(ReportPeriod *rp, Int16 dir)
{
    if (rp->mode == rptRange) {
        /* step the range by its own length */
        UInt16 len = (UInt16)(rp->to - rp->from + 1);
        if (dir < 0 && rp->from >= len) { rp->from -= len; rp->to -= len; }
        else if (dir > 0 && rp->to <= 0xFFFF - len) { rp->from += len; rp->to += len; }
        return;
    }
    rp->year = (UInt16)(rp->year + dir * (rp->mode == rptMonthly ? 1 : rptMaxRows));
}

void ReportViewPeriodText(const ReportPeriod *rp, char *buf)
{
    buf[0] = 0;
    if (rp->mode == rptMonthly) TextFixed(buf, rp->year, 0, '.', 0);
    else if (rp->mode == rptYearly) {
        UInt16 last = (UInt16)((rp->year + rptMaxRows - 1) % 100);
        TextFixed(buf, rp->year, 0, '.', 0);
        strcat(buf, "-");
        if (last < 10) strcat(buf, "0");
        TextFixed(buf + strlen(buf), last, 0, '.', 0);
    }
}

/* ---- lines ---- */

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

static const char *EcoHeader(const FuelPrefs *p)
{
    if (p->economyUnit == unitEcoMpgUS || p->economyUnit == unitEcoMpgImp) return "MPG";
    if (p->economyUnit == unitEcoKmPerL) return "KPL";
    return "L/100";
}

/* One row of the table: label, fills, volume, spend, economy. */
static void RowText(Out *o, const ReportRow *row, const char *label, int bold, const FuelPrefs *p, char point, char thousands)
{
    TextLine *l = Line(o, bold ? tvTableBold : tvTable);
    UInt32 eco = ReportRowEco(row), shown, div = 1;
    UInt8 d = UnitVolDecimals(p->volumeUnit);
    Put(l->cell[0], label);
    TextFixed(l->cell[1], row->fills, 0, point, 0);
    shown = UnitVolToDisplay(row->ml, p->volumeUnit);
    while (d-- > 1) div *= 10;
    TextFixed(l->cell[2], (Int32)((shown + div / 2) / div), 1, point, 0);
    TextFixed(l->cell[3], (Int32)row->homeCents, 2, point, thousands);
    if (eco) TextFixed(l->cell[4], (Int32)UnitEcoFuelToDisplay(eco, p->economyUnit < 4 ? p->economyUnit : unitEcoL100km), 1, point, 0);
}

/* An indented sum under By payment and By currency, in the spend column. */
static void SumRow(Out *o, const char *label, UInt32 cents, char point, char thousands)
{
    TextLine *l = Line(o, tvTable);
    strcpy(l->cell[0], "  ");
    strcat(l->cell[0], label);
    TextFixed(l->cell[3], (Int32)cents, 2, point, thousands);
}

UInt16 ReportViewBuild(const FuelPrefs *p, const ReportPeriod *rp, char point, char thousands, TextLine *lines, UInt16 max)
{
    Out o;
    UInt16 n = DbCount(dbFills), i;
    UInt8 r, v, shown = 0;
    char b[64];
    TextLine *l;

    o.lines = lines; o.count = 0; o.max = max;
    ReportBegin(&gRpt, rp->mode, rp->year);
    if (rp->mode == rptRange) ReportSetRange(&gRpt, rp->from, rp->to);
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gFill) != dbOk) continue;
        if (p->currentVehicle != 255 && gFill.vehicle != p->currentVehicle) continue;
        ReportAdd(&gRpt, &gFill);
    }

    l = Line(&o, tvTableBold);
    Put(l->cell[0], rp->mode == rptRange ? "Range" : rp->mode == rptMonthly ? "Month" : "Year");
    Put(l->cell[1], "#");
    Put(l->cell[2], UnitVolLabel(p->volumeUnit));
    Put(l->cell[3], CurrencySymbol(p->homeCurrency));
    Put(l->cell[4], EcoHeader(p));
    for (r = 0; r < rptMaxRows; r++) {
        if (gRpt.rows[r].fills == 0) continue;
        if (rp->mode == rptRange) strcpy(b, "Period");
        else if (rp->mode == rptMonthly) strcpy(b, DateMonthName((UInt8)(r + 1)));
        else TextFixed(b, rp->year + r, 0, point, 0);
        RowText(&o, &gRpt.rows[r], b, 0, p, point, thousands);
        shown++;
    }
    if (!shown) { l = Line(&o, tvNote); Put(l->cell[0], "No fills in this period."); }
    Line(&o, tvRule);
    RowText(&o, &gRpt.total, "Total", 1, p, point, thousands);
    if (gRpt.total.dkm) {
        l = Line(&o, tvNote);
        strcpy(b, "Distance "); TextFixed(b + 9, (Int32)(UnitDistToDisplay(gRpt.total.dkm, p->distanceUnit) / 10), 0, point, thousands);
        strcat(b, " "); strcat(b, UnitDistLabel(p->distanceUnit));
        if (ReportRowPrice(&gRpt.total)) {
            strcat(b, ", avg ");
            TextPrice(b + strlen(b), UnitPriceToDisplay(ReportRowPrice(&gRpt.total), p->priceUnit), p->priceFormat == unitPriceFormatCents, point);
            strcat(b, p->priceFormat == unitPriceFormatCents ? " c" : " $"); strcat(b, UnitPriceLabel(p->priceUnit));
        }
        Put(l->cell[0], b);
    }
    if (gRpt.total.points) {
        l = Line(&o, tvNote);
        strcpy(b, "Points net "); TextFixed(b + 11, gRpt.total.points, 0, point, thousands);
        Put(l->cell[0], b);
    }
    if (p->currentVehicle == 255) {
        /* Per-vehicle breakdown: the same columns, one row per car with fills in the period. */
        int any = 0;
        for (v = 1; v < rptMaxId; v++) if (gRpt.byVehicle[v].fills) any = 1;
        if (any) {
            l = Line(&o, tvHeading); Put(l->cell[0], "By vehicle");
            for (v = 1; v < rptMaxId; v++) {
                VehicleRecord veh;
                if (!gRpt.byVehicle[v].fills) continue;
                if (DbVehicleById(v, &veh)) { strncpy(b, veh.name, 9); b[9] = 0; } else strcpy(b, "Vehicle");
                RowText(&o, &gRpt.byVehicle[v], b, 0, p, point, thousands);
            }
        }
    }
    l = Line(&o, tvHeading); Put(l->cell[0], "By payment");
    SumRow(&o, "Cash", gRpt.cashCents, point, thousands);
    SumRow(&o, "Card", gRpt.cardCents, point, thousands);
    l = Line(&o, tvHeading); Put(l->cell[0], "By currency");
    for (r = 0; r < 4; r++) if (gRpt.curCents[r]) SumRow(&o, CurrencyCode(r), gRpt.curCents[r], point, thousands);
    return o.count;
}

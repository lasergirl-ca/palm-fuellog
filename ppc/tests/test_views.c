/* Host tests for the Pocket PC build's Stats, Reports and Graphs models
 * (app/statsview.c, reportview.c, graphview.c), against a working copy of
 * the fixture set tools/mkpdb writes (argv[1]). The fixture: an Outback with
 * fills on Jul 29 (80,650 km, 45.2 L at 175.6, $79.37 by card, 452
 * Petro-Points) and Aug 10 (81,190 km, 46.0 L at 176.9, $81.37 cash), an
 * Ioniq 5 charged on Aug 12 (38.6 kWh, $0.69), and an Outback service visit
 * on May 14 for $119.95. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../app/textview.h"
#include "../app/graphview.h"
#include "../../src/report.h"
#include "../../src/units.h"
#include "../../src/datecalc.h"

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

static TextLine lines[tvMaxLines];
static GraphData g;

/* The lines as built, when VIEWS_DUMP is set: for checking a change by eye. */
static void Dump(const char *title, UInt16 n)
{
    UInt16 i;
    if (!getenv("VIEWS_DUMP")) return;
    printf("---- %s\n", title);
    for (i = 0; i < n; i++)
        printf("     %u |%s|%s|%s|%s|%s|\n", lines[i].style, lines[i].cell[0], lines[i].cell[1], lines[i].cell[2], lines[i].cell[3], lines[i].cell[4]);
}

/* The value on the line whose label is `label`, or "(none)". */
static const char *Value(UInt16 n, const char *label, int cell)
{
    UInt16 i;
    for (i = 0; i < n; i++) if (strcmp(lines[i].cell[0], label) == 0) return lines[i].cell[cell];
    return "(none)";
}

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : "build/host/work5";
    DbClock clock;
    FuelPrefs p;
    ReportPeriod rp;
    UInt16 n, today = DateYmdToDays(2026, 8, 20);
    char buf[64], text[gvLabels][12];
    Int32 at[gvLabels];

    clock.now = Now; clock.clockShift = 0;
    check("open", DbOpenAll(work, &clock), dbOk);
    PrefsSetDefaults(&p);

    /* ---- Stats ---- */
    p.currentVehicle = 1;
    n = StatsViewBuild(&p, today, '.', ',', lines, tvMaxLines);
    Dump("stats, Outback", n);
    checkStr("since", lines[0].cell[0], "Since Jul 29, 2 fills");
    check("  headline line", lines[1].style, tvHeadline);
    checkStr("  economy 46.0 L over 540 km", lines[1].cell[0], "8.5");
    checkStr("  spend", lines[1].cell[1], "$160.74");
    checkStr("  caption", lines[2].cell[0], "L/100 km average");
    checkStr("  caption right", lines[2].cell[1], "fuel spend, CAD");
    checkStr("distance", Value(n, "Distance", 1), "540 km");
    checkStr("fuel used", Value(n, "Fuel used", 1), "91.2 L");
    checkStr("best / worst", Value(n, "Best / worst", 1), "8.5 / 8.5");
    checkStr("low / high price", Value(n, "Low / high price", 1), "175.6 / 176.9");
    checkStr("cost per 100 km", Value(n, "Cost per 100 km", 1), "$29.77");
    checkStr("with service", Value(n, "With service /100 km", 1), "$51.98");
    checkStr("service spend", Value(n, "Service spend", 1), "$119.95");
    checkStr("average fill", Value(n, "Average fill", 1), "45.6 L");
    checkStr("days between", Value(n, "Days between fills", 1), "12.0");
    checkStr("cash / card", Value(n, "Cash / card", 1), "$81.37 / $79.37");
    checkStr("points heading", Value(n, "Points", 0), "Points");
    checkStr("Petro-Points", Value(n, "Petro-Points", 1), "452 pts (about $0.45)");
    check("service cents, all", (long)StatsViewServiceCents(255), 11995);
    check("service cents, Ioniq", (long)StatsViewServiceCents(2), 0);
    p.economyUnit = unitEcoMpgUS; p.distanceUnit = unitDistMiles;
    n = StatsViewBuild(&p, today, '.', ',', lines, tvMaxLines);
    checkStr("in MPG", lines[1].cell[0], "27.7");
    checkStr("  caption", lines[2].cell[0], "MPG (US) average");
    checkStr("  distance in whole miles", Value(n, "Distance", 1), "335 mi");
    checkStr("  cost per 100 mi", Value(n, "Cost per 100 mi", 1), "$29.77");
    PrefsSetDefaults(&p);
    p.currentVehicle = 2;
    n = StatsViewBuild(&p, today, '.', ',', lines, tvMaxLines);
    Dump("stats, Ioniq", n);
    checkStr("electric: one charge, no economy yet", lines[1].cell[0], "--");
    checkStr("  caption", lines[2].cell[0], "kWh/100 km average");
    checkStr("  energy spend", lines[2].cell[1], "energy spend, CAD");
    checkStr("  energy used", Value(n, "Energy used", 1), "38.6 kWh, $0.69");
    checkStr("  price per kWh: $0.69 over 38.6 kWh", Value(n, "Average price", 1), "1.8 c/kWh");
    p.currentVehicle = 255;
    n = StatsViewBuild(&p, today, '.', ',', lines, tvMaxLines);
    checkStr("all vehicles: three fills", lines[0].cell[0], "Since Jul 29, 3 fills");
    checkStr("  fuel and power", lines[2].cell[1], "fuel and power, CAD");
    checkStr("  electricity", Value(n, "Electricity", 1), "38.6 kWh, $0.69");
    p.currentVehicle = 9;
    n = StatsViewBuild(&p, today, '.', ',', lines, tvMaxLines);
    check("no such vehicle: one line", n, 1);
    checkStr("  says so", lines[0].cell[0], "No fills yet for this choice.");

    /* ---- Reports ---- */
    PrefsSetDefaults(&p);
    memset(&rp, 0, sizeof rp);
    ReportViewSetMode(&rp, rptMonthly, today);
    ReportViewPeriodText(&rp, buf);
    checkStr("monthly: this year", buf, "2026");
    n = ReportViewBuild(&p, &rp, '.', ',', lines, tvMaxLines);
    Dump("report, monthly 2026, all", n);
    checkStr("header", lines[0].cell[0], "Month");
    checkStr("  volume column", lines[0].cell[2], "L");
    checkStr("  economy column", lines[0].cell[4], "L/100");
    checkStr("July", lines[1].cell[0], "Jul");
    checkStr("  one fill", lines[1].cell[1], "1");
    checkStr("  45.2 L", lines[1].cell[2], "45.2");
    checkStr("  spend", lines[1].cell[3], "79.37");
    checkStr("  no economy yet", lines[1].cell[4], "");
    checkStr("August", lines[2].cell[0], "Aug");
    checkStr("  two fills", lines[2].cell[1], "2");
    checkStr("  spend with the charge", lines[2].cell[3], "82.06");
    checkStr("  economy", lines[2].cell[4], "8.5");
    checkStr("total", Value(n, "Total", 3), "161.43");
    checkStr("by vehicle", Value(n, "By vehicle", 0), "By vehicle");
    checkStr("  Outback", Value(n, "Outback", 3), "160.74");
    checkStr("  Ioniq 5", Value(n, "Ioniq 5", 3), "0.69");
    checkStr("by payment: cash", Value(n, "  Cash", 3), "82.06");
    checkStr("  card", Value(n, "  Card", 3), "79.37");
    checkStr("by currency", Value(n, "  CAD", 3), "161.43");
    p.currentVehicle = 1;
    n = ReportViewBuild(&p, &rp, '.', ',', lines, tvMaxLines);
    checkStr("one vehicle: no breakdown", Value(n, "By vehicle", 0), "(none)");
    p.currentVehicle = 255;
    ReportViewStep(&rp, -1);
    ReportViewPeriodText(&rp, buf);
    checkStr("back a year", buf, "2025");
    n = ReportViewBuild(&p, &rp, '.', ',', lines, tvMaxLines);
    checkStr("  nothing in 2025", lines[1].cell[0], "No fills in this period.");
    ReportViewSetMode(&rp, rptYearly, today);
    ReportViewPeriodText(&rp, buf);
    checkStr("yearly: twelve years", buf, "2015-26");
    n = ReportViewBuild(&p, &rp, '.', ',', lines, tvMaxLines);
    checkStr("  header", lines[0].cell[0], "Year");
    checkStr("  one row, 2026", lines[1].cell[0], "2026");
    checkStr("  three fills", lines[1].cell[1], "3");
    ReportViewStep(&rp, -1);
    ReportViewPeriodText(&rp, buf);
    checkStr("  back twelve", buf, "2003-14");
    ReportViewSetMode(&rp, rptRange, today);
    rp.from = DateYmdToDays(2026, 8, 1); rp.to = DateYmdToDays(2026, 8, 11);
    n = ReportViewBuild(&p, &rp, '.', ',', lines, tvMaxLines);
    checkStr("range: header", lines[0].cell[0], "Range");
    checkStr("  Aug 1 to 11: one fill", lines[1].cell[1], "1");
    ReportViewStep(&rp, 1);
    check("  steps by its length", rp.from, DateYmdToDays(2026, 8, 12));
    check("  to", rp.to, DateYmdToDays(2026, 8, 22));

    /* ---- Graphs ---- */
    p.currentVehicle = 1;
    GraphViewCollect(&g, gvEconomy, &p);
    check("economy, Outback: one segment", g.n, 1);
    check("  8.5 L/100 km x 10", (long)g.y[0], 85);
    check("  on Aug 10", (long)g.x[0], DateYmdToDays(2026, 8, 10));
    check("  range widened", (long)(g.max - g.min), 2);
    check("  mean", (long)g.mean, 85);
    checkStr("  unit", GraphViewUnit(&g, &p), "L/100 km");
    GraphViewPointText(&g, &p, 0, '.', buf);
    checkStr("  point", buf, "Aug 10: 8.5");
    GraphViewSummary(&g, &p, '.', buf);
    checkStr("  summary", buf, "Low 8.4, high 8.6, mean 8.5");
    check("  x labels: the two dates", GraphViewXLabels(&g, at, text), 2);
    checkStr("  first", text[0], "Aug 10");
    p.economyUnit = unitEcoKmPerL;
    GraphViewCollect(&g, gvEconomy, &p);
    check("  in km/L", (long)g.y[0], 118);
    PrefsSetDefaults(&p);
    p.currentVehicle = 255;
    GraphViewCollect(&g, gvPrice, &p);
    check("price, all: two fuel fills", g.n, 2);
    check("  175.6 c", (long)g.y[0], 17560);
    check("  two series named", g.seriesCount, 2);
    checkStr("  first series", g.seriesName[0], "Outback");
    GraphViewValueText(&g, &p, g.y[1], '.', buf);
    checkStr("  in cents", buf, "176.9");
    p.priceFormat = unitPriceFormatDollars;
    GraphViewValueText(&g, &p, g.y[1], '.', buf);
    checkStr("  in dollars", buf, "1.769");
    PrefsSetDefaults(&p);
    p.currentVehicle = 255;
    GraphViewCollect(&g, gvSpend, &p);
    check("spend: two months", g.n, 2);
    check("  July", (long)g.y[0], 7937);
    check("  August with the charge", (long)g.y[1], 8206);
    GraphViewPointText(&g, &p, 1, '.', buf);
    checkStr("  point", buf, "Aug 2026: $82.06");
    check("  labels", GraphViewXLabels(&g, at, text), 2);
    checkStr("  Jul", text[0], "Jul");
    p.currentVehicle = 2;
    GraphViewCollect(&g, gvEconomy, &p);
    check("Ioniq economy: electric", g.electric, 1);
    checkStr("  its unit", GraphViewUnit(&g, &p), "kWh/100 km");
    check("  one charge: nothing to plot", g.n, 0);
    {
        Int32 px[3] = { 10, 50, 90 }, py[3] = { 10, 20, 30 };
        check("nearest point", GraphViewNearest(px, py, 3, 52, 25, 400), 1);
        check("  none within 20", GraphViewNearest(px, py, 3, 200, 200, 400), -1);
    }
    DbCloseAll();

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

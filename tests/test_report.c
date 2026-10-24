/* The mock-ups' monthly report for the eight-fill Outback log, 2026. */
#include <stdio.h>
#include <string.h>
#include "../src/report.h"
#include "../src/calc.h"
#include "../src/datecalc.h"

static int failures;
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}
static void add(Report *r, UInt16 date, UInt32 odoKm, UInt32 ml, UInt32 cents, int full)
{
    FillRecord f;
    memset(&f, 0, sizeof f);
    f.date = date; f.vehicle = 1; f.odometer = odoKm * 10; f.volume = ml; f.total = cents;
    f.fxRate = 1000000UL; f.currency = 1; f.flags = full ? fillFlagFullTank : 0;
    f.pointCount = 1; f.points[0].programme = 2; f.points[0].earned = (Int32)(ml / 100);
    ReportAdd(r, &f);
}
int main(void)
{
    Report r;
    ReportBegin(&r, rptMonthly, 2026);
    add(&r, DateYmdToDays(2026, 7, 29), 80650, 45200, 7937, 1);
    add(&r, DateYmdToDays(2026, 8, 10), 81190, 46000, 8137, 1);
    add(&r, DateYmdToDays(2026, 8, 22), 81724, 44100, 7832, 1);
    add(&r, DateYmdToDays(2026, 9, 3),  82262, 43900, 7713, 1);
    add(&r, DateYmdToDays(2026, 9, 15), 82800, 44600, 7983, 1);
    add(&r, DateYmdToDays(2026, 9, 27), 83092, 22000, 3870, 0);
    add(&r, DateYmdToDays(2026, 10, 8), 83640, 45900, 8106, 1);
    add(&r, DateYmdToDays(2026, 10, 19), 84212, 48200, 8430, 1);
    check("Jul fills", r.rows[6].fills, 1);
    check("Aug fills", r.rows[7].fills, 2);
    check("Sep fills", r.rows[8].fills, 3);
    check("Oct fills", r.rows[9].fills, 2);
    check("Aug litres", r.rows[7].ml, 90100);
    check("Sep spend", r.rows[8].homeCents, 19566);
    check("Oct spend", r.rows[9].homeCents, 16536);
    check("Jul price 175.6", (ReportRowPrice(&r.rows[6]) + 5) / 10, 1756);
    check("Aug price 177.2", (ReportRowPrice(&r.rows[7]) + 5) / 10, 1772);
    check("Aug economy 8.4", ReportRowEco(&r.rows[7]), 84);
    check("Sep economy 8.2", ReportRowEco(&r.rows[8]), 82);
    check("Oct economy 8.2 (partial rolled in)", ReportRowEco(&r.rows[9]), 82);
    check("total fills", r.total.fills, 8);
    check("total litres 339.9", r.total.ml, 339900);
    check("total spend 600.08", r.total.homeCents, 60008);
    check("total economy 8.3", ReportRowEco(&r.total), 83);
    check("total distance 3,562 km", r.total.dkm, 35620);
    check("CAD spend", r.curCents[1], 60008);
    check("points", r.total.points, 452 + 460 + 441 + 439 + 446 + 220 + 459 + 482);
    check("row for Nov is empty", r.rows[10].fills, 0);
    check("date outside the year", ReportRowFor(&r, DateYmdToDays(2025, 12, 31)), -1);

    ReportBegin(&r, rptYearly, 2020);
    add(&r, DateYmdToDays(2025, 11, 2), 70000, 40000, 6000, 1);
    add(&r, DateYmdToDays(2026, 1, 9), 70500, 40000, 6200, 1);
    check("yearly: 2025 row", r.rows[5].fills, 1);
    check("yearly: 2026 row", r.rows[6].fills, 1);
    check("yearly: 2026 segment 8.0", ReportRowEco(&r.rows[6]), 80);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

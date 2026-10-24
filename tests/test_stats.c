/* The Stats screen's sample log from the mock-ups: eight Outback fills,
 * Jul 29 to Oct 19 2026, one partial. Figures from the spec's formulas. */
#include <stdio.h>
#include <string.h>
#include "../src/stats.h"
#include "../src/calc.h"
#include "../src/datecalc.h"

static int failures;
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}

static void add(Stats *s, UInt16 date, UInt32 odoKm, UInt32 ml, UInt32 cents, int full, int card)
{
    FillRecord f;
    memset(&f, 0, sizeof f);
    f.date = date; f.vehicle = 1; f.odometer = odoKm * 10; f.volume = ml; f.total = cents;
    f.price = CalcMulDiv(cents, 100000UL, ml);
    f.fxRate = 1000000UL; f.currency = 1; f.payment = card ? 1 : 0;
    f.flags = full ? fillFlagFullTank : 0;
    f.pointCount = 1; f.points[0].programme = 2; f.points[0].earned = (Int32)(ml / 100);
    StatsAdd(s, &f);
}

int main(void)
{
    Stats s;
    UInt16 d;
    StatsBegin(&s);
    d = DateYmdToDays(2026, 7, 29);  add(&s, d, 80650, 45200, 7937, 1, 1);
    d = DateYmdToDays(2026, 8, 10);  add(&s, d, 81190, 46000, 8137, 1, 0);
    d = DateYmdToDays(2026, 8, 22);  add(&s, d, 81724, 44100, 7832, 1, 0);
    d = DateYmdToDays(2026, 9, 3);   add(&s, d, 82262, 43900, 7713, 1, 0);
    d = DateYmdToDays(2026, 9, 15);  add(&s, d, 82800, 44600, 7983, 1, 0);
    d = DateYmdToDays(2026, 9, 27);  add(&s, d, 83092, 22000, 3870, 0, 0);
    d = DateYmdToDays(2026, 10, 8);  add(&s, d, 83640, 45900, 8106, 1, 0);
    d = DateYmdToDays(2026, 10, 19); add(&s, d, 84212, 48200, 8430, 1, 0);

    check("fills", s.count, 8);
    check("distance km x10", s.distance, 35620);
    check("fuel used mL", s.fuelMl, 339900);
    check("spend cents", s.homeCents, 60008);
    check("average 8.3 L/100", StatsAvgEco(&s), 83);
    check("best 8.1", s.bestEco, 81);
    check("worst 8.5", s.worstEco, 85);
    check("best is Oct 8", s.bestDate, DateYmdToDays(2026, 10, 8));
    check("average price $1.7655/L (176.5 c)", StatsAvgPrice(&s), 17655);
    check("low price 174.9", (s.lowPrice + 5) / 10, 1749);
    check("high price 179.0", (s.highPrice + 5) / 10, 1790);
    check("cost per 100 km $16.85", StatsCostPer100km(&s), 1685);
    check("average fill 42.5 L", (StatsAvgFillMl(&s) + 50) / 100, 425);
    check("days between 11.7", StatsDaysBetweenX10(&s), 117);
    check("spend per month (4 months)", StatsSpendPerMonth(&s), 15002);
    check("cash spend", s.cashCents, 60008 - 7937);
    check("card spend", s.cardCents, 7937);
    check("CAD spend", s.curCents[1], 60008);
    check("Petro points earned", s.earned[2], 452 + 460 + 441 + 439 + 446 + 220 + 459 + 482);

    /* A missed fill breaks the chain: the next full fill starts a new one. */
    {
        Stats t;
        FillRecord f;
        StatsBegin(&t);
        memset(&f, 0, sizeof f);
        f.vehicle = 1; f.fxRate = 1000000UL; f.flags = fillFlagFullTank;
        f.date = 100; f.odometer = 10000; f.volume = 40000; f.total = 6000; StatsAdd(&t, &f);
        f.date = 110; f.odometer = 15000; f.volume = 40000; f.flags = fillFlagFullTank | fillFlagMissedPrev; StatsAdd(&t, &f);
        f.date = 120; f.odometer = 20000; f.volume = 40000; f.flags = fillFlagFullTank; StatsAdd(&t, &f);
        check("missed fill: one segment only", t.ftfDkm, 5000);
        check("missed fill: 8.0 L/100", StatsAvgEco(&t), 80);
    }
    /* Two vehicles: distance sums per car, economy pools the full-to-full totals. */
    {
        Stats t;
        FillRecord f;
        StatsBegin(&t);
        memset(&f, 0, sizeof f);
        f.fxRate = 1000000UL; f.flags = fillFlagFullTank;
        f.vehicle = 1; f.date = 100; f.odometer = 10000; f.volume = 40000; f.total = 6000; StatsAdd(&t, &f);
        f.vehicle = 2; f.date = 101; f.odometer = 500000; f.volume = 30000; f.total = 4500; StatsAdd(&t, &f);
        f.vehicle = 1; f.date = 110; f.odometer = 15000; f.volume = 40000; f.total = 6000; StatsAdd(&t, &f);
        f.vehicle = 2; f.date = 111; f.odometer = 510000; f.volume = 60000; f.total = 9000; StatsAdd(&t, &f);
        check("fleet distance 1,500 km", t.distance, 15000);
        check("fleet economy 6.7 L/100 (100 L over 1,500 km)", StatsAvgEco(&t), 67);
    }
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

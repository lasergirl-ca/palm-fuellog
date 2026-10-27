/* Host tests for src/calc.c. The "worst case" values are the SP2 bounds:
 * 20 years, 4 vehicles, two fills a week, 200 L per fill, $2,000 per receipt,
 * exchange rate 10.0, 100 points per litre at a 300 % premium multiplier. */
#include <stdio.h>
#include "../src/calc.h"

static int failures;

static void check(const char *name, UInt32 got, UInt32 want)
{
    if (got != want) {
        printf("FAIL %s: got %lu want %lu\n", name, (unsigned long)got, (unsigned long)want);
        failures++;
    } else {
        printf("ok   %s = %lu\n", name, (unsigned long)got);
    }
}

int main(void)
{
    /* Economy, full tank to full tank (sample data from the mock-ups). */
    check("Oct 19 segment 48.2 L / 572 km", CalcL100kmX10(48200, 5720), 84);
    check("Oct 08 segment 67.9 L / 840 km", CalcL100kmX10(67900, 8400), 81);
    check("lifetime 294.7 L / 3,562 km", CalcL100kmX10(294700, 35620), 83);
    check("zero distance", CalcL100kmX10(1000, 0), 0);
    /* Fleet lifetime: 10 x 1,664,000,000 mL passes 32 bits. */
    check("fleet 1,664,000 L / 4,000,000 km", CalcL100kmX10(1664000000UL, 40000000UL), 416);

    /* Totals: 178.9 c/L x 47.30 L = $84.62 (fill form sample); prices are ten-thousandths per litre. */
    check("total 17890 x 47300", CalcTotalCents(17890, 47300), 8462);
    /* $21.475/L x 200 L: the product (4.3e10) passes 32 bits, so this is always a 64-bit path. */
    check("total 214750 x 200000", CalcTotalCents(214750, 200000), 429500);
    check("total of a hydro rate 11.62 c/kWh x 38.6 kWh", CalcTotalCents(1162, 38600), 449);

    /* Home currency: $84.62 x 1.36 (product 11,508,320,000 passes 32 bits). */
    check("home 8462 x 1.36", CalcHomeCents(8462, 1360000), 11508);
    check("home 1.00", CalcHomeCents(8462, 1000000), 8462);
    /* Largest per-fill case: $2,000 x 10.0. */
    check("home 200000 x 10.0", CalcHomeCents(200000, 10000000), 2000000);

    /* Display conversions from 8.4 L/100 km. */
    check("US MPG x10 from 8.4", CalcMpgX10(84, 0), 280);
    check("Imp MPG x10 from 8.4", CalcMpgX10(84, 1), 336);
    check("US MPG x10 from 200.0", CalcMpgX10(2000, 0), 12);
    check("MPG of zero", CalcMpgX10(0, 0), 0);

    /* Points (Points dialog sample): Petro-Points 10 pts/L x 47.30 L = 473;
     * Triangle 3 c/L x 47.30 L = $1.42. */
    check("Petro-Points 10/L", CalcEarnedPoints(1000, calcEarnPerLitre, 47300, 8462, 100), 473);
    check("Triangle 3c/L", CalcEarnedPoints(300, calcEarnPerLitre, 47300, 8462, 100), 142);
    check("76 2 pts/US gal", CalcEarnedPoints(200, calcEarnPerUSGallon, 80308, 10499, 100), 42);
    check("1 pt per $", CalcEarnedPoints(100, calcEarnPerCurrency, 47300, 8462, 100), 85);
    check("premium 150%", CalcEarnedPoints(1000, calcEarnPerLitre, 47300, 8462, 150), 710);
    check("basis none", CalcEarnedPoints(1000, calcEarnNone, 47300, 8462, 100), 0);
    /* Worst case: 100 pts/L x 200 L x 300 % (product 6e11). */
    check("worst 100/L x 200 L x 300%", CalcEarnedPoints(10000, calcEarnPerLitre, 200000, 200000, 300), 60000);
    check("worst per gal", CalcEarnedPoints(10000, calcEarnPerUSGallon, 200000, 200000, 300), 15850);
    check("worst per $", CalcEarnedPoints(10000, calcEarnPerCurrency, 200000, 200000, 300), 600000);

    /* Charging (New Charge sample): 38.6 kWh over 254 km is 15.2 kWh/100 km,
     * and $4.25 over 254 km is $1.67 per 100 km. */
    check("EV 38.6 kWh / 254 km", CalcKwh100kmX10(38600, 2540), 152);
    check("EV cost per 100 km", CalcCostPer100kmCents(425, 2540), 167);
    check("fuel cost per 100 km (Stats sample)", CalcCostPer100kmCents(52071, 35620), 1462);
    check("cost per 100 km of zero distance", CalcCostPer100kmCents(425, 0), 0);

    /* Redemptions. Petro-Points: 1,000 pts = $1.00 off, whole steps only. */
    {
        CalcRedeemRule petro, kroger, pco, none;
        petro.kind = calcRedeemMoneyOff; petro.block = 1000; petro.value = 1000; petro.volUnit = 0; petro.maxVol = 0; petro.maxPerUnit = 0;
        check("Petro 3,399 pts -> $3.00", CalcDiscountCents(&petro, 3399, 47300, 8462), 300);
        check("Petro 999 pts -> nothing", CalcDiscountCents(&petro, 999, 47300, 8462), 0);
        check("Petro never above the total", CalcDiscountCents(&petro, 20000, 47300, 1500), 1500);
        /* Kroger: 100 pts = 10 c/gal off, up to $1/gal, on up to 35 gal. */
        kroger.kind = calcRedeemPerVolume; kroger.block = 100; kroger.value = 100; kroger.volUnit = calcVolUSGallon;
        kroger.maxVol = 132489; kroger.maxPerUnit = 1000;
        check("Kroger 100 pts on 12.495 gal -> $1.25", CalcDiscountCents(&kroger, 100, 47300, 8462), 125);
        check("Kroger 1,240 pts -> $1.00/gal cap -> $12.50", CalcDiscountCents(&kroger, 1240, 47300, 8462), 1250);
        check("Kroger 35 gal volume cap on 50 gal", CalcDiscountCents(&kroger, 1000, 189271, 30000), 3500);
        /* PC Optimum: 4,000 pts = 10 c/L off up to 40 L. */
        pco.kind = calcRedeemPerVolume; pco.block = 4000; pco.value = 100; pco.volUnit = calcVolLitre; pco.maxVol = 40000; pco.maxPerUnit = 0;
        check("PC Optimum 4,000 pts on 47.3 L -> 40 L cap -> $4.00", CalcDiscountCents(&pco, 4000, 47300, 8462), 400);
        check("PC Optimum 3,999 pts -> nothing", CalcDiscountCents(&pco, 3999, 47300, 8462), 0);
        none.kind = calcRedeemNone; none.block = 0; none.value = 0; none.volUnit = 0; none.maxVol = 0; none.maxPerUnit = 0;
        check("no redemption rule", CalcDiscountCents(&none, 5000, 47300, 8462), 0);
    }

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

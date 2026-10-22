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

    /* Totals: 178.9 c/L x 47.30 L = $84.62 (fill form sample). */
    check("total 1789 x 47300", CalcTotalCents(1789, 47300), 8462);
    /* $21.475/L x 200 L: the product passes 32 bits. */
    check("total 21475 x 200000", CalcTotalCents(21475, 200000), 429500);

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
    check("76 3c/US gal", CalcEarnedPoints(300, calcEarnPerUSGallon, 47300, 8462, 100), 37);
    check("1 pt per $", CalcEarnedPoints(100, calcEarnPerCurrency, 47300, 8462, 100), 85);
    check("premium 150%", CalcEarnedPoints(1000, calcEarnPerLitre, 47300, 8462, 150), 710);
    check("basis none", CalcEarnedPoints(1000, calcEarnNone, 47300, 8462, 100), 0);
    /* Worst case: 100 pts/L x 200 L x 300 % (product 6e11). */
    check("worst 100/L x 200 L x 300%", CalcEarnedPoints(10000, calcEarnPerLitre, 200000, 200000, 300), 60000);
    check("worst per gal", CalcEarnedPoints(10000, calcEarnPerUSGallon, 200000, 200000, 300), 15850);
    check("worst per $", CalcEarnedPoints(10000, calcEarnPerCurrency, 200000, 200000, 300), 600000);

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

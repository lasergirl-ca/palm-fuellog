/* Unit and currency conversions: known values from the design spec's factor
 * table, and round trips that must land within one display digit. */
#include <stdio.h>
#include "../src/units.h"
#include "../src/currency.h"
#include "../src/calc.h"

static int failures;

static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}

static void near(const char *name, long got, long want, long tol)
{
    long d = got > want ? got - want : want - got;
    if (d > tol) { printf("FAIL %s: got %ld want %ld (tolerance %ld)\n", name, got, want, tol); failures++; }
}

int main(void)
{
    UInt32 v;
    int bad = 0;

    /* Distance: 84,212.0 km = 52,326.7 mi. */
    check("km stays km", UnitDistToDisplay(842120, unitDistKm), 842120);
    check("84212.0 km in mi x10", UnitDistToDisplay(842120, unitDistMiles), 523269);
    check("100.0 mi in km x10", UnitDistFromDisplay(1000, unitDistMiles), 1609);
    /* Volume: 47.30 L = 12.495 US gal = 10.404 Imp gal. */
    check("47300 mL in L x100", UnitVolToDisplay(47300, unitVolLitres), 4730);
    check("47300 mL in US gal x1000", UnitVolToDisplay(47300, unitVolUSGal), 12495);
    check("47300 mL in Imp gal x1000", UnitVolToDisplay(47300, unitVolImpGal), 10405);
    check("12.495 US gal in mL", UnitVolFromDisplay(12495, unitVolUSGal), 47299);
    check("10.000 Imp gal in mL", UnitVolFromDisplay(10000, unitVolImpGal), 45461);
    /* Economy from 8.4 L/100 km. */
    check("8.4 L/100 as km/L x10", UnitEcoFuelToDisplay(84, unitEcoKmPerL), 119);
    check("8.4 L/100 as US MPG x10", UnitEcoFuelToDisplay(84, unitEcoMpgUS), 280);
    check("8.4 L/100 as Imp MPG x10", UnitEcoFuelToDisplay(84, unitEcoMpgImp), 336);
    /* Electric: 38.6 kWh over 254 km (New Charge sample) = 15.2 kWh/100 km = 152 Wh/km = 4.09 mi/kWh = 137.8 MPGe. */
    check("EV kWh/100 x10", UnitEcoElectricToDisplay(38600, 2540, unitEcoKwh100km), 152);
    check("EV Wh/km x10", UnitEcoElectricToDisplay(38600, 2540, unitEcoWhPerKm), 1520);
    check("EV mi/kWh x100", UnitEcoElectricToDisplay(38600, 2540, unitEcoMiPerKwh), 409);
    check("EV MPGe x10", UnitEcoElectricToDisplay(38600, 2540, unitEcoMpge), 1378);
    /* Price: $1.789/L = $6.772/US gal = $8.133/Imp gal; the paper log's $3.459/gal = $0.914/L. */
    check("1789 th/L per US gal", UnitPriceToDisplay(1789, unitPricePerUSGal), 6772);
    check("1789 th/L per Imp gal", UnitPriceToDisplay(1789, unitPricePerImpGal), 8133);
    check("3459 th/US gal per L", UnitPriceFromDisplay(3459, unitPricePerUSGal), 914);
    check("per kWh is identity", UnitPriceFromDisplay(1162, unitPricePerKwh), 1162);

    /* Round trips: display -> stored -> display within one display digit,
     * and stored -> display -> stored within the display digit's worth. */
    for (v = 1; v < 2000000UL; v = v * 3 / 2 + 7) {
        UInt32 d;
        d = UnitDistToDisplay(UnitDistFromDisplay(v, unitDistMiles), unitDistMiles);
        if (d != v && d + 1 != v && d != v + 1) { bad++; printf("  miles %lu -> %lu\n", (unsigned long)v, (unsigned long)d); }
        d = UnitVolToDisplay(UnitVolFromDisplay(v, unitVolUSGal), unitVolUSGal);
        if (d != v && d + 1 != v && d != v + 1) { bad++; printf("  US gal %lu -> %lu\n", (unsigned long)v, (unsigned long)d); }
        d = UnitVolToDisplay(UnitVolFromDisplay(v, unitVolImpGal), unitVolImpGal);
        if (d != v && d + 1 != v && d != v + 1) { bad++; printf("  Imp gal %lu -> %lu\n", (unsigned long)v, (unsigned long)d); }
        d = UnitVolToDisplay(UnitVolFromDisplay(v, unitVolLitres), unitVolLitres);
        if (d != v) { bad++; printf("  litres %lu -> %lu\n", (unsigned long)v, (unsigned long)d); }
        /* Prices are stored in thousandths per litre, and a gallon is 3.785 L,
         * so a per-gallon price can come back up to 2 thousandths off: the
         * stored grain is coarser than the typed one. See docs/M1.md. */
        d = UnitPriceToDisplay(UnitPriceFromDisplay(v, unitPricePerUSGal), unitPricePerUSGal);
        if (d + 2 < v || d > v + 2) { bad++; printf("  price/gal %lu -> %lu\n", (unsigned long)v, (unsigned long)d); }
    }
    check("round trips off by more than one digit", bad, 0);
    near("mL -> gal -> mL within 2 mL", UnitVolFromDisplay(UnitVolToDisplay(47300, unitVolUSGal), unitVolUSGal), 47300, 2);
    near("dkm -> mi -> dkm within 0.1 km", UnitDistFromDisplay(UnitDistToDisplay(842120, unitDistMiles), unitDistMiles), 842120, 1);

    /* Currency rules. */
    check("vehicle override wins", CurrencyForNewEntry(curUSD, curCAD, curEUR), curUSD);
    check("prefs default next", CurrencyForNewEntry(255, curCAD, curEUR), curCAD);
    check("last used last", CurrencyForNewEntry(255, curLastUsed, curEUR), curEUR);
    check("nothing set falls to CAD", CurrencyForNewEntry(255, curLastUsed, 255), curCAD);
    {
        UInt32 rates[curCount] = { 1360000, 0, 1480000, 0 };
        check("home currency rate is 1.0", CurrencyDefaultRate(curCAD, curCAD, rates), 1000000);
        check("USD rate from table", CurrencyDefaultRate(curUSD, curCAD, rates), 1360000);
        check("AUD unset rate is 1.0", CurrencyDefaultRate(curAUD, curCAD, rates), 1000000);
    }
    /* Totals never move when a rate is edited later: the stored fxRate is what converts. */
    check("stored rate converts $84.62 -> $115.08", CalcHomeCents(8462, 1360000), 11508);
    check("Excel format for CAD", CurrencyExcelFormat(curCAD)[2], 'C');

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

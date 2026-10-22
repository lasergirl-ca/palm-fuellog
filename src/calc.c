#include "calc.h"

typedef unsigned long long U64;

UInt32 CalcMulDiv(UInt32 a, UInt32 b, UInt32 c)
{
    U64 n = (U64)a * b;
    return (UInt32)((n + c / 2) / c);
}

UInt32 CalcTotalCents(UInt32 priceTh, UInt32 volumeMl)
{
    /* th/L * mL = th*mL; / 1,000 (mL->L) / 1,000 (th->units) * 100 (cents) */
    return CalcMulDiv(priceTh, volumeMl, 10000);
}

UInt32 CalcHomeCents(UInt32 cents, UInt32 fxRate)
{
    return CalcMulDiv(cents, fxRate, 1000000);
}

UInt32 CalcL100kmX10(UInt32 ml, UInt32 dkm)
{
    /* L/100km = 100 * (ml / 1000) / (dkm / 10) = ml / dkm, so x10 = 10 ml / dkm */
    if (dkm == 0)
        return 0;
    return CalcMulDiv(ml, 10, dkm);
}

UInt32 CalcMpgX10(UInt32 l100x10, UInt8 imperial)
{
    /* US: 235.2146 / (L/100km); Imperial: 282.4810. With l100x10 = 10 x L/100km,
     * MPG x 10 = 2,352,146 / (100 x l100x10). */
    UInt32 k = imperial ? 2824810UL : 2352146UL;
    if (l100x10 == 0)
        return 0;
    return (k + 50 * l100x10) / (100 * l100x10);
}

UInt32 CalcEarnedPoints(UInt32 earnRate, UInt8 basis, UInt32 volumeMl,
                        UInt32 grossCents, UInt16 premiumMult)
{
    U64 n;
    U64 d;

    switch (basis) {
    case calcEarnPerLitre:
        /* rate (pts x100 / L) * mL / 1000 / 100 */
        n = (U64)earnRate * volumeMl;
        d = 100000ULL;
        break;
    case calcEarnPerUSGallon:
        /* rate (pts x100 / gal) * mL / 3785.411784 / 100 */
        n = (U64)earnRate * volumeMl * 10000ULL;
        d = 3785411784ULL;
        break;
    case calcEarnPerCurrency:
        /* rate (pts x100 / unit) * cents / 100 / 100 */
        n = (U64)earnRate * grossCents;
        d = 10000ULL;
        break;
    default:
        return 0;
    }
    n *= premiumMult;
    d *= 100;
    return (UInt32)((n + d / 2) / d);
}

UInt32 CalcKwh100kmX10(UInt32 wh, UInt32 dkm)
{
    if (dkm == 0)
        return 0;
    return CalcMulDiv(wh, 10, dkm);
}

UInt32 CalcCostPer100kmCents(UInt32 homeCents, UInt32 dkm)
{
    if (dkm == 0)
        return 0;
    return CalcMulDiv(homeCents, 1000, dkm);   /* cents / (dkm / 1000) */
}

UInt32 CalcDiscountCents(const CalcRedeemRule *rule, UInt32 points, UInt32 volumeMl, UInt32 grossCents)
{
    UInt32 steps, cents;
    U64 n;

    if (rule->kind == calcRedeemNone || rule->block == 0 || points < rule->block)
        return 0;
    steps = points / rule->block;
    if (rule->kind == calcRedeemMoneyOff) {
        n = (U64)steps * rule->value;                       /* thousandths */
        cents = (UInt32)((n + 5) / 10);
    } else {
        UInt32 vol = volumeMl;
        U64 perUnitTh = (U64)steps * rule->value;           /* thousandths per volume unit */
        if (rule->maxVol && vol > rule->maxVol) vol = rule->maxVol;
        if (rule->maxPerUnit && perUnitTh > rule->maxPerUnit) perUnitTh = rule->maxPerUnit;
        if (rule->volUnit == calcVolUSGallon)
            n = perUnitTh * vol * 1000000ULL / 3785411784ULL; /* thousandths over the gallons (mL / 3785.411784) */
        else
            n = perUnitTh * vol / 1000ULL;                  /* thousandths over the litres */
        cents = (UInt32)((n + 5) / 10);
    }
    return cents > grossCents ? grossCents : cents;
}

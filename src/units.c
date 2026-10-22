#include "units.h"
#include "calc.h"

/* 1 mi = 1.609344 km; 1 US gal = 3.785411784 L; 1 Imp gal = 4.54609 L;
 * US MPG = 235.2146 / (L/100 km); Imp MPG = 282.4810 / (L/100 km);
 * MPGe uses 33.705 kWh per US gallon. */

UInt32 UnitDistToDisplay(UInt32 dkm, UInt8 unit)
{
    if (unit == unitDistMiles) return CalcMulDiv(dkm, 1000000UL, 1609344UL);
    return dkm;
}

UInt32 UnitDistFromDisplay(UInt32 valueX10, UInt8 unit)
{
    if (unit == unitDistMiles) return CalcMulDiv(valueX10, 1609344UL, 1000000UL);
    return valueX10;
}

UInt8 UnitVolDecimals(UInt8 unit)
{
    return unit == unitVolLitres ? 2 : 3;
}

UInt32 UnitVolToDisplay(UInt32 ml, UInt8 unit)
{
    switch (unit) {
    case unitVolUSGal:  return CalcMulDiv(ml, 1000000000UL, 3785411784UL); /* gal x 1000 */
    case unitVolImpGal: return CalcMulDiv(ml, 100000UL, 454609UL);          /* gal x 1000 */
    default:            return (ml + 5) / 10;                            /* L x 100 */
    }
}

UInt32 UnitVolFromDisplay(UInt32 value, UInt8 unit)
{
    switch (unit) {
    case unitVolUSGal:  return CalcMulDiv(value, 3785411784UL, 1000000000UL);
    case unitVolImpGal: return CalcMulDiv(value, 454609UL, 100000UL);
    default:            return value * 10;
    }
}

UInt32 UnitEcoFuelToDisplay(UInt32 l100x10, UInt8 unit)
{
    if (l100x10 == 0) return 0;
    switch (unit) {
    case unitEcoKmPerL:  return (10000UL + l100x10 / 2) / l100x10;    /* km/L x 10 = 10000 / l100x10 */
    case unitEcoMpgUS:   return CalcMpgX10(l100x10, 0);
    case unitEcoMpgImp:  return CalcMpgX10(l100x10, 1);
    default:             return l100x10;
    }
}

UInt32 UnitEcoElectricToDisplay(UInt32 wh, UInt32 dkm, UInt8 unit)
{
    if (dkm == 0 || wh == 0) return 0;
    switch (unit) {
    case unitEcoWhPerKm:   return CalcMulDiv(wh, 100, dkm);           /* Wh/km x 10 = 100 Wh / dkm */
    case unitEcoMiPerKwh:  return CalcMulDiv(dkm, 621371UL, wh * 100UL); /* mi/kWh x 100 */
    case unitEcoMpge: {    /* MPGe x 10 = mi/kWh x 33.705 x 10, in one 64-bit step */
        unsigned long long num = (unsigned long long)dkm * 621371ULL * 33705ULL;
        unsigned long long den = (unsigned long long)wh * 1000000ULL;
        return (UInt32)((num + den / 2) / den);
    }
    default:               return CalcMulDiv(wh, 10, dkm);            /* kWh/100 km x 10 = 10 Wh / dkm */
    }
}

UInt32 UnitPriceToDisplay(UInt32 thPerLitre, UInt8 unit)
{
    switch (unit) {
    case unitPricePerUSGal:  return CalcMulDiv(thPerLitre, 3785411784UL, 1000000000UL);
    case unitPricePerImpGal: return CalcMulDiv(thPerLitre, 454609UL, 100000UL);
    default:                 return thPerLitre;
    }
}

UInt32 UnitPriceFromDisplay(UInt32 thPerUnit, UInt8 unit)
{
    switch (unit) {
    case unitPricePerUSGal:  return CalcMulDiv(thPerUnit, 1000000000UL, 3785411784UL);
    case unitPricePerImpGal: return CalcMulDiv(thPerUnit, 100000UL, 454609UL);
    default:                 return thPerUnit;
    }
}

const char *UnitDistLabel(UInt8 unit) { return unit == unitDistMiles ? "mi" : "km"; }

const char *UnitVolLabel(UInt8 unit)
{
    switch (unit) {
    case unitVolUSGal:  return "US gal";
    case unitVolImpGal: return "Imp gal";
    default:            return "L";
    }
}

const char *UnitEcoLabel(UInt8 unit)
{
    switch (unit) {
    case unitEcoKmPerL:    return "km/L";
    case unitEcoMpgUS:     return "MPG (US)";
    case unitEcoMpgImp:    return "MPG (Imp)";
    case unitEcoKwh100km:  return "kWh/100 km";
    case unitEcoWhPerKm:   return "Wh/km";
    case unitEcoMiPerKwh:  return "mi/kWh";
    case unitEcoMpge:      return "MPGe";
    default:               return "L/100 km";
    }
}

const char *UnitPriceLabel(UInt8 unit)
{
    switch (unit) {
    case unitPricePerUSGal:  return "/US gal";
    case unitPricePerImpGal: return "/Imp gal";
    case unitPricePerKwh:    return "/kWh";
    default:                 return "/L";
    }
}

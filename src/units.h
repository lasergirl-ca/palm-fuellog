/* Display-unit conversions. Everything is stored metric in fixed point:
 * distance km x 10, volume mL (or Wh), economy L/100 km x 10, price in
 * ten-thousandths of a currency unit per litre (or per kWh). These functions
 * convert to and from the units the user picked in Prefs, always through
 * 64-bit intermediates, rounding to nearest. Display values carry a fixed
 * number of decimals noted beside each function. */
#ifndef UNITS_H
#define UNITS_H
#include "ptypes.h"
#include "sections.h"

#define unitDistKm    0
#define unitDistMiles 1

#define unitVolLitres 0
#define unitVolUSGal  1
#define unitVolImpGal 2

#define unitEcoL100km 0
#define unitEcoKmPerL 1
#define unitEcoMpgUS  2
#define unitEcoMpgImp 3
#define unitEcoKwh100km 4
#define unitEcoWhPerKm  5
#define unitEcoMiPerKwh 6
#define unitEcoMpge     7

#define unitPricePerLitre  0
#define unitPricePerUSGal  1
#define unitPricePerImpGal 2
#define unitPricePerKwh    3

#define unitPriceFormatDollars 0   /* $1.789 */
#define unitPriceFormatCents   1   /* 178.9 c */

/* Distance: stored km x 10 <-> display units x 10 (one decimal). */
UInt32 UnitDistToDisplay(UInt32 dkm, UInt8 unit) EXPORT_SEC;
UInt32 UnitDistFromDisplay(UInt32 valueX10, UInt8 unit) EXPORT_SEC;

/* Volume: stored mL <-> display units x 100 (litres, two decimals) or
 * x 1000 (gallons, three decimals). The decimals function says which. */
UInt8  UnitVolDecimals(UInt8 unit) EXPORT_SEC;
UInt32 UnitVolToDisplay(UInt32 ml, UInt8 unit) EXPORT_SEC;
UInt32 UnitVolFromDisplay(UInt32 value, UInt8 unit) EXPORT_SEC;

/* Economy: from stored L/100 km x 10 (fuel) or Wh and km x 10 (charging)
 * to the chosen unit x 10 (one decimal); 0 when undefined. */
UInt32 UnitEcoFuelToDisplay(UInt32 l100x10, UInt8 unit) EXPORT_SEC;
UInt32 UnitEcoElectricToDisplay(UInt32 wh, UInt32 dkm, UInt8 unit) EXPORT_SEC;

/* Price: stored ten-thousandths per litre <-> ten-thousandths per display unit.
 * Per-kWh prices are stored as typed, so that case is the identity. */
UInt32 UnitPriceToDisplay(UInt32 thPerLitre, UInt8 unit) EXPORT_SEC;
UInt32 UnitPriceFromDisplay(UInt32 thPerUnit, UInt8 unit) EXPORT_SEC;

/* Label text for each unit, for form labels and export headers. */
const char *UnitDistLabel(UInt8 unit) EXPORT_SEC;
const char *UnitVolLabel(UInt8 unit) EXPORT_SEC;
const char *UnitEcoLabel(UInt8 unit) EXPORT_SEC;
const char *UnitPriceLabel(UInt8 unit) EXPORT_SEC;

#endif

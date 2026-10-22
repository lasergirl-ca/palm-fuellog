/* Fixed-point fuel maths. Plain C with no Palm headers, so it is unit-tested
 * on the host. Units follow the data model: mL (or Wh), km x 10 (dkm),
 * cents, price in thousandths of a currency unit per litre (or per kWh),
 * fxRate in home units per fill unit x 1,000,000.
 *
 * Every product that can pass 32 bits goes through a 64-bit intermediate
 * (see docs/spikes/SP2.md for the bounds). Results are rounded to nearest. */
#ifndef CALC_H
#define CALC_H
#include "ptypes.h"

/* a * b / c rounded to nearest, with a 64-bit intermediate. c must be > 0. */
UInt32 CalcMulDiv(UInt32 a, UInt32 b, UInt32 c);

/* Total in cents for priceTh thousandths per litre and volumeMl millilitres
 * (or thousandths per kWh and Wh). */
UInt32 CalcTotalCents(UInt32 priceTh, UInt32 volumeMl);

/* Cents in the home currency for cents in the fill currency at fxRate. */
UInt32 CalcHomeCents(UInt32 cents, UInt32 fxRate);

/* L/100 km x 10 for ml millilitres over dkm tenths of a kilometre; 0 when
 * dkm is 0. Safe for fleet lifetime sums. */
UInt32 CalcL100kmX10(UInt32 ml, UInt32 dkm);

/* MPG x 10 from L/100 km x 10; imperial selects the Imperial gallon.
 * 0 when l100x10 is 0. */
UInt32 CalcMpgX10(UInt32 l100x10, UInt8 imperial);

/* Earn bases, matching the programme record. */
#define calcEarnNone        0
#define calcEarnPerLitre    1
#define calcEarnPerUSGallon 2
#define calcEarnPerCurrency 3

/* Points earned on one fill. earnRate is points x 100 per basis unit,
 * premiumMult is a percentage (100 = none), grossCents is price x volume
 * before any discount. */
UInt32 CalcEarnedPoints(UInt32 earnRate, UInt8 basis, UInt32 volumeMl,
                        UInt32 grossCents, UInt16 premiumMult);

#endif

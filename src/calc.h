/* Fixed-point fuel maths. Plain C, no Palm headers, so it is unit-tested on
 * the host. Units follow the data model: mL, km x 10 (dkm), cents. */
#ifndef CALC_H
#define CALC_H
#include "ptypes.h"

/* L/100 km x 10 for `ml` millilitres over `dkm` tenths of a kilometre,
 * rounded to nearest. Returns 0 when dkm is 0. */
UInt32 CalcL100kmX10(UInt32 ml, UInt32 dkm);

#endif

#include "calc.h"

/* L/100km = 100 * (ml / 1000) / (dkm / 10) = ml / dkm, so x10 = 10 * ml / dkm.
 * ml is at most a few hundred thousand per full-to-full segment, so 10 * ml
 * stays well inside 32 bits. */
UInt32 CalcL100kmX10(UInt32 ml, UInt32 dkm)
{
    if (dkm == 0)
        return 0;
    return (10UL * ml + dkm / 2) / dkm;
}

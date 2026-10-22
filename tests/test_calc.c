#include <stdio.h>
#include "../src/calc.h"

static int failures;

static void check(const char *name, UInt32 got, UInt32 want)
{
    if (got != want) {
        printf("FAIL %s: got %lu want %lu\n", name, (unsigned long)got, (unsigned long)want);
        failures++;
    } else {
        printf("ok   %s\n", name);
    }
}

int main(void)
{
    /* Oct 19 sample fill: 48.2 L over 84,212 - 83,640 = 572 km -> 8.4 */
    check("Oct 19 segment", CalcL100kmX10(48200, 5720), 84);
    /* Oct 08 full-to-full over the Sep 27 partial: 67.9 L over 840 km -> 8.1 */
    check("Oct 08 segment", CalcL100kmX10(67900, 8400), 81);
    /* Lifetime: 294.7 L after the baseline over 3,562 km -> 8.3 */
    check("lifetime", CalcL100kmX10(294700, 35620), 83);
    check("zero distance", CalcL100kmX10(1000, 0), 0);
    return failures ? 1 : 0;
}

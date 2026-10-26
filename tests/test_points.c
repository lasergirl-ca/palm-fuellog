/* Kroger-style ageing: points expire at the end of the month after earning,
 * and redemptions drain the oldest lots first. */
#include <stdio.h>
#include "../src/points.h"
#include "../src/records.h"
#include "../src/datecalc.h"
static int failures;
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}
int main(void)
{
    PointsAge a; UInt16 when;
    UInt16 oct1 = DateYmdToDays(2026, 10, 1), sep20 = DateYmdToDays(2026, 9, 20), oct10 = DateYmdToDays(2026, 10, 10), oct22 = DateYmdToDays(2026, 10, 22);
    check("Sep 20 earns expire Oct 31", PointsExpiryDate(pgmExpiryMonthAfter, 0, sep20), DateYmdToDays(2026, 10, 31));
    check("Oct 10 earns expire Nov 30", PointsExpiryDate(pgmExpiryMonthAfter, 0, oct10), DateYmdToDays(2026, 11, 30));
    check("12 months rule", PointsExpiryDate(pgmExpiryMonths, 12, oct10), DateYmdToDays(2027, 10, 10));
    check("no rule never expires", PointsExpiryDate(pgmExpiryNone, 0, oct10), 0);
    PointsBegin(&a, pgmExpiryMonthAfter, 0);
    PointsAdd(&a, sep20, 400, 0);
    PointsAdd(&a, oct10, 600, 0);
    PointsAdd(&a, oct22, 0, 300);          /* drains the September lot first */
    check("expiring by Oct 31 from Oct 22: 100 left of September", PointsExpiringBy(&a, oct22, DateYmdToDays(2026, 10, 31), &when), 100);
    check("...on Oct 31", when, DateYmdToDays(2026, 10, 31));
    check("expiring by Nov 30: 700", PointsExpiringBy(&a, oct22, DateYmdToDays(2026, 11, 30), &when), 700);
    check("nothing expired yet", PointsExpired(&a, oct22), 0);
    check("by Nov 5 the September lot has gone", PointsExpired(&a, DateYmdToDays(2026, 11, 5)), 100);
    {
        /* a redemption after the September lot expired comes out of October */
        PointsAge b;
        UInt16 nov10 = DateYmdToDays(2026, 11, 10);
        PointsBegin(&b, pgmExpiryMonthAfter, 0);
        PointsAdd(&b, 0, 50, 0);               /* opening balance: never expires, spent first */
        PointsAdd(&b, sep20, 400, 0);
        PointsAdd(&b, oct10, 600, 0);
        PointsAdd(&b, nov10, 0, 300);
        check("expired September lot stays expired", PointsExpired(&b, nov10), 400);
        check("November redemption drains opening then October", PointsExpiringBy(&b, nov10, DateYmdToDays(2026, 11, 30), &when), 350);
    }
    PointsBegin(&a, pgmExpiryNone, 0);
    PointsAdd(&a, oct1, 1000, 0);
    check("never-expiring programme flags nothing", PointsExpiringBy(&a, oct22, DateYmdToDays(2027, 12, 31), &when), 0);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

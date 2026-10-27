#include <stdio.h>
#include <string.h>
#include "../src/datecalc.h"
static int failures;
static void check(const char *name, long got, long want)
{ if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; } else printf("ok   %s = %ld\n", name, got); }
int main(void)
{
    UInt16 y; UInt8 m, d; char b[8]; UInt32 v; int bad = 0;
    check("1904-01-01", DateYmdToDays(1904, 1, 1), 0);
    check("1904-03-01 (leap year)", DateYmdToDays(1904, 3, 1), 60);
    check("2026-07-29", DateYmdToDays(2026, 7, 29), 44770);
    check("2026-10-22", DateYmdToDays(2026, 10, 22), 44855);
    check("2080-03-01 (leap year)", DateYmdToDays(2080, 3, 1), DateYmdToDays(2080, 2, 29) + 1);
    check("range ends in 2083", DateYmdToDays(2083, 6, 6), 65535);
    DateDaysToYmd(44852, &y, &m, &d); check("44852 year", y, 2026); check("44852 month", m, 10); check("44852 day", d, 19);
    DateShortText(b, 44852); check("short text Oct 19", strcmp(b, "Oct 19"), 0);
    DateShortText(b, 44806); check("short text Sep 3", strcmp(b, "Sep 3"), 0);
    for (v = 0; v < 65536UL; v += 37) { DateDaysToYmd((UInt16)v, &y, &m, &d); if (DateYmdToDays(y, m, d) != v) bad++; }
    check("round trips over the whole range", bad, 0);
    check("May 14 + 6 months = Nov 14", DateAddMonths(DateYmdToDays(2026, 5, 14), 6), DateYmdToDays(2026, 11, 14));
    check("Jan 31 + 1 month = Feb 28", DateAddMonths(DateYmdToDays(2026, 1, 31), 1), DateYmdToDays(2026, 2, 28));
    check("Nov 18 2024 + 24 months", DateAddMonths(DateYmdToDays(2024, 11, 18), 24), DateYmdToDays(2026, 11, 18));
    check("1998 clock on a 2026 build is shifted", DateClockShift(1998, 2026), dateClockShiftDays);
    check("2026 clock is not", DateClockShift(2026, 2026), 0);
    check("2011 clock is shifted, 2012 is not", DateClockShift(2011, 2026) != 0 && DateClockShift(2012, 2026) == 0, 1);
    check("28 years is 10227 days", DateYmdToDays(2026, 10, 27) - DateYmdToDays(1998, 10, 27), dateClockShiftDays);
    {
        UInt16 dev = DateYmdToDays(1998, 10, 27), s = dateClockShiftDays;
        check("device-dated fill moves to 2026", DateUnshift(DateYmdToDays(1998, 9, 1), dev, s), DateYmdToDays(2026, 9, 1));
        check("real 2026 date left alone", DateUnshift(DateYmdToDays(2026, 9, 1), dev, s), DateYmdToDays(2026, 9, 1));
        check("1996 is inside the window", DateUnshift(DateYmdToDays(1996, 1, 1), dev, s), DateYmdToDays(2024, 1, 1));
        check("1994 is outside it", DateUnshift(DateYmdToDays(1994, 1, 1), dev, s), DateYmdToDays(1994, 1, 1));
        check("no shift, nothing moves", DateUnshift(DateYmdToDays(1998, 9, 1), dev, 0), DateYmdToDays(1998, 9, 1));
    }
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

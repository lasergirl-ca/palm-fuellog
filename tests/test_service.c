/* The spec's Outback example: oil every 8,000 km or 6 months, last done
 * May 14 2026 at 76,852 km; today Oct 22 2026 at 84,212 km, 43 km/day. */
#include <stdio.h>
#include <string.h>
#include "../src/service.h"
#include "../src/datecalc.h"

static int failures;
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}

int main(void)
{
    ServiceTypeRecord oil, wipers, blank;
    SvcDue d;
    UInt16 today = DateYmdToDays(2026, 10, 22), last = DateYmdToDays(2026, 5, 14);
    UInt32 pace = SvcPaceX10(803420, 842120, today - 90, today);   /* 3,870 km in 90 days */

    check("pace 43 km/day", pace, 430);
    memset(&oil, 0, sizeof oil);
    oil.intervalDist = 80000; oil.intervalMonths = 6; oil.warnDist = SvcDefaultWarnDist(80000); oil.warnDays = 14;
    check("warn 800 km", oil.warnDist, 8000);
    SvcCompute(&oil, 1, last, 768520, 842120, today, pace, &d);
    check("oil: 640 km left", d.kmLeftX10, 6400);
    check("oil: time due Nov 14", d.timeDue, DateYmdToDays(2026, 11, 14));
    check("oil: projected Nov 6 at 43 km/day", d.distDue, today + 15);
    check("oil: due is the earlier (projected)", d.due, d.distDue);
    check("oil: Soon", d.status, svcStatusSoon);

    SvcCompute(&oil, 1, last, 768520, 850000, today, pace, &d);
    check("oil past 85,000: Overdue", d.status, svcStatusOverdue);
    check("oil past: km left negative", d.kmLeftX10, -(85000 - 84852) * 10);

    SvcCompute(&oil, 1, last, 768520, 800000, today, pace, &d);
    check("oil at 80,000: OK", d.status, svcStatusOK);

    memset(&wipers, 0, sizeof wipers);
    wipers.intervalMonths = 12; wipers.warnDays = 14;
    SvcCompute(&wipers, 1, DateYmdToDays(2025, 10, 15), 700000, 842120, today, pace, &d);
    check("wipers: Overdue since Oct 15", d.status, svcStatusOverdue);
    check("wipers: 7 days over", d.daysLeft, -7);
    check("wipers: no distance limit", d.hasDist, 0);

    memset(&blank, 0, sizeof blank);
    SvcCompute(&blank, 1, last, 768520, 842120, today, pace, &d);
    check("no interval set", d.status, svcStatusNoInterval);
    SvcCompute(&oil, 0, 0, 0, 842120, today, pace, &d);
    check("no visit yet", d.status, svcStatusNoVisit);

    /* No pace known: the distance has no projected date, status still from km left. */
    SvcCompute(&oil, 1, last, 768520, 842120, today, 0, &d);
    check("no pace: projected 0", d.distDue, 0);
    check("no pace: due is the time date", d.due, DateYmdToDays(2026, 11, 14));
    check("no pace: still Soon by km", d.status, svcStatusSoon);

    check("167 days is 5.5 months", SvcMonthsX10(167), 55);
    check("365 days is 12.0 months", SvcMonthsX10(365), 120);
    check("fuel presets", SvcPresetCount(0), 9);
    check("electric presets", SvcPresetCount(3), 5);
    SvcPreset(0, 0, &oil);
    check("preset warn days", oil.warnDays, 14);
    check("preset interval blank", oil.intervalDist, 0);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

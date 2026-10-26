#include "points.h"
#include "records.h"
#include "datecalc.h"

void PointsBegin(PointsAge *a, UInt8 rule, UInt8 months)
{
    a->rule = rule; a->months = months; a->count = 0;
}

UInt16 PointsExpiryDate(UInt8 rule, UInt8 months, UInt16 date)
{
    UInt16 y; UInt8 m, d;
    if (rule == pgmExpiryMonthAfter) {
        /* the last day of the month after the month earned */
        UInt16 next = DateAddMonths(date, 1);
        DateDaysToYmd(next, &y, &m, &d);
        return DateYmdToDays(y, m, DateDaysInMonth(y, m));
    }
    if (rule == pgmExpiryMonths && months) return DateAddMonths(date, months);
    return 0;
}

void PointsAdd(PointsAge *a, UInt16 date, Int32 earned, Int32 redeemed)
{
    UInt16 i;
    if (earned > 0) {
        if (a->count == ptsMaxLots) {
            /* full: fold the two oldest lots together */
            a->left[1] += a->left[0];
            for (i = 0; i + 1 < a->count; i++) { a->left[i] = a->left[i + 1]; a->expires[i] = a->expires[i + 1]; }
            a->count--;
        }
        a->expires[a->count] = date ? PointsExpiryDate(a->rule, a->months, date) : 0;   /* date 0: the opening balance, which never expires */
        a->left[a->count] = earned;
        a->count++;
    }
    if (redeemed > 0) {
        for (i = 0; i < a->count && redeemed > 0; i++) {
            Int32 take;
            if (a->expires[i] && a->expires[i] < date) continue;   /* already gone when this was redeemed */
            take = a->left[i] < redeemed ? a->left[i] : redeemed;
            a->left[i] -= take; redeemed -= take;
        }
    }
}

Int32 PointsExpiringBy(const PointsAge *a, UInt16 today, UInt16 until, UInt16 *when)
{
    UInt16 i; Int32 sum = 0; *when = 0;
    for (i = 0; i < a->count; i++) {
        if (!a->expires[i] || a->left[i] <= 0) continue;
        if (a->expires[i] >= today && a->expires[i] <= until) { sum += a->left[i]; if (!*when || a->expires[i] < *when) *when = a->expires[i]; }
    }
    return sum;
}

Int32 PointsExpired(const PointsAge *a, UInt16 today)
{
    UInt16 i; Int32 sum = 0;
    for (i = 0; i < a->count; i++) if (a->expires[i] && a->expires[i] < today && a->left[i] > 0) sum += a->left[i];
    return sum;
}

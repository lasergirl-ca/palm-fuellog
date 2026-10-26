/* Points ageing: earned lots are consumed first in, first out by redemptions,
 * and the lots that remain expire by the programme's rule. Plain C. */
#ifndef POINTS_H
#define POINTS_H
#include "ptypes.h"
#include "sections.h"

#define ptsMaxLots 96

typedef struct {
    UInt8  rule, months;        /* pgmExpiry* and the N for pgmExpiryMonths */
    UInt16 count;
    UInt16 expires[ptsMaxLots]; /* date each lot expires, 0 = never */
    Int32  left[ptsMaxLots];    /* points still in the lot */
} PointsAge;

void   PointsBegin(PointsAge *a, UInt8 rule, UInt8 months) EXPORT_SEC;
/* Fills in date order: earned opens a lot (date 0 = the opening balance, never expiring),
 * redeemed drains the oldest lots still alive on that date. */
void   PointsAdd(PointsAge *a, UInt16 date, Int32 earned, Int32 redeemed) EXPORT_SEC;
/* The expiry date of a lot earned on `date` under the rule (0 = never). */
UInt16 PointsExpiryDate(UInt8 rule, UInt8 months, UInt16 date) EXPORT_SEC;
/* Points in lots that expire on or after today and by `until` (inclusive); *when gets the earliest such date. */
Int32  PointsExpiringBy(const PointsAge *a, UInt16 today, UInt16 until, UInt16 *when) EXPORT_SEC;
Int32  PointsExpired(const PointsAge *a, UInt16 today) EXPORT_SEC;   /* lots already past */

#endif

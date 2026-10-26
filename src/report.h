/* Period reports: one summary row per month of a year, or per year. Plain
 * C, fed fills oldest first (the caller filters the vehicle). Economy per
 * row pools the full-to-full segments that close in that period. */
#ifndef REPORT_H
#define REPORT_H
#include "ptypes.h"
#include "records.h"
#include "sections.h"

#define rptMonthly 0
#define rptYearly  1
#define rptRange   2          /* one row: the dates from..to */
#define rptMaxRows 12
#define rptMaxId   17

typedef struct {
    UInt16 fills;
    UInt32 ml, wh, homeCents, dkm, ftfMl, ftfDkm;
    Int32  points;           /* earned minus redeemed */
} ReportRow;

typedef struct {
    UInt8  mode;
    UInt16 year;             /* monthly: the year shown; yearly: the first of up to 12 years */
    UInt16 from, to;         /* rptRange: inclusive dates */
    ReportRow rows[rptMaxRows];
    ReportRow total;
    ReportRow byVehicle[rptMaxId];   /* the same sums per vehicle id, for the breakdown */
    UInt32 cashCents, cardCents, curCents[4], serviceCents;
    struct { UInt32 lastFullOdo, pending; UInt8 haveFull; } chain[rptMaxId][2];
    struct { UInt32 lastOdo; UInt8 seen; } veh[rptMaxId];
} Report;

void  ReportBegin(Report *r, UInt8 mode, UInt16 year) EXPORT_SEC;
void  ReportSetRange(Report *r, UInt16 from, UInt16 to) EXPORT_SEC;   /* for rptRange */
Int16 ReportRowFor(const Report *r, UInt16 date) EXPORT_SEC;   /* row index, or -1 outside the period */
void  ReportAdd(Report *r, const FillRecord *f) EXPORT_SEC;
UInt32 ReportRowEco(const ReportRow *row) EXPORT_SEC;        /* L/100 km x 10, 0 when none */
UInt32 ReportRowPrice(const ReportRow *row) EXPORT_SEC;      /* ten-thousandths per L, home currency */

#endif

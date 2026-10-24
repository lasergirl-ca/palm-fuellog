/* Lifetime stats over a run of fills, oldest first. Plain C: the device
 * feeds it records from the database, the host feeds it the spec's
 * sample log. Economy is full tank to full tank per vehicle, never per
 * fill; everything else is a running sum. */
#ifndef STATS_H
#define STATS_H
#include "ptypes.h"
#include "records.h"
#include "sections.h"

#define statsMaxId 17          /* vehicle and programme ids run 1..16 */

typedef struct {
    UInt32 lastFullOdo, pending;   /* volume since the last full fill, mL or Wh */
    UInt8  haveFull;
} StatsChain;

typedef struct {
    UInt16 count, fuelCount, chargeCount;
    UInt16 firstDate, lastDate;
    UInt32 distance;               /* km x 10, last minus first odometer, summed per vehicle */
    UInt32 fuelMl, wh;
    UInt32 homeCents, fuelHomeCents, elecHomeCents, discountCents;
    UInt32 cashCents, cardCents;   /* home cents by payment */
    UInt32 curCents[4];            /* spend in each fill currency, its own units */
    UInt32 ftfMl, ftfDkm;          /* fuel full-to-full totals */
    UInt32 ftfWh, ftfDkmE;         /* electric full-to-full totals */
    UInt32 bestEco, worstEco;      /* L/100 km x 10 over fuel segments, 0 = none */
    UInt16 bestDate, worstDate;
    UInt32 bestEcoE, worstEcoE;    /* kWh/100 km x 10 over charge segments */
    UInt32 lowPrice, highPrice;    /* ten-thousandths per L over fuel fills with a price */
    UInt16 lowDate, highDate;
    Int32  earned[statsMaxId], redeemed[statsMaxId];
    UInt32 saved[statsMaxId];      /* discount cents by the programme redeemed */
    UInt8  pgmUsed[statsMaxId];
    struct { UInt32 firstOdo, lastOdo; UInt8 seen; StatsChain chain[2]; } veh[statsMaxId];
} Stats;

void   StatsBegin(Stats *s) EXPORT_SEC;
void   StatsAdd(Stats *s, const FillRecord *f) EXPORT_SEC;
UInt32 StatsAvgEco(const Stats *s) EXPORT_SEC;          /* L/100 km x 10, 0 when unknown */
UInt32 StatsAvgEcoElectric(const Stats *s) EXPORT_SEC;  /* kWh/100 km x 10 */
UInt32 StatsAvgPrice(const Stats *s) EXPORT_SEC;        /* ten-thousandths per L, home currency */
UInt32 StatsAvgPriceElectric(const Stats *s) EXPORT_SEC;/* ten-thousandths per kWh */
UInt32 StatsCostPer100km(const Stats *s) EXPORT_SEC;    /* home cents, fuel and electricity */
UInt32 StatsAvgFillMl(const Stats *s) EXPORT_SEC;
UInt32 StatsDaysBetweenX10(const Stats *s) EXPORT_SEC;
UInt32 StatsSpendPerMonth(const Stats *s) EXPORT_SEC;   /* home cents over the months covered */

#endif

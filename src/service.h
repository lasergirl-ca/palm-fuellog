/* Service due logic and presets. Plain C, host-tested: the forms feed it
 * a service type, its last visit, the current odometer and the pace. */
#ifndef SERVICE_H
#define SERVICE_H
#include "ptypes.h"
#include "records.h"
#include "sections.h"

#define svcStatusOK         0
#define svcStatusSoon       1
#define svcStatusOverdue    2
#define svcStatusNoInterval 3   /* nothing set yet */
#define svcStatusNoVisit    4   /* interval set, no visit logged */

typedef struct {
    UInt8  status;
    UInt8  hasDist, hasTime;
    Int32  kmLeftX10;    /* when hasDist: negative once passed */
    Int32  daysLeft;     /* when hasTime */
    UInt16 timeDue;      /* when hasTime: the date the months run out */
    UInt16 distDue;      /* when hasDist and the pace is known: projected date, else 0 */
    UInt16 due;          /* the earlier date that exists, else 0 */
} SvcDue;

/* paceX10 is km x 10 per day (0 = unknown). */
void   SvcCompute(const ServiceTypeRecord *t, UInt8 haveLast, UInt16 lastDate, UInt32 lastOdo,
                  UInt32 odoNow, UInt16 today, UInt32 paceX10, SvcDue *d) EXPORT_SEC;
/* km x 10 per day between two readings; 0 when the dates are the same. */
UInt32 SvcPaceX10(UInt32 odoFirst, UInt32 odoLast, UInt16 dateFirst, UInt16 dateLast) EXPORT_SEC;
UInt32 SvcDefaultWarnDist(UInt32 intervalDist) EXPORT_SEC;   /* 10 % */

/* Presets by powertrain: intervals blank, warn 14 days. id is the index. */
UInt8  SvcPresetCount(UInt8 powertrain) EXPORT_SEC;
void   SvcPreset(UInt8 powertrain, UInt8 i, ServiceTypeRecord *t) EXPORT_SEC;

#endif

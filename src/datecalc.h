/* Calendar arithmetic for dates stored as days since 1904-01-01, done here
 * rather than with DateType so the record format never depends on Palm's
 * 7-bit year. Plain C, host-tested. */
#ifndef DATECALC_H
#define DATECALC_H
#include "ptypes.h"
#include "sections.h"

UInt16 DateYmdToDays(UInt16 year, UInt8 month, UInt8 day) EXPORT_SEC;   /* month 1-12 */
void   DateDaysToYmd(UInt16 days, UInt16 *year, UInt8 *month, UInt8 *day) EXPORT_SEC;
const char *DateMonthName(UInt8 month) EXPORT_SEC;                      /* "Jan" */
/* "Oct 19" into buf (at least 8 bytes); returns the length. */
UInt16 DateShortText(char *buf, UInt16 days) EXPORT_SEC;
/* Days in the month, leap years included. */
UInt8  DateDaysInMonth(UInt16 year, UInt8 month) EXPORT_SEC;
/* days plus n months, clamped to the end of the target month. */
UInt16 DateAddMonths(UInt16 days, UInt16 months) EXPORT_SEC;

#endif

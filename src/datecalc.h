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

/* Palm OS clocks stop at 2031, so a Palm may run with its clock set 28 years
 * back (the calendar repeats every 28 years, so weekdays still match). The
 * app stores real dates: when the device year is more than 14 years behind
 * the year this build was made, the clock is taken as shifted and these
 * many days are added to what it says. 0 when the clock looks right. */
#define dateClockShiftYears 28
#define dateClockShiftDays  10227     /* 28 years with seven leap days (good until 2072) */
UInt16 DateClockShift(UInt16 deviceYear, UInt16 buildYear) EXPORT_SEC;
/* A date within three years before or one year after the device's own today
 * was made from the shifted clock (a record saved by an older build, the
 * date picker's Today): it is moved forward by shift. Any other date, and
 * every date when shift is 0, comes back unchanged. */
UInt16 DateUnshift(UInt16 days, UInt16 deviceToday, UInt16 shift) EXPORT_SEC;

#endif

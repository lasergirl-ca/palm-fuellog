#include "datecalc.h"

static const char *const kMonth[12] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
static const UInt8 kDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

static int Leap(UInt16 y) EXPORT_SEC;
static int Leap(UInt16 y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

UInt8 DateDaysInMonth(UInt16 year, UInt8 month)
{
    if (month == 2 && Leap(year)) return 29;
    return (month >= 1 && month <= 12) ? kDays[month - 1] : 30;
}

UInt16 DateYmdToDays(UInt16 year, UInt8 month, UInt8 day)
{
    UInt32 days = 0;
    UInt16 y;
    UInt8 m;
    if (year < 1904) return 0;
    for (y = 1904; y < year; y++) days += Leap(y) ? 366 : 365;
    for (m = 1; m < month && m <= 12; m++) days += DateDaysInMonth(year, m);
    days += (day ? day : 1) - 1;
    return days > 65535UL ? 65535 : (UInt16)days;
}

void DateDaysToYmd(UInt16 days, UInt16 *year, UInt8 *month, UInt8 *day)
{
    UInt32 d = days;
    UInt16 y = 1904;
    UInt8 m = 1;
    for (;;) {
        UInt32 n = Leap(y) ? 366 : 365;
        if (d < n) break;
        d -= n; y++;
    }
    for (;;) {
        UInt8 n = DateDaysInMonth(y, m);
        if (d < n) break;
        d -= n; m++;
    }
    *year = y; *month = m; *day = (UInt8)(d + 1);
}

const char *DateMonthName(UInt8 month) { return (month >= 1 && month <= 12) ? kMonth[month - 1] : "???"; }

UInt16 DateShortText(char *buf, UInt16 days)
{
    UInt16 y; UInt8 m, d, n = 0;
    const char *name;
    DateDaysToYmd(days, &y, &m, &d);
    name = DateMonthName(m);
    while (*name) buf[n++] = *name++;
    buf[n++] = ' ';
    if (d >= 10) buf[n++] = (char)('0' + d / 10);
    buf[n++] = (char)('0' + d % 10);
    buf[n] = 0;
    return n;
}

UInt16 DateAddMonths(UInt16 days, UInt16 months)
{
    UInt16 y; UInt8 m, d, dim;
    UInt32 total;
    DateDaysToYmd(days, &y, &m, &d);
    total = (UInt32)(m - 1) + months;
    y += (UInt16)(total / 12);
    m = (UInt8)(total % 12 + 1);
    dim = DateDaysInMonth(y, m);
    if (d > dim) d = dim;
    return DateYmdToDays(y, m, d);
}

UInt16 DateClockShift(UInt16 deviceYear, UInt16 buildYear)
{
    return (UInt16)(deviceYear + 14 < buildYear ? dateClockShiftDays : 0);
}

UInt16 DateUnshift(UInt16 days, UInt16 deviceToday, UInt16 shift)
{
    UInt16 from = deviceToday > 1098 ? deviceToday - 1098 : 0;   /* three years back */
    if (!shift) return days;
    if (days >= from && days <= deviceToday + 366 && (UInt32)days + shift <= 65535UL) return (UInt16)(days + shift);
    return days;
}

UInt32 DateEpochOffsetDays(UInt16 offsetYears)
{
    return (UInt32)(offsetYears / 4) * 1461UL;
}

/* Stats and Reports as lines of text the window paints: what each line says
 * is worked out here (plain C over store/db.h, host-tested), how it looks is
 * the window's. The same rules and wording as the Palm's Stats and Reports
 * forms (src/stats_form.c, src/reports_form.c). Text is Palm Latin (cp1252). */
#ifndef TEXTVIEW_H
#define TEXTVIEW_H
#include "../../src/ptypes.h"
#include "../../src/prefs.h"

#define tvCells 5
#define tvText 64
#define tvMaxLines 96

enum {
    tvRow,          /* cell 0 on the left, cell 1 on the right */
    tvHeadline,     /* cells 0 and 1 in large type, left and right */
    tvCaption,      /* cells 0 and 1 under a headline */
    tvRule,         /* a line across */
    tvHeading,      /* cell 0 in bold */
    tvTable,        /* cell 0 on the left, cells 1 to 4 right-aligned in the report's columns */
    tvTableBold,
    tvNote,         /* cell 0 across the width */
    tvTriple,       /* cells 0 and 1 from the left and from 36 %, cell 2 on the right */
    tvTripleBold
};

typedef struct {
    UInt8 style;
    char cell[tvCells][tvText];
} TextLine;

/* Stats for p->currentVehicle (255 = all), as the Palm's Stats form shows them. Returns the line count. */
UInt16 StatsViewBuild(const FuelPrefs *p, UInt16 today, char point, char thousands, TextLine *lines, UInt16 max);
/* Home cents spent on service visits of the vehicle (255 = all). */
UInt32 StatsViewServiceCents(UInt8 vehicle);

/* A report's period: monthly (one year by month), yearly (twelve years) or a date range. */
typedef struct {
    UInt8 mode;                 /* rptMonthly, rptYearly, rptRange */
    UInt16 year;                /* monthly: the year; yearly: the first of twelve */
    UInt16 from, to;            /* range: inclusive dates */
} ReportPeriod;

/* The period a mode starts on: this year, the twelve years to this one, or the last 90 days. */
void   ReportViewSetMode(ReportPeriod *rp, UInt8 mode, UInt16 today);
/* Back or forward a year, twelve years, or the range's own length. */
void   ReportViewStep(ReportPeriod *rp, Int16 dir);
/* "2026", "2015-26", or empty for a range (the window shows its dates). 16 bytes. */
void   ReportViewPeriodText(const ReportPeriod *rp, char *buf);
UInt16 ReportViewBuild(const FuelPrefs *p, const ReportPeriod *rp, char point, char thousands, TextLine *lines, UInt16 max);

#endif

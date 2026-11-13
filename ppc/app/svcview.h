/* Service without the window: what is due and when, the Service list and
 * History rows, and the rules of the Palm's Service Type and Log Service
 * forms (src/service_form.c), over the shared due logic in src/service.c.
 * Plain C over store/db.h, host-tested. Text is Palm Latin (cp1252). */
#ifndef SVCVIEW_H
#define SVCVIEW_H
#include "../../src/ptypes.h"
#include "../../src/records.h"
#include "../../src/prefs.h"
#include "../../src/service.h"
#include "textview.h"

/* ---- what is due ---- */

/* km x 10 per day over the last 90 days of fills (at least 3), else over all fills; 0 under a week. */
UInt32 SvcViewPace(UInt8 vehicle, UInt16 today);
/* The latest visit of the vehicle covering service type typeId (255 = any). */
int    SvcViewLastVisit(UInt8 vehicle, UInt8 typeId, UInt16 *date, UInt32 *odo);
UInt16 SvcViewVisitsCovering(UInt8 vehicle, UInt8 typeId);
/* The presets for the vehicle's powertrain, when it has no service types yet. Returns how many. */
UInt8  SvcViewInstallPresets(UInt8 vehicle);
UInt8  SvcViewNextTypeId(UInt8 vehicle);            /* 0 to 30, or 255 when all 31 are taken */
/* The vehicle the Service screens show: the list's, else the first in use. */
UInt8  SvcViewVehicle(const FuelPrefs *p);
/* The most urgent service of the vehicle (255 = every live vehicle), and a line for the fill
 * list such as "Oil and filter due in 640 km" (64 bytes); svcStatusOK with "" when nothing is due. */
UInt8  SvcViewBanner(UInt8 vehicle, UInt16 today, const FuelPrefs *p, char point, char thousands, char *buf);
/* The earliest due date after today among live services with the alarm ticked; 0 = none. */
UInt16 SvcViewNextAlarm(UInt16 today);

/* ---- the Service list and History ---- */

typedef struct {
    char left[40], mid[24], right[24];   /* first line: left, a column right-aligned at 60 %, and the right edge */
    char line2[64], right2[24];          /* second line */
    UInt8 badge;                         /* 0 none, else 1 + svcStatusOK / Soon / Overdue */
    UInt8 bold2;                         /* the second line in bold: a gap past its interval */
    UInt16 dbIndex;                      /* the service type's or visit's database index */
    UInt8 hasVisit;                      /* Service list: the row opens History, else the Service Type */
} SvcRow;

#define svcMaxRows 64

/* The vehicle's live service types, most urgent first; head gets "Odo 81,190 km, pace 45 km/day"
 * and foot "Last visit May 14 at 76,852 km" (64 bytes each). */
UInt8 SvcViewList(UInt8 vehicle, UInt16 today, const FuelPrefs *p, char point, char thousands,
                  SvcRow *rows, UInt8 max, char *head, char *foot);
/* The vehicle's visits, newest first, all of them or those covering the service type at typeIndex
 * (0xFFFF = all), with the gaps between them; head gets the title, the interval, the headlines
 * (longest gap, average gap, spend) and their captions. Returns the row count; *headCount the lines. */
UInt8 SvcViewHistory(UInt8 vehicle, UInt16 typeIndex, UInt16 today, const FuelPrefs *p, char point, char thousands,
                     SvcRow *rows, UInt8 max, TextLine head[4], UInt8 *headCount);
/* A date with its year when that is not this year's: "Oct 23 2027". */
void SvcViewDueDate(char *buf, UInt16 days, UInt16 today);

/* ---- Service Type ---- */

typedef struct {
    char name[svtNameLen], dist[16], months[16], warnDist[16], warnDays[16], detail[svtDetailLen];
    UInt8 alarm;
} SvtForm;

typedef struct {
    ServiceTypeRecord t;
    UInt16 index;               /* database index, or 0xFFFF for a new one */
} SvtEdit;

#define svtEditNew 0xFFFF
#define seSaved      0
#define seNeedName   1
#define seBadNumber  2
#define seSaveFailed 3

/* Returns 0 when the vehicle has all 31 service types already. */
int  SvtEditBegin(SvtEdit *e, UInt16 index, UInt8 vehicle, const FuelPrefs *p, char point, SvtForm *f);
/* The three summary lines under the form ("Due at 84,852 km or Nov 14", ...); 64 bytes each. */
void SvtEditSummary(const SvtEdit *e, UInt16 today, const FuelPrefs *p, char point, char thousands, char lines[3][64]);
const char *SvtEditArchiveLabel(const SvtEdit *e);  /* NULL when new; "Restore", "Archive" (visits cover it) or "Delete" */
Int32 SvtEditSave(SvtEdit *e, const SvtForm *f, const FuelPrefs *p, char point, char thousands);
Int32 SvtEditArchive(SvtEdit *e);                    /* on the type as stored */

/* ---- Log Service and Edit Visit ---- */

#define svlChecks 12

typedef struct {
    UInt16 date;
    char odo[16], other[svlOtherLen], cost[16], shop[svlShopLen], detail[svlDetailLen], notes[fillMaxNotes];
    UInt8 checked[svlChecks], otherOn, currency, payment;
} SvlForm;

typedef struct {
    ServiceVisitRecord v;
    UInt16 index;               /* database index, or svtEditNew for a new visit */
    UInt8 vehicle, count;       /* the vehicle, and how many service types the form offers */
    UInt8 typeId[svlChecks];
    char names[svlChecks][svtNameLen];
    int prefilledOdo;           /* a new visit starts at the last fill's odometer */
} SvlEdit;

#define svSaved      0
#define svBadNumber  1
#define svNothing    2          /* nothing ticked */
#define svAskTrack   3          /* Other names something that is not a service type yet: ask */
#define svSaveFailed 4
#define svOtherAsk   0
#define svOtherTrack 1          /* make it a service type and tick that */
#define svOtherOnce  2          /* keep it as Other on this visit only */

void  SvlEditBegin(SvlEdit *e, UInt16 index, UInt8 vehicle, UInt16 today, const FuelPrefs *p, char point, SvlForm *f);
/* "Next Oil and filter: 84,852 km or Nov 14" for the first ticked service with an interval; 64 bytes. */
void  SvlEditNextLine(const SvlEdit *e, const SvlForm *f, const FuelPrefs *p, char point, char thousands, char *buf);
Int32 SvlEditSave(SvlEdit *e, const SvlForm *f, UInt8 otherChoice, FuelPrefs *p, char point, char thousands);
Int32 SvlEditDelete(SvlEdit *e);

#endif

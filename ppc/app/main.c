/* FuelLog for Pocket PC: the main window. Pages in one window: the fill
 * list (P1) over the six FuelLog databases kept as the Palm's own .pdb
 * files in \My Documents\FuelLog, the New and Edit Fill form (P2), and
 * Preferences, the vehicle list and the vehicle form (P3). Everything they
 * show comes from fillview.c, filledit.c, vehedit.c and the shared core in
 * ../src; this file is only the window. */
#include <windows.h>
#include <commctrl.h>
#include <aygshell.h>
#include <string.h>
#include "fillview.h"
#include "filledit.h"
#include "vehedit.h"
#include "pgmedit.h"
#include "settings.h"
#include "textview.h"
#include "graphview.h"
#include "svcview.h"
#include <notify.h>
#include "../../src/report.h"
#include "../store/db.h"
#include "../../src/datecalc.h"
#include "../../src/units.h"
#include "../../src/currency.h"

#define kVersion "0.6"
#define kClass L"FuelLogPPC"
#define kFormClass L"FuelLogForm"
#define kSheetClass L"FuelLogSheet"
#define kRegKey L"Software\\FuelLog"
#define kDataDir "\\My Documents\\FuelLog"
#define kDataDirW L"\\My Documents\\FuelLog"
#define kPalmBackup L"\\PALM\\Programs\\FuelLog\\Backup"

enum { IDC_VEHICLE = 100, IDC_LIST, IDC_AVG, IDC_MONTH, IDC_EMPTY, IDC_VEHLIST, IDC_VEHARCH, IDC_PGMLIST, IDC_BANNER };
enum { IDM_ABOUT = 200, IDM_PALM, IDM_EXIT, IDM_NEW, IDM_SAVE, IDM_CANCEL, IDM_DELETE, IDM_DONE, IDM_VEHICLES, IDM_PREFS,
       IDM_PROGRAMMES, IDM_OK, IDM_ARCHIVE, IDM_STATS, IDM_REPORTS, IDM_GRAPHS, IDM_SERVICE, IDM_HISTORY,
       IDM_INTERVALS, IDM_LOG, IDM_NEWSVC };
/* The Service Type and Log Service sheets. */
enum { IDY_NAME = 700, IDY_DIST, IDY_MONTHS, IDY_WARNDIST, IDY_WARNDAYS, IDY_ALARM, IDY_DETAIL, IDY_SUM1, IDY_SUM2, IDY_SUM3 };
#define IDS_CHK(i) (742 + (i))
enum { IDS_DATE = 740, IDS_ODO, IDS_OTHERON = 754, IDS_OTHER, IDS_COST, IDS_CUR, IDS_PAY, IDS_SHOP, IDS_DETAIL, IDS_NOTE, IDS_NEXT };
/* The Reports and Graphs pages' pickers. */
enum { IDR_MODE = 600, IDR_PREV, IDR_LABEL, IDR_NEXT, IDR_FROM, IDR_TO, IDG_KIND, IDG_UNIT, IDG_UNITTEXT };
/* The form's controls, in the order they are laid out. */
enum { IDF_LVEH = 300, IDF_VEH, IDF_DATE, IDF_TIME, IDF_LODO, IDF_ODO, IDF_ODOHINT, IDF_LTRIP, IDF_TRIP, IDF_TRIPHINT,
       IDF_LVOL, IDF_VOL, IDF_UNIT, IDF_LPRICE, IDF_PRICE, IDF_CALC, IDF_LTOTAL, IDF_TOTAL, IDF_CUR,
       IDF_LRATE, IDF_RATE, IDF_RATEHINT, IDF_LGRADE, IDF_GRADE, IDF_FULL, IDF_MISSED,
       IDF_LPAY, IDF_CASH, IDF_CARD, IDF_LPTS, IDF_PTS, IDF_LNOTE, IDF_NOTE };
/* The rows of the two sheets (label ids are these plus 1000). */
enum { IDP_DIST = 400, IDP_VOL, IDP_ECO, IDP_PRICE, IDP_FORMAT, IDP_HOME, IDP_NEW };
enum { IDV_NAME = 420, IDV_POWER, IDV_UNIT, IDV_START, IDV_TANK, IDV_HOME, IDV_GRADE, IDV_CUR, IDV_TRIP, IDV_ARCH, IDV_STATUS };
enum { IDE_NAME = 440, IDE_CUR, IDE_MONEY, IDE_EARN, IDE_BASIS, IDE_PREM, IDE_PAY, IDE_AUTO, IDE_REDEEM, IDE_BLOCK,
       IDE_VALUE, IDE_VOLUNIT, IDE_MAXVOL, IDE_CAP, IDE_EXPIRY, IDE_MONTHS, IDE_OPEN, IDE_ALIAS, IDE_STATUS };
/* The Points page: four ids per row (programme, earned, redeemed, hint), then Add and the total. */
#define IDT_ROW(i) (500 + 4 * (i))
enum { IDT_ADD = 520, IDT_TOTAL };
enum { pageList, pageForm, pagePrefs, pageVehicles, pageVehicle, pagePoints, pageProgrammes, pageProgramme,
       pageStats, pageReports, pageGraphs, pageService, pageHistory, pageSvt, pageVisit };
#define kRowsClass L"FuelLogRows"
#define kTextClass L"FuelLogText"
#define kGraphClass L"FuelLogGraph"
#define WM_SHOWPAGE (WM_APP + 1)     /* posted, so a menu bar is never replaced inside its own command */

#define kMaxRows 4096
#define kRow 22                      /* the form's row pitch at 96 dpi */

static HINSTANCE gInst;
static HWND gWin, gMenuBar, gVehicle, gList, gAvg, gMonth, gEmpty, gForm, gVehList, gVehArch, gPgmList;
/* Stats, Reports and Graphs. */
static HWND gTextPanel, gGraph, gRptMode, gRptPrev, gRptLabel, gRptNext, gRptFrom, gRptTo, gGrKind, gGrUnit, gGrUnitText;
static HFONT gFontBold, gFontLarge;
static TextLine gLines[tvMaxLines];
static UInt16 gLineCount;
static int gTextScroll, gTextHeight;
static ReportPeriod gRptPeriod;
static GraphData gGraphData;
static UInt8 gGraphKind;
static Int16 gGraphSel = -1;
/* Service. */
static HWND gRowsPanel, gBanner;
static TextLine gRowsHead[6];
static UInt8 gRowsHeadCount, gSvcRowCount, gSvcVehicle, gBannerShown;
static SvcRow gSvcRows[svcMaxRows];
static char gRowsFoot[64];
static int gRowsScroll, gRowsHeight;
static UInt16 gHistType = 0xFFFF;
static SvtEdit gSvtEdit;
static SvlEdit gSvlEdit;
static int gSvtReturn = pageService, gVisitReturn = pageService;
static SHACTIVATEINFO gSai;
static int gDpi = 96, gPage = pageList, gDataOk;
static FuelPrefs gPrefs;
static char gPoint = '.', gThousands = ',';
static UInt16 gRows[kMaxRows], gRowCount;
static UInt8 gVehIds[vehMaxVehicles + 1];
static int gCachedRow = -1;
static WCHAR gCached[fillViewCols][fillViewText];
/* The form. */
static FillEdit gEdit;
static FillEditText gText;
static int gFormBusy, gFormScroll;
static UInt8 gFormVehIds[vehMaxVehicles + 1], gGradeMap[7], gGradeCount;
/* Vehicles. */
static VehEdit gVehEdit;
static UInt16 gVehIdx[vehMaxVehicles], gVehCount;
static int gShowArchived;
/* Prefs as last written to the registry, so they are written only when they change. */
static UInt8 gStoredPrefs[settingsSize];

/* Programmes and the Points page. */
static PgmEdit gPgmEdit;
static UInt16 gPgmIdx[pgmMaxProgrammes], gPgmCount;
static FillEdit gEditSaved;          /* the fill as it was when the Points page opened: Cancel puts it back */
static UInt8 gPtsIds[pgmMaxProgrammes], gPtsChoices;
static char gPtsNames[pgmMaxProgrammes][pgmNameLen];
static int gFormBack;                /* the form page comes back from Points: focus the Points button */

/* A sheet is a simple scrolling form, a label and a control per row: Preferences,
 * a vehicle, a programme and the Points page. */
enum { rowEdit, rowCombo, rowCheck, rowText, rowDate };
typedef struct {
    int id, kind;
    const WCHAR *label;
    const WCHAR *choices;       /* a combo's rows, each ended by a NUL; NULL when filled at run time */
    int limit;                  /* an edit's longest text */
} SheetRow;
#define kSheetRows 24
typedef struct {
    HWND panel;
    const SheetRow *rows;
    int count, labelW, scroll, busy, quiet;
    UInt8 hidden[kSheetRows];
    void (*changed)(int id);    /* a combo's choice changed */
    void (*edited)(int id);     /* an edit's text changed by the user */
} Sheet;
static Sheet gPrefsSheet, gVehSheet, gPgmSheet, gPtsSheet, gSvtSheet, gSvlSheet;
static void PtsOpen(void);
static void ScheduleAlarm(void);
static void Layout(void);
static void ServiceOpen(void);

/* Layout numbers are for a 96 dpi screen (QVGA); a VGA device reports 192. Windows CE has no MulDiv. */
static int S(int x) { return (x * gDpi + 48) / 96; }

/* Record text is Palm Latin, which is Windows-1252. */
static void ToWide(const char *s, WCHAR *w, int cap)
{
    if (MultiByteToWideChar(1252, 0, s, -1, w, cap) == 0 && MultiByteToWideChar(CP_ACP, 0, s, -1, w, cap) == 0) w[0] = 0;
}

static void FromWide(const WCHAR *w, char *s, int cap)
{
    if (WideCharToMultiByte(1252, 0, w, -1, s, cap, NULL, NULL) == 0 && WideCharToMultiByte(CP_ACP, 0, w, -1, s, cap, NULL, NULL) == 0) s[0] = 0;
    s[cap - 1] = 0;
}

static UInt16 Today(void)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return DateYmdToDays((UInt16)t.wYear, (UInt8)t.wMonth, (UInt8)t.wDay);
}

static UInt16 NowMinutes(void)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return (UInt16)(t.wHour * 60 + t.wMinute);
}

/* Seconds since 1904 by the device's local clock, as the Palm counts them. */
static UInt32 Now(void)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return (UInt32)DateYmdToDays((UInt16)t.wYear, (UInt8)t.wMonth, (UInt8)t.wDay) * 86400UL
        + (UInt32)t.wHour * 3600UL + (UInt32)t.wMinute * 60UL + t.wSecond;
}

static void ReadFormats(void)
{
    WCHAR b[8];
    if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, b, 8) > 0 && b[0] < 128) gPoint = (char)b[0];
    if (GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, b, 8) > 0 && b[0] < 128) gThousands = (char)b[0];
    if (gThousands == gPoint) gThousands = 0;
}

static Int32 OpenData(void)
{
    DbClock clock;
    Int32 e;
    CreateDirectoryW(L"\\My Documents", NULL);
    CreateDirectoryW(kDataDirW, NULL);
    clock.now = Now;
    clock.clockShift = 0;          /* a Windows Mobile clock runs past 2031: no shift */
    e = DbOpenAll(kDataDir, &clock);
    gDataOk = e == dbOk;
    if (e != dbOk) return e;
    DbEnsureUids();
    PgmInstallPresets(gPrefs.homeCurrency);     /* a new log starts with the Palm's six programmes, as on the Palm */
    if (DbDirty()) DbSave();
    return dbOk;
}

/* Write what changed; say so when the storage refuses. */
static void SaveData(void)
{
    if (DbDirty() && DbSave() != dbOk)
        MessageBoxW(gWin, L"The change is in the list but could not be written to \\My Documents\\FuelLog. Free some storage; FuelLog tries again when it closes.",
            L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
}

/* Prefs live in the registry: the Palm keeps its own with the system, not in the databases. */
static void LoadPrefs(void)
{
    HKEY k;
    UInt8 blob[256];
    DWORD type = 0, len = sizeof blob;
    PrefsSetDefaults(&gPrefs);
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegKey, 0, 0, &k) == ERROR_SUCCESS) {
        if (RegQueryValueExW(k, L"Prefs", NULL, &type, blob, &len) == ERROR_SUCCESS && type == REG_BINARY)
            SettingsUnpack(blob, len, &gPrefs);
        RegCloseKey(k);
    }
    SettingsPack(&gPrefs, gStoredPrefs);
}

static void StorePrefs(void)
{
    HKEY k;
    DWORD disp;
    UInt8 blob[settingsSize];
    SettingsPack(&gPrefs, blob);
    if (memcmp(blob, gStoredPrefs, settingsSize) == 0) return;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, NULL, 0, 0, NULL, &k, &disp) != ERROR_SUCCESS) return;
    if (RegSetValueExW(k, L"Prefs", 0, REG_BINARY, blob, settingsSize) == ERROR_SUCCESS) memcpy(gStoredPrefs, blob, settingsSize);
    RegCloseKey(k);
}

/* ---- the list ---- */

static void SetColumn(int i, const char *title)
{
    LVCOLUMNW c;
    WCHAR w[16];
    ToWide(title, w, 16);
    memset(&c, 0, sizeof c);
    c.mask = LVCF_TEXT;
    c.pszText = w;
    SendMessageW(gList, LVM_SETCOLUMNW, (WPARAM)i, (LPARAM)&c);
}

static void AddColumn(HWND list, int i, int width, int right, WCHAR *title)
{
    LVCOLUMNW c;
    memset(&c, 0, sizeof c);
    c.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    c.fmt = right ? LVCFMT_RIGHT : LVCFMT_LEFT;
    c.cx = S(width);
    c.pszText = title;
    c.iSubItem = i;
    SendMessageW(list, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
}

static void Refresh(void)
{
    char avg[40], month[40];
    WCHAR w[48];
    int electric = FillViewElectric(&gPrefs);
    gRowCount = FillViewCollect(gPrefs.currentVehicle, gRows, kMaxRows);
    gCachedRow = -1;
    SetColumn(0, "Date");
    SetColumn(1, "Odo");
    SetColumn(2, electric ? "kWh" : UnitVolLabel(gPrefs.volumeUnit));
    SetColumn(3, "Total");
    SetColumn(4, FillViewEcoHeader(electric, &gPrefs));
    SendMessageW(gList, LVM_SETITEMCOUNT, (WPARAM)gRowCount, 0);
    FillViewFooter(&gPrefs, Today(), gPoint, gThousands, avg, month);
    ToWide(avg, w, 48); SetWindowTextW(gAvg, w);
    ToWide(month, w, 48); SetWindowTextW(gMonth, w);
    ShowWindow(gList, gRowCount ? SW_SHOW : SW_HIDE);
    ShowWindow(gEmpty, gRowCount ? SW_HIDE : SW_SHOW);
    InvalidateRect(gList, NULL, TRUE);
    /* The most urgent service for the vehicle shown; Soon or Overdue earns a banner line, as on the Palm. */
    {
        char b[64];
        UInt8 st = gDataOk ? SvcViewBanner(gPrefs.currentVehicle, Today(), &gPrefs, gPoint, gThousands, b) : svcStatusOK;
        UInt8 shown = (UInt8)((st == svcStatusSoon || st == svcStatusOverdue) && b[0]);
        if (shown) { ToWide(b, w, 48); SetWindowTextW(gBanner, w); }
        if (shown != gBannerShown) {
            gBannerShown = shown;
            Layout();
            if (gPage == pageList) PostMessageW(gWin, WM_SHOWPAGE, pageList, 0);   /* the menu's "Service!" follows */
        }
        ShowWindow(gBanner, shown && gPage == pageList ? SW_SHOW : SW_HIDE);
    }
}

static void BuildVehicles(void)
{
    VehicleRecord v;
    WCHAR w[vehNameLen + 2];
    UInt16 i, n = DbCount(dbVehicles);
    int count = 0, sel = 0;
    SendMessageW(gVehicle, CB_RESETCONTENT, 0, 0);
    SendMessageW(gVehicle, CB_ADDSTRING, 0, (LPARAM)L"All vehicles");
    gVehIds[count++] = 255;
    for (i = 0; i < n && count <= vehMaxVehicles; i++) {
        if (DbGetVehicle(i, &v) != dbOk || v.archived) continue;
        ToWide(v.name, w, vehNameLen + 2);
        SendMessageW(gVehicle, CB_ADDSTRING, 0, (LPARAM)w);
        if (v.id == gPrefs.currentVehicle) sel = count;
        gVehIds[count++] = v.id;
    }
    if (sel == 0) gPrefs.currentVehicle = 255;
    SendMessageW(gVehicle, CB_SETCURSEL, (WPARAM)sel, 0);
}

static void Layout(void)
{
    RECT r;
    int w, h, top = S(26), foot = S(20);
    GetClientRect(gWin, &r);
    w = r.right; h = r.bottom;
    MoveWindow(gVehicle, S(2), S(2), w - S(4), S(140), TRUE);      /* the height is the dropped list's */
    MoveWindow(gBanner, S(2), top, w - S(4), S(20), TRUE);
    MoveWindow(gRowsPanel, 0, gPage == pageHistory ? 0 : top, w, gPage == pageHistory ? h : h - top, TRUE);
    MoveWindow(gSvtSheet.panel, 0, 0, w, h, TRUE);
    MoveWindow(gSvlSheet.panel, 0, 0, w, h, TRUE);
    {
        int listTop = top + (gBannerShown ? S(22) : 0);     /* the service banner sits over the list */
        MoveWindow(gList, 0, listTop, w, h - listTop - foot, TRUE);
        MoveWindow(gEmpty, S(8), listTop + S(8), w - S(16), h - listTop - foot - S(16), TRUE);
    }
    MoveWindow(gAvg, S(4), h - foot + S(2), w / 2 - S(4), foot - S(2), TRUE);
    MoveWindow(gMonth, w / 2, h - foot + S(2), w / 2 - S(4), foot - S(2), TRUE);
    MoveWindow(gForm, 0, 0, w, h, TRUE);
    MoveWindow(gPrefsSheet.panel, 0, 0, w, h, TRUE);
    MoveWindow(gVehSheet.panel, 0, 0, w, h, TRUE);
    MoveWindow(gVehList, 0, 0, w, h - S(24), TRUE);
    MoveWindow(gVehArch, S(4), h - S(22), w - S(8), S(20), TRUE);
    MoveWindow(gPgmSheet.panel, 0, 0, w, h, TRUE);
    MoveWindow(gPtsSheet.panel, 0, 0, w, h, TRUE);
    MoveWindow(gPgmList, 0, 0, w, h, TRUE);
    /* Stats under the vehicle picker; Reports and Graphs have a second row of pickers. */
    {
        int row2 = S(26), top2 = S(50);
        MoveWindow(gTextPanel, 0, gPage == pageStats ? top : top2, w, h - (gPage == pageStats ? top : top2), TRUE);
        MoveWindow(gGraph, 0, top2, w, h - top2, TRUE);
        MoveWindow(gRptMode, S(2), row2, S(74), S(100), TRUE);
        MoveWindow(gRptPrev, S(80), row2, S(24), S(20), TRUE);
        MoveWindow(gRptLabel, S(106), row2 + S(3), w - S(136), S(16), TRUE);
        MoveWindow(gRptNext, w - S(28), row2, S(24), S(20), TRUE);
        MoveWindow(gRptFrom, S(80), row2, (w - S(84)) / 2 - S(1), S(20), TRUE);
        MoveWindow(gRptTo, S(80) + (w - S(84)) / 2 + S(1), row2, (w - S(84)) / 2 - S(1), S(20), TRUE);
        MoveWindow(gGrKind, S(2), row2, S(118), S(100), TRUE);
        MoveWindow(gGrUnit, S(124), row2, w - S(128), S(100), TRUE);
        MoveWindow(gGrUnitText, S(124), row2 + S(3), w - S(128), S(16), TRUE);
    }
}

/* ---- the form: New Fill and Edit Fill ---- */

static HWND F(int id) { return GetDlgItem(gForm, id); }

/* Set a control's text only when it differs, so nothing flickers and a caret stays put. */
static void SetText(int id, const char *s)
{
    WCHAR w[fillMaxNotes], old[fillMaxNotes];
    ToWide(s, w, fillMaxNotes);
    old[0] = 0;
    GetWindowTextW(F(id), old, fillMaxNotes);
    if (wcscmp(w, old) != 0) SetWindowTextW(F(id), w);
}

static void GetText(int id, char *s, int cap)
{
    WCHAR w[fillMaxNotes];
    w[0] = 0;
    GetWindowTextW(F(id), w, fillMaxNotes);
    FromWide(w, s, cap);
}

static void PullTexts(void)
{
    GetText(IDF_ODO, gText.odo, feText);
    GetText(IDF_TRIP, gText.trip, feText);
    GetText(IDF_VOL, gText.volume, feText);
    GetText(IDF_PRICE, gText.price, feText);
    GetText(IDF_TOTAL, gText.total, feText);
    GetText(IDF_RATE, gText.rate, feText);
}

/* The fields from gText; EN_CHANGE from our own writes is ignored. */
static void PushTexts(void)
{
    gFormBusy++;
    SetText(IDF_ODO, gText.odo);
    SetText(IDF_TRIP, gText.trip);
    SetText(IDF_VOL, gText.volume);
    SetText(IDF_PRICE, gText.price);
    SetText(IDF_TOTAL, gText.total);
    SetText(IDF_RATE, gText.rate);
    gFormBusy--;
}

static void Place(int id, int x, int y, int w, int h)
{
    MoveWindow(F(id), S(x), S(y) - gFormScroll, S(w), S(h), TRUE);
}

/* Label column to x 74, fields from 76 to 138, the unit or hint from 142. The
 * panel scrolls when the rows do not fit, as with the keyboard up. */
static void FormLayout(void)
{
    static int busy;
    RECT r;
    SCROLLINFO si;
    int W, y, total, foreign;
    if (!gForm || !gEdit.prefs) return;
    if (busy) { busy = 2; return; }          /* the scroll bar came or went while we were placing */
    do {
        busy = 1;
        GetClientRect(gForm, &r);
        W = (r.right * 96) / gDpi;
        foreign = FillEditForeign(&gEdit);
        total = S(3 + (foreign ? 13 : 12) * kRow + 2);
        if (gFormScroll > total - r.bottom) gFormScroll = total - r.bottom;
        if (gFormScroll < 0) gFormScroll = 0;
        y = 3;
        Place(IDF_LVEH, 2, y + 3, 72, 16); Place(IDF_VEH, 76, y, W - 80, 120); y += kRow;
        Place(IDF_DATE, 2, y, 104, 20); Place(IDF_TIME, 110, y, W - 114, 20); y += kRow;
        Place(IDF_LODO, 2, y + 3, 72, 16); Place(IDF_ODO, 76, y, 62, 20); Place(IDF_ODOHINT, 142, y + 3, W - 144, 16); y += kRow;
        Place(IDF_LTRIP, 2, y + 3, 72, 16); Place(IDF_TRIP, 76, y, 62, 20); Place(IDF_TRIPHINT, 142, y + 3, W - 144, 16); y += kRow;
        Place(IDF_LVOL, 2, y + 3, 72, 16); Place(IDF_VOL, 76, y, 62, 20); Place(IDF_UNIT, 142, y, W - 146, 80); y += kRow;
        Place(IDF_LPRICE, 2, y + 3, 72, 16); Place(IDF_PRICE, 76, y, 62, 20); Place(IDF_CALC, 142, y + 3, W - 144, 16); y += kRow;
        Place(IDF_LTOTAL, 2, y + 3, 72, 16); Place(IDF_TOTAL, 76, y, 62, 20); Place(IDF_CUR, 142, y, 64, 120); y += kRow;
        ShowWindow(F(IDF_LRATE), foreign ? SW_SHOW : SW_HIDE);
        ShowWindow(F(IDF_RATE), foreign ? SW_SHOW : SW_HIDE);
        ShowWindow(F(IDF_RATEHINT), foreign ? SW_SHOW : SW_HIDE);
        if (foreign) { Place(IDF_LRATE, 2, y + 3, 72, 16); Place(IDF_RATE, 76, y, 62, 20); Place(IDF_RATEHINT, 142, y + 3, W - 144, 16); y += kRow; }
        Place(IDF_LGRADE, 2, y + 3, 72, 16); Place(IDF_GRADE, 76, y, W - 80, 120); y += kRow;
        Place(IDF_FULL, 4, y + 1, 108, 18); Place(IDF_MISSED, 114, y + 1, W - 116, 18); y += kRow;
        Place(IDF_LPAY, 2, y + 3, 72, 16); Place(IDF_CASH, 76, y + 1, 64, 18); Place(IDF_CARD, 142, y + 1, 64, 18); y += kRow;
        Place(IDF_LPTS, 2, y + 3, 72, 16); Place(IDF_PTS, 76, y, W - 80, 20); y += kRow;
        Place(IDF_LNOTE, 2, y + 3, 72, 16); Place(IDF_NOTE, 76, y, W - 80, 20);
        memset(&si, 0, sizeof si);
        si.cbSize = sizeof si;
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMax = total - 1;
        si.nPage = (UINT)r.bottom;
        si.nPos = gFormScroll;
        SetScrollInfo(gForm, SB_VERT, &si, TRUE);
    } while (busy == 2);
    busy = 0;
}

/* Scroll so the control with the focus is not under the keyboard or off the top. */
static void FormReveal(HWND c)
{
    RECT r, client;
    POINT a, b;
    if (!c || !IsChild(gForm, c)) return;
    GetWindowRect(c, &r);
    a.x = r.left; a.y = r.top; b.x = r.right; b.y = r.bottom;
    ScreenToClient(gForm, &a); ScreenToClient(gForm, &b);
    GetClientRect(gForm, &client);
    if (b.y - a.y > S(24)) b.y = a.y + S(20);           /* a combo box reports its dropped height on some devices */
    if (b.y > client.bottom) gFormScroll += b.y - client.bottom + S(2);
    else if (a.y < 0) gFormScroll += a.y - S(2);
    else return;
    FormLayout();
}

/* Everything around the number fields: labels, the grade list, hints, points. */
static void FormLabels(const char *calcHint)
{
    char b[64];
    WCHAR w[32];
    UInt8 i;
    int sel = 0;
    SetText(IDF_LVOL, FillEditVolumeLabel(&gEdit));
    FillEditPriceLabel(&gEdit, b); SetText(IDF_LPRICE, b);
    SetText(IDF_LGRADE, FillEditGradeLabel(&gEdit));
    SetText(IDF_FULL, FillEditFullLabel(&gEdit));
    SetText(IDF_CASH, FillEditCashLabel(&gEdit));
    ShowWindow(F(IDF_UNIT), gEdit.charge ? SW_HIDE : SW_SHOW);
    SendMessageW(F(IDF_UNIT), CB_SETCURSEL, (WPARAM)gEdit.entryVol, 0);
    SendMessageW(F(IDF_CUR), CB_SETCURSEL, (WPARAM)gEdit.fill.currency, 0);
    SendMessageW(F(IDF_GRADE), CB_RESETCONTENT, 0, 0);
    gGradeCount = FillEditGrades(&gEdit, gGradeMap);
    for (i = 0; i < gGradeCount; i++) {
        ToWide(FillEditGradeName(gGradeMap[i]), w, 32);
        SendMessageW(F(IDF_GRADE), CB_ADDSTRING, 0, (LPARAM)w);
        if (gGradeMap[i] == gEdit.fill.grade) sel = i;
    }
    SendMessageW(F(IDF_GRADE), CB_SETCURSEL, (WPARAM)sel, 0);
    SendMessageW(F(IDF_CASH), BM_SETCHECK, gEdit.fill.payment == fillPaymentCard ? 0 : 1, 0);
    SendMessageW(F(IDF_CARD), BM_SETCHECK, gEdit.fill.payment == fillPaymentCard ? 1 : 0, 0);
    FillEditOdoHint(&gEdit, b); SetText(IDF_ODOHINT, b);
    FillEditTripHint(&gEdit, &gText, b); SetText(IDF_TRIPHINT, b);
    FillEditRateLabel(&gEdit, b); SetText(IDF_RATEHINT, b);
    FillEditPointsText(&gEdit, b); SetText(IDF_PTS, b);
    if (calcHint) SetText(IDF_CALC, calcHint);
    ToWide(FillEditTitle(&gEdit), w, 32);
    SetWindowTextW(gWin, w);
    FormLayout();                           /* the rate row comes and goes with the currency */
}

/* After a choice (vehicle, unit, currency, grade, payment): work the linked field out again and show it all. */
static void FormChanged(void)
{
    char hint[32];
    FillEditRecompute(&gEdit, &gText, hint);
    PushTexts();
    FormLabels(hint);
}

/* The controls from the fill just begun. */
static void FormFill(void)
{
    VehicleRecord v;
    SYSTEMTIME st;
    WCHAR w[fillMaxNotes];
    UInt16 i, n = DbCount(dbVehicles), year;
    UInt8 month, day;
    int count = 0, sel = 0;

    gFormBusy++;
    SendMessageW(F(IDF_VEH), CB_RESETCONTENT, 0, 0);
    for (i = 0; i < n && count < vehMaxVehicles; i++) {
        if (DbGetVehicle(i, &v) != dbOk || (v.archived && v.id != gEdit.fill.vehicle)) continue;
        ToWide(v.name, w, vehNameLen + 2);
        SendMessageW(F(IDF_VEH), CB_ADDSTRING, 0, (LPARAM)w);
        if (v.id == gEdit.fill.vehicle) sel = count;
        gFormVehIds[count++] = v.id;
    }
    SendMessageW(F(IDF_VEH), CB_SETCURSEL, (WPARAM)sel, 0);

    DateDaysToYmd(gEdit.fill.date, &year, &month, &day);
    memset(&st, 0, sizeof st);
    st.wYear = year; st.wMonth = month; st.wDay = day;
    SendMessageW(F(IDF_DATE), DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    if (gEdit.fill.time == fillTimeUnknown) {
        SendMessageW(F(IDF_TIME), DTM_SETSYSTEMTIME, GDT_NONE, 0);
    } else {
        st.wHour = (WORD)(gEdit.fill.time / 60); st.wMinute = (WORD)(gEdit.fill.time % 60);
        SendMessageW(F(IDF_TIME), DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    }
    SendMessageW(F(IDF_FULL), BM_SETCHECK, (gEdit.fill.flags & fillFlagFullTank) ? 1 : 0, 0);
    SendMessageW(F(IDF_MISSED), BM_SETCHECK, (gEdit.fill.flags & fillFlagMissedPrev) ? 1 : 0, 0);
    ToWide(gEdit.fill.notes, w, fillMaxNotes);
    SetWindowTextW(F(IDF_NOTE), w);
    gFormBusy--;
    PushTexts();
    gFormScroll = 0;
    FormLabels("");
}

static void FormOpen(UInt16 index)
{
    if (gPage == pageForm) return;
    if (!gDataOk) {
        MessageBoxW(gWin, L"The log in \\My Documents\\FuelLog could not be read, so nothing can be added to it.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (index == fillEditNew && FillEditEnsureVehicle()) SaveData();     /* an empty log gets "My car" */
    FillEditBegin(&gEdit, index, &gPrefs, Today(), NowMinutes(), gPoint, gThousands, &gText);
    FormFill();
    PostMessageW(gWin, WM_SHOWPAGE, pageForm, 0);
}

static void FormSave(void)
{
    FillEditInput in;
    SYSTEMTIME st;
    char note[fillMaxNotes], ask[3][24];
    WCHAR msg[400];
    UInt16 at = 0;
    Int32 r;
    char before[64], after[64];
    UInt8 wasDue, nowDue;

    PullTexts();
    wasDue = SvcViewBanner(gEdit.fill.vehicle, Today(), &gPrefs, gPoint, gThousands, before);
    memset(&in, 0, sizeof in);
    in.date = gEdit.fill.date;
    if (SendMessageW(F(IDF_DATE), DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID)
        in.date = DateYmdToDays((UInt16)st.wYear, (UInt8)st.wMonth, (UInt8)st.wDay);
    in.time = fillTimeUnknown;
    if (SendMessageW(F(IDF_TIME), DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID)
        in.time = (UInt16)(st.wHour * 60 + st.wMinute);
    in.full = (UInt8)(SendMessageW(F(IDF_FULL), BM_GETCHECK, 0, 0) == 1);
    in.missed = (UInt8)(SendMessageW(F(IDF_MISSED), BM_GETCHECK, 0, 0) == 1);
    GetText(IDF_NOTE, note, fillMaxNotes);
    in.note = note;
    in.odoChoice = feOdoAsk;
    for (;;) {
        int hit;
        r = FillEditSave(&gEdit, &gText, &in, ask, &at);
        if (r != feOdoMismatch) break;
        wsprintfW(msg, L"The trip meter reads %S, but the odometer moved %S since the last fill.\r\n\r\n"
            L"Yes: use %S instead.\r\nNo: save it as typed.\r\nCancel: go back and edit.", ask[0], ask[1], ask[2]);
        hit = MessageBoxW(gWin, msg, L"Check the odometer", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (hit == IDYES) in.odoChoice = feOdoUseTrip;
        else if (hit == IDNO) in.odoChoice = feOdoKeep;
        else return;
    }
    if (r == feBadNumber) {
        MessageBoxW(gWin, L"A number could not be read, or the odometer is missing. Fix the field with the stray character and choose Save again.",
            L"Check the numbers", MB_OK | MB_ICONEXCLAMATION);
    } else if (r == feNeedVolume) {
        MessageBoxW(gWin, L"Enter the volume, or the total and the price, so FuelLog has something to work out.", L"Nothing to log", MB_OK | MB_ICONEXCLAMATION);
    } else if (r != feSaved) {
        MessageBoxW(gWin, L"The fill was not stored. Free some memory and try again.", L"Could not save", MB_OK | MB_ICONEXCLAMATION);
    } else {
        SaveData();
        StorePrefs();                       /* the last payment, currency and rate */
        /* One alert when this fill tips a service into Soon or Overdue. */
        nowDue = SvcViewBanner(gEdit.fill.vehicle, Today(), &gPrefs, gPoint, gThousands, after);
        if ((nowDue == svcStatusSoon || nowDue == svcStatusOverdue) && nowDue != wasDue && after[0]) {
            WCHAR w[64];
            ToWide(after, w, 64);
            wsprintfW(msg, L"%s. Choose Menu, Service on the fill list for details.", w);
            MessageBoxW(gWin, msg, L"Service coming up", MB_OK | MB_ICONINFORMATION);
        }
        ScheduleAlarm();
        PostMessageW(gWin, WM_SHOWPAGE, pageList, 0);
    }
}

static void FormDelete(void)
{
    if (gEdit.index == fillEditNew) return;
    if (MessageBoxW(gWin, L"Delete this fill?\r\n\r\nBalances and economy will be recalculated without it.", L"FuelLog",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    FillEditDelete(&gEdit);
    SaveData();
    ScheduleAlarm();
    PostMessageW(gWin, WM_SHOWPAGE, pageList, 0);
}

static HWND Ctl(const WCHAR *cls, const WCHAR *text, DWORD style, int id)
{
    return CreateWindowW(cls, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, gForm, (HMENU)id, gInst, NULL);
}

static void FormCreate(HWND h)
{
    WCHAR b[4], w[16];
    SYSTEMTIME range[2];
    UInt8 i;
    int h24;
    gForm = h;
    Ctl(L"STATIC", L"Vehicle:", SS_LEFT, IDF_LVEH);
    Ctl(L"COMBOBOX", L"", WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, IDF_VEH);
    Ctl(DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_SHORTDATEFORMAT, IDF_DATE);
    Ctl(DATETIMEPICK_CLASSW, L"", WS_TABSTOP | DTS_TIMEFORMAT | DTS_UPDOWN | DTS_SHOWNONE, IDF_TIME);
    Ctl(L"STATIC", L"Odometer:", SS_LEFT, IDF_LODO);
    Ctl(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDF_ODO);
    Ctl(L"STATIC", L"", SS_LEFT, IDF_ODOHINT);
    Ctl(L"STATIC", L"Trip:", SS_LEFT, IDF_LTRIP);
    Ctl(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDF_TRIP);
    Ctl(L"STATIC", L"", SS_LEFT, IDF_TRIPHINT);
    Ctl(L"STATIC", L"Volume:", SS_LEFT, IDF_LVOL);
    Ctl(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDF_VOL);
    Ctl(L"COMBOBOX", L"", WS_TABSTOP | CBS_DROPDOWNLIST, IDF_UNIT);
    Ctl(L"STATIC", L"Price:", SS_LEFT, IDF_LPRICE);
    Ctl(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDF_PRICE);
    Ctl(L"STATIC", L"", SS_LEFT, IDF_CALC);
    Ctl(L"STATIC", L"Total:", SS_LEFT, IDF_LTOTAL);
    Ctl(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDF_TOTAL);
    Ctl(L"COMBOBOX", L"", WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, IDF_CUR);
    Ctl(L"STATIC", L"Rate:", SS_LEFT, IDF_LRATE);
    Ctl(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDF_RATE);
    Ctl(L"STATIC", L"", SS_LEFT, IDF_RATEHINT);
    Ctl(L"STATIC", L"Grade:", SS_LEFT, IDF_LGRADE);
    Ctl(L"COMBOBOX", L"", WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, IDF_GRADE);
    Ctl(L"BUTTON", L"Full tank", WS_TABSTOP | BS_AUTOCHECKBOX, IDF_FULL);
    Ctl(L"BUTTON", L"Skipped last fill", WS_TABSTOP | BS_AUTOCHECKBOX, IDF_MISSED);
    Ctl(L"STATIC", L"Payment:", SS_LEFT, IDF_LPAY);
    Ctl(L"BUTTON", L"Cash", WS_TABSTOP | WS_GROUP | BS_AUTORADIOBUTTON, IDF_CASH);
    Ctl(L"BUTTON", L"Card", BS_AUTORADIOBUTTON, IDF_CARD);
    Ctl(L"STATIC", L"Points:", SS_LEFT, IDF_LPTS);
    Ctl(L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, IDF_PTS);    /* opens the Points page */
    Ctl(L"STATIC", L"Note:", SS_LEFT, IDF_LNOTE);
    Ctl(L"EDIT", L"", WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, IDF_NOTE);

    SendMessageW(F(IDF_ODO), EM_LIMITTEXT, feText - 1, 0);
    SendMessageW(F(IDF_TRIP), EM_LIMITTEXT, feText - 1, 0);
    SendMessageW(F(IDF_VOL), EM_LIMITTEXT, feText - 1, 0);
    SendMessageW(F(IDF_PRICE), EM_LIMITTEXT, feText - 1, 0);
    SendMessageW(F(IDF_TOTAL), EM_LIMITTEXT, feText - 1, 0);
    SendMessageW(F(IDF_RATE), EM_LIMITTEXT, feText - 1, 0);
    SendMessageW(F(IDF_NOTE), EM_LIMITTEXT, fillMaxNotes - 1, 0);
    SendMessageW(F(IDF_UNIT), CB_ADDSTRING, 0, (LPARAM)L"L");
    SendMessageW(F(IDF_UNIT), CB_ADDSTRING, 0, (LPARAM)L"US gal");
    SendMessageW(F(IDF_UNIT), CB_ADDSTRING, 0, (LPARAM)L"Imp gal");
    for (i = 0; i < curCount; i++) {
        ToWide(CurrencyCode(i), w, 16);
        SendMessageW(F(IDF_CUR), CB_ADDSTRING, 0, (LPARAM)w);
    }
    /* Hours and minutes only, in the device's 12 or 24 hour style; dates the record format can hold. */
    h24 = GetLocaleInfoW(LOCALE_USER_DEFAULT, LOCALE_ITIME, b, 4) > 0 && b[0] == L'1';
    SendMessageW(F(IDF_TIME), DTM_SETFORMATW, 0, (LPARAM)(h24 ? L"HH':'mm" : L"h':'mm tt"));
    memset(range, 0, sizeof range);
    range[0].wYear = 1990; range[0].wMonth = 1; range[0].wDay = 1;
    range[1].wYear = 2079; range[1].wMonth = 12; range[1].wDay = 31;
    SendMessageW(F(IDF_DATE), DTM_SETRANGE, GDTR_MIN | GDTR_MAX, (LPARAM)range);
}

static void FormCommand(int id, int code, HWND ctl)
{
    char hint[32], b[32];
    int sel;
    if (gFormBusy) return;
    switch (id) {
    case IDF_VOL: case IDF_PRICE: case IDF_TOTAL:
        if (code == EN_CHANGE) {
            /* Any two of the three give the third; the one being typed is never rewritten. */
            PullTexts();
            FillEditTyped(&gEdit, id == IDF_VOL ? feVolume : id == IDF_PRICE ? fePrice : feTotal, &gText, hint);
            PushTexts();
            SetText(IDF_CALC, hint);
            FillEditPointsText(&gEdit, hint); SetText(IDF_PTS, hint);
        }
        break;
    case IDF_ODO: case IDF_TRIP:
        if (code == EN_CHANGE) {
            PullTexts();
            FillEditTripHint(&gEdit, &gText, b); SetText(IDF_TRIPHINT, b);
        }
        break;
    case IDF_VEH:
        if (code == CBN_SELCHANGE && (sel = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0)) >= 0 && sel < vehMaxVehicles) {
            PullTexts();
            FillEditSetVehicle(&gEdit, gFormVehIds[sel], &gText);
            FormChanged();
        }
        break;
    case IDF_UNIT:
        if (code == CBN_SELCHANGE && (sel = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0)) >= 0) {
            PullTexts();
            FillEditSetEntryUnit(&gEdit, (UInt8)sel, &gText);
            FormChanged();
        }
        break;
    case IDF_CUR:
        if (code == CBN_SELCHANGE && (sel = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0)) >= 0) {
            PullTexts();
            FillEditSetCurrency(&gEdit, (UInt8)sel, &gText);
            FormChanged();
        }
        break;
    case IDF_GRADE:
        if (code == CBN_SELCHANGE && (sel = (int)SendMessageW(ctl, CB_GETCURSEL, 0, 0)) >= 0 && sel < gGradeCount) {
            PullTexts();
            FillEditSetGrade(&gEdit, gGradeMap[sel], &gText);
            FormChanged();
        }
        break;
    case IDF_PTS:
        if (code == BN_CLICKED) PtsOpen();
        break;
    case IDF_CASH: case IDF_CARD:
        if (code == BN_CLICKED) {
            PullTexts();
            FillEditSetPayment(&gEdit, id == IDF_CARD ? fillPaymentCard : fillPaymentCash);
            FormChanged();
        }
        break;
    }
    /* A text field with the focus wants the keyboard, and must not sit under it. */
    if (id == IDF_ODO || id == IDF_TRIP || id == IDF_VOL || id == IDF_PRICE || id == IDF_TOTAL || id == IDF_RATE || id == IDF_NOTE) {
        if (code == EN_SETFOCUS) { SHSipPreference(gWin, SIP_UP); FormReveal(ctl); }
        else if (code == EN_KILLFOCUS) SHSipPreference(gWin, SIP_DOWN);
    }
}

static LRESULT CALLBACK FormProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        FormCreate(h);
        return 0;
    case WM_SIZE:
        FormLayout();
        FormReveal(GetFocus());              /* the keyboard just came up over the field being typed in */
        return 0;
    case WM_VSCROLL: {
        RECT r;
        GetClientRect(h, &r);
        switch (LOWORD(w)) {
        case SB_LINEUP: gFormScroll -= S(kRow); break;
        case SB_LINEDOWN: gFormScroll += S(kRow); break;
        case SB_PAGEUP: gFormScroll -= r.bottom - S(kRow); break;
        case SB_PAGEDOWN: gFormScroll += r.bottom - S(kRow); break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: gFormScroll = HIWORD(w); break;
        default: return 0;
        }
        FormLayout();
        return 0;
    }
    case WM_COMMAND:
        FormCommand(LOWORD(w), HIWORD(w), (HWND)l);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---- sheets ---- */

static HWND SC(Sheet *s, int id) { return GetDlgItem(s->panel, id); }

static int SheetRowOf(const Sheet *s, int id)
{
    int i;
    for (i = 0; i < s->count; i++) if (s->rows[i].id == id) return i;
    return -1;
}

static void SheetLayout(Sheet *s)
{
    RECT r;
    SCROLLINFO si;
    int W, y, total, i, shown;
    if (!s->panel) return;
    if (s->busy) { s->busy = 2; return; }       /* the scroll bar came or went while we were placing */
    do {
        s->busy = 1;
        GetClientRect(s->panel, &r);
        W = (r.right * 96) / gDpi;
        for (i = 0, shown = 0; i < s->count; i++) if (!s->hidden[i]) shown++;
        total = S(3 + shown * kRow + 2);
        if (s->scroll > total - r.bottom) s->scroll = total - r.bottom;
        if (s->scroll < 0) s->scroll = 0;
        y = 3;
        for (i = 0; i < s->count; i++) {
            const SheetRow *row = &s->rows[i];
            HWND c = SC(s, row->id), label = SC(s, row->id + 1000);
            ShowWindow(c, s->hidden[i] ? SW_HIDE : SW_SHOW);
            if (label) ShowWindow(label, s->hidden[i] ? SW_HIDE : SW_SHOW);
            if (s->hidden[i]) continue;
            if (row->kind == rowCheck || row->kind == rowText) {
                MoveWindow(c, S(4), S(y + 1) - s->scroll, S(W - 8), S(18), TRUE);
            } else {
                MoveWindow(label, S(2), S(y + 3) - s->scroll, S(s->labelW - 4), S(16), TRUE);
                MoveWindow(c, S(s->labelW), S(y) - s->scroll, S(W - s->labelW - 4), S(row->kind == rowCombo ? 120 : 20), TRUE);   /* a combo's height is its dropped list's */
            }
            y += kRow;
        }
        memset(&si, 0, sizeof si);
        si.cbSize = sizeof si;
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMax = total - 1;
        si.nPage = (UINT)r.bottom;
        si.nPos = s->scroll;
        SetScrollInfo(s->panel, SB_VERT, &si, TRUE);
    } while (s->busy == 2);
    s->busy = 0;
}

static void SheetReveal(Sheet *s, HWND c)
{
    RECT r, client;
    POINT a, b;
    if (!c || !IsChild(s->panel, c)) return;
    GetWindowRect(c, &r);
    a.x = r.left; a.y = r.top; b.x = r.right; b.y = r.bottom;
    ScreenToClient(s->panel, &a); ScreenToClient(s->panel, &b);
    GetClientRect(s->panel, &client);
    if (b.y - a.y > S(24)) b.y = a.y + S(20);
    if (b.y > client.bottom) s->scroll += b.y - client.bottom + S(2);
    else if (a.y < 0) s->scroll += a.y - S(2);
    else return;
    SheetLayout(s);
}

static void SheetCreate(Sheet *s, HWND h)
{
    int i;
    s->panel = h;
    for (i = 0; i < s->count; i++) {
        const SheetRow *row = &s->rows[i];
        const WCHAR *o;
        HWND c;
        switch (row->kind) {
        case rowEdit:
            CreateWindowW(L"STATIC", row->label, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, h, (HMENU)(row->id + 1000), gInst, NULL);
            c = CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL, 0, 0, 10, 10, h, (HMENU)row->id, gInst, NULL);
            SendMessageW(c, EM_LIMITTEXT, (WPARAM)row->limit, 0);
            break;
        case rowCombo:
            CreateWindowW(L"STATIC", row->label, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, h, (HMENU)(row->id + 1000), gInst, NULL);
            c = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP | CBS_DROPDOWNLIST, 0, 0, 10, 10, h, (HMENU)row->id, gInst, NULL);
            for (o = row->choices; o && *o; o += wcslen(o) + 1) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)o);
            break;
        case rowCheck:
            CreateWindowW(L"BUTTON", row->label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 10, 10, h, (HMENU)row->id, gInst, NULL);
            break;
        case rowDate: {
            SYSTEMTIME range[2];
            CreateWindowW(L"STATIC", row->label, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, h, (HMENU)(row->id + 1000), gInst, NULL);
            c = CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | DTS_SHORTDATEFORMAT, 0, 0, 10, 10, h, (HMENU)row->id, gInst, NULL);
            memset(range, 0, sizeof range);
            range[0].wYear = 1990; range[0].wMonth = 1; range[0].wDay = 1;
            range[1].wYear = 2079; range[1].wMonth = 12; range[1].wDay = 31;
            SendMessageW(c, DTM_SETRANGE, GDTR_MIN | GDTR_MAX, (LPARAM)range);
            break;
        }
        default:
            CreateWindowW(L"STATIC", row->label, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, h, (HMENU)row->id, gInst, NULL);
            break;
        }
    }
}

/* Text set here is not the user's typing: the edited callback does not hear of it. */
static void SheetSetText(Sheet *s, int id, const char *t)
{
    WCHAR w[fillMaxNotes + 2];
    ToWide(t, w, fillMaxNotes + 2);
    s->quiet++;
    SetWindowTextW(SC(s, id), w);
    s->quiet--;
}

static void SheetSetLabel(Sheet *s, int id, const char *t)
{
    WCHAR w[32];
    ToWide(t, w, 32);
    SetWindowTextW(SC(s, id + 1000), w);
}

/* A combo's rows from Palm text: `first` (if any), then names. */
static void SheetSetChoices(Sheet *s, int id, const WCHAR *first, char names[][pgmNameLen], int count, int sel)
{
    WCHAR w[pgmNameLen + 2];
    HWND c = SC(s, id);
    int i;
    SendMessageW(c, CB_RESETCONTENT, 0, 0);
    if (first) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)first);
    for (i = 0; i < count; i++) {
        ToWide(names[i], w, pgmNameLen + 2);
        SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)w);
    }
    SendMessageW(c, CB_SETCURSEL, (WPARAM)sel, 0);
}

static void SheetGetText(Sheet *s, int id, char *t, int cap)
{
    WCHAR w[fillMaxNotes + 2];
    w[0] = 0;
    GetWindowTextW(SC(s, id), w, fillMaxNotes + 2);
    FromWide(w, t, cap);
}

static void SheetSetDate(Sheet *s, int id, UInt16 days)
{
    SYSTEMTIME st;
    UInt16 y; UInt8 m, d;
    DateDaysToYmd(days, &y, &m, &d);
    memset(&st, 0, sizeof st);
    st.wYear = y; st.wMonth = m; st.wDay = d;
    s->quiet++;
    SendMessageW(SC(s, id), DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    s->quiet--;
}

/* The picked date, or `otherwise` when the picker has none. */
static UInt16 SheetGetDate(Sheet *s, int id, UInt16 otherwise)
{
    SYSTEMTIME st;
    if (SendMessageW(SC(s, id), DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID) return otherwise;
    return DateYmdToDays((UInt16)st.wYear, (UInt8)st.wMonth, (UInt8)st.wDay);
}

static int SheetGetCheck(Sheet *s, int id) { return SendMessageW(SC(s, id), BM_GETCHECK, 0, 0) == 1; }
static void SheetSetCheck(Sheet *s, int id, int on) { SendMessageW(SC(s, id), BM_SETCHECK, on ? 1 : 0, 0); }

static void SheetSetSel(Sheet *s, int id, int sel) { SendMessageW(SC(s, id), CB_SETCURSEL, (WPARAM)sel, 0); }
/* The chosen row, or `otherwise` when the list somehow has none. */
static int SheetGetSel(Sheet *s, int id, int otherwise)
{
    int sel = (int)SendMessageW(SC(s, id), CB_GETCURSEL, 0, 0);
    return sel >= 0 ? sel : otherwise;
}
static void SheetHide(Sheet *s, int id, int hide)
{
    int i = SheetRowOf(s, id);
    if (i >= 0) s->hidden[i] = (UInt8)(hide != 0);
}

static LRESULT CALLBACK SheetProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    Sheet *s = h == gPrefsSheet.panel ? &gPrefsSheet : h == gVehSheet.panel ? &gVehSheet
             : h == gPgmSheet.panel ? &gPgmSheet : h == gPtsSheet.panel ? &gPtsSheet
             : h == gSvtSheet.panel ? &gSvtSheet : h == gSvlSheet.panel ? &gSvlSheet : NULL;
    if (m == WM_CREATE) {
        SheetCreate((Sheet *)((CREATESTRUCTW *)l)->lpCreateParams, h);
        return 0;
    }
    if (!s) return DefWindowProcW(h, m, w, l);
    switch (m) {
    case WM_SIZE:
        SheetLayout(s);
        SheetReveal(s, GetFocus());
        return 0;
    case WM_VSCROLL: {
        RECT r;
        GetClientRect(h, &r);
        switch (LOWORD(w)) {
        case SB_LINEUP: s->scroll -= S(kRow); break;
        case SB_LINEDOWN: s->scroll += S(kRow); break;
        case SB_PAGEUP: s->scroll -= r.bottom - S(kRow); break;
        case SB_PAGEDOWN: s->scroll += r.bottom - S(kRow); break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: s->scroll = HIWORD(w); break;
        default: return 0;
        }
        SheetLayout(s);
        return 0;
    }
    case WM_COMMAND: {
        int i = SheetRowOf(s, LOWORD(w));
        if (i < 0) return 0;
        if (s->rows[i].kind == rowEdit) {
            if (HIWORD(w) == EN_SETFOCUS) { SHSipPreference(gWin, SIP_UP); SheetReveal(s, (HWND)l); }
            else if (HIWORD(w) == EN_KILLFOCUS) SHSipPreference(gWin, SIP_DOWN);
            else if (HIWORD(w) == EN_CHANGE && !s->quiet && s->edited) s->edited(LOWORD(w));
        } else if (s->rows[i].kind == rowCombo && HIWORD(w) == CBN_SELCHANGE && s->changed) {
            s->changed(LOWORD(w));
        } else if (s->rows[i].kind == rowCheck && HIWORD(w) == BN_CLICKED && s->changed) {
            s->changed(LOWORD(w));
        }
        return 0;
    }
    case WM_NOTIFY: {
        NMHDR *n = (NMHDR *)l;
        if (n->code == (UINT)DTN_DATETIMECHANGE && !s->quiet && s->changed) s->changed((int)n->idFrom);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---- Preferences ---- */

static const SheetRow kPrefsRows[] = {
    { IDP_DIST, rowCombo, L"Distance:", L"km\0miles\0", 0 },
    { IDP_VOL, rowCombo, L"Volume:", L"Litres\0US gallons\0Imp gallons\0", 0 },
    { IDP_ECO, rowCombo, L"Economy:", L"L/100 km\0km/L\0MPG (US)\0MPG (Imp)\0", 0 },
    { IDP_PRICE, rowCombo, L"Price per:", L"Litre\0US gallon\0Imp gallon\0", 0 },
    { IDP_FORMAT, rowCombo, L"Price as:", L"Dollars (1.789)\0Cents (178.9)\0", 0 },
    { IDP_HOME, rowCombo, L"Home currency:", L"USD\0CAD\0EUR\0AUD\0NZD\0GBP\0", 0 },
    { IDP_NEW, rowCombo, L"New fills in:", L"Last used\0USD\0CAD\0EUR\0AUD\0NZD\0GBP\0", 0 },
};

static void PrefsOpen(void)
{
    Sheet *s = &gPrefsSheet;
    SheetSetSel(s, IDP_DIST, gPrefs.distanceUnit);
    SheetSetSel(s, IDP_VOL, gPrefs.volumeUnit);
    SheetSetSel(s, IDP_ECO, gPrefs.economyUnit < 4 ? gPrefs.economyUnit : 0);
    SheetSetSel(s, IDP_PRICE, gPrefs.priceUnit < 3 ? gPrefs.priceUnit : 0);
    SheetSetSel(s, IDP_FORMAT, gPrefs.priceFormat == unitPriceFormatCents ? 1 : 0);
    SheetSetSel(s, IDP_HOME, gPrefs.homeCurrency);
    SheetSetSel(s, IDP_NEW, gPrefs.defaultCurrency == curLastUsed ? 0 : gPrefs.defaultCurrency + 1);
    s->scroll = 0;
    PostMessageW(gWin, WM_SHOWPAGE, pagePrefs, 0);
}

static void PrefsSaveSheet(void)
{
    Sheet *s = &gPrefsSheet;
    int sel;
    gPrefs.distanceUnit = (UInt8)SheetGetSel(s, IDP_DIST, gPrefs.distanceUnit);
    gPrefs.volumeUnit = (UInt8)SheetGetSel(s, IDP_VOL, gPrefs.volumeUnit);
    gPrefs.economyUnit = (UInt8)SheetGetSel(s, IDP_ECO, gPrefs.economyUnit);
    gPrefs.priceUnit = (UInt8)SheetGetSel(s, IDP_PRICE, gPrefs.priceUnit);
    gPrefs.priceFormat = SheetGetSel(s, IDP_FORMAT, gPrefs.priceFormat) == 1 ? unitPriceFormatCents : unitPriceFormatDollars;
    gPrefs.homeCurrency = (UInt8)SheetGetSel(s, IDP_HOME, gPrefs.homeCurrency);
    sel = SheetGetSel(s, IDP_NEW, 0);
    gPrefs.defaultCurrency = sel == 0 ? curLastUsed : (UInt8)(sel - 1);
    StorePrefs();
    PostMessageW(gWin, WM_SHOWPAGE, pageList, 0);
}

/* ---- Vehicles ---- */

static const SheetRow kVehRows[] = {
    { IDV_NAME, rowEdit, L"Name:", NULL, vehNameLen - 1 },
    { IDV_POWER, rowCombo, L"Type:", L"Fuel\0Hybrid\0Plug-in hybrid\0Electric\0", 0 },
    { IDV_UNIT, rowCombo, L"Odometer in:", L"km\0miles\0", 0 },
    { IDV_START, rowEdit, L"Start odo:", NULL, veText - 1 },
    { IDV_TANK, rowEdit, L"Tank (L):", NULL, veText - 1 },
    { IDV_HOME, rowEdit, L"Home c/kWh:", NULL, veText - 1 },
    { IDV_GRADE, rowCombo, L"Fuel:", L"Regular\0Mid\0Premium\0Diesel\0", 0 },
    { IDV_CUR, rowCombo, L"Currency:", L"Use Prefs\0USD\0CAD\0EUR\0AUD\0NZD\0GBP\0", 0 },
    { IDV_TRIP, rowCombo, L"Trip meter:", L"Reset each fill\0Never reset\0Not used\0", 0 },
    { IDV_ARCH, rowCheck, L"Archived (left out of the pickers)", NULL, 0 },
    { IDV_STATUS, rowText, L"", NULL, 0 },
};

/* The tank row is the battery for an electric car, which has no fuel grade; only a car that plugs in has a home rate. */
static void VehSheetRows(void)
{
    Sheet *s = &gVehSheet;
    UInt8 power = (UInt8)SheetGetSel(s, IDV_POWER, 0);
    WCHAR w[24];
    ToWide(VehEditTankLabel(power), w, 24);
    SetWindowTextW(SC(s, IDV_TANK + 1000), w);
    SheetHide(s, IDV_GRADE, !VehEditShowsGrade(power));
    SheetHide(s, IDV_HOME, !VehEditShowsHomeRate(power));
    SheetLayout(s);
}

static void VehSheetChanged(int id)
{
    if (id == IDV_POWER) VehSheetRows();
}

static void VehiclesRefresh(void)
{
    char name[40], kind[40], fills[40], odo[40];
    WCHAR w[40];
    LVITEMW it;
    UInt16 i;
    SendMessageW(gVehList, LVM_DELETEALLITEMS, 0, 0);
    gVehCount = VehListCollect(gShowArchived, gVehIdx, vehMaxVehicles);
    for (i = 0; i < gVehCount; i++) {
        const char *col[4];
        int c;
        if (!VehListRow(gVehIdx[i], &gPrefs, gPoint, gThousands, name, kind, fills, odo)) name[0] = kind[0] = fills[0] = odo[0] = 0;
        col[0] = name; col[1] = kind; col[2] = fills; col[3] = odo;
        for (c = 0; c < 4; c++) {
            ToWide(col[c], w, 40);
            memset(&it, 0, sizeof it);
            it.mask = LVIF_TEXT;
            it.iItem = i;
            it.iSubItem = c;
            it.pszText = w;
            SendMessageW(gVehList, c == 0 ? LVM_INSERTITEMW : LVM_SETITEMW, 0, (LPARAM)&it);
        }
    }
}

static void VehOpen(UInt16 index)
{
    Sheet *s = &gVehSheet;
    VehEditForm f;
    char status[48];
    if (gPage == pageVehicle) return;
    if (!gDataOk) {
        MessageBoxW(gWin, L"The log in \\My Documents\\FuelLog could not be read, so nothing can be added to it.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (!VehEditBegin(&gVehEdit, index, gPoint, &f)) {
        MessageBoxW(gWin, L"FuelLog keeps up to 16 vehicles. Archive or delete one first.", L"Vehicle limit", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    SheetSetText(s, IDV_NAME, f.name);
    SheetSetSel(s, IDV_POWER, f.powertrain);
    SheetSetSel(s, IDV_UNIT, f.odoUnit);
    SheetSetText(s, IDV_START, f.start);
    SheetSetText(s, IDV_TANK, f.tank);
    SheetSetText(s, IDV_HOME, f.homeRate);
    SheetSetSel(s, IDV_GRADE, f.grade);
    SheetSetSel(s, IDV_CUR, f.currency);
    SheetSetSel(s, IDV_TRIP, f.tripMode);
    SendMessageW(SC(s, IDV_ARCH), BM_SETCHECK, f.archived, 0);
    VehEditStatus(&gVehEdit, status);
    SheetSetText(s, IDV_STATUS, status);
    s->scroll = 0;
    VehSheetRows();
    PostMessageW(gWin, WM_SHOWPAGE, pageVehicle, 0);
}

static void VehSave(void)
{
    Sheet *s = &gVehSheet;
    VehEditForm f;
    Int32 r;
    memset(&f, 0, sizeof f);
    SheetGetText(s, IDV_NAME, f.name, vehNameLen);
    f.powertrain = (UInt8)SheetGetSel(s, IDV_POWER, 0);
    f.odoUnit = (UInt8)SheetGetSel(s, IDV_UNIT, 0);
    SheetGetText(s, IDV_START, f.start, veText);
    SheetGetText(s, IDV_TANK, f.tank, veText);
    SheetGetText(s, IDV_HOME, f.homeRate, veText);
    f.grade = (UInt8)SheetGetSel(s, IDV_GRADE, 0);
    f.currency = (UInt8)SheetGetSel(s, IDV_CUR, 0);
    f.tripMode = (UInt8)SheetGetSel(s, IDV_TRIP, 0);
    f.archived = (UInt8)(SendMessageW(SC(s, IDV_ARCH), BM_GETCHECK, 0, 0) == 1);
    r = VehEditSave(&gVehEdit, &f, gPoint, gThousands, &gPrefs);
    if (r == veNeedName) {
        MessageBoxW(gWin, L"Give the vehicle a name so it can be picked from the list.", L"Name the vehicle", MB_OK | MB_ICONEXCLAMATION);
    } else if (r == veBadNumber) {
        MessageBoxW(gWin, L"A number could not be read. Fix the field with the stray character and choose Save again.", L"Check the numbers", MB_OK | MB_ICONEXCLAMATION);
    } else if (r != veSaved) {
        MessageBoxW(gWin, L"The vehicle was not stored. Free some memory and try again.", L"Could not save", MB_OK | MB_ICONEXCLAMATION);
    } else {
        SaveData();
        StorePrefs();
        PostMessageW(gWin, WM_SHOWPAGE, pageVehicles, 0);
    }
}

static void VehDelete(void)
{
    if (!VehEditCanDelete(&gVehEdit)) return;
    if (MessageBoxW(gWin, L"Delete this vehicle?\r\n\r\nIt has no fills, so nothing else is affected.", L"FuelLog",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    VehEditDelete(&gVehEdit, &gPrefs);
    SaveData();
    StorePrefs();
    PostMessageW(gWin, WM_SHOWPAGE, pageVehicles, 0);
}

/* ---- Programmes ---- */

static void ProgrammesRefresh(void)
{
    char name[48], bal[48], note[48];
    WCHAR w[48];
    LVITEMW it;
    UInt16 i, today = Today();
    SendMessageW(gPgmList, LVM_DELETEALLITEMS, 0, 0);
    gPgmCount = PgmListCollect(gPgmIdx, pgmMaxProgrammes);
    for (i = 0; i < gPgmCount; i++) {
        const char *col[3];
        int c;
        if (!PgmListRow(gPgmIdx[i], today, gPoint, gThousands, name, bal, note)) name[0] = bal[0] = note[0] = 0;
        col[0] = name; col[1] = bal; col[2] = note;
        for (c = 0; c < 3; c++) {
            ToWide(col[c], w, 48);
            memset(&it, 0, sizeof it);
            it.mask = LVIF_TEXT;
            it.iItem = i;
            it.iSubItem = c;
            it.pszText = w;
            SendMessageW(gPgmList, c == 0 ? LVM_INSERTITEMW : LVM_SETITEMW, 0, (LPARAM)&it);
        }
    }
}

static const SheetRow kPgmRows[] = {
    { IDE_NAME, rowEdit, L"Name:", NULL, pgmNameLen - 1 },
    { IDE_CUR, rowCombo, L"Currency:", L"USD\0CAD\0EUR\0AUD\0NZD\0GBP\0", 0 },
    { IDE_MONEY, rowCombo, L"Counts in:", L"Points\0Money\0", 0 },
    { IDE_EARN, rowEdit, L"Earn pts:", NULL, 7 },
    { IDE_BASIS, rowCombo, L"Per:", L"none\0litre\0US gal\0dollar\0", 0 },
    { IDE_PREM, rowEdit, L"Premium %:", NULL, 4 },
    { IDE_PAY, rowCombo, L"Only on:", L"Any payment\0Cash\0Card\0", 0 },
    { IDE_AUTO, rowCheck, L"Add to new fills", NULL, 0 },
    { IDE_REDEEM, rowCombo, L"Redeem:", L"None\0Money off\0Per volume\0", 0 },
    { IDE_BLOCK, rowEdit, L"Points:", NULL, 9 },
    { IDE_VALUE, rowEdit, L"= $:", NULL, 8 },
    { IDE_VOLUNIT, rowCombo, L"Per:", L"litre\0US gal\0", 0 },
    { IDE_MAXVOL, rowEdit, L"Up to:", NULL, 4 },
    { IDE_CAP, rowEdit, L"Cap c/unit:", NULL, 6 },
    { IDE_EXPIRY, rowCombo, L"Expires:", L"Never\0Month after\0After months\0", 0 },
    { IDE_MONTHS, rowEdit, L"Months:", NULL, 3 },
    { IDE_OPEN, rowEdit, L"Opening pts:", NULL, 12 },
    { IDE_ALIAS, rowEdit, L"Aliases:", NULL, pgmAliasLen - 1 },
    { IDE_STATUS, rowText, L"", NULL, 0 },
};

/* Labels and rows that follow Counts in, Redeem and Expires. */
static void PgmSheetRows(void)
{
    Sheet *s = &gPgmSheet;
    int money = SheetGetSel(s, IDE_MONEY, 0) == 1;
    UInt8 redeem = (UInt8)SheetGetSel(s, IDE_REDEEM, 0), expiry = (UInt8)SheetGetSel(s, IDE_EXPIRY, 0);
    SheetSetLabel(s, IDE_EARN, PgmEditEarnLabel(money));
    SheetSetLabel(s, IDE_OPEN, PgmEditOpeningLabel(money));
    SheetSetLabel(s, IDE_VALUE, PgmEditValueLabel(redeem));
    SheetHide(s, IDE_BLOCK, redeem == pgmRedeemNone);
    SheetHide(s, IDE_VALUE, redeem == pgmRedeemNone);
    SheetHide(s, IDE_VOLUNIT, redeem != pgmRedeemPerVolume);
    SheetHide(s, IDE_MAXVOL, redeem != pgmRedeemPerVolume);
    SheetHide(s, IDE_CAP, redeem != pgmRedeemPerVolume);
    SheetHide(s, IDE_MONTHS, expiry != pgmExpiryMonths);
    SheetLayout(s);
}

static void PgmSheetChanged(int id)
{
    if (id == IDE_MONEY || id == IDE_REDEEM || id == IDE_EXPIRY) PgmSheetRows();
}

static void PgmOpen(UInt16 index)
{
    Sheet *s = &gPgmSheet;
    PgmEditForm f;
    char status[64];
    if (gPage == pageProgramme) return;
    if (!gDataOk) {
        MessageBoxW(gWin, L"The log in \\My Documents\\FuelLog could not be read, so nothing can be added to it.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (!PgmEditBegin(&gPgmEdit, index, gPrefs.homeCurrency, gPoint, gThousands, &f)) {
        MessageBoxW(gWin, L"FuelLog keeps up to 16 programmes. Archive one you no longer use first.", L"Programme limit", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    SheetSetText(s, IDE_NAME, f.name);
    SheetSetSel(s, IDE_CUR, f.currency);
    SheetSetSel(s, IDE_MONEY, f.money);
    SheetSetText(s, IDE_EARN, f.earn);
    SheetSetSel(s, IDE_BASIS, f.basis);
    SheetSetText(s, IDE_PREM, f.premium);
    SheetSetSel(s, IDE_PAY, f.payment);
    SendMessageW(SC(s, IDE_AUTO), BM_SETCHECK, f.autoApply, 0);
    SheetSetSel(s, IDE_REDEEM, f.redeem);
    SheetSetText(s, IDE_BLOCK, f.block);
    SheetSetText(s, IDE_VALUE, f.value);
    SheetSetSel(s, IDE_VOLUNIT, f.volUnit);
    SheetSetText(s, IDE_MAXVOL, f.maxVol);
    SheetSetText(s, IDE_CAP, f.cap);
    SheetSetSel(s, IDE_EXPIRY, f.expiry);
    SheetSetText(s, IDE_MONTHS, f.months);
    SheetSetText(s, IDE_OPEN, f.opening);
    SheetSetText(s, IDE_ALIAS, f.aliases);
    PgmEditStatus(&gPgmEdit, gPoint, gThousands, status);
    SheetSetText(s, IDE_STATUS, status);
    s->scroll = 0;
    PgmSheetRows();
    PostMessageW(gWin, WM_SHOWPAGE, pageProgramme, 0);
}

static void PgmSave(void)
{
    Sheet *s = &gPgmSheet;
    PgmEditForm f;
    Int32 r;
    memset(&f, 0, sizeof f);
    SheetGetText(s, IDE_NAME, f.name, pgmNameLen);
    f.currency = (UInt8)SheetGetSel(s, IDE_CUR, 0);
    f.money = (UInt8)SheetGetSel(s, IDE_MONEY, 0);
    SheetGetText(s, IDE_EARN, f.earn, peText);
    f.basis = (UInt8)SheetGetSel(s, IDE_BASIS, 0);
    SheetGetText(s, IDE_PREM, f.premium, peText);
    f.payment = (UInt8)SheetGetSel(s, IDE_PAY, 0);
    f.autoApply = (UInt8)(SendMessageW(SC(s, IDE_AUTO), BM_GETCHECK, 0, 0) == 1);
    f.redeem = (UInt8)SheetGetSel(s, IDE_REDEEM, 0);
    SheetGetText(s, IDE_BLOCK, f.block, peText);
    SheetGetText(s, IDE_VALUE, f.value, peText);
    f.volUnit = (UInt8)SheetGetSel(s, IDE_VOLUNIT, 0);
    SheetGetText(s, IDE_MAXVOL, f.maxVol, peText);
    SheetGetText(s, IDE_CAP, f.cap, peText);
    f.expiry = (UInt8)SheetGetSel(s, IDE_EXPIRY, 0);
    SheetGetText(s, IDE_MONTHS, f.months, peText);
    SheetGetText(s, IDE_OPEN, f.opening, peText);
    SheetGetText(s, IDE_ALIAS, f.aliases, pgmAliasLen);
    r = PgmEditSave(&gPgmEdit, &f, gPoint, gThousands);
    if (r == peNeedName) {
        MessageBoxW(gWin, L"Give the programme a name so it can be picked on a fill.", L"Name the programme", MB_OK | MB_ICONEXCLAMATION);
    } else if (r == peBadNumber) {
        MessageBoxW(gWin, L"A number could not be read. Fix the field with the stray character and choose Save again.", L"Check the numbers", MB_OK | MB_ICONEXCLAMATION);
    } else if (r != peSaved) {
        MessageBoxW(gWin, L"The programme was not stored. Free some memory and try again.", L"Could not save", MB_OK | MB_ICONEXCLAMATION);
    } else {
        SaveData();
        PostMessageW(gWin, WM_SHOWPAGE, pageProgrammes, 0);
    }
}

/* Archive, Restore, or Delete when no fill uses it; on the programme as stored. */
static void PgmArchive(void)
{
    const char *what = PgmEditArchiveLabel(&gPgmEdit);
    if (!what) return;
    if (strcmp(what, "Delete") == 0 && MessageBoxW(gWin, L"Delete this programme?\r\n\r\nNo fill uses it, so nothing else is lost.", L"FuelLog",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    if (PgmEditArchive(&gPgmEdit) != dbOk) {
        MessageBoxW(gWin, L"The programme was not changed. Free some memory and try again.", L"Could not save", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    SaveData();
    PostMessageW(gWin, WM_SHOWPAGE, pageProgrammes, 0);
}

/* ---- the Points page: the programmes on the fill being edited ---- */

static const SheetRow kPtsRows[] = {
    { IDT_ROW(0), rowCombo, L"Programme:", NULL, 0 }, { IDT_ROW(0) + 1, rowEdit, L"Earned:", NULL, 9 },
    { IDT_ROW(0) + 2, rowEdit, L"Redeemed:", NULL, 9 }, { IDT_ROW(0) + 3, rowText, L"", NULL, 0 },
    { IDT_ROW(1), rowCombo, L"Programme:", NULL, 0 }, { IDT_ROW(1) + 1, rowEdit, L"Earned:", NULL, 9 },
    { IDT_ROW(1) + 2, rowEdit, L"Redeemed:", NULL, 9 }, { IDT_ROW(1) + 3, rowText, L"", NULL, 0 },
    { IDT_ROW(2), rowCombo, L"Programme:", NULL, 0 }, { IDT_ROW(2) + 1, rowEdit, L"Earned:", NULL, 9 },
    { IDT_ROW(2) + 2, rowEdit, L"Redeemed:", NULL, 9 }, { IDT_ROW(2) + 3, rowText, L"", NULL, 0 },
    { IDT_ROW(3), rowCombo, L"Programme:", NULL, 0 }, { IDT_ROW(3) + 1, rowEdit, L"Earned:", NULL, 9 },
    { IDT_ROW(3) + 2, rowEdit, L"Redeemed:", NULL, 9 }, { IDT_ROW(3) + 3, rowText, L"", NULL, 0 },
    { IDT_ADD, rowCombo, L"Add:", NULL, 0 },
    { IDT_TOTAL, rowText, L"", NULL, 0 },
};

/* The hints and the total line, from what the rows hold now. */
static void PtsHints(void)
{
    char b[64];
    UInt8 i;
    for (i = 0; i < fillMaxPoints; i++) { FillEditPtsHint(&gEdit, i, b); SheetSetText(&gPtsSheet, IDT_ROW(i) + 3, b); }
    if (!gPtsChoices) strcpy(b, "No programmes yet: add them under Menu, Programmes.");
    else FillEditPtsTotal(&gEdit, b);
    SheetSetText(&gPtsSheet, IDT_TOTAL, b);
}

/* Every row from the fill being edited. */
static void PtsFill(void)
{
    Sheet *s = &gPtsSheet;
    char b[16];
    UInt8 i, k;
    gPtsChoices = FillEditPtsChoices(&gEdit, gPtsIds, gPtsNames);
    for (i = 0; i < fillMaxPoints; i++) {
        int used = i < gEdit.ptsCount, sel = 0;
        for (k = 0; used && k < gPtsChoices; k++) if (gPtsIds[k] == gEdit.pts[i].pgm) sel = k + 1;
        SheetSetChoices(s, IDT_ROW(i), L"(none)", gPtsNames, gPtsChoices, sel);
        FillEditPtsEarnedText(&gEdit, i, b); SheetSetText(s, IDT_ROW(i) + 1, b);
        FillEditPtsRedeemedText(&gEdit, i, b); SheetSetText(s, IDT_ROW(i) + 2, b);
        for (k = 0; k < 4; k++) SheetHide(s, IDT_ROW(i) + k, !used);
    }
    SheetSetChoices(s, IDT_ADD, L"Add a programme", gPtsNames, gPtsChoices, 0);
    SheetHide(s, IDT_ADD, gEdit.ptsCount >= fillMaxPoints || !gPtsChoices);
    PtsHints();
    SheetLayout(s);
}

static void PtsChanged(int id)
{
    int sel = SheetGetSel(&gPtsSheet, id, 0);
    UInt8 row = id == IDT_ADD ? gEdit.ptsCount : (UInt8)((id - IDT_ROW(0)) / 4);
    UInt8 pgm = sel > 0 && sel <= gPtsChoices ? gPtsIds[sel - 1] : 0;
    if (id == IDT_ADD && !pgm) return;
    if (!FillEditPtsSetProgramme(&gEdit, row, pgm))
        MessageBoxW(gWin, L"That programme is on this fill already.", L"FuelLog", MB_OK | MB_ICONINFORMATION);
    PtsFill();
}

static void PtsEdited(int id)
{
    char b[16];
    UInt8 row = (UInt8)((id - IDT_ROW(0)) / 4), which = (UInt8)((id - IDT_ROW(0)) % 4);
    SheetGetText(&gPtsSheet, id, b, sizeof b);
    if (which == 1) FillEditPtsSetEarned(&gEdit, row, b);
    else if (which == 2) FillEditPtsSetRedeemed(&gEdit, row, b);
    PtsHints();
}

static void PtsOpen(void)
{
    if (gPage != pageForm) return;
    PullTexts();
    gEditSaved = gEdit;
    FillEditPtsOpen(&gEdit, &gText);
    gPtsSheet.scroll = 0;
    PtsFill();
    PostMessageW(gWin, WM_SHOWPAGE, pagePoints, 0);
}

static void PtsClose(int ok)
{
    if (ok) {
        gEdit.ptsTouched = 1;           /* chosen by hand: a currency or payment change no longer resets them */
        FormChanged();                  /* the total follows the redemptions */
    } else {
        gEdit = gEditSaved;
    }
    gFormBack = 1;
    PostMessageW(gWin, WM_SHOWPAGE, pageForm, 0);
}

/* ---- Stats and Reports: lines of text, painted ---- */

static int LineHeight(UInt8 style)
{
    if (style == tvHeadline) return S(24);
    if (style == tvRule) return S(7);
    return S(15);
}

static void TextScrollBar(void)
{
    RECT r;
    SCROLLINFO si;
    GetClientRect(gTextPanel, &r);
    if (gTextScroll > gTextHeight - r.bottom) gTextScroll = gTextHeight - r.bottom;
    if (gTextScroll < 0) gTextScroll = 0;
    memset(&si, 0, sizeof si);
    si.cbSize = sizeof si;
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMax = gTextHeight - 1;
    si.nPage = (UINT)r.bottom;
    si.nPos = gTextScroll;
    SetScrollInfo(gTextPanel, SB_VERT, &si, TRUE);
}

static void DrawCell(HDC dc, const char *s, int left, int right, int top, int h, UINT align)
{
    WCHAR w[tvText + 2];
    RECT rc;
    if (!s[0]) return;
    ToWide(s, w, tvText + 2);
    rc.left = left; rc.right = right; rc.top = top; rc.bottom = top + h;
    DrawTextW(dc, w, -1, &rc, align | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
}

/* Lines from y down; returns where the next would start. */
static int PaintLines(HDC dc, const TextLine *lines, UInt16 count, int y, int W, int H)
{
    static const int edge[4] = { 44, 78, 118, 150 };     /* the Palm report's right edges, of 160 */
    HFONT normal = (HFONT)GetStockObject(SYSTEM_FONT);
    int i, c;
    for (i = 0; i < count; i++) {
        const TextLine *l = &lines[i];
        int h = LineHeight(l->style);
        if (y + h > 0 && y < H) {
            switch (l->style) {
            case tvHeadline:
                SelectObject(dc, gFontLarge ? gFontLarge : normal);
                DrawCell(dc, l->cell[0], S(4), W - S(6), y, h, DT_LEFT);
                DrawCell(dc, l->cell[1], S(4), W - S(6), y, h, DT_RIGHT);
                break;
            case tvRule: {
                HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x99, 0x99, 0x99)), op = (HPEN)SelectObject(dc, pen);
                POINT pt[2];
                pt[0].x = S(4); pt[0].y = y + h / 2; pt[1].x = W - S(6); pt[1].y = y + h / 2;
                Polyline(dc, pt, 2);
                SelectObject(dc, op);
                DeleteObject(pen);
                break;
            }
            case tvHeading:
                SelectObject(dc, gFontBold ? gFontBold : normal);
                DrawCell(dc, l->cell[0], S(4), W - S(6), y, h, DT_LEFT);
                break;
            case tvTable: case tvTableBold:
                SelectObject(dc, l->style == tvTableBold && gFontBold ? gFontBold : normal);
                DrawCell(dc, l->cell[0], S(4), W, y, h, DT_LEFT);
                for (c = 1; c < tvCells; c++) DrawCell(dc, l->cell[c], 0, (W - S(2)) * edge[c - 1] / 160, y, h, DT_RIGHT);
                break;
            case tvTriple: case tvTripleBold:
                SelectObject(dc, l->style == tvTripleBold && gFontBold ? gFontBold : normal);
                DrawCell(dc, l->cell[0], S(4), W, y, h, DT_LEFT);
                DrawCell(dc, l->cell[1], W * 36 / 100, W, y, h, DT_LEFT);
                DrawCell(dc, l->cell[2], 0, W - S(6), y, h, DT_RIGHT);
                break;
            case tvNote:
                SelectObject(dc, normal);
                DrawCell(dc, l->cell[0], S(4), W - S(4), y, h, DT_LEFT);
                break;
            default:                                    /* tvRow, tvCaption */
                SelectObject(dc, normal);
                DrawCell(dc, l->cell[0], S(4), W - S(6), y, h, DT_LEFT);
                DrawCell(dc, l->cell[1], S(4), W - S(6), y, h, DT_RIGHT);
                break;
            }
        }
        y += h;
    }
    return y;
}

static void TextPaint(HDC dc, int W, int H)
{
    RECT all;
    HFONT old;
    all.left = 0; all.top = 0; all.right = W; all.bottom = H;
    FillRect(dc, &all, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    old = (HFONT)SelectObject(dc, GetStockObject(SYSTEM_FONT));
    PaintLines(dc, gLines, gLineCount, -gTextScroll, W, H);
    SelectObject(dc, old);
}

/* Paint into a bitmap first, so scrolling does not flicker. */
static void PaintBuffered(HWND h, void (*paint)(HDC, int, int))
{
    PAINTSTRUCT ps;
    RECT r;
    HDC dc = BeginPaint(h, &ps), mem;
    HBITMAP bmp, ob;
    GetClientRect(h, &r);
    mem = CreateCompatibleDC(dc);
    bmp = mem ? CreateCompatibleBitmap(dc, r.right, r.bottom) : NULL;
    if (mem && bmp) {
        ob = (HBITMAP)SelectObject(mem, bmp);
        paint(mem, r.right, r.bottom);
        BitBlt(dc, 0, 0, r.right, r.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, ob);
    } else {
        paint(dc, r.right, r.bottom);
    }
    if (bmp) DeleteObject(bmp);
    if (mem) DeleteDC(mem);
    EndPaint(h, &ps);
}

static LRESULT CALLBACK TextProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: PaintBuffered(h, TextPaint); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: TextScrollBar(); return 0;
    case WM_VSCROLL: {
        RECT r;
        GetClientRect(h, &r);
        switch (LOWORD(w)) {
        case SB_LINEUP: gTextScroll -= S(15); break;
        case SB_LINEDOWN: gTextScroll += S(15); break;
        case SB_PAGEUP: gTextScroll -= r.bottom - S(15); break;
        case SB_PAGEDOWN: gTextScroll += r.bottom - S(15); break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: gTextScroll = HIWORD(w); break;
        default: return 0;
        }
        TextScrollBar();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (w == VK_DOWN || w == VK_UP) { SendMessageW(h, WM_VSCROLL, w == VK_DOWN ? SB_LINEDOWN : SB_LINEUP, 0); return 0; }
        break;
    }
    return DefWindowProcW(h, m, w, l);
}

/* The Reports page's period pickers from gRptPeriod. */
static void ReportControls(void)
{
    char b[16];
    WCHAR w[16];
    SYSTEMTIME st;
    UInt16 y; UInt8 mo, d;
    int range = gRptPeriod.mode == rptRange, show = gPage == pageReports;
    SendMessageW(gRptMode, CB_SETCURSEL, gRptPeriod.mode, 0);
    ReportViewPeriodText(&gRptPeriod, b);
    ToWide(b, w, 16);
    SetWindowTextW(gRptLabel, w);
    ShowWindow(gRptMode, show ? SW_SHOW : SW_HIDE);
    ShowWindow(gRptPrev, show && !range ? SW_SHOW : SW_HIDE);
    ShowWindow(gRptLabel, show && !range ? SW_SHOW : SW_HIDE);
    ShowWindow(gRptNext, show && !range ? SW_SHOW : SW_HIDE);
    ShowWindow(gRptFrom, show && range ? SW_SHOW : SW_HIDE);
    ShowWindow(gRptTo, show && range ? SW_SHOW : SW_HIDE);
    if (range) {
        memset(&st, 0, sizeof st);
        DateDaysToYmd(gRptPeriod.from, &y, &mo, &d);
        st.wYear = y; st.wMonth = mo; st.wDay = d;
        SendMessageW(gRptFrom, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
        DateDaysToYmd(gRptPeriod.to, &y, &mo, &d);
        st.wYear = y; st.wMonth = mo; st.wDay = d;
        SendMessageW(gRptTo, DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st);
    }
}

/* ---- Graphs ---- */

static const COLORREF kSeriesColour[gvSeries] = {
    RGB(0x00, 0x55, 0xCC), RGB(0xCC, 0x33, 0x00), RGB(0x00, 0x88, 0x33), RGB(0x88, 0x33, 0xAA),
    RGB(0xCC, 0x88, 0x00), RGB(0x00, 0x88, 0x88), RGB(0x66, 0x66, 0x66), RGB(0x00, 0x00, 0x00)
};

/* The plot area inside the graph page, and the mapping to it. */
static RECT gPlot;
static int gLo, gHi;      /* the y range with a tenth's margin either side */
static Int32 gPx[gvMax], gPy[gvMax];

static int PX(Int32 x)
{
    const GraphData *g = &gGraphData;
    return gPlot.left + (int)((long long)(x - g->xmin) * (gPlot.right - gPlot.left) / (g->xmax - g->xmin));
}
static int PY(Int32 y)
{
    long long span = (long long)gHi - gLo;
    if (span <= 0) span = 1;
    return gPlot.bottom - (int)(((long long)y - gLo) * (gPlot.bottom - gPlot.top) / span);
}

static void GraphLayout(int W, int H)
{
    const GraphData *g = &gGraphData;
    UInt16 k;
    gPlot.left = S(38); gPlot.right = W - S(8);
    gPlot.top = S(g->seriesCount > 1 ? 22 : 8);
    gPlot.bottom = H - S(48);
    gLo = g->min - (g->max - g->min) / 10; gHi = g->max + (g->max - g->min) / 10;
    if (gHi == gLo) gHi = gLo + 1;
    for (k = 0; k < g->n; k++) { gPx[k] = PX(g->x[k]); gPy[k] = PY(g->y[k]); }
}

static void GText(HDC dc, const char *s, int x, int y, UINT align)
{
    WCHAR w[64];
    RECT rc;
    ToWide(s, w, 64);
    rc.left = align == DT_RIGHT ? 0 : x; rc.right = align == DT_RIGHT ? x : x + S(200);
    rc.top = y; rc.bottom = y + S(14);
    DrawTextW(dc, w, -1, &rc, align | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
}

static void GraphPaint(HDC dc, int W, int H)
{
    const GraphData *g = &gGraphData;
    RECT all;
    HFONT normal = (HFONT)GetStockObject(SYSTEM_FONT), old;
    HPEN axis, dash, op;
    char b[64];
    UInt16 k;
    UInt8 s;
    int tick;

    all.left = 0; all.top = 0; all.right = W; all.bottom = H;
    FillRect(dc, &all, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    old = (HFONT)SelectObject(dc, normal);
    GraphLayout(W, H);
    if (!g->n) {
        GText(dc, "Nothing to plot yet.", S(30), S(40), DT_LEFT);
        if (g->kind == gvEconomy) GText(dc, "Economy needs two full fills.", S(14), S(56), DT_LEFT);
        SelectObject(dc, old);
        return;
    }
    /* legend under All vehicles */
    if (g->seriesCount > 1) {
        int x = S(4);
        for (s = 0; s < g->seriesCount && x < W - S(20); s++) {
            HBRUSH br = CreateSolidBrush(kSeriesColour[s]);
            RECT sw;
            char name[10];
            SIZE sz;
            WCHAR w[12];
            sw.left = x; sw.top = S(4); sw.right = x + S(7); sw.bottom = S(11);
            FillRect(dc, &sw, br);
            DeleteObject(br);
            strncpy(name, g->seriesName[s], 9); name[9] = 0;
            GText(dc, name, x + S(10), S(1), DT_LEFT);
            ToWide(name, w, 12);
            GetTextExtentPoint32W(dc, w, (int)wcslen(w), &sz);
            x += S(10) + sz.cx + S(8);
        }
    }
    /* axes and the dashed mean */
    axis = CreatePen(PS_SOLID, 1, RGB(0x66, 0x66, 0x66));
    dash = CreatePen(PS_DASH, 1, RGB(0x99, 0x99, 0x99));
    op = (HPEN)SelectObject(dc, axis);
    {
        POINT pt[3];
        pt[0].x = gPlot.left; pt[0].y = gPlot.top; pt[1].x = gPlot.left; pt[1].y = gPlot.bottom;
        pt[2].x = gPlot.right; pt[2].y = gPlot.bottom;
        Polyline(dc, pt, 3);
        SelectObject(dc, dash);
        pt[0].x = gPlot.left; pt[0].y = PY(g->mean); pt[1].x = gPlot.right; pt[1].y = pt[0].y;
        Polyline(dc, pt, 2);
    }
    SelectObject(dc, op);
    DeleteObject(axis);
    DeleteObject(dash);
    /* lines and dots per series */
    for (s = 0; s < (g->seriesCount ? g->seriesCount : 1); s++) {
        COLORREF col = g->seriesCount > 1 ? kSeriesColour[s] : kSeriesColour[0];
        HPEN pen = CreatePen(PS_SOLID, S(1) < 1 ? 1 : S(1), col);
        HBRUSH br = CreateSolidBrush(col);
        POINT prev;
        int have = 0;
        op = (HPEN)SelectObject(dc, pen);
        for (k = 0; k < g->n; k++) {
            RECT dot;
            if (g->s[k] != s) continue;
            if (have) {
                POINT pt[2];
                pt[0] = prev; pt[1].x = gPx[k]; pt[1].y = gPy[k];
                Polyline(dc, pt, 2);
            }
            dot.left = gPx[k] - S(2); dot.top = gPy[k] - S(2); dot.right = gPx[k] + S(3); dot.bottom = gPy[k] + S(3);
            FillRect(dc, &dot, br);
            prev.x = gPx[k]; prev.y = gPy[k]; have = 1;
        }
        SelectObject(dc, op);
        DeleteObject(pen);
        DeleteObject(br);
    }
    /* y labels: max, min, and the mean in bold, at the left */
    tick = S(7);
    GraphViewValueText(g, &gPrefs, g->max, gPoint, b); GText(dc, b, gPlot.left - S(3), PY(g->max) - tick, DT_RIGHT);
    GraphViewValueText(g, &gPrefs, g->min, gPoint, b); GText(dc, b, gPlot.left - S(3), PY(g->min) - tick, DT_RIGHT);
    if (gFontBold) SelectObject(dc, gFontBold);
    GraphViewValueText(g, &gPrefs, g->mean, gPoint, b); GText(dc, b, gPlot.left - S(3), PY(g->mean) - tick, DT_RIGHT);
    SelectObject(dc, normal);
    /* x labels: months along the bottom */
    {
        Int32 at[gvLabels];
        char text[gvLabels][12];
        UInt8 i, n = GraphViewXLabels(g, at, text);
        for (i = 0; i < n; i++) {
            int x = PX(at[i]);
            /* Under a month of data the labels are the first and last dates ("Aug 10"): the last ends at its point. */
            if (n == 2 && i == 1 && g->kind != gvSpend && text[1][3] == ' ') GText(dc, text[i], x, gPlot.bottom + S(3), DT_RIGHT);
            else GText(dc, text[i], x - S(8) < 0 ? 0 : x - S(8), gPlot.bottom + S(3), DT_LEFT);
        }
    }
    /* the summary, then the selected point's callout or the hint */
    GraphViewSummary(g, &gPrefs, gPoint, b);
    GText(dc, b, S(4), H - S(32), DT_LEFT);
    if (gGraphSel >= 0 && gGraphSel < (Int16)g->n) {
        WCHAR w[48];
        SIZE sz;
        RECT box;
        HPEN frame = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        GraphViewPointText(g, &gPrefs, (UInt16)gGraphSel, gPoint, b);
        ToWide(b, w, 48);
        GetTextExtentPoint32W(dc, w, (int)wcslen(w), &sz);
        box.left = gPx[gGraphSel] - sz.cx / 2 - S(4);
        if (box.left < S(2)) box.left = S(2);
        if (box.left + sz.cx + S(8) > W - S(2)) box.left = W - S(2) - sz.cx - S(8);
        box.right = box.left + sz.cx + S(8);
        box.top = gPy[gGraphSel] - S(22);
        if (box.top < gPlot.top) box.top = gPy[gGraphSel] + S(6);
        box.bottom = box.top + sz.cy + S(4);
        op = (HPEN)SelectObject(dc, frame);
        SelectObject(dc, GetStockObject(WHITE_BRUSH));
        RoundRect(dc, box.left, box.top, box.right, box.bottom, S(6), S(6));
        SelectObject(dc, op);
        DeleteObject(frame);
        box.left += S(4); box.top += S(2);
        DrawTextW(dc, w, -1, &box, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
        GText(dc, "Tap it again to clear", S(4), H - S(16), DT_LEFT);
    } else {
        GText(dc, "Tap a point for its date and value", S(4), H - S(16), DT_LEFT);
    }
    SelectObject(dc, old);
}

static LRESULT CALLBACK GraphProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: PaintBuffered(h, GraphPaint); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: InvalidateRect(h, NULL, FALSE); return 0;
    case WM_LBUTTONDOWN: {
        RECT r;
        Int16 hit;
        int x = (short)LOWORD(l), y = (short)HIWORD(l);
        if (!gGraphData.n) return 0;
        GetClientRect(h, &r);
        GraphLayout(r.right, r.bottom);
        hit = GraphViewNearest(gPx, gPy, gGraphData.n, x, y, (Int32)S(20) * S(20));
        gGraphSel = hit == gGraphSel ? -1 : hit;
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

/* The unit picker over an economy graph; the unit's name alone for one electric car. */
static void GraphControls(void)
{
    int show = gPage == pageGraphs, economy = gGraphKind == gvEconomy;
    WCHAR w[16];
    SendMessageW(gGrKind, CB_SETCURSEL, gGraphKind, 0);
    SendMessageW(gGrUnit, CB_SETCURSEL, gPrefs.economyUnit < 4 ? gPrefs.economyUnit : 0, 0);
    ToWide(GraphViewUnit(&gGraphData, &gPrefs), w, 16);
    SetWindowTextW(gGrUnitText, w);
    ShowWindow(gGrKind, show ? SW_SHOW : SW_HIDE);
    ShowWindow(gGrUnit, show && economy && !gGraphData.electric ? SW_SHOW : SW_HIDE);
    ShowWindow(gGrUnitText, show && economy && gGraphData.electric ? SW_SHOW : SW_HIDE);
}

/* Work the page out again after a change of vehicle, period, kind or unit. */
static void InfoRefresh(int keepScroll)
{
    static int busy;            /* setting the range pickers must not come back here */
    if (busy) return;
    busy = 1;
    if (gPage == pageStats) gLineCount = StatsViewBuild(&gPrefs, Today(), gPoint, gThousands, gLines, tvMaxLines);
    else if (gPage == pageReports) gLineCount = ReportViewBuild(&gPrefs, &gRptPeriod, gPoint, gThousands, gLines, tvMaxLines);
    if (gPage == pageStats || gPage == pageReports) {
        UInt16 i;
        gTextHeight = S(4);
        for (i = 0; i < gLineCount; i++) gTextHeight += LineHeight(gLines[i].style);
        if (!keepScroll) gTextScroll = 0;
        TextScrollBar();
        InvalidateRect(gTextPanel, NULL, FALSE);
        if (gPage == pageReports) ReportControls();
    }
    if (gPage == pageGraphs) {
        GraphViewCollect(&gGraphData, gGraphKind, &gPrefs);
        gGraphSel = -1;
        GraphControls();
        InvalidateRect(gGraph, NULL, FALSE);
    }
    busy = 0;
}

static void InfoOpen(int page)
{
    if (!gRptPeriod.year) ReportViewSetMode(&gRptPeriod, rptMonthly, Today());
    PostMessageW(gWin, WM_SHOWPAGE, (WPARAM)page, 0);
}

/* A range end picked: keep from <= to, as the Palm does. */
static void RangePicked(int which)
{
    SYSTEMTIME st;
    HWND c = which == IDR_FROM ? gRptFrom : gRptTo;
    UInt16 d;
    if (SendMessageW(c, DTM_GETSYSTEMTIME, 0, (LPARAM)&st) != GDT_VALID) return;
    d = DateYmdToDays((UInt16)st.wYear, (UInt8)st.wMonth, (UInt8)st.wDay);
    if (which == IDR_FROM) { gRptPeriod.from = d; if (gRptPeriod.to < d) gRptPeriod.to = d; }
    else { gRptPeriod.to = d; if (gRptPeriod.from > d) gRptPeriod.from = d; }
    InfoRefresh(0);
}

static void InfoCreate(HWND h)
{
    LOGFONTW lf;
    SYSTEMTIME range[2];
    HFONT sys = (HFONT)GetStockObject(SYSTEM_FONT);
    memset(&lf, 0, sizeof lf);
    if (!GetObjectW(sys, sizeof lf, &lf)) { lf.lfHeight = -S(11); lstrcpyW(lf.lfFaceName, L"Tahoma"); }
    lf.lfWeight = FW_BOLD;
    gFontBold = CreateFontIndirectW(&lf);
    lf.lfHeight = lf.lfHeight * 17 / 10;
    gFontLarge = CreateFontIndirectW(&lf);

    gTextPanel = CreateWindowW(kTextClass, L"", WS_CHILD | WS_VSCROLL, 0, 0, 10, 10, h, NULL, gInst, NULL);
    gGraph = CreateWindowW(kGraphClass, L"", WS_CHILD, 0, 0, 10, 10, h, NULL, gInst, NULL);
    gRptMode = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 10, 10, h, (HMENU)IDR_MODE, gInst, NULL);
    SendMessageW(gRptMode, CB_ADDSTRING, 0, (LPARAM)L"Monthly");
    SendMessageW(gRptMode, CB_ADDSTRING, 0, (LPARAM)L"Yearly");
    SendMessageW(gRptMode, CB_ADDSTRING, 0, (LPARAM)L"Range");
    gRptPrev = CreateWindowW(L"BUTTON", L"<", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, h, (HMENU)IDR_PREV, gInst, NULL);
    gRptLabel = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_CENTER, 0, 0, 10, 10, h, (HMENU)IDR_LABEL, gInst, NULL);
    gRptNext = CreateWindowW(L"BUTTON", L">", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, h, (HMENU)IDR_NEXT, gInst, NULL);
    gRptFrom = CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_CHILD | DTS_SHORTDATEFORMAT, 0, 0, 10, 10, h, (HMENU)IDR_FROM, gInst, NULL);
    gRptTo = CreateWindowW(DATETIMEPICK_CLASSW, L"", WS_CHILD | DTS_SHORTDATEFORMAT, 0, 0, 10, 10, h, (HMENU)IDR_TO, gInst, NULL);
    memset(range, 0, sizeof range);
    range[0].wYear = 1990; range[0].wMonth = 1; range[0].wDay = 1;
    range[1].wYear = 2079; range[1].wMonth = 12; range[1].wDay = 31;
    SendMessageW(gRptFrom, DTM_SETRANGE, GDTR_MIN | GDTR_MAX, (LPARAM)range);
    SendMessageW(gRptTo, DTM_SETRANGE, GDTR_MIN | GDTR_MAX, (LPARAM)range);
    gGrKind = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 10, 10, h, (HMENU)IDG_KIND, gInst, NULL);
    SendMessageW(gGrKind, CB_ADDSTRING, 0, (LPARAM)L"Economy");
    SendMessageW(gGrKind, CB_ADDSTRING, 0, (LPARAM)L"Price per fill");
    SendMessageW(gGrKind, CB_ADDSTRING, 0, (LPARAM)L"Spend per month");
    gGrUnit = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 10, 10, h, (HMENU)IDG_UNIT, gInst, NULL);
    SendMessageW(gGrUnit, CB_ADDSTRING, 0, (LPARAM)L"L/100 km");
    SendMessageW(gGrUnit, CB_ADDSTRING, 0, (LPARAM)L"km/L");
    SendMessageW(gGrUnit, CB_ADDSTRING, 0, (LPARAM)L"MPG (US)");
    SendMessageW(gGrUnit, CB_ADDSTRING, 0, (LPARAM)L"MPG (Imp)");
    gGrUnitText = CreateWindowW(L"STATIC", L"", WS_CHILD | SS_RIGHT, 0, 0, 10, 10, h, (HMENU)IDG_UNITTEXT, gInst, NULL);
}

/* ---- Service: the reminder ---- */

/* The system notification for the earliest due date of a service with the alarm ticked, at 9 am:
 * a dialog and a sound, and FuelLog's icon to tap. The handle is kept in the registry, so the
 * old one is replaced, as the Palm re-sets its alarm. */
static void ScheduleAlarm(void)
{
    HKEY k;
    DWORD disp, type, len, oldHandle = 0, oldDate = 0, date;
    UInt16 due = gDataOk ? SvcViewNextAlarm(Today()) : 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRegKey, 0, NULL, 0, 0, NULL, &k, &disp) != ERROR_SUCCESS) return;
    len = sizeof oldHandle;
    if (RegQueryValueExW(k, L"AlarmHandle", NULL, &type, (LPBYTE)&oldHandle, &len) != ERROR_SUCCESS) oldHandle = 0;
    len = sizeof oldDate;
    if (RegQueryValueExW(k, L"AlarmDate", NULL, &type, (LPBYTE)&oldDate, &len) != ERROR_SUCCESS) oldDate = 0;
    date = due;
    if (date == oldDate && (oldHandle || !date)) { RegCloseKey(k); return; }
    if (oldHandle) CeClearUserNotification((HANDLE)oldHandle);
    oldHandle = 0;
    if (due) {
        WCHAR exe[MAX_PATH];
        SYSTEMTIME st;
        CE_USER_NOTIFICATION un;
        UInt16 y; UInt8 m, d;
        HANDLE hn;
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        DateDaysToYmd(due, &y, &m, &d);
        memset(&st, 0, sizeof st);
        st.wYear = y; st.wMonth = m; st.wDay = d; st.wHour = 9;
        memset(&un, 0, sizeof un);
        un.ActionFlags = PUN_DIALOG | PUN_SOUND;
        un.pwszDialogTitle = L"FuelLog: service due";
        un.pwszDialogText = L"A service reaches its due date today. Open FuelLog and choose Menu, Service for details.";
        un.pwszSound = L"\\Windows\\Alarm1.wav";
        hn = CeSetUserNotification(NULL, exe, &st, &un);
        oldHandle = (DWORD)hn;
    }
    RegSetValueExW(k, L"AlarmHandle", 0, REG_DWORD, (const BYTE *)&oldHandle, sizeof oldHandle);
    RegSetValueExW(k, L"AlarmDate", 0, REG_DWORD, (const BYTE *)&date, sizeof date);
    RegCloseKey(k);
}

/* ---- Service: two-line rows under a few lines of heading ---- */

#define kSvcRowH 30

static void RowsScrollBar(void)
{
    RECT r;
    SCROLLINFO si;
    GetClientRect(gRowsPanel, &r);
    if (gRowsScroll > gRowsHeight - r.bottom) gRowsScroll = gRowsHeight - r.bottom;
    if (gRowsScroll < 0) gRowsScroll = 0;
    memset(&si, 0, sizeof si);
    si.cbSize = sizeof si;
    si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    si.nMax = gRowsHeight - 1;
    si.nPage = (UINT)r.bottom;
    si.nPos = gRowsScroll;
    SetScrollInfo(gRowsPanel, SB_VERT, &si, TRUE);
}

static int RowsHeadHeight(void)
{
    int h = S(2);
    UInt8 i;
    for (i = 0; i < gRowsHeadCount; i++) h += LineHeight(gRowsHead[i].style);
    return h;
}

/* A status badge: green with a tick, yellow with "!", red with a bar, as on the Palm. */
static void PaintBadge(HDC dc, UInt8 status, int cx, int cy)
{
    COLORREF col = status == svcStatusOK ? RGB(0x00, 0x99, 0x33) : status == svcStatusSoon ? RGB(0xFF, 0xCC, 0x00) : RGB(0xDD, 0x22, 0x22);
    HBRUSH br = CreateSolidBrush(col), ob = (HBRUSH)SelectObject(dc, br);
    HPEN pen = CreatePen(PS_SOLID, 1, col), op = (HPEN)SelectObject(dc, pen);
    int r = S(5);
    Ellipse(dc, cx - r, cy - r, cx + r + 1, cy + r + 1);
    SelectObject(dc, op); DeleteObject(pen);
    SelectObject(dc, ob); DeleteObject(br);
    if (status == svcStatusOK) {
        HPEN white = CreatePen(PS_SOLID, S(1) > 1 ? S(1) + 1 : 2, RGB(0xFF, 0xFF, 0xFF));
        POINT pt[3];
        op = (HPEN)SelectObject(dc, white);
        pt[0].x = cx - S(3); pt[0].y = cy; pt[1].x = cx - S(1); pt[1].y = cy + S(2); pt[2].x = cx + S(3); pt[2].y = cy - S(2);
        Polyline(dc, pt, 3);
        SelectObject(dc, op); DeleteObject(white);
    } else {
        RECT bar;
        HBRUSH mark = CreateSolidBrush(status == svcStatusSoon ? RGB(0, 0, 0) : RGB(0xFF, 0xFF, 0xFF));
        if (status == svcStatusSoon) {
            bar.left = cx - S(1) / 2; bar.right = bar.left + (S(1) > 1 ? S(1) : 2);
            bar.top = cy - S(3); bar.bottom = cy + S(1);
            FillRect(dc, &bar, mark);
            bar.top = cy + S(2); bar.bottom = cy + S(3) + 1;
            FillRect(dc, &bar, mark);
        } else {
            bar.left = cx - S(3); bar.right = cx + S(3) + 1; bar.top = cy - S(1); bar.bottom = cy + S(1) + 1;
            FillRect(dc, &bar, mark);
        }
        DeleteObject(mark);
    }
}

static void RowsPaint(HDC dc, int W, int H)
{
    RECT all;
    HFONT normal = (HFONT)GetStockObject(SYSTEM_FONT), old;
    HPEN grey = CreatePen(PS_SOLID, 1, RGB(0xDD, 0xDD, 0xDD)), op;
    int y = -gRowsScroll;
    UInt8 i;
    all.left = 0; all.top = 0; all.right = W; all.bottom = H;
    FillRect(dc, &all, (HBRUSH)GetStockObject(WHITE_BRUSH));
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));
    old = (HFONT)SelectObject(dc, normal);
    y = PaintLines(dc, gRowsHead, gRowsHeadCount, y + S(2), W, H);
    for (i = 0; i < gSvcRowCount; i++, y += S(kSvcRowH)) {
        const SvcRow *r = &gSvcRows[i];
        /* Service rows: the name, the due date just before the badge. History rows: the date, the
         * odometer ending at 60 % and the cost at the right, as the Palm draws them. */
        int l1 = y + S(1), l2 = y + S(15), edge = W - S(6);
        int midRight = r->badge ? edge - S(14) : W * 60 / 100;
        int leftRight = (r->badge || !r->mid[0]) ? W * 58 / 100 : W * 40 / 100;
        if (y + S(kSvcRowH) < 0 || y > H) continue;
        SelectObject(dc, gFontBold ? gFontBold : normal);
        DrawCell(dc, r->left, S(4), leftRight, l1, S(14), DT_LEFT);
        SelectObject(dc, normal);
        if (r->badge) PaintBadge(dc, (UInt8)(r->badge - 1), edge - S(5), l1 + S(7));
        else DrawCell(dc, r->right, 0, edge, l1, S(14), DT_RIGHT);
        DrawCell(dc, r->mid, 0, midRight, l1, S(14), DT_RIGHT);
        SelectObject(dc, r->bold2 && gFontBold ? gFontBold : normal);
        DrawCell(dc, r->line2, S(12), W - S(6), l2, S(14), DT_LEFT);
        SelectObject(dc, normal);
        DrawCell(dc, r->right2, 0, W - S(6), l2, S(14), DT_RIGHT);
        op = (HPEN)SelectObject(dc, grey);
        {
            POINT pt[2];
            pt[0].x = S(4); pt[0].y = y + S(kSvcRowH) - 1; pt[1].x = W - S(4); pt[1].y = pt[0].y;
            Polyline(dc, pt, 2);
        }
        SelectObject(dc, op);
    }
    if (gRowsFoot[0]) DrawCell(dc, gRowsFoot, S(4), W - S(4), y + S(4), S(14), DT_LEFT);
    DeleteObject(grey);
    SelectObject(dc, old);
}

static void RowsChanged(void)
{
    gRowsHeight = RowsHeadHeight() + gSvcRowCount * S(kSvcRowH) + (gRowsFoot[0] ? S(22) : S(4));
    RowsScrollBar();
    InvalidateRect(gRowsPanel, NULL, FALSE);
}

static void RowTapped(int row);

static LRESULT CALLBACK RowsProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_PAINT: PaintBuffered(h, RowsPaint); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_SIZE: RowsScrollBar(); return 0;
    case WM_VSCROLL: {
        RECT r;
        GetClientRect(h, &r);
        switch (LOWORD(w)) {
        case SB_LINEUP: gRowsScroll -= S(kSvcRowH); break;
        case SB_LINEDOWN: gRowsScroll += S(kSvcRowH); break;
        case SB_PAGEUP: gRowsScroll -= r.bottom - S(kSvcRowH); break;
        case SB_PAGEDOWN: gRowsScroll += r.bottom - S(kSvcRowH); break;
        case SB_THUMBTRACK: case SB_THUMBPOSITION: gRowsScroll = HIWORD(w); break;
        default: return 0;
        }
        RowsScrollBar();
        InvalidateRect(h, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int y = (short)HIWORD(l) + gRowsScroll - RowsHeadHeight() - S(2);
        if (y >= 0 && y / S(kSvcRowH) < gSvcRowCount) RowTapped(y / S(kSvcRowH));
        return 0;
    }
    }
    return DefWindowProcW(h, m, w, l);
}

/* ---- Service: the list and History ---- */

static void ServiceRefresh(void)
{
    char head[64];
    memset(gRowsHead, 0, sizeof gRowsHead);
    gSvcRowCount = SvcViewList(gSvcVehicle, Today(), &gPrefs, gPoint, gThousands, gSvcRows, svcMaxRows, head, gRowsFoot);
    gRowsHead[0].style = tvNote;
    strcpy(gRowsHead[0].cell[0], head);
    gRowsHeadCount = 1;
    if (!gSvcRowCount) { gRowsHead[1].style = tvNote; strcpy(gRowsHead[1].cell[0], "No services for this vehicle."); gRowsHeadCount = 2; }
    RowsChanged();
}

static void HistoryRefresh(void)
{
    memset(gRowsHead, 0, sizeof gRowsHead);
    gSvcRowCount = SvcViewHistory(gSvcVehicle, gHistType, Today(), &gPrefs, gPoint, gThousands, gSvcRows, svcMaxRows, gRowsHead, &gRowsHeadCount);
    gRowsFoot[0] = 0;
    if (!gSvcRowCount && gRowsHeadCount < 6) {
        gRowsHead[gRowsHeadCount].style = tvNote;
        strcpy(gRowsHead[gRowsHeadCount].cell[0], "No visits yet: Menu, Log visit.");
        gRowsHeadCount++;
    }
    RowsChanged();
}

/* The vehicle picker on the Service page names a vehicle, never All. */
static void ServiceVehiclePick(void)
{
    int i;
    for (i = 0; i <= vehMaxVehicles; i++) if (gVehIds[i] == gSvcVehicle) { SendMessageW(gVehicle, CB_SETCURSEL, (WPARAM)i, 0); return; }
}

static void ServiceOpen(void)
{
    if (!gDataOk) {
        MessageBoxW(gWin, L"The log in \\My Documents\\FuelLog could not be read.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (FillEditEnsureVehicle()) SaveData();
    gSvcVehicle = SvcViewVehicle(&gPrefs);
    if (SvcViewInstallPresets(gSvcVehicle)) SaveData();
    PostMessageW(gWin, WM_SHOWPAGE, pageService, 0);
}

/* ---- Service Type ---- */

static const SheetRow kSvtRows[] = {
    { IDY_NAME, rowEdit, L"Name:", NULL, svtNameLen - 1 },
    { IDY_DIST, rowEdit, L"Every km:", NULL, 9 },
    { IDY_MONTHS, rowEdit, L"Or months:", NULL, 3 },
    { IDY_WARNDIST, rowEdit, L"Warn km:", NULL, 9 },
    { IDY_WARNDAYS, rowEdit, L"Or days:", NULL, 5 },
    { IDY_ALARM, rowCheck, L"Alarm when due", NULL, 0 },
    { IDY_DETAIL, rowEdit, L"Detail:", NULL, svtDetailLen - 1 },
    { IDY_SUM1, rowText, L"", NULL, 0 },
    { IDY_SUM2, rowText, L"", NULL, 0 },
    { IDY_SUM3, rowText, L"", NULL, 0 },
};

static void SvtOpen(UInt16 index, int back)
{
    Sheet *s = &gSvtSheet;
    SvtForm f;
    char lines[3][64], b[32];
    if (!SvtEditBegin(&gSvtEdit, index, gSvcVehicle, &gPrefs, gPoint, &f)) {
        MessageBoxW(gWin, L"A vehicle keeps up to 31 service types. Archive one first.", L"Service limit", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    gSvtReturn = back;
    SheetSetText(s, IDY_NAME, f.name);
    strcpy(b, "Every "); strcat(b, UnitDistLabel(gPrefs.distanceUnit)); strcat(b, ":");
    SheetSetLabel(s, IDY_DIST, b);
    strcpy(b, "Warn "); strcat(b, UnitDistLabel(gPrefs.distanceUnit)); strcat(b, ":");
    SheetSetLabel(s, IDY_WARNDIST, b);
    SheetSetText(s, IDY_DIST, f.dist);
    SheetSetText(s, IDY_MONTHS, f.months);
    SheetSetText(s, IDY_WARNDIST, f.warnDist);
    SheetSetText(s, IDY_WARNDAYS, f.warnDays);
    SheetSetCheck(s, IDY_ALARM, f.alarm);
    SheetSetText(s, IDY_DETAIL, f.detail);
    SvtEditSummary(&gSvtEdit, Today(), &gPrefs, gPoint, gThousands, lines);
    SheetSetText(s, IDY_SUM1, lines[0]);
    SheetSetText(s, IDY_SUM2, lines[1]);
    SheetSetText(s, IDY_SUM3, lines[2]);
    s->scroll = 0;
    SheetLayout(s);
    PostMessageW(gWin, WM_SHOWPAGE, pageSvt, 0);
}

static void SvtSave(void)
{
    Sheet *s = &gSvtSheet;
    SvtForm f;
    Int32 r;
    memset(&f, 0, sizeof f);
    SheetGetText(s, IDY_NAME, f.name, svtNameLen);
    SheetGetText(s, IDY_DIST, f.dist, sizeof f.dist);
    SheetGetText(s, IDY_MONTHS, f.months, sizeof f.months);
    SheetGetText(s, IDY_WARNDIST, f.warnDist, sizeof f.warnDist);
    SheetGetText(s, IDY_WARNDAYS, f.warnDays, sizeof f.warnDays);
    f.alarm = (UInt8)SheetGetCheck(s, IDY_ALARM);
    SheetGetText(s, IDY_DETAIL, f.detail, svtDetailLen);
    r = SvtEditSave(&gSvtEdit, &f, &gPrefs, gPoint, gThousands);
    if (r == seNeedName) MessageBoxW(gWin, L"Give the service a name, such as Oil and filter.", L"Name the service", MB_OK | MB_ICONEXCLAMATION);
    else if (r == seBadNumber) MessageBoxW(gWin, L"A number could not be read. Fix the field with the stray character and choose Save again.", L"Check the numbers", MB_OK | MB_ICONEXCLAMATION);
    else if (r != seSaved) MessageBoxW(gWin, L"The service was not stored. Free some memory and try again.", L"Could not save", MB_OK | MB_ICONEXCLAMATION);
    else {
        SaveData();
        ScheduleAlarm();
        PostMessageW(gWin, WM_SHOWPAGE, (WPARAM)gSvtReturn, 0);
    }
}

static void SvtArchive(void)
{
    const char *what = SvtEditArchiveLabel(&gSvtEdit);
    if (!what) return;
    if (strcmp(what, "Delete") == 0 && MessageBoxW(gWin, L"Delete this service?\r\n\r\nNo visit covers it, so nothing else is lost.", L"FuelLog",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    if (SvtEditArchive(&gSvtEdit) != dbOk) {
        MessageBoxW(gWin, L"The service was not changed. Free some memory and try again.", L"Could not save", MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    SaveData();
    ScheduleAlarm();
    PostMessageW(gWin, WM_SHOWPAGE, pageService, 0);
}

/* ---- Log Service and Edit Visit ---- */

static const SheetRow kSvlRows[] = {
    { IDS_DATE, rowDate, L"Date:", NULL, 0 },
    { IDS_ODO, rowEdit, L"Odometer:", NULL, 15 },
    { IDS_CHK(0), rowCheck, L"", NULL, 0 }, { IDS_CHK(1), rowCheck, L"", NULL, 0 }, { IDS_CHK(2), rowCheck, L"", NULL, 0 },
    { IDS_CHK(3), rowCheck, L"", NULL, 0 }, { IDS_CHK(4), rowCheck, L"", NULL, 0 }, { IDS_CHK(5), rowCheck, L"", NULL, 0 },
    { IDS_CHK(6), rowCheck, L"", NULL, 0 }, { IDS_CHK(7), rowCheck, L"", NULL, 0 }, { IDS_CHK(8), rowCheck, L"", NULL, 0 },
    { IDS_CHK(9), rowCheck, L"", NULL, 0 }, { IDS_CHK(10), rowCheck, L"", NULL, 0 }, { IDS_CHK(11), rowCheck, L"", NULL, 0 },
    { IDS_OTHERON, rowCheck, L"Other, not on the list:", NULL, 0 },
    { IDS_OTHER, rowEdit, L"Other item:", NULL, svlOtherLen - 1 },
    { IDS_COST, rowEdit, L"Cost:", NULL, 15 },
    { IDS_CUR, rowCombo, L"Currency:", L"USD\0CAD\0EUR\0AUD\0NZD\0GBP\0", 0 },
    { IDS_PAY, rowCombo, L"Paid by:", L"Cash\0Card\0", 0 },
    { IDS_SHOP, rowEdit, L"Shop:", NULL, svlShopLen - 1 },
    { IDS_DETAIL, rowEdit, L"Detail:", NULL, svlDetailLen - 1 },
    { IDS_NOTE, rowEdit, L"Note:", NULL, fillMaxNotes - 1 },
    { IDS_NEXT, rowText, L"", NULL, 0 },
};

static void VisitForm(SvlForm *f)
{
    Sheet *s = &gSvlSheet;
    UInt8 i;
    memset(f, 0, sizeof *f);
    f->date = SheetGetDate(s, IDS_DATE, Today());
    SheetGetText(s, IDS_ODO, f->odo, sizeof f->odo);
    for (i = 0; i < svlChecks; i++) f->checked[i] = (UInt8)(i < gSvlEdit.count && SheetGetCheck(s, IDS_CHK(i)));
    f->otherOn = (UInt8)SheetGetCheck(s, IDS_OTHERON);
    SheetGetText(s, IDS_OTHER, f->other, svlOtherLen);
    SheetGetText(s, IDS_COST, f->cost, sizeof f->cost);
    f->currency = (UInt8)SheetGetSel(s, IDS_CUR, 0);
    f->payment = SheetGetSel(s, IDS_PAY, 0) == 1 ? fillPaymentCard : fillPaymentCash;
    SheetGetText(s, IDS_SHOP, f->shop, svlShopLen);
    SheetGetText(s, IDS_DETAIL, f->detail, svlDetailLen);
    SheetGetText(s, IDS_NOTE, f->notes, fillMaxNotes);
}

static void VisitNextLine(void)
{
    SvlForm f;
    char b[64];
    VisitForm(&f);
    SvlEditNextLine(&gSvlEdit, &f, &gPrefs, gPoint, gThousands, b);
    SheetSetText(&gSvlSheet, IDS_NEXT, b);
}

static void VisitChanged(int id)
{
    if (id == IDS_DATE || (id >= IDS_CHK(0) && id <= IDS_CHK(svlChecks - 1))) VisitNextLine();
}

static void VisitEdited(int id)
{
    if (id == IDS_ODO) VisitNextLine();
}

static void VisitOpen(UInt16 index, int back)
{
    Sheet *s = &gSvlSheet;
    SvlForm f;
    UInt8 i;
    WCHAR w[svtNameLen + 2];
    SvlEditBegin(&gSvlEdit, index, gSvcVehicle, Today(), &gPrefs, gPoint, &f);
    gVisitReturn = back;
    SheetSetDate(s, IDS_DATE, f.date);
    SheetSetText(s, IDS_ODO, f.odo);
    SheetSetLabel(s, IDS_ODO, gSvlEdit.prefilledOdo ? "Odo (last fill):" : "Odometer:");
    for (i = 0; i < svlChecks; i++) {
        if (i < gSvlEdit.count) { ToWide(gSvlEdit.names[i], w, svtNameLen + 2); SetWindowTextW(SC(s, IDS_CHK(i)), w); }
        SheetSetCheck(s, IDS_CHK(i), i < gSvlEdit.count && f.checked[i]);
        SheetHide(s, IDS_CHK(i), i >= gSvlEdit.count);
    }
    SheetSetCheck(s, IDS_OTHERON, f.otherOn);
    SheetSetText(s, IDS_OTHER, f.other);
    SheetSetText(s, IDS_COST, f.cost);
    SheetSetSel(s, IDS_CUR, f.currency);
    SheetSetSel(s, IDS_PAY, f.payment == fillPaymentCard ? 1 : 0);
    SheetSetText(s, IDS_SHOP, f.shop);
    SheetSetText(s, IDS_DETAIL, f.detail);
    SheetSetText(s, IDS_NOTE, f.notes);
    SheetSetText(s, IDS_NEXT, "");
    s->scroll = 0;
    SheetLayout(s);
    if (index != svtEditNew) VisitNextLine();
    PostMessageW(gWin, WM_SHOWPAGE, pageVisit, 0);
}

static void VisitSave(void)
{
    SvlForm f;
    UInt8 choice = svOtherAsk;
    Int32 r;
    WCHAR msg[200], name[svlOtherLen + 2];
    VisitForm(&f);
    for (;;) {
        int hit;
        r = SvlEditSave(&gSvlEdit, &f, choice, &gPrefs, gPoint, gThousands);
        if (r != svAskTrack) break;
        ToWide(f.other, name, svlOtherLen + 2);
        wsprintfW(msg, L"Make \"%s\" a service type for this vehicle, so it can have an interval and a history?\r\n\r\nYes: track it.\r\nNo: just this once.", name);
        hit = MessageBoxW(gWin, msg, L"Track this service?", MB_YESNOCANCEL | MB_ICONQUESTION);
        if (hit == IDYES) choice = svOtherTrack;
        else if (hit == IDNO) choice = svOtherOnce;
        else return;
    }
    if (r == svNothing) MessageBoxW(gWin, L"Tick what was done at this visit, or Other with a few words.", L"Nothing ticked", MB_OK | MB_ICONEXCLAMATION);
    else if (r == svBadNumber) MessageBoxW(gWin, L"A number could not be read. Fix the field with the stray character and choose Save again.", L"Check the numbers", MB_OK | MB_ICONEXCLAMATION);
    else if (r != svSaved) MessageBoxW(gWin, L"The visit was not stored. Free some memory and try again.", L"Could not save", MB_OK | MB_ICONEXCLAMATION);
    else {
        SaveData();
        StorePrefs();
        ScheduleAlarm();
        PostMessageW(gWin, WM_SHOWPAGE, (WPARAM)gVisitReturn, 0);
    }
}

static void VisitDelete(void)
{
    if (gSvlEdit.index == svtEditNew) return;
    if (MessageBoxW(gWin, L"Delete this visit?\r\n\r\nThe services it covered will count from the visit before it.", L"FuelLog",
            MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    SvlEditDelete(&gSvlEdit);
    SaveData();
    ScheduleAlarm();
    PostMessageW(gWin, WM_SHOWPAGE, (WPARAM)gVisitReturn, 0);
}

/* A row with a visit opens its History; one without goes to its Service Type. On History a row opens the visit. */
static void RowTapped(int row)
{
    const SvcRow *r = &gSvcRows[row];
    if (gPage == pageService) {
        if (r->hasVisit) { gHistType = r->dbIndex; PostMessageW(gWin, WM_SHOWPAGE, pageHistory, 0); }
        else SvtOpen(r->dbIndex, pageService);
    } else if (gPage == pageHistory) {
        VisitOpen(r->dbIndex, pageHistory);
    }
}

/* ---- a backup set from a Palm, on a storage card ---- */

/* The newest set (their names are dates, so the greatest name) under
 * \<card>\PALM\Programs\FuelLog\Backup on any storage card; its folder in dir. */
static int FindPalmSet(WCHAR *dir, WCHAR *name)
{
    WIN32_FIND_DATAW card, set;
    HANDLE hc, hs;
    WCHAR pat[MAX_PATH];
    int found = 0;
    name[0] = 0;
    hc = FindFirstFileW(L"\\*", &card);
    if (hc == INVALID_HANDLE_VALUE) return 0;
    do {
        /* A storage card is a root folder with the temporary attribute. */
        if ((card.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_TEMPORARY)) != (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_TEMPORARY)) continue;
        if (lstrlenW(card.cFileName) > 60) continue;
        wsprintfW(pat, L"\\%s%s\\*", card.cFileName, kPalmBackup);
        hs = FindFirstFileW(pat, &set);
        if (hs == INVALID_HANDLE_VALUE) continue;
        do {
            if (!(set.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || set.cFileName[0] == L'.') continue;
            if (lstrlenW(set.cFileName) > 60) continue;
            if (!found || wcscmp(set.cFileName, name) > 0) {
                lstrcpyW(name, set.cFileName);
                wsprintfW(dir, L"\\%s%s\\%s", card.cFileName, kPalmBackup, set.cFileName);
                found = 1;
            }
        } while (FindNextFileW(hs, &set));
        FindClose(hs);
    } while (FindNextFileW(hc, &card));
    FindClose(hc);
    return found;
}

static void TakePalmSet(void)
{
    WCHAR dir[MAX_PATH], name[MAX_PATH], msg[400], src[MAX_PATH], dst[MAX_PATH], file[48];
    UInt8 i;
    int copied = 0;
    if (!FindPalmSet(dir, name)) {
        MessageBoxW(gWin, L"No FuelLog backup found on a storage card.\r\n\r\nOn the Palm, choose Export, Back up now, then put its card in this device. "
            L"Or copy the .pdb files of a backup set into \\My Documents\\FuelLog and start FuelLog again.", L"FuelLog", MB_OK | MB_ICONINFORMATION);
        return;
    }
    if (DbCount(dbFills))
        wsprintfW(msg, L"Replace the %u fills on this device with the Palm backup %s?", (unsigned)DbCount(dbFills), name);
    else
        wsprintfW(msg, L"Use the Palm backup %s?", name);
    if (MessageBoxW(gWin, msg, L"FuelLog", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    DbCloseAll();
    for (i = 0; i < dbAll; i++) {
        ToWide(DbName(i), file, 48);
        wsprintfW(src, L"%s\\%s.pdb", dir, file);
        wsprintfW(dst, L"%s\\%s.pdb", kDataDirW, file);
        /* A set from before sync has no deletion log: the one here stays, as on the Palm. */
        if (GetFileAttributesW(src) == 0xFFFFFFFF) continue;
        if (CopyFileW(src, dst, FALSE)) copied++;
    }
    if (OpenData() != dbOk) {
        MessageBoxW(gWin, L"That backup could not be opened. It may come from a newer FuelLog than this one.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
    } else if (!copied) {
        MessageBoxW(gWin, L"The backup folder holds no FuelLog databases.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
    }
    gPrefs.currentVehicle = 255;
    StorePrefs();
    BuildVehicles();
    Refresh();
}

static void About(void)
{
    WCHAR msg[400];
    wsprintfW(msg, L"FuelLog for Pocket PC %S\r\nSo far: the fill list, New and Edit Fill with points, Stats, Reports, Graphs, Service, Vehicles, Programmes and Preferences.\r\n\r\n%u fills, %u vehicles, %u programmes, %u service visits\r\n"
        L"Data: %s\r\nScreen %d dpi\r\n\r\nFree software under the GNU GPLv3.",
        kVersion, (unsigned)DbCount(dbFills), (unsigned)DbCount(dbVehicles), (unsigned)DbCount(dbProgrammes),
        (unsigned)DbCount(dbServiceVisits), kDataDirW, gDpi);
    MessageBoxW(gWin, msg, L"FuelLog", MB_OK | MB_ICONINFORMATION);
}

/* ---- the window ---- */

/* The soft keys: New and Menu over the list; Save and Menu over a fill, a vehicle or a
 * programme; Save and Cancel over Preferences, OK and Cancel over Points; Done and New
 * over the vehicle and programme lists. */
static void SetMenuBar(int page)
{
    SHMENUBARINFO mbi;
    HMENU bar = CreateMenu(), menu = CreatePopupMenu();
    if (gMenuBar) { CommandBar_Destroy(gMenuBar); gMenuBar = NULL; }
    switch (page) {
    case pageForm: case pageVehicle: case pageProgramme: case pageSvt: case pageVisit:
        if (page == pageProgramme || page == pageSvt) {
            const char *what = page == pageSvt ? SvtEditArchiveLabel(&gSvtEdit) : PgmEditArchiveLabel(&gPgmEdit);
            WCHAR w[16];
            if (what) { ToWide(what, w, 16); AppendMenuW(menu, MF_STRING, IDM_ARCHIVE, w); }
        } else if (page == pageVisit) {
            if (gSvlEdit.index != svtEditNew) AppendMenuW(menu, MF_STRING, IDM_DELETE, L"Delete");
        } else if (page == pageForm ? gEdit.index != fillEditNew : VehEditCanDelete(&gVehEdit)) {
            AppendMenuW(menu, MF_STRING, IDM_DELETE, L"Delete");
        }
        AppendMenuW(menu, MF_STRING, IDM_CANCEL, L"Cancel");
        AppendMenuW(bar, MF_STRING, IDM_SAVE, L"Save");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)menu, L"Menu");
        break;
    case pagePrefs: case pagePoints:        /* two plain keys, no menu */
        DestroyMenu(menu);
        AppendMenuW(bar, MF_STRING, page == pagePoints ? IDM_OK : IDM_SAVE, page == pagePoints ? L"OK" : L"Save");
        AppendMenuW(bar, MF_STRING, IDM_CANCEL, L"Cancel");
        break;
    case pageVehicles: case pageProgrammes:
        DestroyMenu(menu);
        AppendMenuW(bar, MF_STRING, IDM_DONE, L"Done");
        AppendMenuW(bar, MF_STRING, IDM_NEW, L"New");
        break;
    case pageService:
        AppendMenuW(menu, MF_STRING, IDM_NEWSVC, L"New service");
        AppendMenuW(menu, MF_STRING, IDM_HISTORY, L"History");
        AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(menu, MF_STRING, IDM_DONE, L"Done");
        AppendMenuW(bar, MF_STRING, IDM_LOG, L"Log");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)menu, L"Menu");
        break;
    case pageHistory:
        if (gHistType != 0xFFFF) AppendMenuW(menu, MF_STRING, IDM_INTERVALS, L"Intervals");
        AppendMenuW(menu, MF_STRING, IDM_LOG, L"Log visit");
        AppendMenuW(bar, MF_STRING, IDM_DONE, L"Done");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)menu, L"Menu");
        break;
    case pageStats: case pageReports: case pageGraphs:
        AppendMenuW(menu, MF_STRING | (page == pageStats ? MF_CHECKED : 0), IDM_STATS, L"Stats");
        AppendMenuW(menu, MF_STRING | (page == pageReports ? MF_CHECKED : 0), IDM_REPORTS, L"Reports");
        AppendMenuW(menu, MF_STRING | (page == pageGraphs ? MF_CHECKED : 0), IDM_GRAPHS, L"Graphs");
        AppendMenuW(bar, MF_STRING, IDM_DONE, L"Done");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)menu, L"Menu");
        break;
    default:
        AppendMenuW(menu, MF_STRING, IDM_STATS, L"Stats");
        AppendMenuW(menu, MF_STRING, IDM_REPORTS, L"Reports");
        AppendMenuW(menu, MF_STRING, IDM_GRAPHS, L"Graphs");
        AppendMenuW(menu, MF_STRING, IDM_SERVICE, gBannerShown ? L"Service!" : L"Service");
        AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(menu, MF_STRING, IDM_VEHICLES, L"Vehicles");
        AppendMenuW(menu, MF_STRING, IDM_PROGRAMMES, L"Programmes");
        AppendMenuW(menu, MF_STRING, IDM_PREFS, L"Preferences");
        AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(menu, MF_STRING, IDM_PALM, L"Use Palm backup...");
        AppendMenuW(menu, MF_STRING, IDM_ABOUT, L"About");
        AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
        AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");
        AppendMenuW(bar, MF_STRING, IDM_NEW, L"New");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)menu, L"Menu");
        break;
    }
    memset(&mbi, 0, sizeof mbi);
    mbi.cbSize = sizeof mbi;
    mbi.hwndParent = gWin;
    mbi.dwFlags = SHCMBF_HMENU;
    mbi.nToolBarId = (UINT)bar;
    mbi.hInstRes = gInst;
    if (SHCreateMenuBar(&mbi)) gMenuBar = mbi.hwndMB;
}

static void ShowPage(int page)
{
    int list = page == pageList;
    gPage = page;
    SetMenuBar(page);
    ShowWindow(gForm, page == pageForm ? SW_SHOW : SW_HIDE);
    ShowWindow(gPrefsSheet.panel, page == pagePrefs ? SW_SHOW : SW_HIDE);
    ShowWindow(gVehSheet.panel, page == pageVehicle ? SW_SHOW : SW_HIDE);
    ShowWindow(gVehList, page == pageVehicles ? SW_SHOW : SW_HIDE);
    ShowWindow(gVehArch, page == pageVehicles ? SW_SHOW : SW_HIDE);
    ShowWindow(gPgmSheet.panel, page == pageProgramme ? SW_SHOW : SW_HIDE);
    ShowWindow(gPtsSheet.panel, page == pagePoints ? SW_SHOW : SW_HIDE);
    ShowWindow(gPgmList, page == pageProgrammes ? SW_SHOW : SW_HIDE);
    ShowWindow(gTextPanel, page == pageStats || page == pageReports ? SW_SHOW : SW_HIDE);
    ShowWindow(gGraph, page == pageGraphs ? SW_SHOW : SW_HIDE);
    ShowWindow(gRowsPanel, page == pageService || page == pageHistory ? SW_SHOW : SW_HIDE);
    ShowWindow(gSvtSheet.panel, page == pageSvt ? SW_SHOW : SW_HIDE);
    ShowWindow(gSvlSheet.panel, page == pageVisit ? SW_SHOW : SW_HIDE);
    if (!list) ShowWindow(gBanner, SW_HIDE);
    ReportControls();                       /* each shows its pickers only on its own page */
    GraphControls();
    ShowWindow(gVehicle, list || page == pageStats || page == pageReports || page == pageGraphs || page == pageService ? SW_SHOW : SW_HIDE);
    ShowWindow(gAvg, list ? SW_SHOW : SW_HIDE);
    ShowWindow(gMonth, list ? SW_SHOW : SW_HIDE);
    if (!list) {
        ShowWindow(gList, SW_HIDE);
        ShowWindow(gEmpty, SW_HIDE);
    }
    switch (page) {
    case pageList:
        SHSipPreference(gWin, SIP_FORCEDOWN);
        SetWindowTextW(gWin, L"FuelLog");
        BuildVehicles();                    /* vehicles and units may have changed */
        Refresh();
        SetFocus(gList);
        break;
    case pageForm:
        FormLabels(NULL);                   /* the title, and the points summary after the Points page */
        /* A new fill starts in the odometer, keyboard up; an edit waits for a tap. */
        if (gFormBack) { SHSipPreference(gWin, SIP_FORCEDOWN); SetFocus(F(IDF_PTS)); }
        else SetFocus(gEdit.index == fillEditNew ? F(IDF_ODO) : gForm);
        gFormBack = 0;
        break;
    case pageService: case pageHistory:
        SHSipPreference(gWin, SIP_FORCEDOWN);
        SetWindowTextW(gWin, page == pageService ? L"Service" : L"History");
        Layout();                           /* History has no vehicle picker */
        gRowsScroll = 0;
        if (page == pageService) { BuildVehicles(); ServiceVehiclePick(); ServiceRefresh(); }
        else HistoryRefresh();
        SetFocus(gRowsPanel);
        break;
    case pageSvt:
        SetWindowTextW(gWin, gSvtEdit.index == svtEditNew ? L"New Service" : L"Service Type");
        SheetLayout(&gSvtSheet);
        SetFocus(gSvtEdit.index == svtEditNew ? SC(&gSvtSheet, IDY_NAME) : gSvtSheet.panel);
        break;
    case pageVisit:
        SetWindowTextW(gWin, gSvlEdit.index == svtEditNew ? L"Log Service" : L"Edit Visit");
        SheetLayout(&gSvlSheet);
        SetFocus(gSvlSheet.panel);
        break;
    case pageStats: case pageReports: case pageGraphs:
        SHSipPreference(gWin, SIP_FORCEDOWN);
        SetWindowTextW(gWin, page == pageStats ? L"Stats" : page == pageReports ? L"Reports" : L"Graphs");
        Layout();                           /* Stats has no second row of pickers */
        BuildVehicles();
        InfoRefresh(0);
        SetFocus(page == pageGraphs ? gGraph : gTextPanel);
        break;
    case pagePoints:
        SetWindowTextW(gWin, L"Points");
        SheetLayout(&gPtsSheet);
        SetFocus(gPtsSheet.panel);
        break;
    case pageProgrammes:
        SHSipPreference(gWin, SIP_FORCEDOWN);
        SetWindowTextW(gWin, L"Programmes");
        ProgrammesRefresh();
        SetFocus(gPgmList);
        break;
    case pageProgramme:
        SetWindowTextW(gWin, gPgmEdit.index == pgmEditNew ? L"New Programme" : L"Edit Programme");
        SheetLayout(&gPgmSheet);
        SetFocus(gPgmEdit.index == pgmEditNew ? SC(&gPgmSheet, IDE_NAME) : gPgmSheet.panel);
        break;
    case pagePrefs:
        SetWindowTextW(gWin, L"Preferences");
        SheetLayout(&gPrefsSheet);
        SetFocus(gPrefsSheet.panel);
        break;
    case pageVehicles:
        SHSipPreference(gWin, SIP_FORCEDOWN);
        SetWindowTextW(gWin, L"Vehicles");
        VehiclesRefresh();
        SetFocus(gVehList);
        break;
    case pageVehicle:
        SetWindowTextW(gWin, gVehEdit.index == vehEditNew ? L"New Vehicle" : L"Edit Vehicle");
        SheetLayout(&gVehSheet);
        SetFocus(gVehEdit.index == vehEditNew ? SC(&gVehSheet, IDV_NAME) : gVehSheet.panel);
        break;
    }
}

/* The row under the last tap, or -1: the whole row counts, not only its date. */
static int TappedRow(HWND list)
{
    LVHITTESTINFO ht;
    DWORD pos = GetMessagePos();
    memset(&ht, 0, sizeof ht);
    ht.pt.x = (short)LOWORD(pos); ht.pt.y = (short)HIWORD(pos);
    ScreenToClient(list, &ht.pt);
    return (int)SendMessageW(list, LVM_SUBITEMHITTEST, 0, (LPARAM)&ht);
}

static void Create(HWND h)
{
    HDC dc = GetDC(NULL);
    gDpi = GetDeviceCaps(dc, LOGPIXELSX);
    ReleaseDC(NULL, dc);
    if (gDpi < 96) gDpi = 96;
    gWin = h;
    memset(&gSai, 0, sizeof gSai);
    gSai.cbSize = sizeof gSai;
    SetMenuBar(pageList);
    gVehicle = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 10, 10, h, (HMENU)IDC_VEHICLE, gInst, NULL);
    gList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | LVS_OWNERDATA,
        0, 0, 10, 10, h, (HMENU)IDC_LIST, gInst, NULL);
    SendMessageW(gList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    AddColumn(gList, 0, 46, 0, L"");
    AddColumn(gList, 1, 56, 1, L"");
    AddColumn(gList, 2, 38, 1, L"");
    AddColumn(gList, 3, 48, 1, L"");
    AddColumn(gList, 4, 36, 1, L"");
    gAvg = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, h, (HMENU)IDC_AVG, gInst, NULL);
    gMonth = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 10, 10, h, (HMENU)IDC_MONTH, gInst, NULL);
    gEmpty = CreateWindowW(L"STATIC", L"No fills yet.\r\n\r\nChoose New to log one. To bring your log from a Palm instead: on the Palm choose Export, Back up now, "
        L"put its card in this device, then Menu, Use Palm backup.", WS_CHILD | SS_LEFT, 0, 0, 10, 10, h, (HMENU)IDC_EMPTY, gInst, NULL);
    CreateWindowW(kFormClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, h, NULL, gInst, NULL);

    gPrefsSheet.rows = kPrefsRows; gPrefsSheet.count = sizeof kPrefsRows / sizeof kPrefsRows[0]; gPrefsSheet.labelW = 96;
    CreateWindowW(kSheetClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, h, NULL, gInst, &gPrefsSheet);
    gVehSheet.rows = kVehRows; gVehSheet.count = sizeof kVehRows / sizeof kVehRows[0]; gVehSheet.labelW = 84;
    gVehSheet.changed = VehSheetChanged;
    CreateWindowW(kSheetClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, h, NULL, gInst, &gVehSheet);
    gVehList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        0, 0, 10, 10, h, (HMENU)IDC_VEHLIST, gInst, NULL);
    SendMessageW(gVehList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    AddColumn(gVehList, 0, 92, 0, L"Name");
    AddColumn(gVehList, 1, 52, 0, L"Type");
    AddColumn(gVehList, 2, 32, 1, L"Fills");
    AddColumn(gVehList, 3, 56, 1, L"Odo");
    gVehArch = CreateWindowW(L"BUTTON", L"Show archived", WS_CHILD | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 10, 10, h, (HMENU)IDC_VEHARCH, gInst, NULL);

    gPgmSheet.rows = kPgmRows; gPgmSheet.count = sizeof kPgmRows / sizeof kPgmRows[0]; gPgmSheet.labelW = 84;
    gPgmSheet.changed = PgmSheetChanged;
    CreateWindowW(kSheetClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, h, NULL, gInst, &gPgmSheet);
    gPtsSheet.rows = kPtsRows; gPtsSheet.count = sizeof kPtsRows / sizeof kPtsRows[0]; gPtsSheet.labelW = 74;
    gPtsSheet.changed = PtsChanged;
    gPtsSheet.edited = PtsEdited;
    CreateWindowW(kSheetClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, h, NULL, gInst, &gPtsSheet);
    gPgmList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER,
        0, 0, 10, 10, h, (HMENU)IDC_PGMLIST, gInst, NULL);
    SendMessageW(gPgmList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    AddColumn(gPgmList, 0, 92, 0, L"Programme");
    AddColumn(gPgmList, 1, 66, 1, L"Balance");
    AddColumn(gPgmList, 2, 170, 0, L"Rule or expiry");
    InfoCreate(h);
    gRowsPanel = CreateWindowW(kRowsClass, L"", WS_CHILD | WS_VSCROLL, 0, 0, 10, 10, h, NULL, gInst, NULL);
    gBanner = CreateWindowW(L"BUTTON", L"", WS_CHILD | BS_PUSHBUTTON, 0, 0, 10, 10, h, (HMENU)IDC_BANNER, gInst, NULL);
    gSvtSheet.rows = kSvtRows; gSvtSheet.count = sizeof kSvtRows / sizeof kSvtRows[0]; gSvtSheet.labelW = 84;
    CreateWindowW(kSheetClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, h, NULL, gInst, &gSvtSheet);
    gSvlSheet.rows = kSvlRows; gSvlSheet.count = sizeof kSvlRows / sizeof kSvlRows[0]; gSvlSheet.labelW = 84;
    gSvlSheet.changed = VisitChanged;
    gSvlSheet.edited = VisitEdited;
    CreateWindowW(kSheetClass, L"", WS_CHILD | WS_VSCROLL | WS_CLIPCHILDREN, 0, 0, 10, 10, h, NULL, gInst, &gSvlSheet);
}

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_CREATE:
        Create(h);
        return 0;
    case WM_SIZE:
        if (gList) Layout();
        return 0;
    case WM_ACTIVATE:
        SHHandleWMActivate(h, w, l, &gSai, FALSE);
        return 0;
    case WM_SETTINGCHANGE:
        SHHandleWMSettingChange(h, w, l, &gSai);
        return 0;
    case WM_SHOWPAGE:
        ShowPage((int)w);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDM_ABOUT: About(); return 0;
        case IDM_PALM: if (gPage == pageList) TakePalmSet(); return 0;
        case IDM_EXIT: SendMessageW(h, WM_CLOSE, 0, 0); return 0;
        case IDM_VEHICLES: if (gPage == pageList) PostMessageW(h, WM_SHOWPAGE, pageVehicles, 0); return 0;
        case IDM_PROGRAMMES: if (gPage == pageList) PostMessageW(h, WM_SHOWPAGE, pageProgrammes, 0); return 0;
        case IDM_PREFS: if (gPage == pageList) PrefsOpen(); return 0;
        case IDM_STATS: case IDM_REPORTS: case IDM_GRAPHS:
            if (gPage == pageList || gPage == pageStats || gPage == pageReports || gPage == pageGraphs)
                InfoOpen(LOWORD(w) == IDM_STATS ? pageStats : LOWORD(w) == IDM_REPORTS ? pageReports : pageGraphs);
            return 0;
        case IDM_DONE:
            if (gPage == pageVehicles || gPage == pageProgrammes || gPage == pageStats || gPage == pageReports || gPage == pageGraphs
                || gPage == pageService)
                PostMessageW(h, WM_SHOWPAGE, pageList, 0);
            else if (gPage == pageHistory) PostMessageW(h, WM_SHOWPAGE, pageService, 0);
            return 0;
        case IDR_MODE:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(gRptMode, CB_GETCURSEL, 0, 0);
                if (sel >= 0) { ReportViewSetMode(&gRptPeriod, (UInt8)sel, Today()); InfoRefresh(0); }
            }
            return 0;
        case IDR_PREV: case IDR_NEXT:
            if (HIWORD(w) == BN_CLICKED) { ReportViewStep(&gRptPeriod, LOWORD(w) == IDR_PREV ? -1 : 1); InfoRefresh(0); }
            return 0;
        case IDG_KIND:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(gGrKind, CB_GETCURSEL, 0, 0);
                if (sel >= 0) { gGraphKind = (UInt8)sel; InfoRefresh(0); }
            }
            return 0;
        case IDG_UNIT:
            if (HIWORD(w) == CBN_SELCHANGE) {
                /* the list is in unitEco* order; Stats and the fill list follow, as on the Palm */
                int sel = (int)SendMessageW(gGrUnit, CB_GETCURSEL, 0, 0);
                if (sel >= 0) { gPrefs.economyUnit = (UInt8)sel; StorePrefs(); InfoRefresh(0); }
            }
            return 0;
        case IDM_NEW:
            if (gPage == pageList) FormOpen(fillEditNew);
            else if (gPage == pageVehicles) VehOpen(vehEditNew);
            else if (gPage == pageProgrammes) PgmOpen(pgmEditNew);
            return 0;
        case IDM_SAVE:
            if (gPage == pageForm) FormSave();
            else if (gPage == pagePrefs) PrefsSaveSheet();
            else if (gPage == pageVehicle) VehSave();
            else if (gPage == pageProgramme) PgmSave();
            else if (gPage == pageSvt) SvtSave();
            else if (gPage == pageVisit) VisitSave();
            return 0;
        case IDM_OK: if (gPage == pagePoints) PtsClose(1); return 0;
        case IDM_SERVICE: if (gPage == pageList) ServiceOpen(); return 0;
        case IDC_BANNER: if (HIWORD(w) == BN_CLICKED && gPage == pageList) ServiceOpen(); return 0;
        case IDM_NEWSVC: if (gPage == pageService) SvtOpen(svtEditNew, pageService); return 0;
        case IDM_HISTORY: if (gPage == pageService) { gHistType = 0xFFFF; PostMessageW(h, WM_SHOWPAGE, pageHistory, 0); } return 0;
        case IDM_INTERVALS: if (gPage == pageHistory && gHistType != 0xFFFF) SvtOpen(gHistType, pageHistory); return 0;
        case IDM_LOG: if (gPage == pageService || gPage == pageHistory) VisitOpen(svtEditNew, gPage); return 0;
        case IDM_ARCHIVE:
            if (gPage == pageProgramme) PgmArchive();
            else if (gPage == pageSvt) SvtArchive();
            return 0;
        case IDM_DELETE:
            if (gPage == pageForm) FormDelete();
            else if (gPage == pageVehicle) VehDelete();
            else if (gPage == pageVisit) VisitDelete();
            return 0;
        case IDM_CANCEL:
            if (gPage == pageForm || gPage == pagePrefs) PostMessageW(h, WM_SHOWPAGE, pageList, 0);
            else if (gPage == pageVehicle) PostMessageW(h, WM_SHOWPAGE, pageVehicles, 0);
            else if (gPage == pageProgramme) PostMessageW(h, WM_SHOWPAGE, pageProgrammes, 0);
            else if (gPage == pagePoints) PtsClose(0);
            else if (gPage == pageSvt) PostMessageW(h, WM_SHOWPAGE, (WPARAM)gSvtReturn, 0);
            else if (gPage == pageVisit) PostMessageW(h, WM_SHOWPAGE, (WPARAM)gVisitReturn, 0);
            return 0;
        case IDC_VEHICLE:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(gVehicle, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel <= vehMaxVehicles) {
                    gPrefs.currentVehicle = gVehIds[sel];
                    StorePrefs();
                    if (gPage == pageService) {
                        /* Service names a vehicle: All goes to the first in use, as on the Palm. */
                        if (gPrefs.currentVehicle == 255) gPrefs.currentVehicle = SvcViewVehicle(&gPrefs);
                        gSvcVehicle = gPrefs.currentVehicle;
                        StorePrefs();
                        ServiceVehiclePick();
                        if (SvcViewInstallPresets(gSvcVehicle)) SaveData();
                        gRowsScroll = 0;
                        ServiceRefresh();
                    } else if (gPage == pageList) Refresh(); else InfoRefresh(0);
                }
            }
            return 0;
        case IDC_VEHARCH:
            if (HIWORD(w) == BN_CLICKED) {
                gShowArchived = SendMessageW(gVehArch, BM_GETCHECK, 0, 0) == 1;
                VehiclesRefresh();
            }
            return 0;
        }
        break;
    case WM_NOTIFY: {
        NMHDR *n = (NMHDR *)l;
        if ((n->idFrom == IDR_FROM || n->idFrom == IDR_TO) && n->code == (UINT)DTN_DATETIMECHANGE) {
            if (gPage == pageReports) RangePicked((int)n->idFrom);
            return 0;
        }
        if (n->idFrom == IDC_VEHLIST) {
            /* A tap on a vehicle opens it. */
            if (n->code == (UINT)NM_CLICK || n->code == (UINT)NM_RETURN) {
                int row = n->code == (UINT)NM_CLICK ? TappedRow(gVehList) : (int)SendMessageW(gVehList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
                if (gPage == pageVehicles && row >= 0 && row < (int)gVehCount) VehOpen(gVehIdx[row]);
                return 0;
            }
            break;
        }
        if (n->idFrom == IDC_PGMLIST) {
            /* A tap on a programme opens it. */
            if (n->code == (UINT)NM_CLICK || n->code == (UINT)NM_RETURN) {
                int row = n->code == (UINT)NM_CLICK ? TappedRow(gPgmList) : (int)SendMessageW(gPgmList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
                if (gPage == pageProgrammes && row >= 0 && row < (int)gPgmCount) PgmOpen(gPgmIdx[row]);
                return 0;
            }
            break;
        }
        if (n->idFrom != IDC_LIST) break;
        if (n->code == LVN_GETDISPINFOW) {
            NMLVDISPINFOW *d = (NMLVDISPINFOW *)l;
            int row = d->item.iItem, c = d->item.iSubItem;
            if ((d->item.mask & LVIF_TEXT) && row >= 0 && row < (int)gRowCount && c >= 0 && c < fillViewCols) {
                if (row != gCachedRow) {
                    char col[fillViewCols][fillViewText];
                    int i;
                    memset(col, 0, sizeof col);
                    FillViewRow(gRows[row], &gPrefs, gPoint, gThousands, col);
                    for (i = 0; i < fillViewCols; i++) ToWide(col[i], gCached[i], fillViewText);
                    gCachedRow = row;
                }
                d->item.pszText = gCached[c];
            }
            return 0;
        }
        /* A tap on a fill opens it, as on the Palm; so does the action key on the selected row. */
        if (n->code == (UINT)NM_CLICK || n->code == (UINT)NM_RETURN) {
            int row = n->code == (UINT)NM_CLICK ? TappedRow(gList) : (int)SendMessageW(gList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
            if (gPage == pageList && row >= 0 && row < (int)gRowCount) FormOpen(gRows[row]);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
        StorePrefs();
        if (DbDirty()) DbSave();
        DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        if (gMenuBar) CommandBar_Destroy(gMenuBar);
        DbCloseAll();
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    WNDCLASSW wc;
    INITCOMMONCONTROLSEX icc;
    HWND existing, win;
    MSG msg;
    Int32 e;
    (void)prev; (void)cmd;

    /* One window: a second start brings the first forward. */
    if ((existing = FindWindowW(kClass, NULL)) != NULL) {
        SetForegroundWindow((HWND)((ULONG)existing | 1));
        return 0;
    }
    gInst = inst;
    icc.dwSize = sizeof icc;
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_DATE_CLASSES;
    InitCommonControlsEx(&icc);
    LoadPrefs();
    ReadFormats();
    e = OpenData();

    memset(&wc, 0, sizeof wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = FormProc;
    wc.hInstance = inst;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = kFormClass;
    if (!RegisterClassW(&wc)) return 1;
    wc.lpfnWndProc = SheetProc;
    wc.lpszClassName = kSheetClass;
    if (!RegisterClassW(&wc)) return 1;
    wc.lpfnWndProc = TextProc;
    wc.lpszClassName = kTextClass;
    if (!RegisterClassW(&wc)) return 1;
    wc.lpfnWndProc = RowsProc;
    wc.lpszClassName = kRowsClass;
    if (!RegisterClassW(&wc)) return 1;
    wc.lpfnWndProc = GraphProc;
    wc.lpszClassName = kGraphClass;
    if (!RegisterClassW(&wc)) return 1;
    wc.lpfnWndProc = Proc;
    wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc)) return 1;
    win = CreateWindowW(kClass, L"FuelLog", WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, inst, NULL);
    if (!win) return 1;
    if (gMenuBar) {
        /* The window covers the work area; the menu bar takes its own height off the bottom. */
        RECT r, mb;
        GetWindowRect(win, &r);
        GetWindowRect(gMenuBar, &mb);
        r.bottom -= mb.bottom - mb.top;
        MoveWindow(win, r.left, r.top, r.right - r.left, r.bottom - r.top, FALSE);
    }
    BuildVehicles();
    Refresh();
    ScheduleAlarm();                    /* the reminder for what is due, as the Palm re-sets its alarm at launch */
    ShowWindow(win, show);
    UpdateWindow(win);
    if (e == dbErrNewerSchema)
        MessageBoxW(win, L"The log in \\My Documents\\FuelLog was written by a newer FuelLog than this one. Nothing was changed.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
    else if (e != dbOk)
        MessageBoxW(win, L"The log in \\My Documents\\FuelLog could not be read. Nothing was changed.", L"FuelLog", MB_OK | MB_ICONEXCLAMATION);
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

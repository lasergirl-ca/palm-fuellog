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
#include "settings.h"
#include "../store/db.h"
#include "../../src/datecalc.h"
#include "../../src/units.h"
#include "../../src/currency.h"

#define kVersion "0.3"
#define kClass L"FuelLogPPC"
#define kFormClass L"FuelLogForm"
#define kSheetClass L"FuelLogSheet"
#define kRegKey L"Software\\FuelLog"
#define kDataDir "\\My Documents\\FuelLog"
#define kDataDirW L"\\My Documents\\FuelLog"
#define kPalmBackup L"\\PALM\\Programs\\FuelLog\\Backup"

enum { IDC_VEHICLE = 100, IDC_LIST, IDC_AVG, IDC_MONTH, IDC_EMPTY, IDC_VEHLIST, IDC_VEHARCH };
enum { IDM_ABOUT = 200, IDM_PALM, IDM_EXIT, IDM_NEW, IDM_SAVE, IDM_CANCEL, IDM_DELETE, IDM_DONE, IDM_VEHICLES, IDM_PREFS };
/* The form's controls, in the order they are laid out. */
enum { IDF_LVEH = 300, IDF_VEH, IDF_DATE, IDF_TIME, IDF_LODO, IDF_ODO, IDF_ODOHINT, IDF_LTRIP, IDF_TRIP, IDF_TRIPHINT,
       IDF_LVOL, IDF_VOL, IDF_UNIT, IDF_LPRICE, IDF_PRICE, IDF_CALC, IDF_LTOTAL, IDF_TOTAL, IDF_CUR,
       IDF_LRATE, IDF_RATE, IDF_RATEHINT, IDF_LGRADE, IDF_GRADE, IDF_FULL, IDF_MISSED,
       IDF_LPAY, IDF_CASH, IDF_CARD, IDF_LPTS, IDF_PTS, IDF_LNOTE, IDF_NOTE };
/* The rows of the two sheets (label ids are these plus 1000). */
enum { IDP_DIST = 400, IDP_VOL, IDP_ECO, IDP_PRICE, IDP_FORMAT, IDP_HOME, IDP_NEW };
enum { IDV_NAME = 420, IDV_POWER, IDV_UNIT, IDV_START, IDV_TANK, IDV_HOME, IDV_GRADE, IDV_CUR, IDV_TRIP, IDV_ARCH, IDV_STATUS };
enum { pageList, pageForm, pagePrefs, pageVehicles, pageVehicle };
#define WM_SHOWPAGE (WM_APP + 1)     /* posted, so a menu bar is never replaced inside its own command */

#define kMaxRows 4096
#define kRow 22                      /* the form's row pitch at 96 dpi */

static HINSTANCE gInst;
static HWND gWin, gMenuBar, gVehicle, gList, gAvg, gMonth, gEmpty, gForm, gVehList, gVehArch;
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

/* A sheet is a simple scrolling form, a label and a control per row: Preferences and a vehicle. */
enum { rowEdit, rowCombo, rowCheck, rowText };
typedef struct {
    int id, kind;
    const WCHAR *label;
    const WCHAR *choices;       /* a combo's rows, each ended by a NUL */
    int limit;                  /* an edit's longest text */
} SheetRow;
#define kSheetRows 12
typedef struct {
    HWND panel;
    const SheetRow *rows;
    int count, labelW, scroll, busy;
    UInt8 hidden[kSheetRows];
    void (*changed)(int id);    /* a combo's choice changed */
} Sheet;
static Sheet gPrefsSheet, gVehSheet;

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
    if (DbEnsureUids() || DbDirty()) DbSave();
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
    MoveWindow(gList, 0, top, w, h - top - foot, TRUE);
    MoveWindow(gEmpty, S(8), top + S(8), w - S(16), h - top - foot - S(16), TRUE);
    MoveWindow(gAvg, S(4), h - foot + S(2), w / 2 - S(4), foot - S(2), TRUE);
    MoveWindow(gMonth, w / 2, h - foot + S(2), w / 2 - S(4), foot - S(2), TRUE);
    MoveWindow(gForm, 0, 0, w, h, TRUE);
    MoveWindow(gPrefsSheet.panel, 0, 0, w, h, TRUE);
    MoveWindow(gVehSheet.panel, 0, 0, w, h, TRUE);
    MoveWindow(gVehList, 0, 0, w, h - S(24), TRUE);
    MoveWindow(gVehArch, S(4), h - S(22), w - S(8), S(20), TRUE);
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
        Place(IDF_LPTS, 2, y + 3, 72, 16); Place(IDF_PTS, 76, y + 3, W - 78, 16); y += kRow;
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

    PullTexts();
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
    Ctl(L"STATIC", L"", SS_LEFT, IDF_PTS);
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
                MoveWindow(c, S(s->labelW), S(y) - s->scroll, S(W - s->labelW - 4), S(row->kind == rowCombo ? 120 : 20), TRUE);
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
            for (o = row->choices; *o; o += wcslen(o) + 1) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)o);
            break;
        case rowCheck:
            CreateWindowW(L"BUTTON", row->label, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, 0, 0, 10, 10, h, (HMENU)row->id, gInst, NULL);
            break;
        default:
            CreateWindowW(L"STATIC", row->label, WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, h, (HMENU)row->id, gInst, NULL);
            break;
        }
    }
}

static void SheetSetText(Sheet *s, int id, const char *t)
{
    WCHAR w[64];
    ToWide(t, w, 64);
    SetWindowTextW(SC(s, id), w);
}

static void SheetGetText(Sheet *s, int id, char *t, int cap)
{
    WCHAR w[64];
    w[0] = 0;
    GetWindowTextW(SC(s, id), w, 64);
    FromWide(w, t, cap);
}

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
    Sheet *s = h == gPrefsSheet.panel ? &gPrefsSheet : h == gVehSheet.panel ? &gVehSheet : NULL;
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
        } else if (s->rows[i].kind == rowCombo && HIWORD(w) == CBN_SELCHANGE && s->changed) {
            s->changed(LOWORD(w));
        }
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
    wsprintfW(msg, L"FuelLog for Pocket PC %S\r\nSo far: the fill list, New and Edit Fill, Vehicles and Preferences.\r\n\r\n%u fills, %u vehicles, %u programmes, %u service visits\r\n"
        L"Data: %s\r\nScreen %d dpi\r\n\r\nFree software under the GNU GPLv3.",
        kVersion, (unsigned)DbCount(dbFills), (unsigned)DbCount(dbVehicles), (unsigned)DbCount(dbProgrammes),
        (unsigned)DbCount(dbServiceVisits), kDataDirW, gDpi);
    MessageBoxW(gWin, msg, L"FuelLog", MB_OK | MB_ICONINFORMATION);
}

/* ---- the window ---- */

/* The soft keys: New and Menu over the list, Save and Menu over a fill or a vehicle,
 * Save and Cancel over Preferences, Done and New over the vehicle list. */
static void SetMenuBar(int page)
{
    SHMENUBARINFO mbi;
    HMENU bar = CreateMenu(), menu = CreatePopupMenu();
    if (gMenuBar) { CommandBar_Destroy(gMenuBar); gMenuBar = NULL; }
    switch (page) {
    case pageForm: case pageVehicle:
        if (page == pageForm ? gEdit.index != fillEditNew : VehEditCanDelete(&gVehEdit)) AppendMenuW(menu, MF_STRING, IDM_DELETE, L"Delete");
        AppendMenuW(menu, MF_STRING, IDM_CANCEL, L"Cancel");
        AppendMenuW(bar, MF_STRING, IDM_SAVE, L"Save");
        AppendMenuW(bar, MF_POPUP, (UINT_PTR)menu, L"Menu");
        break;
    case pagePrefs:                         /* two plain keys, no menu */
        DestroyMenu(menu);
        AppendMenuW(bar, MF_STRING, IDM_SAVE, L"Save");
        AppendMenuW(bar, MF_STRING, IDM_CANCEL, L"Cancel");
        break;
    case pageVehicles:
        DestroyMenu(menu);
        AppendMenuW(bar, MF_STRING, IDM_DONE, L"Done");
        AppendMenuW(bar, MF_STRING, IDM_NEW, L"New");
        break;
    default:
        AppendMenuW(menu, MF_STRING, IDM_VEHICLES, L"Vehicles");
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
    ShowWindow(gVehicle, list ? SW_SHOW : SW_HIDE);
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
        FormLayout();
        /* A new fill starts in the odometer, keyboard up; an edit waits for a tap. */
        SetFocus(gEdit.index == fillEditNew ? F(IDF_ODO) : gForm);
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
        case IDM_PREFS: if (gPage == pageList) PrefsOpen(); return 0;
        case IDM_DONE: if (gPage == pageVehicles) PostMessageW(h, WM_SHOWPAGE, pageList, 0); return 0;
        case IDM_NEW:
            if (gPage == pageList) FormOpen(fillEditNew);
            else if (gPage == pageVehicles) VehOpen(vehEditNew);
            return 0;
        case IDM_SAVE:
            if (gPage == pageForm) FormSave();
            else if (gPage == pagePrefs) PrefsSaveSheet();
            else if (gPage == pageVehicle) VehSave();
            return 0;
        case IDM_DELETE:
            if (gPage == pageForm) FormDelete();
            else if (gPage == pageVehicle) VehDelete();
            return 0;
        case IDM_CANCEL:
            if (gPage == pageForm || gPage == pagePrefs) PostMessageW(h, WM_SHOWPAGE, pageList, 0);
            else if (gPage == pageVehicle) PostMessageW(h, WM_SHOWPAGE, pageVehicles, 0);
            return 0;
        case IDC_VEHICLE:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(gVehicle, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel <= vehMaxVehicles) { gPrefs.currentVehicle = gVehIds[sel]; StorePrefs(); Refresh(); }
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
        if (n->idFrom == IDC_VEHLIST) {
            /* A tap on a vehicle opens it. */
            if (n->code == (UINT)NM_CLICK || n->code == (UINT)NM_RETURN) {
                int row = n->code == (UINT)NM_CLICK ? TappedRow(gVehList) : (int)SendMessageW(gVehList, LVM_GETNEXTITEM, (WPARAM)-1, LVNI_SELECTED);
                if (gPage == pageVehicles && row >= 0 && row < (int)gVehCount) VehOpen(gVehIdx[row]);
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

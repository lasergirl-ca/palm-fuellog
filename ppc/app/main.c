/* FuelLog for Pocket PC: the main window. First slice (P1): the fill list
 * over the six FuelLog databases kept as the Palm's own .pdb files in
 * \My Documents\FuelLog, a vehicle picker, the footer, and taking a backup
 * set a Palm wrote to a storage card. Everything it shows comes from
 * fillview.c and the shared core in ../src; this file is only the window. */
#include <windows.h>
#include <commctrl.h>
#include <aygshell.h>
#include <string.h>
#include "fillview.h"
#include "../store/db.h"
#include "../../src/datecalc.h"
#include "../../src/units.h"

#define kVersion "0.1"
#define kClass L"FuelLogPPC"
#define kDataDir "\\My Documents\\FuelLog"
#define kDataDirW L"\\My Documents\\FuelLog"
#define kPalmBackup L"\\PALM\\Programs\\FuelLog\\Backup"

enum { IDC_VEHICLE = 100, IDC_LIST, IDC_AVG, IDC_MONTH, IDC_EMPTY };
enum { IDM_ABOUT = 200, IDM_PALM, IDM_EXIT };

#define kMaxRows 4096

static HINSTANCE gInst;
static HWND gWin, gMenuBar, gVehicle, gList, gAvg, gMonth, gEmpty;
static SHACTIVATEINFO gSai;
static int gDpi = 96;
static FuelPrefs gPrefs;
static char gPoint = '.', gThousands = ',';
static UInt16 gRows[kMaxRows], gRowCount;
static UInt8 gVehIds[vehMaxVehicles + 1];
static int gCachedRow = -1;
static WCHAR gCached[fillViewCols][fillViewText];

/* Layout numbers are for a 96 dpi screen (QVGA); a VGA device reports 192. Windows CE has no MulDiv. */
static int S(int x) { return (x * gDpi + 48) / 96; }

/* Record text is Palm Latin, which is Windows-1252. */
static void ToWide(const char *s, WCHAR *w, int cap)
{
    if (MultiByteToWideChar(1252, 0, s, -1, w, cap) == 0 && MultiByteToWideChar(CP_ACP, 0, s, -1, w, cap) == 0) w[0] = 0;
}

static UInt16 Today(void)
{
    SYSTEMTIME t;
    GetLocalTime(&t);
    return DateYmdToDays((UInt16)t.wYear, (UInt8)t.wMonth, (UInt8)t.wDay);
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
    if (e != dbOk) return e;
    if (DbEnsureUids() || DbDirty()) DbSave();
    return dbOk;
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

static void AddColumn(int i, int width, int right)
{
    LVCOLUMNW c;
    memset(&c, 0, sizeof c);
    c.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
    c.fmt = right ? LVCFMT_RIGHT : LVCFMT_LEFT;
    c.cx = S(width);
    c.pszText = L"";
    c.iSubItem = i;
    SendMessageW(gList, LVM_INSERTCOLUMNW, (WPARAM)i, (LPARAM)&c);
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
    BuildVehicles();
    Refresh();
}

static void About(void)
{
    WCHAR msg[400];
    wsprintfW(msg, L"FuelLog for Pocket PC %S\r\nFirst slice: the fill list.\r\n\r\n%u fills, %u vehicles, %u programmes, %u service visits\r\n"
        L"Data: %s\r\nScreen %d dpi\r\n\r\nFree software under the GNU GPLv3.",
        kVersion, (unsigned)DbCount(dbFills), (unsigned)DbCount(dbVehicles), (unsigned)DbCount(dbProgrammes),
        (unsigned)DbCount(dbServiceVisits), kDataDirW, gDpi);
    MessageBoxW(gWin, msg, L"FuelLog", MB_OK | MB_ICONINFORMATION);
}

/* ---- the window ---- */

static void CreateMenuBar(HWND h)
{
    SHMENUBARINFO mbi;
    HMENU bar = CreateMenu(), menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, IDM_PALM, L"Use Palm backup...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, IDM_EXIT, L"Exit");
    AppendMenuW(bar, MF_STRING, IDM_ABOUT, L"About");
    AppendMenuW(bar, MF_POPUP, (UINT_PTR)menu, L"Menu");
    memset(&mbi, 0, sizeof mbi);
    mbi.cbSize = sizeof mbi;
    mbi.hwndParent = h;
    mbi.dwFlags = SHCMBF_HMENU;
    mbi.nToolBarId = (UINT)bar;
    mbi.hInstRes = gInst;
    if (SHCreateMenuBar(&mbi)) gMenuBar = mbi.hwndMB;
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
    CreateMenuBar(h);
    gVehicle = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 0, 0, 10, 10, h, (HMENU)IDC_VEHICLE, gInst, NULL);
    gList = CreateWindowW(WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | LVS_OWNERDATA,
        0, 0, 10, 10, h, (HMENU)IDC_LIST, gInst, NULL);
    SendMessageW(gList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
    AddColumn(0, 46, 0);
    AddColumn(1, 56, 1);
    AddColumn(2, 38, 1);
    AddColumn(3, 48, 1);
    AddColumn(4, 36, 1);
    gAvg = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 10, 10, h, (HMENU)IDC_AVG, gInst, NULL);
    gMonth = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT, 0, 0, 10, 10, h, (HMENU)IDC_MONTH, gInst, NULL);
    gEmpty = CreateWindowW(L"STATIC", L"No fills yet.\r\n\r\nTo bring your log from a Palm: on the Palm choose Export, Back up now, put its card in this device, "
        L"then Menu, Use Palm backup.", WS_CHILD | SS_LEFT, 0, 0, 10, 10, h, (HMENU)IDC_EMPTY, gInst, NULL);
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
    case WM_COMMAND:
        switch (LOWORD(w)) {
        case IDM_ABOUT: About(); return 0;
        case IDM_PALM: TakePalmSet(); return 0;
        case IDM_EXIT: SendMessageW(h, WM_CLOSE, 0, 0); return 0;
        case IDC_VEHICLE:
            if (HIWORD(w) == CBN_SELCHANGE) {
                int sel = (int)SendMessageW(gVehicle, CB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel <= vehMaxVehicles) { gPrefs.currentVehicle = gVehIds[sel]; Refresh(); }
            }
            return 0;
        }
        break;
    case WM_NOTIFY: {
        NMHDR *n = (NMHDR *)l;
        if (n->idFrom == IDC_LIST && n->code == LVN_GETDISPINFOW) {
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
        break;
    }
    case WM_CLOSE:
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
    icc.dwICC = ICC_LISTVIEW_CLASSES;
    InitCommonControlsEx(&icc);
    PrefsSetDefaults(&gPrefs);
    ReadFormats();
    e = OpenData();

    memset(&wc, 0, sizeof wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = Proc;
    wc.hInstance = inst;
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
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

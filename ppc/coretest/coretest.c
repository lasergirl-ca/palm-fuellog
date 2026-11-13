/* coretest: FuelLog's portable core on Windows Mobile. Runs the Palm
 * build's host test suites, compiled for ARM, one after another; shows one
 * line per suite and writes everything the suites printed beside itself as
 * coretest.txt. Zero failures everywhere means the record formats, the
 * maths, the dates, the reports and the sync rules behave on this device
 * exactly as on the Palm and the Mac. */
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "ct.h"

#undef printf

static char gOut[32768];
static unsigned gLen;

int ct_printf(const char *fmt, ...)
{
    va_list ap;
    int n;
    if (gLen >= sizeof gOut - 1) return 0;
    va_start(ap, fmt);
    n = _vsnprintf(gOut + gLen, sizeof gOut - 1 - gLen, fmt, ap);
    va_end(ap);
    if (n < 0) n = (int)(sizeof gOut - 1 - gLen);
    gLen += (unsigned)n;
    gOut[gLen] = 0;
    return n;
}

#define SUITE(name) int suite_##name(void);
SUITE(calc) SUITE(records) SUITE(units) SUITE(prefs) SUITE(text) SUITE(date)
SUITE(stats) SUITE(service) SUITE(report) SUITE(points) SUITE(sync) SUITE(csv)
#undef SUITE

static const struct { const char *name; int (*run)(void); } kSuites[] = {
    { "calc", suite_calc }, { "records", suite_records }, { "units", suite_units }, { "prefs", suite_prefs },
    { "text", suite_text }, { "date", suite_date }, { "stats", suite_stats }, { "service", suite_service },
    { "report", suite_report }, { "points", suite_points }, { "sync", suite_sync }, { "csv", suite_csv },
};

static void Save(void)
{
    wchar_t path[MAX_PATH], *slash;
    HANDLE f;
    DWORD n;
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0 || (slash = wcsrchr(path, L'\\')) == NULL) return;
    lstrcpyW(slash + 1, L"coretest.txt");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    WriteFile(f, gOut, gLen, &n, NULL);
    CloseHandle(f);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    static wchar_t text[1024];
    wchar_t line[64];
    unsigned i, bad = 0;
    (void)inst; (void)prev; (void)cmd; (void)show;
    lstrcpyW(text, L"FuelLog core on this device\r\n\r\n");
    for (i = 0; i < sizeof kSuites / sizeof kSuites[0]; i++) {
        int r;
        ct_printf("== %s\n", kSuites[i].name);
        r = kSuites[i].run();
        if (r) bad++;
        wsprintfW(line, L"%S: %s\r\n", kSuites[i].name, r ? L"FAILED" : L"ok");
        lstrcatW(text, line);
    }
    wsprintfW(line, L"\r\n%u of %u suites passed", (unsigned)(sizeof kSuites / sizeof kSuites[0]) - bad,
        (unsigned)(sizeof kSuites / sizeof kSuites[0]));
    lstrcatW(text, line);
    Save();
    MessageBoxW(NULL, text, L"FuelLog", MB_OK | (bad ? MB_ICONEXCLAMATION : MB_ICONINFORMATION));
    return (int)bad;
}

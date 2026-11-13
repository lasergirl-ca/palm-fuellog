/* hello: the Pocket PC spike's first question, whether what arm-mingw32ce
 * builds for FuelLog runs on the device. It shows the Windows CE version,
 * the screen size and the free memory, and writes the same text beside
 * itself as hello.txt. */
#include <windows.h>

static void Describe(wchar_t *out)
{
    OSVERSIONINFOW v;
    MEMORYSTATUS ms;
    v.dwOSVersionInfoSize = sizeof v;
    GetVersionExW(&v);
    ms.dwLength = sizeof ms;
    GlobalMemoryStatus(&ms);
    wsprintfW(out, L"FuelLog for Pocket PC: hello\r\n\r\nWindows CE %u.%u build %u\r\nScreen %d x %d\r\nMemory %u KB, %u KB free\r\n",
        (unsigned)v.dwMajorVersion, (unsigned)v.dwMinorVersion, (unsigned)v.dwBuildNumber,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        (unsigned)(ms.dwTotalPhys / 1024), (unsigned)(ms.dwAvailPhys / 1024));
}

static void Save(const wchar_t *text)
{
    wchar_t path[MAX_PATH], *slash;
    HANDLE f;
    DWORD n;
    const WCHAR bom = 0xFEFF;
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0 || (slash = wcsrchr(path, L'\\')) == NULL) return;
    lstrcpyW(slash + 1, L"hello.txt");
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) return;
    WriteFile(f, &bom, sizeof bom, &n, NULL);
    WriteFile(f, text, lstrlenW(text) * sizeof(WCHAR), &n, NULL);
    CloseHandle(f);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPWSTR cmd, int show)
{
    wchar_t text[512];
    (void)inst; (void)prev; (void)cmd; (void)show;
    Describe(text);
    Save(text);
    MessageBoxW(NULL, text, L"FuelLog", MB_OK | MB_ICONINFORMATION);
    return 0;
}

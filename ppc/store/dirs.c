/* Folders and whole files (dirs.h). */
#include "dirs.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32_WCE
#include <windows.h>

static void Wide(const char *s, WCHAR *w, int cap)
{
    if (MultiByteToWideChar(CP_ACP, 0, s, -1, w, cap) == 0) w[0] = 0;
}

int DirMake(const char *path)
{
    WCHAR w[MAX_PATH];
    DWORD a;
    Wide(path, w, MAX_PATH);
    CreateDirectoryW(w, NULL);
    a = GetFileAttributesW(w);
    return a != 0xFFFFFFFF && (a & FILE_ATTRIBUTE_DIRECTORY);
}

int FileExists(const char *path)
{
    WCHAR w[MAX_PATH];
    Wide(path, w, MAX_PATH);
    return GetFileAttributesW(w) != 0xFFFFFFFF;
}

UInt32 FileSize(const char *path)
{
    WCHAR w[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    Wide(path, w, MAX_PATH);
    h = FindFirstFileW(w, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;
    FindClose(h);
    return fd.nFileSizeLow;
}

int FileCopy(const char *from, const char *to)
{
    WCHAR a[MAX_PATH], b[MAX_PATH];
    Wide(from, a, MAX_PATH);
    Wide(to, b, MAX_PATH);
    return CopyFileW(a, b, FALSE) != 0;
}

/* Calls back for each entry; the name in the device's code page. */
static void Each(const char *dir, void (*fn)(const char *name, int folder, void *ctx), void *ctx)
{
    WCHAR w[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    char path[300], name[MAX_PATH];
    PathJoin(path, dir, "*");
    Wide(path, w, MAX_PATH);
    h = FindFirstFileW(w, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (WideCharToMultiByte(CP_ACP, 0, fd.cFileName, -1, name, sizeof name, NULL, NULL) == 0) continue;
        if (name[0] == '.') continue;
        fn(name, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, ctx);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

static int RemoveFile(const char *path)
{
    WCHAR w[MAX_PATH];
    Wide(path, w, MAX_PATH);
    return DeleteFileW(w) != 0;
}

static int RemoveDir(const char *path)
{
    WCHAR w[MAX_PATH];
    Wide(path, w, MAX_PATH);
    return RemoveDirectoryW(w) != 0;
}

#else
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

int DirMake(const char *path)
{
    struct stat st;
    mkdir(path, 0755);
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int FileExists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

UInt32 FileSize(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 ? (UInt32)st.st_size : 0;
}

int FileCopy(const char *from, const char *to)
{
    FILE *a = fopen(from, "rb"), *b;
    char buf[4096];
    size_t n;
    int ok = 1;
    if (!a) return 0;
    b = fopen(to, "wb");
    if (!b) { fclose(a); return 0; }
    while ((n = fread(buf, 1, sizeof buf, a)) > 0) if (fwrite(buf, 1, n, b) != n) { ok = 0; break; }
    fclose(a);
    if (fclose(b) != 0) ok = 0;
    return ok;
}

static void Each(const char *dir, void (*fn)(const char *name, int folder, void *ctx), void *ctx)
{
    DIR *d = opendir(dir);
    struct dirent *e;
    char path[300];
    struct stat st;
    if (!d) return;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        PathJoin(path, dir, e->d_name);
        if (stat(path, &st) != 0) continue;
        fn(e->d_name, S_ISDIR(st.st_mode), ctx);
    }
    closedir(d);
}

static int RemoveFile(const char *path) { return unlink(path) == 0; }
static int RemoveDir(const char *path) { return rmdir(path) == 0; }
#endif

void PathJoin(char *out, const char *dir, const char *name)
{
    size_t n = strlen(dir);
    if (n > 250) n = 250;
    memcpy(out, dir, n);
    out[n] = 0;
    if (n && out[n - 1] != dirSep[0]) strcat(out, dirSep);
    strncat(out, name, 299 - strlen(out));
}

typedef struct {
    int folders;
    const char *ext;
    char (*names)[dirNameLen];
    UInt16 count, max;
} ListCtx;

static int EndsWith(const char *s, const char *ext)
{
    size_t a = strlen(s), b = strlen(ext), i;
    if (b > a) return 0;
    for (i = 0; i < b; i++) {
        char x = s[a - b + i], y = ext[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return 1;
}

static void ListOne(const char *name, int folder, void *p)
{
    ListCtx *c = (ListCtx *)p;
    UInt16 j;
    if (folder != c->folders || strlen(name) >= dirNameLen) return;
    if (!folder && c->ext && !EndsWith(name, c->ext)) return;
    /* descending: the newest dated name first */
    if (c->count == c->max && strcmp(name, c->names[c->count - 1]) < 0) return;
    j = c->count < c->max ? c->count++ : (UInt16)(c->count - 1);
    while (j > 0 && strcmp(c->names[j - 1], name) < 0) { memcpy(c->names[j], c->names[j - 1], dirNameLen); j--; }
    strcpy(c->names[j], name);
}

UInt16 DirList(const char *dir, int folders, const char *ext, char names[][dirNameLen], UInt16 max)
{
    ListCtx c;
    c.folders = folders; c.ext = ext; c.names = names; c.count = 0; c.max = max;
    if (max) Each(dir, ListOne, &c);
    return c.count;
}

static void RemoveOne(const char *name, int folder, void *p)
{
    char path[300];
    PathJoin(path, (const char *)p, name);
    if (!folder) RemoveFile(path);
}

int DirRemoveTree(const char *dir)
{
    Each(dir, RemoveOne, (void *)dir);
    return RemoveDir(dir);
}

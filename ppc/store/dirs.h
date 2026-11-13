/* Folders and whole files: what export, backup and restore need beyond one
 * .pdb file. Windows CE's wide-character calls on the device, POSIX on the
 * host, so the callers are host-tested. Paths are plain char in the
 * device's code page, absolute on the device. */
#ifndef DIRS_H
#define DIRS_H
#include "../../src/ptypes.h"

#define dirNameLen 32
#ifdef _WIN32_WCE
#define dirSep "\\"
#else
#define dirSep "/"
#endif

int    DirMake(const char *path);              /* 1 when the folder exists afterwards */
/* Names in a folder (sub-folders, or files ending in ext when ext is set), sorted descending. */
UInt16 DirList(const char *dir, int folders, const char *ext, char names[][dirNameLen], UInt16 max);
int    DirRemoveTree(const char *dir);         /* its files, then itself; 1 on success */
int    FileExists(const char *path);
UInt32 FileSize(const char *path);
int    FileCopy(const char *from, const char *to);   /* 1 on success; replaces to */
void   PathJoin(char *out, const char *dir, const char *name);   /* out holds 300 */

#endif

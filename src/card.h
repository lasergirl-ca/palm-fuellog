/* The SD card: finding a volume, the FuelLog folders, buffered writing
 * through an OutStream, byte reading, and folder listings. Device only. */
#ifndef CARD_H
#define CARD_H
#include <PalmOS.h>
#include <VFSMgr.h>
#include "outstream.h"
#include "sections.h"

#define cardRoot    "/PALM/Programs/FuelLog"
#define cardExport  "/PALM/Programs/FuelLog/Export"
#define cardBackup  "/PALM/Programs/FuelLog/Backup"
#define cardImport  "/PALM/Programs/FuelLog/Import"
#define cardNameLen 32
#define cardBufSize 32768UL

Err  CardFind(UInt16 *vol) IO_SEC;                 /* the first mounted card */
Err  CardEnsureDirs(UInt16 vol) IO_SEC;
/* "FuelLog-20261024" plus the extension, in buf (40). */
void CardExportName(char *buf, const char *ext) IO_SEC;

typedef struct {
    OutStream out;
    FileRef f;
    UInt8 *buf;
    UInt32 len;
    Err err;
} CardOut;
Err  CardOutOpen(CardOut *c, UInt16 vol, const char *path) IO_SEC;
Err  CardOutClose(CardOut *c) IO_SEC;              /* flushes; returns the first error */

typedef struct {
    FileRef f;
    UInt8 *buf;
    UInt32 len, pos;
    Boolean eof;
} CardIn;
Err   CardInOpen(CardIn *c, UInt16 vol, const char *path) IO_SEC;
Int16 CardInGetc(CardIn *c) IO_SEC;                /* 0..255 or -1 */
void  CardInClose(CardIn *c) IO_SEC;

/* Names in a folder (files or sub-folders), sorted descending. Returns the count. */
UInt16 CardList(UInt16 vol, const char *dir, Boolean folders, char names[][cardNameLen], UInt16 max) IO_SEC;
Err    CardDeleteFolder(UInt16 vol, const char *dir) IO_SEC;   /* its files, then itself */
UInt32 CardFileSize(UInt16 vol, const char *path) IO_SEC;

#endif

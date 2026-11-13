/* Import of fills from CSV, XLSX and XLS, ported from the Palm build's
 * src/import.c: the same two passes (scan and show, then write with the
 * merge rule), the same vehicle and programme resolution and the same
 * errors, reading a file instead of the card. The readers (csv.c,
 * xlsxread.c, xlsread.c) are the shared core. Below the shims the code is
 * the Palm's. */
#include "importfile.h"
#include "pgmedit.h"
#include "vehedit.h"
#include "svcview.h"
#include "../store/db.h"
#include "../store/dirs.h"
#include "../../src/csv.h"
#include "../../src/xlsxread.h"
#include "../../src/xlsread.h"
#include "../../src/inflate.h"
#include "../../src/units.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32_WCE
#include <unistd.h>
#else
#include <unistd.h>
#endif

/* ---- shims: the Palm calls the ported code makes ---- */
typedef int Boolean;
typedef Int32 Err;
#define true 1
#define false 0
#define errNone 0
#define memErrNotEnoughSpace 0x0102
#define vfsErrFileNotFound 0x2A0A
#define dmErrCorruptDatabase 0x0218
#define StrCopy strcpy
#define StrCat strcat
#define StrLen(s) ((UInt32)strlen(s))
#define StrNCopy strncpy
#define MemSet(p, n, v) memset((p), (v), (n))
#define MemPtrNew malloc
#define MemPtrFree free
#define PgmGetAt DbGetProgramme
#define PgmGet(id, p) DbProgrammeById((id), (p))
static FuelPrefs gPrefs;
static char gImportPath[300];
static void StrIToA(char *b, Int32 n) { TextFixed(b, n, 0, '.', 0); }
/* Palm's StrNCat: n is the most the whole string may hold, terminator included. */
static void StrNCat(char *d, const char *s, UInt32 n)
{
    UInt32 l = strlen(d);
    while (*s && l + 1 < n) d[l++] = *s++;
    d[l] = 0;
}
static int StrCaselessCompare(const char *a, const char *b)
{
    for (;; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y || !x) return x - y;
    }
}

void ImportSetup(const FuelPrefs *p) { gPrefs = *p; }

/* ---- the Palm's code ---- */

ImportSummary gImport;
static FILE *gIn;
static Int16 Getc(void *ctx) { int c = fgetc((FILE *)ctx); return (Int16)(c == EOF ? -1 : c); }
static char *gRecord;                     /* csvMaxRecord bytes, heap during a pass */
static char *gFields[csvMaxFields];

/* ---- the row source: a CSV through the card reader, an XLSX through the reader
 * with its parts inflated to temporary files in Import/, or a BIFF8 XLS ---- */
static Boolean gIsXlsx, gIsXls;
static XlsReader *gXs;                    /* heap while an .xls is open */
static XlsxReader gXr;
static FILE *gXlsxFile, *gTmpW, *gTmpR[3];

static char gTmpDir[300];
static const char *TmpPath(UInt8 which) { static char p[320]; char n[8]; StrCopy(n, ".part0"); n[5] = (char)('0' + which); PathJoin(p, gTmpDir, n); return p; }

static Int32 XrReadAt(void *ctx, UInt32 pos, UInt8 *buf, UInt32 len)
{
    (void)ctx;
    if (fseek(gXlsxFile, (long)pos, SEEK_SET) != 0) return -1;
    return (Int32)fread(buf, 1, len, gXlsxFile);
}
static Int32 XrTmpBegin(void *ctx, UInt8 which)
{
    (void)ctx;
    if (gTmpR[which]) { fclose(gTmpR[which]); gTmpR[which] = 0; }
    gTmpW = fopen(TmpPath(which), "wb");
    return gTmpW ? 0 : -1;
}
static Int32 XrTmpWrite(void *ctx, const UInt8 *buf, UInt32 len)
{
    (void)ctx;
    return fwrite(buf, 1, len, gTmpW) == len ? 0 : -1;
}
static Int32 XrTmpEnd(void *ctx) { (void)ctx; if (gTmpW) fclose(gTmpW); gTmpW = 0; return 0; }
static Int32 XrTmpReadAt(void *ctx, UInt8 which, UInt32 pos, UInt8 *buf, UInt32 len)
{
    (void)ctx;
    if (!gTmpR[which] && (gTmpR[which] = fopen(TmpPath(which), "rb")) == NULL) return -1;
    if (fseek(gTmpR[which], (long)pos, SEEK_SET) != 0) return 0;
    return (Int32)fread(buf, 1, len, gTmpR[which]);
}

static void SrcClose(void)
{
    UInt8 i;
    if (gIsXlsx) {
        if (gXlsxFile) { fclose(gXlsxFile); gXlsxFile = 0; }
        if (gTmpW) { fclose(gTmpW); gTmpW = 0; }
        for (i = 0; i < 3; i++) { if (gTmpR[i]) { fclose(gTmpR[i]); gTmpR[i] = 0; } unlink(TmpPath(i)); }
        if (gXr.window) MemPtrFree(gXr.window); if (gXr.inBuf) MemPtrFree(gXr.inBuf);
        if (gXr.crcTable) MemPtrFree(gXr.crcTable); if (gXr.sstOffsets) MemPtrFree(gXr.sstOffsets);
        MemSet(&gXr, sizeof gXr, 0);
    } else if (gIsXls) {
        if (gXlsxFile) { fclose(gXlsxFile); gXlsxFile = 0; }
        if (gTmpW) { fclose(gTmpW); gTmpW = 0; }
        for (i = 0; i < 3; i++) { if (gTmpR[i]) { fclose(gTmpR[i]); gTmpR[i] = 0; } unlink(TmpPath(i)); }
        if (gXs) { if (gXs->sstOffsets) MemPtrFree(gXs->sstOffsets); MemPtrFree(gXs); gXs = NULL; }
    } else if (gIn) { fclose(gIn); gIn = 0; }
}

static Err SrcOpen(const char *path)
{
    UInt16 l = StrLen(path);
    gIsXlsx = l > 5 && StrCaselessCompare(path + l - 5, ".xlsx") == 0;
    gIsXls = !gIsXlsx && l > 4 && StrCaselessCompare(path + l - 4, ".xls") == 0;
    if (gIsXls) {
        if ((gXlsxFile = fopen(path, "rb")) == NULL) return vfsErrFileNotFound;
        gXs = MemPtrNew(sizeof *gXs);
        if (!gXs) { SrcClose(); return memErrNotEnoughSpace; }
        MemSet(gXs, sizeof *gXs, 0);
        gXs->fileSize = FileSize(path);
        gXs->readAt = XrReadAt; gXs->tmpBegin = XrTmpBegin; gXs->tmpWrite = XrTmpWrite; gXs->tmpEnd = XrTmpEnd; gXs->tmpReadAt = XrTmpReadAt;
        gXs->tmpPart = xrTmpSst;
        gXs->sstOffsets = MemPtrNew(4096 * sizeof(UInt32)); gXs->sstCap = 4096;
        if (!gXs->sstOffsets) { SrcClose(); return memErrNotEnoughSpace; }
        if (XlsrOpen(gXs) != 0) { SrcClose(); return dmErrCorruptDatabase; }
        return errNone;
    }
    if (!gIsXlsx) return (gIn = fopen(path, "rb")) != NULL ? errNone : vfsErrFileNotFound;
    MemSet(&gXr, sizeof gXr, 0);
    if ((gXlsxFile = fopen(path, "rb")) == NULL) return vfsErrFileNotFound;
    gXr.fileSize = FileSize(path);
    gXr.readAt = XrReadAt; gXr.tmpBegin = XrTmpBegin; gXr.tmpWrite = XrTmpWrite; gXr.tmpEnd = XrTmpEnd; gXr.tmpReadAt = XrTmpReadAt;
    gXr.window = MemPtrNew(inflateWindowSize); gXr.inBuf = MemPtrNew(4096); gXr.inSize = 4096;
    gXr.crcTable = MemPtrNew(256 * sizeof(UInt32)); gXr.sstOffsets = MemPtrNew(4096 * sizeof(UInt32)); gXr.sstCap = 4096;
    if (!gXr.window || !gXr.inBuf || !gXr.crcTable || !gXr.sstOffsets) { SrcClose(); return memErrNotEnoughSpace; }
    if (XlsxrOpen(&gXr) != 0) { SrcClose(); return dmErrCorruptDatabase; }
    return errNone;
}

/* Next row into gFields (count), 0 for a blank row, -1 at the end. */
static Int32 SrcNext(UInt32 *line)
{
    if (gIsXlsx) {
        Int32 n = XlsxrNextRow(&gXr, gFields, csvMaxFields, gRecord, csvMaxRecord);
        *line = gXr.rowNo;
        return n;
    } else if (gIsXls) {
        Int32 n = XlsrNextRow(gXs, gFields, csvMaxFields, gRecord, csvMaxRecord);
        *line = gXs->rowNo;
        return n;
    } else {
        Int32 len = CsvNextRecord(Getc, gIn, gRecord, csvMaxRecord, line);
        if (len <= 0) return len;
        return CsvSplit(gRecord, gFields, csvMaxFields);
    }
}
static CsvMap gMap;
static FillRecord gImpFill, gImpScan;
static VehicleRecord gImpVeh;
static ProgrammeRecord gImpPgm;
static UInt8 gPgmIdFor[csvMaxPgms];       /* header programme index -> programme id (0 = none yet) */
static UInt8 gKbId;
static UInt8 gKbOverride;                 /* programme picked on the preview for the KB Points column, 0 = the alias */
static char gLastVehName[impMaxNewVeh + 12][24];   /* vehicles seen in the file, for odometer checks */
static UInt32 gLastOdo[impMaxNewVeh + 12];
static UInt32 gLastLine[impMaxNewVeh + 12];
static UInt8 gSeenCount;
static char gSeenNew[12][24];            /* unknown vehicle names beyond the four listed */
static UInt8 gSeenNewCount;



static Boolean NameEq(const char *a, const char *b) { return StrCaselessCompare(a, b) == 0; }

static Int16 VehIndexByName(const char *name)
{
    UInt16 n = DbCount(dbVehicles), i;
    for (i = 0; i < n; i++) if (DbGetVehicle(i, &gImpVeh) == errNone && NameEq(gImpVeh.name, name)) return (Int16)i;
    return -1;
}

/* A programme by name, or by one of its comma-separated aliases. */
static UInt8 PgmIdByName(const char *name)
{
    UInt16 n = DbCount(dbProgrammes), i;
    for (i = 0; i < n; i++) {
        const char *a;
        if (PgmGetAt(i, &gImpPgm) != errNone) continue;
        if (NameEq(gImpPgm.name, name)) return gImpPgm.id;
        a = gImpPgm.aliases;
        while (*a) {
            char one[pgmAliasLen]; UInt16 k = 0;
            while (*a == ' ') a++;
            while (*a && *a != ',' && k < pgmAliasLen - 1) one[k++] = *a++;
            while (k && one[k - 1] == ' ') k--;
            one[k] = 0;
            if (*a == ',') a++;
            if (k && NameEq(one, name)) return gImpPgm.id;
        }
    }
    return 0;
}

static void AddError(UInt32 line, const char *why)
{
    char *b;
    gImport.errors++;
    if (gImport.errors > impMaxErrors) return;
    b = gImport.errorLines[gImport.errors - 1];
    StrCopy(b, "Line "); StrIToA(b + 5, (Int32)line); StrCat(b, ": ");
    StrNCat(b, why, 43);   /* StrNCat takes the final length */
}

static Boolean IsDuplicate(const FillRecord *f)
{
    UInt16 n = DbCount(dbFills), i;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gImpScan) != errNone) continue;
        if (gImpScan.date > f->date) break;              /* sorted by date */
        if (gImpScan.date == f->date && gImpScan.vehicle == f->vehicle && gImpScan.odometer == f->odometer) return true;
    }
    return false;
}

/* Resolve the vehicle name to an id; in the write pass unknown names are created. */
static UInt8 ResolveVehicle(const char *name, Boolean write)
{
    Int16 i;
    UInt8 k;
    if (!name[0]) return SvcViewVehicle(&gPrefs);
    i = VehIndexByName(name);
    if (i >= 0 && DbGetVehicle((UInt16)i, &gImpVeh) == errNone) return gImpVeh.id;
    /* count each unknown name once: the first four are listed, the rest remembered in gLastVehName */
    for (k = 0; k < gImport.newVehicles && k < impMaxNewVeh; k++) if (NameEq(gImport.newVehicleNames[k], name)) break;
    if (k == gImport.newVehicles || k >= impMaxNewVeh) {
        Boolean seen = false;
        if (k >= impMaxNewVeh) { UInt8 j; for (j = 0; j < gSeenNewCount; j++) if (NameEq(gSeenNew[j], name)) seen = true; }
        if (!seen) {
            if (gImport.newVehicles < impMaxNewVeh) StrCopy(gImport.newVehicleNames[gImport.newVehicles], name);
            else if (gSeenNewCount < 12) StrCopy(gSeenNew[gSeenNewCount++], name);
            gImport.newVehicles++;
        }
    }
    if (!write) return 0;
    {
        UInt8 buf[vehRecordSize + recTrailerSize];
        MemSet(&gImpVeh, sizeof gImpVeh, 0);
        gImpVeh.id = VehNextId();
        if (gImpVeh.id == 0) return 0;
        StrNCopy(gImpVeh.name, name, vehNameLen - 1); gImpVeh.name[vehNameLen - 1] = 0;
        gImpVeh.defaultCurrency = vehCurrencyUsePrefs; gImpVeh.tankSize = 60000;
        gImpVeh.odoUnit = gMap.distUnit == unitDistMiles ? vehOdoMiles : vehOdoKm;
        if (DbAdd(dbVehicles, buf, VehiclePack(&gImpVeh, buf), NULL)) return 0;
        return gImpVeh.id;
    }
}

static UInt8 ResolveProgramme(const char *name, Boolean write)
{
    UInt8 id = PgmIdByName(name);
    if (id || !write) { if (!id) gImport.newProgrammes++; return id; }
    {
        static UInt8 buf[pgmFixedSize + pgmAliasLen + 4 + recTrailerSize];
        MemSet(&gImpPgm, sizeof gImpPgm, 0);
        gImpPgm.id = PgmNextId();
        if (gImpPgm.id == 0) return 0;
        StrNCopy(gImpPgm.name, name, pgmNameLen - 1); gImpPgm.name[pgmNameLen - 1] = 0;
        gImpPgm.premiumMult = 100; gImpPgm.currency = gPrefs.homeCurrency;
        if (DbAdd(dbProgrammes, buf, ProgrammePack(&gImpPgm, buf), NULL)) return 0;
        return gImpPgm.id;
    }
}

static void ResolveProgrammes(Boolean write)
{
    UInt8 j;
    gImport.newProgrammes = 0;
    for (j = 0; j < gMap.pgmCount; j++) gPgmIdFor[j] = ResolveProgramme(gMap.pgmNames[j], write);
    gKbId = 0;
    if (gMap.kbPoints >= 0 && gKbOverride) { gKbId = gKbOverride; PgmGet(gKbId, &gImpPgm); StrCopy(gImport.kbTarget, gImpPgm.name); gImport.kbPoints = true; }
    else if (gMap.kbPoints >= 0) {
        gKbId = PgmIdByName("KB Points");
        if (!gKbId) gKbId = PgmIdByName("76 / Phillips 66");
        if (gKbId) { PgmGet(gKbId, &gImpPgm); StrCopy(gImport.kbTarget, gImpPgm.name); }
        else { StrCopy(gImport.kbTarget, "new KB Points programme");   /* 23 characters: kbTarget holds 24 */ if (write) gKbId = ResolveProgramme("KB Points", true); else gImport.newProgrammes++; }
        gImport.kbPoints = true;
    }
}

/* One pass over the file. write = false counts; write = true adds. */
static Err Pass(Boolean write)
{
    char veh[24], why[40];
    UInt32 line = 0, recLine;
    Int32 len;
    Int16 n;
    Err err;
    gRecord = MemPtrNew(csvMaxRecord);
    if (!gRecord) return memErrNotEnoughSpace;
    err = SrcOpen(gImportPath);
    if (err) { MemPtrFree(gRecord); gRecord = NULL; return err; }
    gSeenCount = 0; gSeenNewCount = 0;
    /* header */
    do { len = SrcNext(&line); n = (Int16)len; } while (len == 0);
    if (len < 0) { SrcClose(); MemPtrFree(gRecord); gRecord = NULL; gImport.badHeader = 1; return errNone; }
    if (CsvMapHeader(gFields, n, &gMap) == 0) { SrcClose(); MemPtrFree(gRecord); gRecord = NULL; gImport.badHeader = 1; return errNone; }
    if (gIsXlsx) XlsxrSetDateColumns(&gXr, gMap.date, gMap.time);
    if (gIsXls) XlsrSetDateColumns(gXs, gMap.date, gMap.time);
    ResolveProgrammes(write);
    for (;;) {
        UInt8 k, vid;
        recLine = line + 1;
        len = SrcNext(&line);
        if (gIsXlsx || gIsXls) recLine = line;
        if (len < 0) break;
        if (len == 0) continue;
        n = (Int16)len;
        if (!write) gImport.rows++;
        if (CsvRowToFill(&gMap, gFields, n, &gImpFill, veh, why) != 0) { if (!write) AddError(recLine, why); continue; }
        /* odometer must not fall for the same vehicle within the file: checked in both passes,
         * so the write skips exactly the rows the preview listed as errors */
        if (gImpFill.odometer) {
            for (k = 0; k < gSeenCount; k++) if (NameEq(gLastVehName[k], veh)) break;
            if (k == gSeenCount && gSeenCount < impMaxNewVeh + 12) { StrCopy(gLastVehName[k], veh); gLastOdo[k] = 0; gLastLine[k] = 0; gSeenCount++; }
            if (k < gSeenCount) {
                if (gLastOdo[k] && gImpFill.odometer < gLastOdo[k]) {
                    char w[40]; StrCopy(w, "odometer lower than line "); StrIToA(w + StrLen(w), (Int32)gLastLine[k]);
                    if (!write) AddError(recLine, w);
                    continue;
                }
                gLastOdo[k] = gImpFill.odometer; gLastLine[k] = recLine;
            }
        }
        vid = ResolveVehicle(veh, write);
        if (write && vid == 0) { continue; }
        gImpFill.vehicle = vid;
        /* programme indexes to ids */
        {
            UInt8 kept = 0;
            for (k = 0; k < gImpFill.pointCount; k++) {
                UInt8 ref = gImpFill.points[k].programme, id = ref == 255 ? gKbId : (ref >= 1 && ref <= gMap.pgmCount ? gPgmIdFor[ref - 1] : 0);
                if (!id) continue;
                gImpFill.points[kept] = gImpFill.points[k]; gImpFill.points[kept].programme = id; kept++;
            }
            gImpFill.pointCount = kept;
        }
        if (write) {
            if (IsDuplicate(&gImpFill)) { gImport.skipped++; continue; }
            gImpFill.clockShift = 0;   /* dates read from a file are real; a Windows Mobile clock has no shift */
            if (DbAddFill(&gImpFill, NULL) == errNone) gImport.written++;
        } else {
            if (vid && IsDuplicate(&gImpFill)) gImport.duplicates++; else gImport.ready++;
        }
    }
    SrcClose();
    MemPtrFree(gRecord); gRecord = NULL;
    return errNone;
}

Err ImportScan(const char *path, const char *tmpDir)
{
    const char *name = path + strlen(path);
    while (name > path && name[-1] != '/' && name[-1] != '\\') name--;
    gKbOverride = 0;
    MemSet(&gImport, sizeof gImport, 0);
    StrNCopy(gImport.file, name, cardNameLen - 1);
    strncpy(gImportPath, path, sizeof gImportPath - 1); gImportPath[sizeof gImportPath - 1] = 0;
    strncpy(gTmpDir, tmpDir, sizeof gTmpDir - 1); gTmpDir[sizeof gTmpDir - 1] = 0;
    return Pass(false);
}

Err ImportWrite(void)
{
    gImport.written = gImport.skipped = 0;
    gImport.newVehicles = 0;
    return Pass(true);
}

/* The programme the KB Points column counts for, picked on the preview (0 = by its alias), and the choices. */
void ImportSetKbTarget(UInt8 id)
{
    ProgrammeRecord p;
    gKbOverride = id;
    if (id && DbProgrammeById(id, &p)) StrCopy(gImport.kbTarget, p.name);
}

UInt8 ImportKbDefault(void)
{
    UInt8 id = gKbOverride ? gKbOverride : PgmIdByName("KB Points");
    return id ? id : PgmIdByName("76 / Phillips 66");
}


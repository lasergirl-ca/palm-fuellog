#include "vfstest.h"
#include <VFSMgr.h>

#define kDir1 "/PALM"
#define kDir2 "/PALM/Programs"
#define kDir3 "/PALM/Programs/FuelLog"
#define kDirExport "/PALM/Programs/FuelLog/Export"
#define kDirBackup "/PALM/Programs/FuelLog/Backup"
#define kDirImport "/PALM/Programs/FuelLog/Import"
#define kTestFile  "/PALM/Programs/FuelLog/Export/sp4-200kb.bin"
#define kPdbFile   "/PALM/Programs/FuelLog/Backup/FuelSP4-ZFlg.pdb"
#define kDbName    "FuelSP4-ZFlg"
#define kCreator   'ZFlg'
#define kDbType    'SP4t'
#define kRecords   1000
#define kRecSize   64
#define kChunk     4096
#define kTotal     (200UL * 1024UL)

typedef struct {
    char (*lines)[vfsTestLineLen];
    UInt16 n;
} Log;

/* Section placement: see sections.h. */
static void LogLine(Log *log, const char *a, Int32 num, const char *b) SEC2;
static Int32 Ms(UInt32 startTicks) SEC2;
static UInt8 NextByte(void) SEC2;
static Err MakeDirs(UInt16 vol) SEC2;
static Err WriteAndReadBack(UInt16 vol, Log *log) SEC2;
static UInt32 ChecksumDb(DmOpenRef db, UInt16 *count) SEC2;
static Err BuildDb(UInt32 *sumOut, Log *log) SEC2;
static Err ExportImport(UInt16 vol, UInt32 sumBefore, Log *log) SEC2;

static void LogLine(Log *log, const char *a, Int32 num, const char *b)
{
    char *s;
    if (log->n >= vfsTestMaxLines) return;
    s = log->lines[log->n++];
    StrNCopy(s, a, vfsTestLineLen - 1);
    s[vfsTestLineLen - 1] = 0;
    if (num >= 0) {
        StrIToA(s + StrLen(s), num);
    }
    if (b) {
        StrNCat(s, b, vfsTestLineLen - 1);
    }
}

static Int32 Ms(UInt32 startTicks)
{
    UInt32 ticks = TimGetTicks() - startTicks;
    return (Int32)(ticks * 1000UL / SysTicksPerSecond());
}

/* Pseudo-random byte stream, the same on every run, so read-back and the
 * database round trip can be checked by a running sum. */
static UInt32 gSeed;
static UInt8 NextByte(void)
{
    gSeed = gSeed * 1103515245UL + 12345UL;
    return (UInt8)(gSeed >> 16);
}

static Err MakeDirs(UInt16 vol)
{
    static const char *const dirs[] = { kDir1, kDir2, kDir3, kDirExport, kDirBackup, kDirImport };
    UInt16 i;
    for (i = 0; i < 6; i++) {
        Err err = VFSDirCreate(vol, dirs[i]);
        if (err != errNone && err != vfsErrFileAlreadyExists)
            return err;
    }
    return errNone;
}

static Err WriteAndReadBack(UInt16 vol, Log *log)
{
    FileRef f;
    Err err;
    UInt8 *buf;
    UInt32 done, got, i, start, sum = 0, sum2 = 0, size = 0;
    Int32 ms;

    buf = MemPtrNew(kChunk);
    if (!buf) return memErrNotEnoughSpace;

    VFSFileDelete(vol, kTestFile);
    err = VFSFileOpen(vol, kTestFile, vfsModeWrite | vfsModeCreate | vfsModeTruncate, &f);
    if (err) { MemPtrFree(buf); return err; }
    gSeed = 1;
    start = TimGetTicks();
    for (done = 0; done < kTotal && !err; done += kChunk) {
        UInt32 wrote;
        for (i = 0; i < kChunk; i++) { buf[i] = NextByte(); sum += buf[i]; }
        err = VFSFileWrite(f, kChunk, buf, &wrote);
        if (!err && wrote != kChunk) err = vfsErrFileGeneric;
    }
    VFSFileClose(f);
    ms = Ms(start);
    if (err) { MemPtrFree(buf); return err; }
    LogLine(log, "Write 200 KB: ", ms, " ms");
    if (ms > 0) LogLine(log, "  = ", (Int32)(kTotal / 1024UL * 1000UL / (UInt32)ms), " KB/s in 4 KB chunks");

    err = VFSFileOpen(vol, kTestFile, vfsModeRead, &f);
    if (err) { MemPtrFree(buf); return err; }
    VFSFileSize(f, &size);
    start = TimGetTicks();
    for (done = 0; done < kTotal && !err; done += kChunk) {
        err = VFSFileRead(f, kChunk, buf, &got);
        if (!err && got != kChunk) err = vfsErrFileEOF;
        for (i = 0; i < kChunk; i++) sum2 += buf[i];
    }
    VFSFileClose(f);
    ms = Ms(start);
    MemPtrFree(buf);
    if (err) return err;
    LogLine(log, "Read 200 KB: ", ms, (sum == sum2 && size == kTotal) ? " ms, verified" : " ms, MISMATCH");
    VFSFileDelete(vol, kTestFile);
    return errNone;
}

static UInt32 ChecksumDb(DmOpenRef db, UInt16 *count)
{
    UInt16 n = DmNumRecords(db), i;
    UInt32 sum = 0;
    for (i = 0; i < n; i++) {
        MemHandle h = DmQueryRecord(db, i);
        if (h) {
            UInt8 *p = MemHandleLock(h);
            UInt32 len = MemHandleSize(h), k;
            for (k = 0; k < len; k++) sum = sum * 31UL + p[k];
            MemHandleUnlock(h);
        }
    }
    *count = n;
    return sum;
}

static Err BuildDb(UInt32 *sumOut, Log *log)
{
    Err err;
    LocalID id;
    DmOpenRef db;
    UInt16 i, attrs, count;
    UInt32 start;
    UInt8 rec[kRecSize];

    id = DmFindDatabase(0, kDbName);
    if (id) DmDeleteDatabase(0, id);
    err = DmCreateDatabase(0, kDbName, kCreator, kDbType, false);
    if (err) return err;
    id = DmFindDatabase(0, kDbName);
    DmDatabaseInfo(0, id, NULL, &attrs, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
    attrs |= dmHdrAttrBackup;
    DmSetDatabaseInfo(0, id, NULL, &attrs, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);

    db = DmOpenDatabase(0, id, dmModeReadWrite);
    if (!db) return DmGetLastErr();
    gSeed = 7;
    start = TimGetTicks();
    for (i = 0; i < kRecords && !err; i++) {
        UInt16 at = dmMaxRecordIndex;
        MemHandle h = DmNewRecord(db, &at, kRecSize);
        UInt16 k;
        if (!h) { err = DmGetLastErr(); break; }
        for (k = 0; k < kRecSize; k++) rec[k] = NextByte();
        rec[0] = (UInt8)(i >> 8); rec[1] = (UInt8)i;
        err = DmWrite(MemHandleLock(h), 0, rec, kRecSize);
        MemHandleUnlock(h);
        DmReleaseRecord(db, at, true);
    }
    LogLine(log, "Create 1000 recs: ", Ms(start), " ms");
    *sumOut = ChecksumDb(db, &count);
    DmCloseDatabase(db);
    return err;
}

static Err ExportImport(UInt16 vol, UInt32 sumBefore, Log *log)
{
    Err err;
    LocalID id;
    UInt16 card = 0, count = 0;
    UInt32 start, size = 0, sumAfter;
    FileRef f;
    DmOpenRef db;

    id = DmFindDatabase(0, kDbName);
    if (!id) return dmErrCantFind;
    VFSFileDelete(vol, kPdbFile);
    start = TimGetTicks();
    err = VFSExportDatabaseToFile(vol, kPdbFile, 0, id);
    LogLine(log, "Export .pdb: ", Ms(start), err ? " ms, FAILED" : " ms");
    if (err) return err;
    if (VFSFileOpen(vol, kPdbFile, vfsModeRead, &f) == errNone) {
        VFSFileSize(f, &size);
        VFSFileClose(f);
    }
    LogLine(log, "  .pdb size: ", (Int32)size, " bytes");

    err = DmDeleteDatabase(0, id);
    if (err) return err;
    start = TimGetTicks();
    err = VFSImportDatabaseFromFile(vol, kPdbFile, &card, &id);
    LogLine(log, "Import .pdb: ", Ms(start), err ? " ms, FAILED" : " ms");
    if (err) return err;

    db = DmOpenDatabase(card, id, dmModeReadOnly);
    if (!db) return DmGetLastErr();
    sumAfter = ChecksumDb(db, &count);
    DmCloseDatabase(db);
    LogLine(log, "Round trip: ", count, (count == kRecords && sumAfter == sumBefore) ? " recs, intact" : " recs, MISMATCH");
    DmDeleteDatabase(card, id);
    return errNone;
}

UInt16 VfsTestRun(char lines[vfsTestMaxLines][vfsTestLineLen])
{
    Log log;
    UInt32 ver, iterator = vfsIteratorStart, sum = 0;
    UInt16 vol = 0;
    Err err;
    char label[32];

    log.lines = lines;
    log.n = 0;

    if (FtrGet(sysFileCVFSMgr, vfsFtrIDVersion, &ver) != errNone) {
        LogLine(&log, "No VFS manager on this device", -1, NULL);
        return log.n;
    }
    err = VFSVolumeEnumerate(&vol, &iterator);
    if (err != errNone) {
        LogLine(&log, "No card mounted (err ", err, ")");
        return log.n;
    }
    label[0] = 0;
    VFSVolumeGetLabel(vol, label, sizeof label);
    LogLine(&log, "Card: ", -1, label[0] ? label : "(no label)");

    err = MakeDirs(vol);
    /* int is 16 bits on prc-tools, so a bare (err ? err : -1) would widen to
     * 65535; keep the conditional in Int32. */
    LogLine(&log, err ? "Dirs: err " : "Dirs: /PALM/Programs/FuelLog/* ok", err ? (Int32)err : (Int32)-1, NULL);
    if (err) return log.n;

    err = WriteAndReadBack(vol, &log);
    if (err) { LogLine(&log, "File test err ", err, NULL); return log.n; }

    err = BuildDb(&sum, &log);
    if (err) { LogLine(&log, "DB build err ", err, NULL); return log.n; }

    err = ExportImport(vol, sum, &log);
    if (err) { LogLine(&log, "Export/import err ", err, NULL); return log.n; }

    LogLine(&log, "Left on card: Backup/FuelSP4-ZFlg.pdb", -1, NULL);
    return log.n;
}

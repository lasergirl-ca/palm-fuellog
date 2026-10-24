/* Reads the spike's own workbook (inline strings, 1900 dates) and the
 * Numbers-saved sp6.xlsx (shared strings, Fills on the second sheet). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/xlsxread.h"
#include "../src/csv.h"

static int failures;
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}
static void checkStr(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want)) { printf("FAIL %s: got \"%s\" want \"%s\"\n", name, got, want); failures++; }
    else printf("ok   %s = \"%s\"\n", name, got);
}

static FILE *gZip, *gTmpW, *gTmpR[3];
static char gTmpName[3][64];
static Int32 ReadAt(void *ctx, UInt32 pos, UInt8 *buf, UInt32 len) { (void)ctx; if (fseek(gZip, (long)pos, SEEK_SET)) return -1; return (Int32)fread(buf, 1, len, gZip); }
static Int32 TmpBegin(void *ctx, UInt8 which) { (void)ctx; if (gTmpR[which]) { fclose(gTmpR[which]); gTmpR[which] = NULL; } gTmpW = fopen(gTmpName[which], "wb"); return gTmpW ? 0 : -1; }
static Int32 TmpWrite(void *ctx, const UInt8 *buf, UInt32 len) { (void)ctx; return fwrite(buf, 1, len, gTmpW) == len ? 0 : -1; }
static Int32 TmpEnd(void *ctx) { (void)ctx; int rc = fclose(gTmpW); gTmpW = NULL; return rc; }
static Int32 TmpReadAt(void *ctx, UInt8 which, UInt32 pos, UInt8 *buf, UInt32 len)
{
    (void)ctx;
    if (!gTmpR[which]) { gTmpR[which] = fopen(gTmpName[which], "rb"); if (!gTmpR[which]) return -1; }
    if (fseek(gTmpR[which], (long)pos, SEEK_SET)) return -1;
    return (Int32)fread(buf, 1, len, gTmpR[which]);
}

static XlsxReader gR;
static char gBuf[2048];
static char *gFields[csvMaxFields];

static int Open(const char *path)
{
    long size;
    int i;
    gZip = fopen(path, "rb");
    if (!gZip) { perror(path); return -1; }
    fseek(gZip, 0, SEEK_END); size = ftell(gZip);
    for (i = 0; i < 3; i++) { sprintf(gTmpName[i], "build/xr_tmp%d", i); if (gTmpR[i]) { fclose(gTmpR[i]); gTmpR[i] = NULL; } }
    memset(&gR, 0, sizeof gR);
    gR.readAt = ReadAt; gR.fileSize = (UInt32)size;
    gR.tmpBegin = TmpBegin; gR.tmpWrite = TmpWrite; gR.tmpEnd = TmpEnd; gR.tmpReadAt = TmpReadAt;
    gR.window = malloc(inflateWindowSize); gR.inBuf = malloc(4096); gR.inSize = 4096;
    gR.crcTable = malloc(256 * sizeof(UInt32)); gR.sstOffsets = malloc(8192 * sizeof(UInt32)); gR.sstCap = 8192;
    return 0;
}

int main(int argc, char **argv)
{
    Int32 n;
    int rows;
    if (argc < 3) { fprintf(stderr, "usage: test_xlsxread ours.xlsx numbers.xlsx\n"); return 1; }

    if (Open(argv[1])) return 1;
    check("open ours", XlsxrOpen(&gR), 0);
    check("ours: no shared strings", gR.hasSst, 0);
    n = XlsxrNextRow(&gR, gFields, csvMaxFields, gBuf, sizeof gBuf);
    check("ours: header has 10 cells", n, 10);
    checkStr("ours: A1", gFields[0], "Date");
    checkStr("ours: B1", gFields[1], "Odometer (km)");
    checkStr("ours: J1", gFields[9], "Notes");
    XlsxrSetDateColumns(&gR, 0, -1);
    n = XlsxrNextRow(&gR, gFields, csvMaxFields, gBuf, sizeof gBuf);
    check("ours: row 2 cells", n, 10);
    checkStr("ours: date from serial 46232", gFields[0], "2026-07-29");
    checkStr("ours: odometer text", gFields[1], "80650.0");
    checkStr("ours: currency", gFields[5], "CAD");
    checkStr("ours: full", gFields[7], "yes");
    rows = 2;
    {
        static char lastNote[128];
        while ((n = XlsxrNextRow(&gR, gFields, csvMaxFields, gBuf, sizeof gBuf)) >= 0) { rows++; if (n > 9) strcpy(lastNote, gFields[9]); }
        check("ours: 9 rows in all", rows, 9);
        checkStr("ours: last note has the ampersand back", lastNote, "Petro-Canada on 152 St. <pump 6 & card reader down>");
    }
    fclose(gZip);

    if (getenv("ONLY_FIRST")) { printf("%d failure(s)\n", failures); return 0; }
    if (Open(argv[2])) return 1;
    check("open numbers", XlsxrOpen(&gR), 0);
    check("numbers: shared strings present", gR.hasSst, 1);
    check("numbers: many shared strings", gR.sstCount > 10, 1);
    n = XlsxrNextRow(&gR, gFields, csvMaxFields, gBuf, sizeof gBuf);
    printf("numbers header (%ld): %s | %s | %s | %s | %s | %s\n", (long)n, gFields[0], gFields[1], gFields[2], gFields[3], gFields[4], gFields[5]);
    check("numbers: picked the Fills sheet (header has 8+ cells)", n >= 8, 1);
    checkStr("numbers: A1 is Date", gFields[0], "Date");
    XlsxrSetDateColumns(&gR, 0, -1);
    n = XlsxrNextRow(&gR, gFields, csvMaxFields, gBuf, sizeof gBuf);
    printf("numbers row 2 (%ld): %s | %s | %s | %s | %s | %s\n", (long)n, gFields[0], gFields[1], gFields[2], gFields[3], gFields[4], gFields[5]);
    check("numbers: row 2 date is in 2026", strncmp(gFields[0], "2026-", 5), 0);
    checkStr("numbers: currency via shared string", gFields[5], "CAD");
    rows = 2;
    while ((n = XlsxrNextRow(&gR, gFields, csvMaxFields, gBuf, sizeof gBuf)) >= 0) rows++;
    check("numbers: 4001 rows", rows, 4001);
    fclose(gZip);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

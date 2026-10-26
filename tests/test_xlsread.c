/* Reads the two fixtures tools/mkxls.py builds: the same workbook stored in
 * the compound file's mini stream and in regular sectors. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/xlsread.h"
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

static FILE *gFile, *gTmpW, *gTmpR;
static Int32 ReadAt(void *ctx, UInt32 pos, UInt8 *buf, UInt32 len) { (void)ctx; if (fseek(gFile, (long)pos, SEEK_SET)) return -1; return (Int32)fread(buf, 1, len, gFile); }
static Int32 TmpBegin(void *ctx, UInt8 which) { (void)ctx; (void)which; if (gTmpR) { fclose(gTmpR); gTmpR = NULL; } gTmpW = fopen("build/xs_tmp", "wb"); return gTmpW ? 0 : -1; }
static Int32 TmpWrite(void *ctx, const UInt8 *buf, UInt32 len) { (void)ctx; return fwrite(buf, 1, len, gTmpW) == len ? 0 : -1; }
static Int32 TmpEnd(void *ctx) { int rc; (void)ctx; rc = fclose(gTmpW); gTmpW = NULL; return rc; }
static Int32 TmpReadAt(void *ctx, UInt8 which, UInt32 pos, UInt8 *buf, UInt32 len)
{
    (void)ctx; (void)which;
    if (!gTmpR) { gTmpR = fopen("build/xs_tmp", "rb"); if (!gTmpR) return -1; }
    if (fseek(gTmpR, (long)pos, SEEK_SET)) return -1;
    return (Int32)fread(buf, 1, len, gTmpR);
}

static XlsReader *gR;
static char gBuf[2048];
static char *gFields[csvMaxFields];

static int Open(const char *path)
{
    long size;
    gFile = fopen(path, "rb");
    if (!gFile) { perror(path); return -1; }
    fseek(gFile, 0, SEEK_END); size = ftell(gFile);
    if (gTmpR) { fclose(gTmpR); gTmpR = NULL; }
    memset(gR, 0, sizeof *gR);
    gR->readAt = ReadAt; gR->fileSize = (UInt32)size;
    gR->tmpBegin = TmpBegin; gR->tmpWrite = TmpWrite; gR->tmpEnd = TmpEnd; gR->tmpReadAt = TmpReadAt; gR->tmpPart = 1;
    gR->sstOffsets = malloc(1024 * sizeof(UInt32)); gR->sstCap = 1024;
    return 0;
}

static void Run(const char *path, int padRows)
{
    Int32 n;
    int rows = 0;
    char last[32];
    printf("-- %s\n", path);
    if (Open(path)) { failures++; return; }
    check("open", XlsrOpen(gR), 0);
    check("mini stream", gR->wbMini, padRows == 0);
    check("sst count", gR->sstCount, 5);
    n = XlsrNextRow(gR, gFields, csvMaxFields, gBuf, sizeof gBuf);
    check("header fields", n, 18);
    checkStr("header[3]", gFields[3], "Odometer (km)");
    checkStr("header[17]", gFields[17], "Half");
    {
        CsvMap m;
        check("header maps", CsvMapHeader(gFields, (Int16)n, &m) > 0, 1);
        XlsrSetDateColumns(gR, m.date, m.time);
        check("date column", m.date, 0);
        check("time column", m.time, 1);
    }
    n = XlsrNextRow(gR, gFields, csvMaxFields, gBuf, sizeof gBuf);
    check("row 2 fields", n, 18);
    check("row 2 number", (long)gR->rowNo, 2);
    checkStr("date from NUMBER", gFields[0], "2023-03-15");
    checkStr("time from fraction", gFields[1], "14:06");
    checkStr("LABELSST", gFields[2], "Equinox");
    checkStr("RK integer", gFields[3], "123456");
    checkStr("NUMBER decimal", gFields[4], "45.123");
    checkStr("RK x100 integer", gFields[5], "1.59");
    checkStr("RK 30-bit double", gFields[6], "71.75");
    checkStr("LABEL", gFields[7], "CAD");
    checkStr("FORMULA string", gFields[8], "Card");
    checkStr("BOOLERR", gFields[9], "TRUE");
    check("split SST string length", (long)strlen(gFields[10]), 159);
    check("split SST string starts", strncmp(gFields[10], "Filled up before", 16), 0);
    checkStr("split SST string ends", gFields[10] + 156, "end");
    checkStr("MULRK 1", gFields[11], "1");
    checkStr("MULRK 2", gFields[12], "2");
    checkStr("MULRK 3", gFields[13], "3");
    checkStr("RK negative", gFields[14], "-5");
    checkStr("NUMBER 0.1", gFields[15], "0.1");
    checkStr("NUMBER too big", gFields[16], "#");
    checkStr("NUMBER 2.675", gFields[17], "2.675");
    n = XlsrNextRow(gR, gFields, csvMaxFields, gBuf, sizeof gBuf);
    check("row 4 fields (blank row skipped)", n, 10);
    check("row 4 number", (long)gR->rowNo, 4);
    checkStr("date from RK", gFields[0], "2023-03-16");
    checkStr("gap is empty", gFields[1], "");
    check("uncompressed SST first char", gFields[2][0], 'Z');
    check("uncompressed SST length", (long)strlen(gFields[2]), 9);
    checkStr("FORMULA number", gFields[4], "7.5");
    checkStr("RK x100 negative double", gFields[5], "-0.015");
    checkStr("BOOLERR false", gFields[9], "FALSE");
    last[0] = 0;
    while ((n = XlsrNextRow(gR, gFields, csvMaxFields, gBuf, sizeof gBuf)) >= 0) { rows++; if (n > 3) strcpy(last, gFields[3]); }
    check("padding rows", rows, padRows);
    if (padRows) { checkStr("last padding odometer", last, "132990"); check("last row number", (long)gR->rowNo, 10 + padRows); }
    check("end stays at end", XlsrNextRow(gR, gFields, csvMaxFields, gBuf, sizeof gBuf), -1);
    fclose(gFile); free(gR->sstOffsets);
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: test_xlsread mini.xls fat.xls\n"); return 1; }
    gR = malloc(sizeof *gR);
    Run(argv[1], 0);
    Run(argv[2], 300);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

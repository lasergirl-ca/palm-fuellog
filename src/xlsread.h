/* Reads the Fills sheet (or the first sheet) of a BIFF8 .xls as rows of text
 * fields, the way xlsxread does for .xlsx. The OLE2 compound file is walked
 * sector by sector through readAt (with a one-sector cache), the shared
 * strings go to a temporary part as Palm text, and numbers are decoded from
 * IEEE doubles with integer arithmetic only. Dates in the date and time
 * columns become ISO text for the row parser. */
#ifndef XLSREAD_H
#define XLSREAD_H
#include "ptypes.h"
#include "zipread.h"        /* ZipReadAtFn */
#include "sections.h"

#define xsDifatMax 237      /* 109 entries in the header plus one DIFAT sector: files to about 15 MB */
#define xsCellText 256

typedef struct { UInt32 start, index, sid; } XsChain;   /* a sector chain with a forward cursor */

typedef struct {
    /* the file */
    ZipReadAtFn readAt; void *ctx; UInt32 fileSize;
    /* one temporary part for the shared strings */
    Int32 (*tmpBegin)(void *ctx, UInt8 which);
    Int32 (*tmpWrite)(void *ctx, const UInt8 *buf, UInt32 len);
    Int32 (*tmpEnd)(void *ctx);
    Int32 (*tmpReadAt)(void *ctx, UInt8 which, UInt32 pos, UInt8 *buf, UInt32 len);
    UInt8 tmpPart;
    UInt32 *sstOffsets; UInt16 sstCap, sstCount;    /* caller-owned */
    /* compound file */
    UInt8 secShift, miniShift;
    UInt32 miniCutoff;
    UInt32 difat[xsDifatMax]; UInt16 difatCount;
    XsChain root, miniFat, wb;
    UInt8 wbMini; UInt32 wbSize;
    UInt32 cacheOff; UInt16 cacheLen; UInt8 cache[512];
    /* BIFF */
    UInt32 pos, recEnd; UInt16 recType, recLen; UInt8 pushed;
    UInt8 date1904;
    Int16 dateCol, timeCol;
    UInt32 rowNo;                 /* spreadsheet row number of the last row returned */
    /* one cell of lookahead */
    UInt8 hasNext, eof, nextKind, nextNeg, nextBig;   /* kind 0 text, 1 number */
    UInt16 nextRow; Int16 nextCol;
    UInt32 nextInt, nextFrac;     /* number: integer part and six fraction digits */
    char nextText[xsCellText];
    UInt16 mulLeft; Int16 mulCol; /* inside a MULRK record */
} XlsReader;

#define xsErrNotOle    (-30)
#define xsErrNoBook    (-31)
#define xsErrNotBiff8  (-32)
#define xsErrNoSheet   (-33)
#define xsErrTmp       (-34)

Int32 XlsrOpen(XlsReader *r) XLS_SEC;
void  XlsrSetDateColumns(XlsReader *r, Int16 dateCol, Int16 timeCol) XLS_SEC;
/* Next row: fields point into buf; returns the field count (missing cells are ""),
 * -1 at the end. Rows without cells are not returned. */
Int32 XlsrNextRow(XlsReader *r, char *fields[], Int16 maxFields, char *buf, UInt16 cap) XLS_SEC;

#endif

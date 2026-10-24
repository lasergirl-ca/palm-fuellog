/* Reads the Fills sheet (or the first sheet) of an .xlsx as rows of text
 * fields, through callbacks so the file can live on the card or on the
 * host. Parts are inflated to temporary files first (the SP6 approach);
 * shared strings are indexed by offset into their temporary file. Dates
 * in the date and time columns become ISO text for the row parser. */
#ifndef XLSXREAD_H
#define XLSXREAD_H
#include "ptypes.h"
#include "zipread.h"
#include "inflate.h"
#include "sections.h"

#define xrTmpSheet 0
#define xrTmpSst   1
#define xrTmpAux   2

typedef struct {
    /* the workbook file */
    ZipReadAtFn readAt; void *ctx; UInt32 fileSize;
    /* temporary parts: write one at a time, read any at a position */
    Int32 (*tmpBegin)(void *ctx, UInt8 which);
    Int32 (*tmpWrite)(void *ctx, const UInt8 *buf, UInt32 len);
    Int32 (*tmpEnd)(void *ctx);
    Int32 (*tmpReadAt)(void *ctx, UInt8 which, UInt32 pos, UInt8 *buf, UInt32 len);
    /* caller-owned buffers */
    UInt8 *window;          /* inflateWindowSize */
    UInt8 *inBuf; UInt32 inSize;
    UInt32 *crcTable;       /* 256 entries, filled by XlsxrOpen */
    UInt32 *sstOffsets; UInt16 sstCap;
    /* state */
    UInt16 sstCount;
    UInt32 sheetSize, sstSize, auxSize;
    UInt8 date1904, hasSst;
    Int16 dateCol, timeCol;
    UInt32 rowNo;           /* spreadsheet row number of the last row returned */
    struct XrCursor { UInt8 which; UInt32 base; UInt16 len, pos; UInt8 buf[256]; } sheet, sst, aux;   /* one read cursor per part */
} XlsxReader;

#define xrErrNoSheet   (-20)
#define xrErrInflate   (-21)
#define xrErrTmp       (-22)

Int32 XlsxrOpen(XlsxReader *r) XP_SEC;
void  XlsxrSetDateColumns(XlsxReader *r, Int16 dateCol, Int16 timeCol) XP_SEC;
/* Next row: fields point into buf; returns the field count (missing cells are ""),
 * 0 for a row with no cells, -1 at the end. */
Int32 XlsxrNextRow(XlsxReader *r, char *fields[], Int16 maxFields, char *buf, UInt16 cap) XP_SEC;

#endif

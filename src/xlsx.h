/* XLSX writer: a zip of STORED parts, no deflate, streamed in small pieces.
 * When the stream can seek, each local header is patched with CRC and size
 * once its part is done; otherwise the part is followed by a data descriptor
 * (general-purpose bit 3) and only the central directory carries the values.
 * Cells use inline strings, so there is no sharedStrings part. */
#ifndef XLSX_H
#define XLSX_H
#include "outstream.h"

#define xlsxMaxParts 12

typedef struct {
    char name[40];
    UInt32 offset;      /* of the local header */
    UInt32 crc;
    UInt32 size;
} XlsxPart;

typedef struct {
    OutStream *out;
    UInt32 *crcTable;   /* 256 entries, filled by XlsxBegin */
    XlsxPart parts[xlsxMaxParts];
    UInt16 nParts;
    UInt32 crc;         /* running, for the open part */
    UInt32 partStart;   /* stream position where the open part's data began */
    UInt8 streaming;    /* 1 = data descriptors */
    UInt16 col;         /* current column while writing a row */
    UInt32 row;
} XlsxWriter;

/* Cell styles, matching styles.xml written by XlsxWriteStyles. */
#define xlsxStyleGeneral 0
#define xlsxStyleDate    1
#define xlsxStyle1dp     2
#define xlsxStyle2dp     3
#define xlsxStyleCAD     4
#define xlsxStyleUSD     5
#define xlsxStyleBold    6

void XlsxBegin(XlsxWriter *w, OutStream *out, UInt32 *crcTable) EXPORT_SEC;
void XlsxBeginPart(XlsxWriter *w, const char *name) EXPORT_SEC;
void XlsxWrite(XlsxWriter *w, const void *buf, UInt32 len) EXPORT_SEC;
void XlsxStr(XlsxWriter *w, const char *str) EXPORT_SEC;
void XlsxEndPart(XlsxWriter *w) EXPORT_SEC;
void XlsxEnd(XlsxWriter *w) EXPORT_SEC;

/* The fixed parts: content types, relationships, workbook with the given
 * sheet names, and styles. */
void XlsxWritePackage(XlsxWriter *w, const char *const *sheetNames, UInt16 nSheets) EXPORT_SEC;

/* Sheet building. A sheet is a part named xl/worksheets/sheetN.xml. */
void XlsxSheetBegin(XlsxWriter *w, UInt16 sheetIndex) EXPORT_SEC;   /* 1-based */
void XlsxRowBegin(XlsxWriter *w) EXPORT_SEC;
void XlsxCellText(XlsxWriter *w, const char *text, UInt8 style) EXPORT_SEC;
void XlsxCellNumber(XlsxWriter *w, Int32 value, UInt8 decimals, UInt8 style) EXPORT_SEC;
void XlsxCellDate(XlsxWriter *w, UInt16 daysSince1904) EXPORT_SEC;
void XlsxCellEmpty(XlsxWriter *w) EXPORT_SEC;
void XlsxRowEnd(XlsxWriter *w) EXPORT_SEC;
void XlsxSheetEnd(XlsxWriter *w) EXPORT_SEC;

#endif

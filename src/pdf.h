/* PDF writer: PDF 1.4, uncompressed, US Letter landscape, base-14 Helvetica
 * and Helvetica-Bold in WinAnsiEncoding so Palm Latin text passes through
 * (the euro is 0x80 in both). Objects are written in one pass; each page's
 * content stream takes its /Length from an object written after it, so no
 * buffering or seeking is needed. The xref table is emitted at the end from
 * the recorded offsets. */
#ifndef PDF_H
#define PDF_H
#include "outstream.h"

#define pdfMaxObjects 64
#define pdfMaxPages   16
#define pdfPageW 792
#define pdfPageH 612

typedef struct {
    OutStream *out;
    UInt32 offsets[pdfMaxObjects + 1];   /* index = object number */
    UInt16 nObjects;
    UInt16 pageObj[pdfMaxPages];
    UInt16 nPages;
    UInt16 contentObj, lengthObj;
    UInt32 contentStart;
} PdfWriter;

#define pdfFontRegular 1
#define pdfFontBold    2

void PdfBegin(PdfWriter *w, OutStream *out) EXPORT_SEC;
void PdfPageBegin(PdfWriter *w) EXPORT_SEC;
void PdfPageEnd(PdfWriter *w) EXPORT_SEC;
void PdfEnd(PdfWriter *w) EXPORT_SEC;

/* Content operators, in points from the bottom-left corner. */
void PdfText(PdfWriter *w, UInt8 font, UInt16 size, Int16 x, Int16 y, const char *text) EXPORT_SEC;
void PdfLine(PdfWriter *w, Int16 x1, Int16 y1, Int16 x2, Int16 y2) EXPORT_SEC;
void PdfRect(PdfWriter *w, Int16 x, Int16 y, Int16 wd, Int16 ht) EXPORT_SEC;
void PdfLineWidth(PdfWriter *w, UInt8 tenths) EXPORT_SEC;
void PdfDash(PdfWriter *w, UInt8 on, UInt8 off) EXPORT_SEC;   /* 0,0 = solid */
void PdfGray(PdfWriter *w, UInt8 percent) EXPORT_SEC;        /* stroke gray */

#endif

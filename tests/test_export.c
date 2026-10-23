/* Generates the SP5 and SP7 samples on the host:
 *   build/out/fuellog-seek.xlsx    local headers patched via seek
 *   build/out/fuellog-stream.xlsx  data descriptors, no seek
 *   build/out/fuellog.pdf          three pages, base-14 fonts, a euro sign
 * tools/check_xlsx.py and the PDF viewers judge them. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/xlsx.h"
#include "../src/pdf.h"

static Int32 FileWrite(OutStream *s, const void *buf, UInt32 len)
{
    return fwrite(buf, 1, len, (FILE *)s->ctx) == len ? 0 : -1;
}

static Int32 FileSeek(OutStream *s, UInt32 pos)
{
    return fseek((FILE *)s->ctx, (long)pos, SEEK_SET) == 0 ? 0 : -1;
}

static int OpenOut(OutStream *s, const char *path, int seekable)
{
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return -1; }
    s->write = FileWrite;
    s->seek = seekable ? FileSeek : NULL;
    s->pos = 0;
    s->err = 0;
    s->ctx = f;
    return 0;
}

/* The eight sample fills from the mock-ups. Dates are days since 1904-01-01. */
typedef struct { UInt16 date; const char *label; UInt32 odo; UInt32 ml; UInt32 price; UInt32 cents; UInt8 full; UInt16 l100x10; } Fill;
#define kJul29 44770   /* 2026-07-29 as days since 1904-01-01 */
static const Fill kFills[] = {
    { kJul29 +  0, "Jul 29", 806500, 45200, 17560, 7937, 1, 0 },
    { kJul29 + 12, "Aug 10", 811900, 46000, 17690, 8137, 1, 85 },
    { kJul29 + 24, "Aug 22", 817240, 44100, 17760, 7832, 1, 83 },
    { kJul29 + 36, "Sep 3",  822620, 43900, 17570, 7713, 1, 82 },
    { kJul29 + 48, "Sep 15", 828000, 44600, 17900, 7983, 1, 83 },
    { kJul29 + 60, "Sep 27", 830920, 22000, 17590, 3870, 0, 0 },
    { kJul29 + 71, "Oct 8",  836400, 45900, 17660, 8106, 1, 81 },
    { kJul29 + 82, "Oct 19", 842120, 48200, 17490, 8430, 1, 84 },
};
#define kNumFills (sizeof kFills / sizeof kFills[0])

static void WriteWorkbook(const char *path, int seekable)
{
    static const char *const sheets[] = { "Fills", "Vehicles" };
    OutStream out;
    XlsxWriter w;
    UInt32 crc[256];
    unsigned i;

    if (OpenOut(&out, path, seekable)) exit(1);
    XlsxBegin(&w, &out, crc);
    XlsxWritePackage(&w, sheets, 2);

    XlsxSheetBegin(&w, 1);
    XlsxRowBegin(&w);
    XlsxCellText(&w, "Date", xlsxStyleBold);
    XlsxCellText(&w, "Odometer (km)", xlsxStyleBold);
    XlsxCellText(&w, "Volume (L)", xlsxStyleBold);
    XlsxCellText(&w, "Price (CAD/L)", xlsxStyleBold);
    XlsxCellText(&w, "Total (CAD)", xlsxStyleBold);
    XlsxCellText(&w, "Currency", xlsxStyleBold);
    XlsxCellText(&w, "Total (home, CAD)", xlsxStyleBold);
    XlsxCellText(&w, "Full tank", xlsxStyleBold);
    XlsxCellText(&w, "L/100 km", xlsxStyleBold);
    XlsxCellText(&w, "Notes", xlsxStyleBold);
    XlsxRowEnd(&w);
    for (i = 0; i < kNumFills; i++) {
        const Fill *f = &kFills[i];
        XlsxRowBegin(&w);
        XlsxCellDate(&w, f->date);
        XlsxCellNumber(&w, (Int32)f->odo, 1, xlsxStyle1dp);
        XlsxCellNumber(&w, (Int32)f->ml, 3, xlsxStyle2dp);
        XlsxCellNumber(&w, (Int32)(f->price + 5) / 10, 3, xlsxStyleGeneral);
        XlsxCellNumber(&w, (Int32)f->cents, 2, xlsxStyleCAD);
        XlsxCellText(&w, "CAD", xlsxStyleGeneral);
        XlsxCellNumber(&w, (Int32)f->cents, 2, xlsxStyleCAD);
        XlsxCellText(&w, f->full ? "yes" : "no", xlsxStyleGeneral);
        if (f->l100x10) XlsxCellNumber(&w, f->l100x10, 1, xlsxStyle1dp); else XlsxCellEmpty(&w);
        XlsxCellText(&w, i == 7 ? "Petro-Canada on 152 St. <pump 6 & card reader down>" : "", xlsxStyleGeneral);
        XlsxRowEnd(&w);
    }
    XlsxSheetEnd(&w);

    XlsxSheetBegin(&w, 2);
    XlsxRowBegin(&w);
    XlsxCellText(&w, "Vehicle", xlsxStyleBold);
    XlsxCellText(&w, "Powertrain", xlsxStyleBold);
    XlsxCellText(&w, "Odometer (km)", xlsxStyleBold);
    XlsxCellText(&w, "Average", xlsxStyleBold);
    XlsxRowEnd(&w);
    XlsxRowBegin(&w);
    XlsxCellText(&w, "Outback", 0); XlsxCellText(&w, "Fuel", 0);
    XlsxCellNumber(&w, 842120, 1, xlsxStyle1dp); XlsxCellNumber(&w, 83, 1, xlsxStyle1dp);
    XlsxRowEnd(&w);
    XlsxRowBegin(&w);
    XlsxCellText(&w, "Ioniq 5", 0); XlsxCellText(&w, "Electric", 0);
    XlsxCellNumber(&w, 184200, 1, xlsxStyle1dp); XlsxCellNumber(&w, 179, 1, xlsxStyle1dp);
    XlsxRowEnd(&w);
    XlsxSheetEnd(&w);

    XlsxEnd(&w);
    fclose((FILE *)out.ctx);
    if (out.err) { fprintf(stderr, "%s: write error %ld\n", path, (long)out.err); exit(1); }
    printf("wrote %s (%lu bytes, %s)\n", path, (unsigned long)out.pos, seekable ? "patched headers" : "data descriptors");
}

static void WritePdf(const char *path)
{
    OutStream out;
    PdfWriter w;
    unsigned i;
    const Int16 left = 54, top = pdfPageH - 54;

    if (OpenOut(&out, path, 1)) exit(1);
    PdfBegin(&w, &out);

    /* Page 1: summary, with a euro sign as Palm Latin / WinAnsi byte 0x80. */
    PdfPageBegin(&w);
    PdfText(&w, pdfFontBold, 18, left, top, "FuelLog report: Outback, Jul 29 to Oct 19, 2026");
    PdfText(&w, pdfFontRegular, 12, left, top - 30, "Distance 3,562 km   Fuel 339.9 L   Spend $600.08 CAD   Average 8.3 L/100 km");
    PdfText(&w, pdfFontRegular, 12, left, top - 50, "Best 8.1, worst 8.5 L/100 km   Average price 176.5 \242/L   Cost per 100 km $14.62");
    PdfText(&w, pdfFontRegular, 12, left, top - 80, "Euro test: a fill in Lyon at 1,789 \200/L, 47.30 L, total 84,62 \200 (EUR)");
    PdfText(&w, pdfFontRegular, 12, left, top - 100, "Accents: Qu\351bec, Montr\351al, caf\351, na\357ve; symbols: \251 \260 \275 (c) \"quotes\" \\backslash");
    PdfText(&w, pdfFontRegular, 10, left, 54, "Page 1 of 3. Helvetica and Helvetica-Bold, WinAnsiEncoding, no embedded fonts.");
    PdfPageEnd(&w);

    /* Page 2: the log laid out like the paper form. */
    PdfPageBegin(&w);
    {
        static const char *const heads[] = { "Date", "Trip", "Odometer", "Fuel Amt.", "$/Litre", "Total", "Points", "Notes" };
        static const Int16 cols[] = { 54, 124, 184, 264, 334, 404, 474, 544, 738 };
        Int16 y = top - 24, rowH = 22;
        PdfText(&w, pdfFontBold, 14, left, top, "Fuel Log, Outback, 2026");
        PdfLineWidth(&w, 8);
        PdfRect(&w, cols[0], y - rowH * (Int16)(kNumFills + 1), cols[8] - cols[0], rowH * (Int16)(kNumFills + 1));
        PdfLineWidth(&w, 4);
        for (i = 0; i <= kNumFills; i++)
            PdfLine(&w, cols[0], y - rowH * (Int16)i, cols[8], y - rowH * (Int16)i);
        for (i = 1; i < 8; i++)
            PdfLine(&w, cols[i], y, cols[i], y - rowH * (Int16)(kNumFills + 1));
        for (i = 0; i < 8; i++)
            PdfText(&w, pdfFontBold, 10, cols[i] + 4, y - 15, heads[i]);
        for (i = 0; i < kNumFills; i++) {
            const Fill *f = &kFills[i];
            Int16 ty = y - rowH * (Int16)(i + 1) - 15;
            char buf[32];
            PdfText(&w, pdfFontRegular, 10, cols[0] + 4, ty, f->label);
            sprintf(buf, "%lu.%lu", (unsigned long)f->odo / 10, (unsigned long)f->odo % 10);
            PdfText(&w, pdfFontRegular, 10, cols[2] + 4, ty, buf);
            sprintf(buf, "%lu.%02lu L", (unsigned long)f->ml / 1000, ((unsigned long)f->ml % 1000) / 10);
            PdfText(&w, pdfFontRegular, 10, cols[3] + 4, ty, buf);
            sprintf(buf, "%lu.%03lu", (unsigned long)(f->price + 5) / 10000, ((unsigned long)(f->price + 5) % 10000) / 10);
            PdfText(&w, pdfFontRegular, 10, cols[4] + 4, ty, buf);
            sprintf(buf, "$%lu.%02lu", (unsigned long)f->cents / 100, (unsigned long)f->cents % 100);
            PdfText(&w, pdfFontRegular, 10, cols[5] + 4, ty, buf);
            sprintf(buf, "%lu", (unsigned long)(f->ml / 100));
            PdfText(&w, pdfFontRegular, 10, cols[6] + 4, ty, buf);
        }
        PdfText(&w, pdfFontRegular, 10, left, 54, "Page 2 of 3. Rules are path operators (m, l, re, S).");
    }
    PdfPageEnd(&w);

    /* Page 3: the economy graph. */
    PdfPageBegin(&w);
    {
        const Int16 gx0 = 100, gy0 = 120, gx1 = 700, gy1 = 480;
        static const Int16 day[] = { 0, 12, 24, 36, 59, 70 };
        static const Int16 val[] = { 85, 83, 82, 83, 81, 84 };
        Int16 px = 0, py = 0;
        PdfText(&w, pdfFontBold, 14, left, top, "Economy, L/100 km, full tank to full tank");
        PdfLineWidth(&w, 8);
        PdfLine(&w, gx0, gy0, gx0, gy1);
        PdfLine(&w, gx0, gy0, gx1, gy0);
        PdfText(&w, pdfFontRegular, 10, gx0 - 30, gy1 - 4, "8.6");
        PdfText(&w, pdfFontRegular, 10, gx0 - 30, gy0 - 4, "8.0");
        PdfGray(&w, 50);
        PdfDash(&w, 4, 3);
        PdfLine(&w, gx0, gy0 + (gy1 - gy0) * 3 / 6, gx1, gy0 + (gy1 - gy0) * 3 / 6);
        PdfText(&w, pdfFontRegular, 10, gx0 - 30, gy0 + (gy1 - gy0) * 3 / 6 - 4, "8.3");
        PdfDash(&w, 0, 0);
        PdfGray(&w, 0);
        PdfLineWidth(&w, 15);
        for (i = 0; i < 6; i++) {
            Int16 x = gx0 + (Int16)((Int32)(gx1 - gx0) * day[i] / 70);
            Int16 y = gy0 + (Int16)((Int32)(gy1 - gy0) * (val[i] - 80) / 6);
            if (i) PdfLine(&w, px, py, x, y);
            PdfRect(&w, x - 3, y - 3, 6, 6);
            px = x; py = y;
        }
        PdfText(&w, pdfFontRegular, 10, gx0, gy0 - 20, "Aug 10");
        PdfText(&w, pdfFontRegular, 10, gx1 - 30, gy0 - 20, "Oct 19");
        PdfText(&w, pdfFontRegular, 10, left, 54, "Page 3 of 3. Best 8.1, worst 8.5, mean 8.3 (dashed).");
    }
    PdfPageEnd(&w);

    PdfEnd(&w);
    fclose((FILE *)out.ctx);
    if (out.err) { fprintf(stderr, "%s: write error %ld\n", path, (long)out.err); exit(1); }
    printf("wrote %s (%lu bytes, %u objects)\n", path, (unsigned long)out.pos, w.nObjects);
}

int main(void)
{
    WriteWorkbook("build/out/fuellog-seek.xlsx", 1);
    WriteWorkbook("build/out/fuellog-stream.xlsx", 0);
    WritePdf("build/out/fuellog.pdf");
    return 0;
}

#include "pdf.h"

static UInt16 ObjBegin(PdfWriter *w) EXPORT_SEC;
static void ObjEnd(PdfWriter *w) EXPORT_SEC;
static void Num(PdfWriter *w, Int32 v) EXPORT_SEC;
static void PdfString(PdfWriter *w, const char *s) EXPORT_SEC;

static UInt16 ObjBegin(PdfWriter *w)
{
    UInt16 n = ++w->nObjects;
    w->offsets[n] = w->out->pos;
    OutDec(w->out, n);
    OutStr(w->out, " 0 obj\n");
    return n;
}

static void ObjEnd(PdfWriter *w)
{
    OutStr(w->out, "\nendobj\n");
}

static void Num(PdfWriter *w, Int32 v)
{
    OutDec(w->out, v);
}

void PdfBegin(PdfWriter *w, OutStream *out)
{
    w->out = out;
    w->nObjects = 0;
    w->nPages = 0;
    OutStr(out, "%PDF-1.4\n%\342\343\317\323\n");
    /* 1: catalog, 2: pages (written at the end), 3 and 4: fonts */
    ObjBegin(w);
    OutStr(out, "<< /Type /Catalog /Pages 2 0 R >>");
    ObjEnd(w);
    w->nObjects = 2;              /* reserve 2 for the page tree */
    ObjBegin(w);
    OutStr(out, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>");
    ObjEnd(w);
    ObjBegin(w);
    OutStr(out, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>");
    ObjEnd(w);
}

void PdfPageBegin(PdfWriter *w)
{
    UInt16 page;
    OutStream *o = w->out;
    if (w->nPages >= pdfMaxPages || w->nObjects + 3 > pdfMaxObjects) { o->err = -3; return; }
    page = ObjBegin(w);
    w->pageObj[w->nPages++] = page;
    w->contentObj = page + 1;
    w->lengthObj = page + 2;
    OutStr(o, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 ");
    Num(w, pdfPageW); OutStr(o, " "); Num(w, pdfPageH);
    OutStr(o, "] /Resources << /Font << /F1 3 0 R /F2 4 0 R >> >> /Contents ");
    Num(w, w->contentObj);
    OutStr(o, " 0 R >>");
    ObjEnd(w);
    ObjBegin(w);
    OutStr(o, "<< /Length ");
    Num(w, w->lengthObj);
    OutStr(o, " 0 R >>\nstream\n");
    w->contentStart = o->pos;
}

void PdfPageEnd(PdfWriter *w)
{
    OutStream *o = w->out;
    UInt32 len = o->pos - w->contentStart;
    OutStr(o, "endstream");
    ObjEnd(w);
    ObjBegin(w);
    Num(w, (Int32)len);
    ObjEnd(w);
}

void PdfEnd(PdfWriter *w)
{
    OutStream *o = w->out;
    UInt32 xref;
    UInt16 i;

    /* The page tree, object 2. */
    w->offsets[2] = o->pos;
    OutStr(o, "2 0 obj\n<< /Type /Pages /Count ");
    Num(w, w->nPages);
    OutStr(o, " /Kids [");
    for (i = 0; i < w->nPages; i++) {
        Num(w, w->pageObj[i]);
        OutStr(o, " 0 R ");
    }
    OutStr(o, "] >>");
    ObjEnd(w);

    xref = o->pos;
    OutStr(o, "xref\n0 ");
    Num(w, w->nObjects + 1);
    OutStr(o, "\n0000000000 65535 f \n");
    for (i = 1; i <= w->nObjects; i++) {
        char line[21];
        UInt32 v = w->offsets[i];
        int k;
        for (k = 9; k >= 0; k--) { line[k] = (char)('0' + v % 10); v /= 10; }
        line[10] = ' '; line[11] = '0'; line[12] = '0'; line[13] = '0'; line[14] = '0'; line[15] = '0';
        line[16] = ' '; line[17] = 'n'; line[18] = ' '; line[19] = '\n'; line[20] = 0;
        OutStr(o, line);
    }
    OutStr(o, "trailer\n<< /Size ");
    Num(w, w->nObjects + 1);
    OutStr(o, " /Root 1 0 R >>\nstartxref\n");
    Num(w, (Int32)xref);
    OutStr(o, "\n%%EOF\n");
}

/* Escape ( ) \ and write bytes outside printable ASCII as octal, so Palm
 * Latin bytes such as the euro (0x80) reach the WinAnsi font untouched. */
static void PdfString(PdfWriter *w, const char *s)
{
    OutStream *o = w->out;
    OutStr(o, "(");
    for (; *s; s++) {
        UInt8 c = (UInt8)*s;
        if (c == '(' || c == ')' || c == '\\') {
            char e[3]; e[0] = '\\'; e[1] = (char)c; e[2] = 0;
            OutStr(o, e);
        } else if (c < 32 || c > 126) {
            char e[5];
            e[0] = '\\'; e[1] = (char)('0' + (c >> 6)); e[2] = (char)('0' + ((c >> 3) & 7));
            e[3] = (char)('0' + (c & 7)); e[4] = 0;
            OutStr(o, e);
        } else {
            OutWrite(o, s, 1);
        }
    }
    OutStr(o, ")");
}

void PdfText(PdfWriter *w, UInt8 font, UInt16 size, Int16 x, Int16 y, const char *text)
{
    OutStream *o = w->out;
    OutStr(o, font == pdfFontBold ? "BT /F2 " : "BT /F1 ");
    Num(w, size); OutStr(o, " Tf ");
    Num(w, x); OutStr(o, " "); Num(w, y); OutStr(o, " Td ");
    PdfString(w, text);
    OutStr(o, " Tj ET\n");
}

void PdfLine(PdfWriter *w, Int16 x1, Int16 y1, Int16 x2, Int16 y2)
{
    OutStream *o = w->out;
    Num(w, x1); OutStr(o, " "); Num(w, y1); OutStr(o, " m ");
    Num(w, x2); OutStr(o, " "); Num(w, y2); OutStr(o, " l S\n");
}

void PdfRect(PdfWriter *w, Int16 x, Int16 y, Int16 wd, Int16 ht)
{
    OutStream *o = w->out;
    Num(w, x); OutStr(o, " "); Num(w, y); OutStr(o, " ");
    Num(w, wd); OutStr(o, " "); Num(w, ht); OutStr(o, " re S\n");
}

void PdfLineWidth(PdfWriter *w, UInt8 tenths)
{
    OutFixed(w->out, tenths, 1);
    OutStr(w->out, " w\n");
}

void PdfDash(PdfWriter *w, UInt8 on, UInt8 off)
{
    OutStream *o = w->out;
    if (on == 0) { OutStr(o, "[] 0 d\n"); return; }
    OutStr(o, "["); Num(w, on); OutStr(o, " "); Num(w, off); OutStr(o, "] 0 d\n");
}

void PdfGray(PdfWriter *w, UInt8 percent)
{
    OutFixed(w->out, percent, 2);
    OutStr(w->out, " G\n");
}

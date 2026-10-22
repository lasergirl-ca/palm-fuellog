#include "xlsx.h"

#define kDosTime 0x6000u   /* 12:00:00 */
#define kDosDate 0x5D56u   /* 2026-10-22: ((2026-1980)<<9)|(10<<5)|22 */

static void CrcInit(UInt32 *table) EXPORT_SEC;
static UInt32 CrcUpdate(const UInt32 *table, UInt32 crc, const UInt8 *p, UInt32 len) EXPORT_SEC;
static void WriteLocalHeader(XlsxWriter *w, const XlsxPart *p, UInt32 crc, UInt32 size) EXPORT_SEC;
static void XmlText(XlsxWriter *w, const char *s) EXPORT_SEC;
static void CellRef(XlsxWriter *w) EXPORT_SEC;

static void CrcInit(UInt32 *table)
{
    UInt32 i, j, c;
    for (i = 0; i < 256; i++) {
        c = i;
        for (j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
}

static UInt32 CrcUpdate(const UInt32 *table, UInt32 crc, const UInt8 *p, UInt32 len)
{
    while (len--)
        crc = table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return crc;
}

void XlsxBegin(XlsxWriter *w, OutStream *out, UInt32 *crcTable)
{
    UInt16 i;
    w->out = out;
    w->crcTable = crcTable;
    w->nParts = 0;
    w->streaming = (out->seek == 0);
    for (i = 0; i < xlsxMaxParts; i++) w->parts[i].name[0] = 0;
    CrcInit(crcTable);
}

static void WriteLocalHeader(XlsxWriter *w, const XlsxPart *p, UInt32 crc, UInt32 size)
{
    OutStream *o = w->out;
    OutU32LE(o, 0x04034b50UL);
    OutU16LE(o, 20);                          /* version needed */
    OutU16LE(o, w->streaming ? 0x0008 : 0);   /* bit 3: data descriptor */
    OutU16LE(o, 0);                           /* STORED */
    OutU16LE(o, kDosTime);
    OutU16LE(o, kDosDate);
    OutU32LE(o, crc);
    OutU32LE(o, size);                        /* compressed = uncompressed */
    OutU32LE(o, size);
    OutU16LE(o, (UInt16)OutStrLen(p->name));
    OutU16LE(o, 0);                           /* extra */
    OutStr(o, p->name);
}

void XlsxBeginPart(XlsxWriter *w, const char *name)
{
    XlsxPart *p;
    UInt16 i;
    if (w->nParts >= xlsxMaxParts) { w->out->err = -2; return; }
    p = &w->parts[w->nParts];
    for (i = 0; name[i] && i < sizeof p->name - 1; i++) p->name[i] = name[i];
    p->name[i] = 0;
    p->offset = w->out->pos;
    p->crc = 0;
    p->size = 0;
    WriteLocalHeader(w, p, 0, 0);
    w->crc = 0xFFFFFFFFUL;
    w->partStart = w->out->pos;
}

void XlsxWrite(XlsxWriter *w, const void *buf, UInt32 len)
{
    w->crc = CrcUpdate(w->crcTable, w->crc, (const UInt8 *)buf, len);
    OutWrite(w->out, buf, len);
}

void XlsxStr(XlsxWriter *w, const char *str)
{
    XlsxWrite(w, str, OutStrLen(str));
}

void XlsxEndPart(XlsxWriter *w)
{
    XlsxPart *p = &w->parts[w->nParts];
    OutStream *o = w->out;
    p->crc = w->crc ^ 0xFFFFFFFFUL;
    p->size = o->pos - w->partStart;
    if (w->streaming) {
        OutU32LE(o, 0x08074b50UL);
        OutU32LE(o, p->crc);
        OutU32LE(o, p->size);
        OutU32LE(o, p->size);
    } else {
        UInt32 end = o->pos;
        OutSeek(o, p->offset);
        WriteLocalHeader(w, p, p->crc, p->size);
        OutSeek(o, end);
    }
    w->nParts++;
}

void XlsxEnd(XlsxWriter *w)
{
    OutStream *o = w->out;
    UInt32 cdStart = o->pos, cdSize;
    UInt16 i;
    for (i = 0; i < w->nParts; i++) {
        const XlsxPart *p = &w->parts[i];
        OutU32LE(o, 0x02014b50UL);
        OutU16LE(o, 20);                          /* made by */
        OutU16LE(o, 20);                          /* needed */
        OutU16LE(o, w->streaming ? 0x0008 : 0);
        OutU16LE(o, 0);
        OutU16LE(o, kDosTime);
        OutU16LE(o, kDosDate);
        OutU32LE(o, p->crc);
        OutU32LE(o, p->size);
        OutU32LE(o, p->size);
        OutU16LE(o, (UInt16)OutStrLen(p->name));
        OutU16LE(o, 0);                           /* extra */
        OutU16LE(o, 0);                           /* comment */
        OutU16LE(o, 0);                           /* disk */
        OutU16LE(o, 0);                           /* internal attrs */
        OutU32LE(o, 0);                           /* external attrs */
        OutU32LE(o, p->offset);
        OutStr(o, p->name);
    }
    cdSize = o->pos - cdStart;
    OutU32LE(o, 0x06054b50UL);
    OutU16LE(o, 0);
    OutU16LE(o, 0);
    OutU16LE(o, w->nParts);
    OutU16LE(o, w->nParts);
    OutU32LE(o, cdSize);
    OutU32LE(o, cdStart);
    OutU16LE(o, 0);
}

/* ---- package parts ---- */

#define XML_HEAD "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"

void XlsxWritePackage(XlsxWriter *w, const char *const *sheetNames, UInt16 nSheets)
{
    UInt16 i;

    XlsxBeginPart(w, "[Content_Types].xml");
    XlsxStr(w, XML_HEAD
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/xl/workbook.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>");
    for (i = 1; i <= nSheets; i++) {
        char n[8];
        n[0] = (char)('0' + i); n[1] = 0;
        XlsxStr(w, "<Override PartName=\"/xl/worksheets/sheet");
        XlsxStr(w, n);
        XlsxStr(w, ".xml\" ContentType=\"application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml\"/>");
    }
    XlsxStr(w, "</Types>");
    XlsxEndPart(w);

    XlsxBeginPart(w, "_rels/.rels");
    XlsxStr(w, XML_HEAD
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>");
    XlsxEndPart(w);

    XlsxBeginPart(w, "xl/workbook.xml");
    XlsxStr(w, XML_HEAD
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\" "
        "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\">"
        "<sheets>");
    for (i = 1; i <= nSheets; i++) {
        char n[8];
        XlsxStr(w, "<sheet name=\"");
        XmlText(w, sheetNames[i - 1]);
        XlsxStr(w, "\" sheetId=\"");
        n[0] = (char)('0' + i); n[1] = 0;
        XlsxStr(w, n);
        XlsxStr(w, "\" r:id=\"rId");
        XlsxStr(w, n);
        XlsxStr(w, "\"/>");
    }
    XlsxStr(w, "</sheets></workbook>");
    XlsxEndPart(w);

    XlsxBeginPart(w, "xl/_rels/workbook.xml.rels");
    XlsxStr(w, XML_HEAD
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">");
    for (i = 1; i <= nSheets; i++) {
        char n[8];
        n[0] = (char)('0' + i); n[1] = 0;
        XlsxStr(w, "<Relationship Id=\"rId");
        XlsxStr(w, n);
        XlsxStr(w, "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet\" Target=\"worksheets/sheet");
        XlsxStr(w, n);
        XlsxStr(w, ".xml\"/>");
    }
    XlsxStr(w, "<Relationship Id=\"rId9\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>");
    XlsxEndPart(w);

    XlsxBeginPart(w, "xl/styles.xml");
    XlsxStr(w, XML_HEAD
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<numFmts count=\"5\">"
        "<numFmt numFmtId=\"164\" formatCode=\"yyyy\\-mm\\-dd\"/>"
        "<numFmt numFmtId=\"165\" formatCode=\"0.0\"/>"
        "<numFmt numFmtId=\"166\" formatCode=\"0.00\"/>"
        "<numFmt numFmtId=\"167\" formatCode=\"[$CAD]\\ #,##0.00\"/>"
        "<numFmt numFmtId=\"168\" formatCode=\"[$USD]\\ #,##0.00\"/>"
        "</numFmts>"
        "<fonts count=\"2\"><font><sz val=\"11\"/><name val=\"Calibri\"/></font>"
        "<font><b/><sz val=\"11\"/><name val=\"Calibri\"/></font></fonts>"
        "<fills count=\"2\"><fill><patternFill patternType=\"none\"/></fill>"
        "<fill><patternFill patternType=\"gray125\"/></fill></fills>"
        "<borders count=\"1\"><border><left/><right/><top/><bottom/><diagonal/></border></borders>"
        "<cellStyleXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"7\">"
        "<xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\"/>"
        "<xf numFmtId=\"164\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"165\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"166\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"167\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"168\" fontId=\"0\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyNumberFormat=\"1\"/>"
        "<xf numFmtId=\"0\" fontId=\"1\" fillId=\"0\" borderId=\"0\" xfId=\"0\" applyFont=\"1\"/>"
        "</cellXfs>"
        "<cellStyles count=\"1\"><cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/></cellStyles>"
        "</styleSheet>");
    XlsxEndPart(w);
}

/* ---- sheets ---- */

static void XmlText(XlsxWriter *w, const char *s)
{
    const char *run = s;
    while (*s) {
        const char *rep = 0;
        switch (*s) {
        case '&': rep = "&amp;"; break;
        case '<': rep = "&lt;"; break;
        case '>': rep = "&gt;"; break;
        case '"': rep = "&quot;"; break;
        default: break;
        }
        if (rep) {
            XlsxWrite(w, run, (UInt32)(s - run));
            XlsxStr(w, rep);
            run = s + 1;
        }
        s++;
    }
    XlsxWrite(w, run, (UInt32)(s - run));
}

static void CellRef(XlsxWriter *w)
{
    char buf[12];
    int i = 11;
    UInt32 r = w->row;
    UInt16 c = w->col;
    buf[i] = 0;
    do { buf[--i] = (char)('0' + r % 10); r /= 10; } while (r);
    if (c >= 26) buf[--i] = (char)('A' + c / 26 - 1);
    buf[--i] = (char)('A' + c % 26);
    XlsxStr(w, buf + i);
}

void XlsxSheetBegin(XlsxWriter *w, UInt16 sheetIndex)
{
    char name[32];
    const char *base = "xl/worksheets/sheet";
    UInt16 i = 0;
    while (base[i]) { name[i] = base[i]; i++; }
    name[i++] = (char)('0' + sheetIndex);
    name[i++] = '.'; name[i++] = 'x'; name[i++] = 'm'; name[i++] = 'l'; name[i] = 0;
    XlsxBeginPart(w, name);
    XlsxStr(w, XML_HEAD
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/2006/main\">"
        "<sheetData>");
    w->row = 0;
}

void XlsxRowBegin(XlsxWriter *w)
{
    char buf[12];
    int i = 11;
    UInt32 r;
    w->row++;
    w->col = 0;
    r = w->row;
    buf[i] = 0;
    do { buf[--i] = (char)('0' + r % 10); r /= 10; } while (r);
    XlsxStr(w, "<row r=\"");
    XlsxStr(w, buf + i);
    XlsxStr(w, "\">");
}

static void CellOpen(XlsxWriter *w, UInt8 style, const char *type) EXPORT_SEC;
static void CellOpen(XlsxWriter *w, UInt8 style, const char *type)
{
    char s[4];
    XlsxStr(w, "<c r=\"");
    CellRef(w);
    XlsxStr(w, "\"");
    if (style) {
        s[0] = ' '; s[1] = 's'; s[2] = 0;
        XlsxStr(w, " s=\"");
        s[0] = (char)('0' + style); s[1] = 0;
        XlsxStr(w, s);
        XlsxStr(w, "\"");
    }
    if (type) { XlsxStr(w, " t=\""); XlsxStr(w, type); XlsxStr(w, "\""); }
    XlsxStr(w, ">");
}

void XlsxCellText(XlsxWriter *w, const char *text, UInt8 style)
{
    CellOpen(w, style, "inlineStr");
    XlsxStr(w, "<is><t>");
    XmlText(w, text);
    XlsxStr(w, "</t></is></c>");
    w->col++;
}

void XlsxCellNumber(XlsxWriter *w, Int32 value, UInt8 decimals, UInt8 style)
{
    char buf[16];
    int i = 15, d;
    UInt32 u = (value < 0) ? (UInt32)(-value) : (UInt32)value;
    CellOpen(w, style, 0);
    XlsxStr(w, "<v>");
    buf[i] = 0;
    for (d = 0; d < decimals; d++) { buf[--i] = (char)('0' + u % 10); u /= 10; }
    if (decimals) buf[--i] = '.';
    do { buf[--i] = (char)('0' + u % 10); u /= 10; } while (u);
    if (value < 0) buf[--i] = '-';
    XlsxStr(w, buf + i);
    XlsxStr(w, "</v></c>");
    w->col++;
}

void XlsxCellDate(XlsxWriter *w, UInt16 daysSince1904)
{
    /* Excel's 1900 date system: 1904-01-01 is serial 1462. */
    XlsxCellNumber(w, (Int32)daysSince1904 + 1462, 0, xlsxStyleDate);
}

void XlsxCellEmpty(XlsxWriter *w)
{
    w->col++;
}

void XlsxRowEnd(XlsxWriter *w)
{
    XlsxStr(w, "</row>");
}

void XlsxSheetEnd(XlsxWriter *w)
{
    XlsxStr(w, "</sheetData></worksheet>");
    XlsxEndPart(w);
}

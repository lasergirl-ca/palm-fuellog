#include "csv.h"
#include "units.h"
#include "currency.h"
#include "calc.h"
#include "datecalc.h"

/* Windows-1252 0x80..0x9F to Unicode; 0 = unassigned. 0xA0..0xFF map straight to Latin-1. */
static UInt16 kCp1252[32] = {   /* plain static: a const table lands in the code section and breaks (see sections.h) */
    0x20AC, 0, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0, 0x017D, 0,
    0, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0, 0x017E, 0x0178 };

static const char *kGradeNames[7] = { "Regular", "Mid", "Premium", "Diesel", "Home AC", "Public AC", "DC fast" };

static UInt16 PalmToUnicode(UInt8 c) IO_SEC;
static void   OutUtf8(OutStream *out, UInt16 u) IO_SEC;
static UInt8 StrEqI(const char *a, const char *b) IO_SEC;
static UInt8 StartsI(const char *s, const char *prefix) IO_SEC;
static const char *Paren(const char *s) IO_SEC;
static Int32 ParseNumber(const char *s, UInt8 decimals, UInt32 *out) IO_SEC;
static Int32 ParseSigned(const char *s, Int32 *out) IO_SEC;
static void OutQuotedPalm(OutStream *out, const char *s) IO_SEC;

static UInt16 PalmToUnicode(UInt8 c)
{
    if (c < 0x80) return c;
    if (c < 0xA0) return kCp1252[c - 0x80] ? kCp1252[c - 0x80] : '?';
    return c;
}

UInt8 CsvUnicodeToPalm(UInt16 u)
{
    UInt8 i;
    if (u < 0x80) return (UInt8)u;
    if (u >= 0xA0 && u <= 0xFF) return (UInt8)u;
    for (i = 0; i < 32; i++) if (kCp1252[i] == u) return (UInt8)(0x80 + i);
    return '?';
}

static void OutUtf8(OutStream *out, UInt16 u)
{
    UInt8 b[3];
    if (u < 0x80) { b[0] = (UInt8)u; OutWrite(out, b, 1); }
    else if (u < 0x800) { b[0] = (UInt8)(0xC0 | (u >> 6)); b[1] = (UInt8)(0x80 | (u & 0x3F)); OutWrite(out, b, 2); }
    else { b[0] = (UInt8)(0xE0 | (u >> 12)); b[1] = (UInt8)(0x80 | ((u >> 6) & 0x3F)); b[2] = (UInt8)(0x80 | (u & 0x3F)); OutWrite(out, b, 3); }
}

void CsvBom(OutStream *out) { static UInt8 bom[3] = { 0xEF, 0xBB, 0xBF }; OutWrite(out, bom, 3); }

static void OutQuotedPalm(OutStream *out, const char *s)
{
    OutStr(out, "\"");
    for (; *s; s++) {
        if (*s == '"') OutStr(out, "\"\"");
        else OutUtf8(out, PalmToUnicode((UInt8)*s));
    }
    OutStr(out, "\"");
}

void CsvText(OutStream *out, const char *palmText) { OutQuotedPalm(out, palmText); }

void CsvHeader(OutStream *out, const CsvCtx *c)
{
    UInt8 i;
    OutStr(out, "Date,Time,Vehicle,Odometer ("); OutStr(out, UnitDistLabel(c->distUnit));
    OutStr(out, "),Trip ("); OutStr(out, UnitDistLabel(c->distUnit));
    OutStr(out, "),Volume ("); OutStr(out, UnitVolLabel(c->volUnit));
    OutStr(out, "),Price (per "); OutStr(out, UnitPriceLabel(c->priceUnit) + 1);   /* skip the slash */
    OutStr(out, "),Total,Currency,Rate,Home total ("); OutStr(out, CurrencyCode(c->homeCurrency));
    OutStr(out, "),Payment,Grade,Full,Missed,Discount");
    for (i = 0; i < c->pgmCount; i++) {
        char h[pgmNameLen + 10]; UInt16 k = 0, q;
        for (q = 0; c->pgmNames[i][q]; q++) h[k++] = c->pgmNames[i][q];
        for (q = 0; " earned"[q]; q++) h[k++] = " earned"[q];
        h[k] = 0;
        OutStr(out, ","); OutQuotedPalm(out, h);
        k = 0;
        for (q = 0; c->pgmNames[i][q]; q++) h[k++] = c->pgmNames[i][q];
        for (q = 0; " redeemed"[q]; q++) h[k++] = " redeemed"[q];
        h[k] = 0;
        OutStr(out, ","); OutQuotedPalm(out, h);
    }
    OutStr(out, ",Notes\r\n");
}

void CsvFillRow(OutStream *out, const CsvCtx *c, const FillRecord *f, const char *vehicleName)
{
    UInt16 y; UInt8 m, d, i;
    char b[12];
    UInt8 charge = (f->flags & fillFlagCharge) != 0;
    DateDaysToYmd(f->date, &y, &m, &d);
    OutDec(out, y); OutStr(out, "-"); if (m < 10) OutStr(out, "0"); OutDec(out, m); OutStr(out, "-"); if (d < 10) OutStr(out, "0"); OutDec(out, d);
    OutStr(out, ",");
    if (f->time != fillTimeUnknown) { if (f->time / 60 < 10) OutStr(out, "0"); OutDec(out, f->time / 60); OutStr(out, ":"); if (f->time % 60 < 10) OutStr(out, "0"); OutDec(out, f->time % 60); }
    OutStr(out, ","); OutQuotedPalm(out, vehicleName);
    OutStr(out, ","); OutFixed(out, (Int32)UnitDistToDisplay(f->odometer, c->distUnit), 1);
    OutStr(out, ","); if (f->trip) OutFixed(out, (Int32)UnitDistToDisplay(f->trip, c->distUnit), 1);
    OutStr(out, ",");
    if (charge) OutFixed(out, (Int32)f->volume, 3);                       /* kWh to three places */
    else OutFixed(out, (Int32)UnitVolToDisplay(f->volume, c->volUnit), UnitVolDecimals(c->volUnit));
    OutStr(out, ",");
    if (f->price) OutFixed(out, (Int32)(charge ? f->price : UnitPriceToDisplay(f->price, c->priceUnit)), 4);
    OutStr(out, ","); OutFixed(out, (Int32)f->total, 2);
    OutStr(out, ","); OutStr(out, CurrencyCode(f->currency));
    OutStr(out, ","); OutFixed(out, (Int32)((f->fxRate ? f->fxRate : fxOne) / 100), 4);
    OutStr(out, ","); OutFixed(out, (Int32)CalcHomeCents(f->total, f->fxRate ? f->fxRate : fxOne), 2);
    OutStr(out, ","); OutStr(out, f->payment == fillPaymentCard ? "Card" : (charge ? "Bill" : "Cash"));
    OutStr(out, ","); OutStr(out, kGradeNames[f->grade < 7 ? f->grade : 0]);
    OutStr(out, ","); OutStr(out, (f->flags & fillFlagFullTank) ? "yes" : "");
    OutStr(out, ","); OutStr(out, (f->flags & fillFlagMissedPrev) ? "yes" : "");
    OutStr(out, ","); if (f->discount) OutFixed(out, (Int32)f->discount, 2);
    for (i = 0; i < c->pgmCount; i++) {
        UInt8 k; Int32 earned = 0, redeemed = 0; UInt8 have = 0;
        for (k = 0; k < f->pointCount && k < fillMaxPoints; k++)
            if (f->points[k].programme == c->pgmIds[i]) { earned = f->points[k].earned; redeemed = f->points[k].redeemed; have = 1; }
        OutStr(out, ","); if (have) OutDec(out, earned);
        OutStr(out, ","); if (have && redeemed) OutDec(out, redeemed);
    }
    OutStr(out, ","); OutQuotedPalm(out, f->notes);
    OutStr(out, "\r\n");
    (void)b;
}

/* ---- reading ---- */

Int32 CsvNextRecord(CsvGetcFn getc, void *ctx, char *buf, UInt16 cap, UInt32 *lineNo)
{
    UInt16 n = 0;
    UInt8 inQuotes = 0, any = 0;
    Int16 c;
    for (;;) {
        c = getc(ctx);
        if (c < 0) break;
        any = 1;
        if (c == 0xEF && n == 0) {          /* a BOM: swallow its three bytes */
            Int16 c2 = getc(ctx), c3 = getc(ctx);
            if (c2 == 0xBB && c3 == 0xBF) continue;
            if (n < cap - 1) buf[n++] = (char)c;
            c = c2; if (c < 0) break;
            if (n < cap - 1) buf[n++] = (char)c;
            c = c3; if (c < 0) break;
        }
        if (c == '\r') continue;
        if (c == '\n') {
            (*lineNo)++;
            if (!inQuotes) break;
            if (n < cap - 1) buf[n++] = '\n';
            continue;
        }
        if (c == '"') inQuotes = (UInt8)!inQuotes;
        if (c >= 0x80) {
            /* UTF-8 lead byte: gather the sequence; a lone high byte is taken as Windows-1252 */
            UInt16 u = 0; Int16 extra = 0;
            if ((c & 0xE0) == 0xC0) { u = (UInt16)(c & 0x1F); extra = 1; }
            else if ((c & 0xF0) == 0xE0) { u = (UInt16)(c & 0x0F); extra = 2; }
            else if ((c & 0xF8) == 0xF0) { u = 0; extra = 3; }
            if (extra) {
                Int16 k, ok = 1, cc;
                for (k = 0; k < extra; k++) {
                    cc = getc(ctx);
                    if (cc < 0 || (cc & 0xC0) != 0x80) { ok = 0; break; }
                    u = (UInt16)((u << 6) | (cc & 0x3F));
                }
                if (ok) { if (n < cap - 1) buf[n++] = (char)(extra == 3 ? '?' : CsvUnicodeToPalm(u)); continue; }
            }
            if (n < cap - 1) buf[n++] = (char)c;   /* Windows-1252 byte as is */
            continue;
        }
        if (n < cap - 1) buf[n++] = (char)c;
    }
    buf[n] = 0;
    if (!any && n == 0) return -1;
    if (c < 0 && n == 0) return -1;
    return n;
}

Int16 CsvSplit(char *record, char *fields[], Int16 maxFields)
{
    Int16 n = 0;
    char *p = record, *w;
    if (!*p) return 0;
    while (n < maxFields) {
        fields[n++] = p;
        w = p;
        if (*p == '"') {
            p++;
            for (;;) {
                if (*p == 0) break;
                if (*p == '"') { if (p[1] == '"') { *w++ = '"'; p += 2; continue; } p++; break; }
                *w++ = *p++;
            }
            while (*p && *p != ',') p++;
        } else {
            while (*p && *p != ',') *w++ = *p++;
        }
        if (*p == ',') { *w = 0; p++; if (*p == 0) { if (n < maxFields) { fields[n++] = p; } break; } }
        else { *w = 0; break; }
    }
    return n;
}

static UInt8 StrEqI(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return 0;
    }
    return *a == *b;
}

static UInt8 StartsI(const char *s, const char *prefix)
{
    for (; *prefix; s++, prefix++) {
        char x = *s, y = *prefix;
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return 0;
    }
    return 1;
}

/* The text inside the first parentheses, or "" */
static const char *Paren(const char *s)
{
    while (*s && *s != '(') s++;
    return *s ? s + 1 : s;
}

Int16 CsvMapHeader(char *fields[], Int16 n, CsvMap *m)
{
    Int16 i, found = 0;
    UInt8 *p = (UInt8 *)m;
    UInt32 k;
    for (k = 0; k < sizeof *m; k++) p[k] = 0;
    m->date = m->time = m->vehicle = m->odometer = m->trip = m->volume = m->price = m->total = m->currency = m->rate = -1;
    m->payment = m->grade = m->full = m->missed = m->discount = m->notes = m->kbPoints = m->kbCumulative = -1;
    m->priceCurrency = 255;
    m->distUnit = unitDistKm; m->volUnit = unitVolLitres; m->priceUnit = unitPricePerLitre;
    for (i = 0; i < n; i++) {
        const char *h = fields[i], *u = Paren(h);
        while (*h == ' ') h++;
        if (!*h) continue;
        if (StartsI(h, "date")) { m->date = i; found++; }
        else if (StartsI(h, "time")) { m->time = i; found++; }
        else if (StartsI(h, "vehicle") || StartsI(h, "car")) { m->vehicle = i; found++; }
        else if (StartsI(h, "odometer") || StartsI(h, "odo")) {
            m->odometer = i; found++;
            if (StartsI(u, "mi")) m->distUnit = unitDistMiles;
        }
        else if (StartsI(h, "trip")) { m->trip = i; found++; }
        else if (StartsI(h, "volume") || StartsI(h, "fuel amt") || StartsI(h, "litres") || StartsI(h, "gallons") || StartsI(h, "fuel")) {
            m->volume = i; found++;
            if (StartsI(u, "us gal") || StartsI(u, "gal") || StartsI(h, "gallons")) m->volUnit = unitVolUSGal;
            else if (StartsI(u, "imp")) m->volUnit = unitVolImpGal;
        }
        else if (StartsI(h, "price") || StartsI(h, "$/gallon") || StartsI(h, "$/litre") || StartsI(h, "$/l")) {
            const char *slash = u;
            m->price = i; found++;
            if (StartsI(h, "$/gal")) { m->priceUnit = unitPricePerUSGal; continue; }
            if (StartsI(h, "$/l")) { m->priceUnit = unitPricePerLitre; continue; }
            if (StartsI(u, "per ")) slash = u + 4;
            else {
                /* "CAD/L" or "USD/US gal": a currency code before the slash */
                if (u[0] && u[1] && u[2] && u[3] == '/') {
                    UInt8 c;
                    for (c = 0; c < curCount; c++) if (StartsI(u, CurrencyCode(c))) m->priceCurrency = c;
                    slash = u + 4;
                }
            }
            if (StartsI(slash, "us gal") || StartsI(slash, "gal")) m->priceUnit = unitPricePerUSGal;
            else if (StartsI(slash, "imp")) m->priceUnit = unitPricePerImpGal;
            else if (StartsI(slash, "kwh")) m->priceUnit = unitPricePerKwh;
            else m->priceUnit = unitPricePerLitre;
        }
        else if (StartsI(h, "total") || StartsI(h, "cost") || StartsI(h, "amount")) { if (!StartsI(h, "home")) { m->total = i; found++; } }
        else if (StartsI(h, "home total")) { /* derived on import */ }
        else if (StartsI(h, "currency")) { m->currency = i; found++; }
        else if (StartsI(h, "rate")) { m->rate = i; found++; }
        else if (StartsI(h, "payment") || StartsI(h, "paid")) { m->payment = i; found++; }
        else if (StartsI(h, "grade")) { m->grade = i; found++; }
        else if (StartsI(h, "full")) { m->full = i; found++; }
        else if (StartsI(h, "missed") || StartsI(h, "skipped")) { m->missed = i; found++; }
        else if (StartsI(h, "discount")) { m->discount = i; found++; }
        else if (StartsI(h, "notes") || StartsI(h, "note")) { m->notes = i; found++; }
        else if (StartsI(h, "kb points") || StrEqI(h, "kb")) { m->kbPoints = i; found++; }
        else if (StartsI(h, "cumulative")) { m->kbCumulative = i; }
        else {
            /* "<programme> earned" / "<programme> redeemed" */
            UInt16 len = 0; const char *e;
            while (h[len]) len++;
            e = h + len;
            if (len > 7 && StrEqI(e - 7, " earned")) {
                UInt8 j; char name[pgmNameLen]; UInt16 nl = (UInt16)(len - 7); UInt16 q;
                if (nl >= pgmNameLen) nl = pgmNameLen - 1;
                for (q = 0; q < nl; q++) name[q] = h[q];
                name[nl] = 0;
                for (j = 0; j < m->pgmCount; j++) if (StrEqI(m->pgmNames[j], name)) break;
                if (j == m->pgmCount && j < csvMaxPgms) { for (q = 0; q <= nl; q++) m->pgmNames[j][q] = name[q]; m->pgmEarned[j] = -1; m->pgmRedeemed[j] = -1; m->pgmCount++; }
                if (j < csvMaxPgms) { m->pgmEarned[j] = i; found++; }
            } else if (len > 9 && StrEqI(e - 9, " redeemed")) {
                UInt8 j; char name[pgmNameLen]; UInt16 nl = (UInt16)(len - 9); UInt16 q;
                if (nl >= pgmNameLen) nl = pgmNameLen - 1;
                for (q = 0; q < nl; q++) name[q] = h[q];
                name[nl] = 0;
                for (j = 0; j < m->pgmCount; j++) if (StrEqI(m->pgmNames[j], name)) break;
                if (j == m->pgmCount && j < csvMaxPgms) { for (q = 0; q <= nl; q++) m->pgmNames[j][q] = name[q]; m->pgmEarned[j] = -1; m->pgmRedeemed[j] = -1; m->pgmCount++; }
                if (j < csvMaxPgms) { m->pgmRedeemed[j] = i; found++; }
            }
        }
    }
    if (m->date < 0 || (m->odometer < 0 && m->trip < 0) || (m->volume < 0 && m->total < 0)) return 0;
    return found;
}

/* Digits with an optional point, thousands separators and a currency sign ignored. */
static Int32 ParseNumber(const char *s, UInt8 decimals, UInt32 *out)
{
    UInt32 v = 0; Int16 frac = -1; UInt8 any = 0;
    for (; *s; s++) {
        char c = *s;
        if (c >= '0' && c <= '9') {
            if (frac >= 0) { if (frac < decimals) { v = v * 10 + (UInt32)(c - '0'); frac++; } else if (frac == decimals) { if (c >= '5') v++; frac++; } }
            else v = v * 10 + (UInt32)(c - '0');
            any = 1;
        }
        else if (c == '.') { if (frac >= 0) return -1; frac = 0; }
        else if (c == ',' || c == ' ' || c == '$' || c == '\'') continue;
        else return -1;
    }
    if (!any) return 0;
    while (frac < 0 ? decimals-- : (frac < decimals && frac++ >= 0)) { if (frac < 0) v *= 10; else v *= 10; }
    *out = v;
    return 1;
}

static Int32 ParseSigned(const char *s, Int32 *out)
{
    UInt8 neg = 0; UInt32 v; Int32 r;
    while (*s == ' ') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    r = ParseNumber(s, 0, &v);
    if (r == 1) *out = neg ? -(Int32)v : (Int32)v;
    return r;
}

Int32 CsvRowToFill(const CsvMap *m, char *fields[], Int16 n, FillRecord *f, char *vehicleName, char *err)
{
    UInt8 *p = (UInt8 *)f;
    UInt32 k, v;
    Int32 r;
    const char *s;
    UInt16 y = 0; UInt8 mo = 0, d = 0;
    for (k = 0; k < sizeof *f; k++) p[k] = 0;
    vehicleName[0] = 0; err[0] = 0;
    f->time = fillTimeUnknown; f->fxRate = fxOne; f->flags = fillFlagFullTank;
#define FIELD(i) ((i) >= 0 && (i) < n ? fields[i] : "")
    /* date: YYYY-MM-DD, MM/DD/YYYY or DD/MM/YYYY (first part > 12 means day first) */
    s = FIELD(m->date);
    {
        UInt32 a = 0, b = 0, c = 0; Int16 part = 0; const char *q = s;
        while (*q == ' ') q++;
        for (; *q; q++) {
            if (*q >= '0' && *q <= '9') { if (part == 0) a = a * 10 + (UInt32)(*q - '0'); else if (part == 1) b = b * 10 + (UInt32)(*q - '0'); else c = c * 10 + (UInt32)(*q - '0'); }
            else if (*q == '-' || *q == '/' || *q == '.') part++;
            else if (*q == ' ' || *q == 'T') break;
            else { for (k = 0; "date not understood"[k]; k++) err[k] = "date not understood"[k]; err[k] = 0; return -1; }
        }
        if (part != 2) { const char *t = "date not understood"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1; }
        if (a > 31) { y = (UInt16)a; mo = (UInt8)b; d = (UInt8)c; }                 /* YYYY-MM-DD */
        else if (c < 100) { y = (UInt16)(c + 2000); mo = (UInt8)(a > 12 ? b : a); d = (UInt8)(a > 12 ? a : b); }
        else { y = (UInt16)c; mo = (UInt8)(a > 12 ? b : a); d = (UInt8)(a > 12 ? a : b); }
        if (mo < 1 || mo > 12 || d < 1 || d > DateDaysInMonth(y, mo) || y < 1904 || y > 2031) {
            const char *t = "date is not valid"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1;
        }
        f->date = DateYmdToDays(y, mo, d);
    }
    s = FIELD(m->time);
    if (*s) {
        UInt32 hh = 0, mm = 0; const char *q = s; UInt8 colon = 0, pm = 0, am = 0;
        for (; *q; q++) {
            if (*q >= '0' && *q <= '9') { if (!colon) hh = hh * 10 + (UInt32)(*q - '0'); else mm = mm * 10 + (UInt32)(*q - '0'); }
            else if (*q == ':') colon = 1;
            else if (*q == 'p' || *q == 'P') pm = 1;
            else if (*q == 'a' || *q == 'A') am = 1;
        }
        if (pm && hh < 12) hh += 12;
        if (am && hh == 12) hh = 0;
        if (hh < 24 && mm < 60) f->time = (UInt16)(hh * 60 + mm);
    }
    s = FIELD(m->vehicle);
    for (k = 0; s[k] && k < vehNameLen - 1; k++) vehicleName[k] = s[k];
    vehicleName[k] = 0;
    r = ParseNumber(FIELD(m->odometer), 1, &v);
    if (r < 0) { const char *t = "odometer is not a number"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1; }
    if (r == 1) f->odometer = UnitDistFromDisplay(v, m->distUnit);
    r = ParseNumber(FIELD(m->trip), 1, &v);
    if (r == 1 && v) { f->trip = UnitDistFromDisplay(v, m->distUnit); f->flags |= fillFlagTripEntered; }
    /* grade first: a charge reads volume as kWh */
    s = FIELD(m->grade);
    if (*s) {
        UInt8 g;
        for (g = 0; g < 7; g++) if (StrEqI(s, kGradeNames[g])) f->grade = g;
        if (StartsI(s, "prem")) f->grade = fillGradePremium;
        if (StartsI(s, "reg")) f->grade = fillGradeRegular;
        if (f->grade >= fillGradeHomeAC) f->flags |= fillFlagCharge;
    }
    if (f->flags & fillFlagCharge) {
        r = ParseNumber(FIELD(m->volume), 3, &v);
        if (r < 0) { const char *t = "energy is not a number"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1; }
        if (r == 1) f->volume = v;
        r = ParseNumber(FIELD(m->price), 4, &v);
        if (r == 1) f->price = v;
    } else {
        r = ParseNumber(FIELD(m->volume), UnitVolDecimals(m->volUnit), &v);
        if (r < 0) { const char *t = "volume is not a number"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1; }
        if (r == 1) f->volume = UnitVolFromDisplay(v, m->volUnit);
        r = ParseNumber(FIELD(m->price), 4, &v);
        if (r < 0) { const char *t = "price is not a number"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1; }
        if (r == 1) f->price = UnitPriceFromDisplay(v, m->priceUnit);
    }
    r = ParseNumber(FIELD(m->total), 2, &v);
    if (r < 0) { const char *t = "total is not a number"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1; }
    if (r == 1) f->total = v;
    else if (f->price && f->volume) f->total = CalcTotalCents(f->price, f->volume);
    else if (f->volume == 0 && f->total == 0 && f->odometer == 0) { const char *t = "row has no fill in it"; for (k = 0; t[k]; k++) err[k] = t[k]; err[k] = 0; return -1; }
    if (f->price == 0 && f->total && f->volume) f->price = CalcMulDiv(f->total, 100000UL, f->volume);
    s = FIELD(m->currency);
    f->currency = m->priceCurrency != 255 ? m->priceCurrency : curCAD;
    if (*s) { UInt8 c; for (c = 0; c < curCount; c++) if (StrEqI(s, CurrencyCode(c))) f->currency = c; }
    r = ParseNumber(FIELD(m->rate), 4, &v);
    if (r == 1 && v) f->fxRate = v * 100;
    s = FIELD(m->payment);
    f->payment = (StartsI(s, "card") || StartsI(s, "credit") || StartsI(s, "debit")) ? fillPaymentCard : fillPaymentCash;
    s = FIELD(m->full);
    if (m->full >= 0 && !(StartsI(s, "y") || StartsI(s, "1") || StartsI(s, "t") || StartsI(s, "full") || StartsI(s, "x"))) f->flags &= ~fillFlagFullTank;
    s = FIELD(m->missed);
    if (StartsI(s, "y") || StartsI(s, "1") || StartsI(s, "t") || StartsI(s, "x")) f->flags |= fillFlagMissedPrev;
    r = ParseNumber(FIELD(m->discount), 2, &v);
    if (r == 1) f->discount = v;
    {
        UInt8 j;
        for (j = 0; j < m->pgmCount && f->pointCount < fillMaxPoints; j++) {
            Int32 e = 0, rd = 0; UInt8 have = 0;
            if (ParseSigned(FIELD(m->pgmEarned[j]), &e) == 1) have = 1;
            if (ParseSigned(FIELD(m->pgmRedeemed[j]), &rd) == 1) have = 1;
            if (!have) continue;
            f->points[f->pointCount].programme = (UInt8)(j + 1);   /* header index; the caller maps to an id */
            f->points[f->pointCount].earned = e; f->points[f->pointCount].redeemed = rd;
            f->pointCount++;
        }
        if (m->kbPoints >= 0 && f->pointCount < fillMaxPoints) {
            Int32 e = 0;
            if (ParseSigned(FIELD(m->kbPoints), &e) == 1 && e) {
                f->points[f->pointCount].programme = 255;              /* the KB Points alias: the caller resolves it */
                f->points[f->pointCount].earned = e; f->pointCount++;
            }
        }
    }
    s = FIELD(m->notes);
    for (k = 0; s[k] && k < fillMaxNotes - 1; k++) f->notes[k] = s[k];
    f->notes[k] = 0;
#undef FIELD
    return 0;
}

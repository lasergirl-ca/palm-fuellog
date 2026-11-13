/* CSV, XLSX and PDF export of the log, ported from the Palm build's
 * src/export.c and src/export2.c: the same files, cell for cell, written
 * to an OutStream over a file instead of the card. The writers themselves
 * (csv.c, xlsx.c, pdf.c) are the shared core, compiled unchanged. Below
 * this header the code is the Palm's, with the Palm calls it makes mapped
 * to the Pocket PC store by the shims here. */
#include "exportfile.h"
#include "pgmedit.h"
#include "vehedit.h"
#include "textview.h"
#include "../store/db.h"
#include "../store/dirs.h"
#include "../../src/csv.h"
#include "../../src/xlsx.h"
#include "../../src/pdf.h"
#include "../../src/crc32.h"
#include "../../src/stats.h"
#include "../../src/report.h"
#include "../../src/calc.h"
#include "../../src/units.h"
#include "../../src/currency.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- shims: the Palm calls the ported code makes ---- */
typedef int Boolean;
#define true 1
#define false 0
#define errNone dbOk
#define StrCopy strcpy
#define StrCat strcat
#define StrLen(s) ((UInt32)strlen(s))
#define StrNCopy strncpy
#define MemSet(p, n, v) memset((p), (v), (n))
#define MemPtrNew malloc
#define MemPtrFree free
static FuelPrefs gPrefs;
static char gPoint = '.', gThousands = ',';
static void StrIToA(char *b, Int32 n) { TextFixed(b, n, 0, '.', 0); }
static Boolean VehGet(UInt8 id, VehicleRecord *v)
{
    if (DbVehicleById(id, v)) return true;
    memset(v, 0, sizeof *v);
    v->id = id; strcpy(v->name, "Vehicle"); v->defaultCurrency = vehCurrencyUsePrefs;
    return false;
}
#define PgmGetAt DbGetProgramme
#define SvcSpendCents StatsViewServiceCents
/* The fill's time as the device shows it: 24-hour "9:05" or 12-hour "9:05 am". */
static int gTime24 = 1;
static void AppTimeText(char *b, UInt16 minutes)
{
    UInt16 h = (UInt16)(minutes / 60), m = (UInt16)(minutes % 60);
    if (minutes == fillTimeUnknown) { strcpy(b, "--:--"); return; }
    if (!gTime24) { TextFixed(b, h % 12 ? h % 12 : 12, 0, '.', 0); }
    else TextFixed(b, h, 0, '.', 0);
    strcat(b, m < 10 ? ":0" : ":");
    TextFixed(b + strlen(b), m, 0, '.', 0);
    if (!gTime24) strcat(b, h < 12 ? " am" : " pm");
}

/* ---- the Palm's code ---- */

static CsvCtx gCsv;
static FillRecord gExpFill;
static VehicleRecord gExpVeh;
static ProgrammeRecord gExpPgm;
UInt8 gExportPgm = 255;


static void BuildCtx(void)
{
    UInt16 n = DbCount(dbProgrammes), i;
    MemSet(&gCsv, sizeof gCsv, 0);
    gCsv.distUnit = gPrefs.distanceUnit; gCsv.volUnit = gPrefs.volumeUnit; gCsv.priceUnit = gPrefs.priceUnit;
    gCsv.priceCents = gPrefs.priceFormat == 1; gCsv.homeCurrency = gPrefs.homeCurrency;
    for (i = 0; i < n && gCsv.pgmCount < csvMaxPgms; i++) {
        if (PgmGetAt(i, &gExpPgm) != errNone) continue;
        if (gExportPgm != 255 && gExpPgm.id != gExportPgm) continue;
        gCsv.pgmIds[gCsv.pgmCount] = gExpPgm.id;
        StrCopy(gCsv.pgmNames[gCsv.pgmCount], gExpPgm.name);
        gCsv.pgmCount++;
    }
}

void ExportCsvTo(OutStream *out, UInt8 vehicle, UInt16 year)
{
    UInt16 n = DbCount(dbFills), i;
    UInt8 lastVeh = 255;
    BuildCtx();
    CsvBom(out);
    CsvHeader(out, &gCsv);
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gExpFill) != errNone || !ExportWants(&gExpFill, vehicle, year)) continue;
        if (gExpFill.vehicle != lastVeh) { VehGet(gExpFill.vehicle, &gExpVeh); lastVeh = gExpFill.vehicle; }
        CsvFillRow(out, &gCsv, &gExpFill, gExpVeh.name);
        if (out->err) break;
    }
}

Boolean ExportWants(const FillRecord *f, UInt8 vehicle, UInt16 year)
{
    if (vehicle != 255 && f->vehicle != vehicle) return false;
    if (gExportPgm != 255) {
        UInt8 k; Boolean has = false;
        for (k = 0; k < f->pointCount; k++) if (f->points[k].programme == gExportPgm) has = true;
        if (!has) return false;
    }
    if (year) { UInt16 y; UInt8 m, d; DateDaysToYmd(f->date, &y, &m, &d); if (y != year) return false; }
    return true;
}

static UInt32 *gCrc;              /* 1 KB, on the heap during an export */
static XlsxWriter gXw;
static PdfWriter gPw;
static FillRecord gXFill;
static VehicleRecord gXVeh;
static ProgrammeRecord gXPgm;
static ServiceVisitRecord gXVisit;
static ServiceTypeRecord gXType;
static Report *gXReport;
static Stats *gXStats;
static UInt8 gXPgmIds[16];
static char gXPgmNames[16][pgmNameLen];
static UInt8 gXPgmCount;

static const char *kGrade[7] = { "Regular", "Mid", "Premium", "Diesel", "Home AC", "Public AC", "DC fast" };


static UInt8 CurrencyStyle(UInt8 currency)
{
    return currency == curCAD ? xlsxStyleCAD : currency == curUSD ? xlsxStyleUSD : xlsxStyle2dp;
}

static void LoadProgrammes(void)
{
    UInt16 n = DbCount(dbProgrammes), i;
    gXPgmCount = 0;
    for (i = 0; i < n && gXPgmCount < 16; i++) {
        if (PgmGetAt(i, &gXPgm) != errNone) continue;
        if (gExportPgm != 255 && gXPgm.id != gExportPgm) continue;
        gXPgmIds[gXPgmCount] = gXPgm.id; StrCopy(gXPgmNames[gXPgmCount], gXPgm.name); gXPgmCount++;
    }
}

static void VolDecimalsCell(XlsxWriter *w, UInt32 ml)
{
    XlsxCellNumber(w, (Int32)UnitVolToDisplay(ml, gPrefs.volumeUnit), UnitVolDecimals(gPrefs.volumeUnit), xlsxStyle2dp);
}

static void FillsSheet(UInt8 vehicle, UInt16 year)
{
    UInt16 n = DbCount(dbFills), i;
    UInt8 k, lastVeh = 255;
    char b[40];
    XlsxSheetBegin(&gXw, 1);
    XlsxRowBegin(&gXw);
    XlsxCellText(&gXw, "Date", xlsxStyleBold); XlsxCellText(&gXw, "Time", xlsxStyleBold); XlsxCellText(&gXw, "Vehicle", xlsxStyleBold);
    StrCopy(b, "Odometer ("); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); StrCat(b, ")"); XlsxCellText(&gXw, b, xlsxStyleBold);
    StrCopy(b, "Trip ("); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); StrCat(b, ")"); XlsxCellText(&gXw, b, xlsxStyleBold);
    StrCopy(b, "Volume ("); StrCat(b, UnitVolLabel(gPrefs.volumeUnit)); StrCat(b, ")"); XlsxCellText(&gXw, b, xlsxStyleBold);
    StrCopy(b, "Price (per "); StrCat(b, UnitPriceLabel(gPrefs.priceUnit) + 1); StrCat(b, ")"); XlsxCellText(&gXw, b, xlsxStyleBold);
    XlsxCellText(&gXw, "Total", xlsxStyleBold); XlsxCellText(&gXw, "Currency", xlsxStyleBold); XlsxCellText(&gXw, "Rate", xlsxStyleBold);
    StrCopy(b, "Home total ("); StrCat(b, CurrencyCode(gPrefs.homeCurrency)); StrCat(b, ")"); XlsxCellText(&gXw, b, xlsxStyleBold);
    XlsxCellText(&gXw, "Payment", xlsxStyleBold); XlsxCellText(&gXw, "Grade", xlsxStyleBold); XlsxCellText(&gXw, "Full", xlsxStyleBold);
    XlsxCellText(&gXw, "Missed", xlsxStyleBold); XlsxCellText(&gXw, "Discount", xlsxStyleBold);
    for (k = 0; k < gXPgmCount; k++) {
        StrCopy(b, gXPgmNames[k]); StrCat(b, " earned"); XlsxCellText(&gXw, b, xlsxStyleBold);
        StrCopy(b, gXPgmNames[k]); StrCat(b, " redeemed"); XlsxCellText(&gXw, b, xlsxStyleBold);
    }
    XlsxCellText(&gXw, "Notes", xlsxStyleBold);
    XlsxRowEnd(&gXw);
    for (i = 0; i < n; i++) {
        Boolean charge;
        if (DbGetFill(i, &gXFill) != errNone || !ExportWants(&gXFill, vehicle, year)) continue;
        if (gXFill.vehicle != lastVeh) { VehGet(gXFill.vehicle, &gXVeh); lastVeh = gXFill.vehicle; }
        charge = (gXFill.flags & fillFlagCharge) != 0;
        XlsxRowBegin(&gXw);
        XlsxCellDate(&gXw, gXFill.date);
        if (gXFill.time != fillTimeUnknown) { AppTimeText(b, gXFill.time); XlsxCellText(&gXw, b, xlsxStyleGeneral); } else XlsxCellEmpty(&gXw);
        XlsxCellText(&gXw, gXVeh.name, xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)UnitDistToDisplay(gXFill.odometer, gPrefs.distanceUnit), 1, xlsxStyle1dp);
        if (gXFill.trip) XlsxCellNumber(&gXw, (Int32)UnitDistToDisplay(gXFill.trip, gPrefs.distanceUnit), 1, xlsxStyle1dp); else XlsxCellEmpty(&gXw);
        if (charge) XlsxCellNumber(&gXw, (Int32)gXFill.volume, 3, xlsxStyle2dp); else VolDecimalsCell(&gXw, gXFill.volume);
        if (gXFill.price) XlsxCellNumber(&gXw, (Int32)(charge ? gXFill.price : UnitPriceToDisplay(gXFill.price, gPrefs.priceUnit)), 4, xlsxStyleGeneral); else XlsxCellEmpty(&gXw);
        XlsxCellNumber(&gXw, (Int32)gXFill.total, 2, CurrencyStyle(gXFill.currency));
        XlsxCellText(&gXw, CurrencyCode(gXFill.currency), xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)((gXFill.fxRate ? gXFill.fxRate : fxOne) / 100), 4, xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)CalcHomeCents(gXFill.total, gXFill.fxRate ? gXFill.fxRate : fxOne), 2, CurrencyStyle(gPrefs.homeCurrency));
        XlsxCellText(&gXw, gXFill.payment == fillPaymentCard ? "Card" : (charge ? "Bill" : "Cash"), xlsxStyleGeneral);
        XlsxCellText(&gXw, kGrade[gXFill.grade < 7 ? gXFill.grade : 0], xlsxStyleGeneral);
        XlsxCellText(&gXw, (gXFill.flags & fillFlagFullTank) ? "yes" : "", xlsxStyleGeneral);
        XlsxCellText(&gXw, (gXFill.flags & fillFlagMissedPrev) ? "yes" : "", xlsxStyleGeneral);
        if (gXFill.discount) XlsxCellNumber(&gXw, (Int32)gXFill.discount, 2, xlsxStyle2dp); else XlsxCellEmpty(&gXw);
        for (k = 0; k < gXPgmCount; k++) {
            UInt8 j; Boolean have = false; Int32 e = 0, r = 0;
            for (j = 0; j < gXFill.pointCount; j++) if (gXFill.points[j].programme == gXPgmIds[k]) { have = true; e = gXFill.points[j].earned; r = gXFill.points[j].redeemed; }
            if (have) XlsxCellNumber(&gXw, e, 0, xlsxStyleGeneral); else XlsxCellEmpty(&gXw);
            if (have && r) XlsxCellNumber(&gXw, r, 0, xlsxStyleGeneral); else XlsxCellEmpty(&gXw);
        }
        XlsxCellText(&gXw, gXFill.notes, xlsxStyleGeneral);
        XlsxRowEnd(&gXw);
    }
    XlsxSheetEnd(&gXw);
}

static void MonthlySheet(UInt8 vehicle, UInt16 year)
{
    UInt16 n = DbCount(dbFills), i, firstY = 0, lastY = 0, y;
    UInt8 m, d;
    XlsxSheetBegin(&gXw, 2);
    XlsxRowBegin(&gXw);
    XlsxCellText(&gXw, "Year", xlsxStyleBold); XlsxCellText(&gXw, "Month", xlsxStyleBold); XlsxCellText(&gXw, "Fills", xlsxStyleBold);
    XlsxCellText(&gXw, UnitVolLabel(gPrefs.volumeUnit), xlsxStyleBold);
    XlsxCellText(&gXw, "Spend (home)", xlsxStyleBold); XlsxCellText(&gXw, "Average price", xlsxStyleBold);
    XlsxCellText(&gXw, UnitEcoLabel(gPrefs.economyUnit), xlsxStyleBold); XlsxCellText(&gXw, "Points", xlsxStyleBold);
    XlsxRowEnd(&gXw);
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gXFill) != errNone || !ExportWants(&gXFill, vehicle, year)) continue;
        DateDaysToYmd(gXFill.date, &y, &m, &d);
        if (!firstY) firstY = y;
        lastY = y;
    }
    gXReport = MemPtrNew(sizeof(Report));
    if (!gXReport) { gXw.out->err = -1; XlsxSheetEnd(&gXw); return; }
    for (y = firstY; y && y <= lastY; y++) {
        UInt8 r;
        ReportBegin(gXReport, rptMonthly, y);
        for (i = 0; i < n; i++) {
            if (DbGetFill(i, &gXFill) != errNone || !ExportWants(&gXFill, vehicle, 0)) continue;
            ReportAdd(gXReport, &gXFill);
        }
        for (r = 0; r < 12; r++) {
            const ReportRow *row = &gXReport->rows[r];
            UInt32 eco;
            if (!row->fills) continue;
            XlsxRowBegin(&gXw);
            XlsxCellNumber(&gXw, y, 0, xlsxStyleGeneral);
            XlsxCellText(&gXw, DateMonthName((UInt8)(r + 1)), xlsxStyleGeneral);
            XlsxCellNumber(&gXw, row->fills, 0, xlsxStyleGeneral);
            VolDecimalsCell(&gXw, row->ml);
            XlsxCellNumber(&gXw, (Int32)row->homeCents, 2, CurrencyStyle(gPrefs.homeCurrency));
            if (ReportRowPrice(row)) XlsxCellNumber(&gXw, (Int32)UnitPriceToDisplay(ReportRowPrice(row), gPrefs.priceUnit), 4, xlsxStyleGeneral); else XlsxCellEmpty(&gXw);
            eco = ReportRowEco(row);
            if (eco) XlsxCellNumber(&gXw, (Int32)UnitEcoFuelToDisplay(eco, gPrefs.economyUnit), 1, xlsxStyle1dp); else XlsxCellEmpty(&gXw);
            XlsxCellNumber(&gXw, row->points, 0, xlsxStyleGeneral);
            XlsxRowEnd(&gXw);
        }
    }
    if (gXReport) { MemPtrFree(gXReport); gXReport = NULL; }
    XlsxSheetEnd(&gXw);
}

static void VehiclesSheet(void)
{
    UInt16 n = DbCount(dbVehicles), i;
    static const char *power[] = { "Fuel", "Hybrid", "Plug-in hybrid", "Electric" };
    XlsxSheetBegin(&gXw, 3);
    XlsxRowBegin(&gXw);
    XlsxCellText(&gXw, "Vehicle", xlsxStyleBold); XlsxCellText(&gXw, "Powertrain", xlsxStyleBold); XlsxCellText(&gXw, "Odometer unit", xlsxStyleBold);
    XlsxCellText(&gXw, "Start odometer", xlsxStyleBold); XlsxCellText(&gXw, "Last odometer", xlsxStyleBold); XlsxCellText(&gXw, "Fills", xlsxStyleBold); XlsxCellText(&gXw, "Archived", xlsxStyleBold);
    XlsxRowEnd(&gXw);
    for (i = 0; i < n; i++) {
        if (DbGetVehicle(i, &gXVeh) != errNone) continue;
        XlsxRowBegin(&gXw);
        XlsxCellText(&gXw, gXVeh.name, xlsxStyleGeneral);
        XlsxCellText(&gXw, power[gXVeh.powertrain < 4 ? gXVeh.powertrain : 0], xlsxStyleGeneral);
        XlsxCellText(&gXw, gXVeh.odoUnit == vehOdoMiles ? "mi" : "km", xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)UnitDistToDisplay(gXVeh.startOdo, gPrefs.distanceUnit), 1, xlsxStyle1dp);
        XlsxCellNumber(&gXw, (Int32)UnitDistToDisplay(VehLastOdometer(gXVeh.id), gPrefs.distanceUnit), 1, xlsxStyle1dp);
        DbGetVehicle(i, &gXVeh);
        XlsxCellNumber(&gXw, VehFillCount(gXVeh.id), 0, xlsxStyleGeneral);
        DbGetVehicle(i, &gXVeh);
        XlsxCellText(&gXw, gXVeh.archived ? "yes" : "", xlsxStyleGeneral);
        XlsxRowEnd(&gXw);
    }
    XlsxSheetEnd(&gXw);
}

static void ProgrammesSheet(void)
{
    UInt16 n = DbCount(dbProgrammes), i;
    static const char *basis[] = { "none", "per litre", "per US gal", "per dollar" };
    static const char *redeem[] = { "none", "money off", "per volume" };
    XlsxSheetBegin(&gXw, 4);
    XlsxRowBegin(&gXw);
    XlsxCellText(&gXw, "Programme", xlsxStyleBold); XlsxCellText(&gXw, "Currency", xlsxStyleBold); XlsxCellText(&gXw, "Earn rate", xlsxStyleBold);
    XlsxCellText(&gXw, "Basis", xlsxStyleBold); XlsxCellText(&gXw, "Redeem", xlsxStyleBold); XlsxCellText(&gXw, "Block", xlsxStyleBold);
    XlsxCellText(&gXw, "Value", xlsxStyleBold); XlsxCellText(&gXw, "Opening", xlsxStyleBold); XlsxCellText(&gXw, "Balance", xlsxStyleBold); XlsxCellText(&gXw, "Archived", xlsxStyleBold);
    XlsxRowEnd(&gXw);
    for (i = 0; i < n; i++) {
        Int32 bal;
        if (PgmGetAt(i, &gXPgm) != errNone) continue;
        if (gExportPgm != 255 && gXPgm.id != gExportPgm) continue;
        bal = PgmBalance(gXPgm.id, gXPgm.openingBalance);
        PgmGetAt(i, &gXPgm);
        XlsxRowBegin(&gXw);
        XlsxCellText(&gXw, gXPgm.name, xlsxStyleGeneral);
        XlsxCellText(&gXw, CurrencyCode(gXPgm.currency), xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)gXPgm.earnRate, 2, xlsxStyle2dp);
        XlsxCellText(&gXw, basis[gXPgm.earnBasis < 4 ? gXPgm.earnBasis : 0], xlsxStyleGeneral);
        XlsxCellText(&gXw, redeem[gXPgm.redeemKind < 3 ? gXPgm.redeemKind : 0], xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)gXPgm.redeemBlock, 0, xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)gXPgm.redeemValue, 3, xlsxStyleGeneral);
        XlsxCellNumber(&gXw, gXPgm.openingBalance, PgmIsMoney(&gXPgm) ? 2 : 0, xlsxStyleGeneral);
        XlsxCellNumber(&gXw, bal, PgmIsMoney(&gXPgm) ? 2 : 0, xlsxStyleGeneral);
        XlsxCellText(&gXw, gXPgm.archived ? "yes" : "", xlsxStyleGeneral);
        XlsxRowEnd(&gXw);
    }
    XlsxSheetEnd(&gXw);
}

static void ServicesSheet(UInt8 vehicle)
{
    UInt16 n = DbCount(dbServiceVisits), i, nt = DbCount(dbServiceTypes), t;
    char names[80];
    XlsxSheetBegin(&gXw, 5);
    XlsxRowBegin(&gXw);
    XlsxCellText(&gXw, "Date", xlsxStyleBold); XlsxCellText(&gXw, "Vehicle", xlsxStyleBold);
    XlsxCellText(&gXw, "Odometer", xlsxStyleBold); XlsxCellText(&gXw, "Services", xlsxStyleBold); XlsxCellText(&gXw, "Cost", xlsxStyleBold);
    XlsxCellText(&gXw, "Currency", xlsxStyleBold); XlsxCellText(&gXw, "Payment", xlsxStyleBold); XlsxCellText(&gXw, "Shop", xlsxStyleBold); XlsxCellText(&gXw, "Detail", xlsxStyleBold);
    XlsxRowEnd(&gXw);
    for (i = 0; i < n; i++) {
        UInt32 len;
        if (DbGet(dbServiceVisits, i, (UInt8 *)names, 0, &len) == errNone) { /* size probe only */ }
        {
            static UInt8 packed[svlFixedSize + fillMaxNotes + 8 + recTrailerSize];
            if (DbGet(dbServiceVisits, i, packed, sizeof packed, &len) != errNone || ServiceVisitUnpack(packed, len, &gXVisit) != 0) continue;
        }
        if (vehicle != 255 && gXVisit.vehicle != vehicle) continue;
        VehGet(gXVisit.vehicle, &gXVeh);
        names[0] = 0;
        for (t = 0; t < nt; t++) {
            static UInt8 tp[svtRecordSize + 4 + recTrailerSize];
            if (DbGet(dbServiceTypes, t, tp, sizeof tp, &len) != errNone || ServiceTypeUnpack(tp, len, &gXType) != 0) continue;
            if (gXType.vehicle != gXVisit.vehicle || !(gXVisit.services & (1UL << gXType.id))) continue;
            if (StrLen(names) + StrLen(gXType.name) + 2 >= sizeof names) break;
            if (names[0]) StrCat(names, "; ");
            StrCat(names, gXType.name);
        }
        if ((gXVisit.services & svlOtherBit) && gXVisit.other[0] && StrLen(names) + StrLen(gXVisit.other) + 2 < sizeof names) { if (names[0]) StrCat(names, "; "); StrCat(names, gXVisit.other); }
        XlsxRowBegin(&gXw);
        XlsxCellDate(&gXw, gXVisit.date);
        XlsxCellText(&gXw, gXVeh.name, xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)UnitDistToDisplay(gXVisit.odometer, gPrefs.distanceUnit), 1, xlsxStyle1dp);
        XlsxCellText(&gXw, names, xlsxStyleGeneral);
        XlsxCellNumber(&gXw, (Int32)gXVisit.cost, 2, CurrencyStyle(gXVisit.currency));
        XlsxCellText(&gXw, CurrencyCode(gXVisit.currency), xlsxStyleGeneral);
        XlsxCellText(&gXw, gXVisit.payment == fillPaymentCard ? "Card" : "Cash", xlsxStyleGeneral);
        XlsxCellText(&gXw, gXVisit.shop, xlsxStyleGeneral);
        XlsxCellText(&gXw, gXVisit.detail, xlsxStyleGeneral);
        XlsxRowEnd(&gXw);
    }
    XlsxSheetEnd(&gXw);
}

void ExportXlsxTo(OutStream *out, UInt8 vehicle, UInt16 year)
{
    static const char *sheets[] = { "Fills", "Monthly", "Vehicles", "Programmes", "Services" };
    gCrc = MemPtrNew(256 * sizeof(UInt32));
    if (!gCrc) { out->err = -1; return; }
    LoadProgrammes();
    XlsxBegin(&gXw, out, gCrc);
    XlsxWritePackage(&gXw, sheets, 5);
    FillsSheet(vehicle, year);
    MonthlySheet(vehicle, year);
    VehiclesSheet();
    ProgrammesSheet();
    ServicesSheet(vehicle);
    XlsxEnd(&gXw);
    MemPtrFree(gCrc); gCrc = NULL;
}

void ExportToStream(OutStream *out, UInt8 fmt, UInt8 vehicle, UInt16 year)
{
    if (fmt == 1) ExportXlsxTo(out, vehicle, year);
    else if (fmt == 2) ExportPdfTo(out, vehicle, year);
    else ExportCsvTo(out, vehicle, year);
}

/* ---- PDF ---- */

#define kPdfLeft 36
#define kPdfTop 560
#define kPdfRowH 16
#define kPdfRows 28

static void PdfLineText(Int16 x, Int16 y, const char *label, const char *value)
{
    char b[96];
    StrCopy(b, label); StrCat(b, value);
    PdfText(&gPw, pdfFontRegular, 12, x, y, b);
}

void ExportPdfTo(OutStream *out, UInt8 vehicle, UInt16 year)
{
    char b[64], n1[24], n2[24];
    UInt16 n = DbCount(dbFills), i, rows = 0, pages = 0, count = 0;
    Int16 y;
    static Int16 cols[9] = { 36, 110, 190, 260, 330, 400, 470, 530, 756 };
    static const char *heads[8] = { "Date", "Odometer", "Trip", "Volume", "Price", "Total", "Points", "Notes" };
    UInt8 lastVeh = 255;

    gXStats = MemPtrNew(sizeof(Stats));
    if (!gXStats) { out->err = -1; return; }
    PdfBegin(&gPw, out);

    /* Page 1: summary from the Stats accumulator. */
    StatsBegin(gXStats);
    for (i = 0; i < n; i++) if (DbGetFill(i, &gXFill) == errNone && ExportWants(&gXFill, vehicle, year)) { StatsAdd(gXStats, &gXFill); count++; }
    PdfPageBegin(&gPw);
    StrCopy(b, "FuelLog report: ");
    if (vehicle != 255) { VehGet(vehicle, &gXVeh); StrCat(b, gXVeh.name); } else StrCat(b, "All vehicles");
    if (count) { StrCat(b, ", "); DateShortText(b + StrLen(b), gXStats->firstDate); StrCat(b, " to "); DateShortText(b + StrLen(b), gXStats->lastDate); }
    PdfText(&gPw, pdfFontBold, 18, kPdfLeft, kPdfTop, b);
    y = kPdfTop - 36;
    StrIToA(n1, count); StrCat(n1, " fills"); PdfLineText(kPdfLeft, y, "Fills: ", n1); y -= 20;
    TextFixed(n1, (Int32)(UnitDistToDisplay(gXStats->distance, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands); StrCat(n1, " "); StrCat(n1, UnitDistLabel(gPrefs.distanceUnit));
    PdfLineText(kPdfLeft, y, "Distance: ", n1); y -= 20;
    TextFixed(n1, (Int32)UnitVolToDisplay(gXStats->fuelMl, gPrefs.volumeUnit), UnitVolDecimals(gPrefs.volumeUnit), gPoint, gThousands); StrCat(n1, " "); StrCat(n1, UnitVolLabel(gPrefs.volumeUnit));
    PdfLineText(kPdfLeft, y, "Fuel used: ", n1); y -= 20;
    StrCopy(n1, "$"); TextFixed(n1 + 1, (Int32)gXStats->homeCents, 2, gPoint, gThousands); StrCat(n1, " "); StrCat(n1, CurrencyCode(gPrefs.homeCurrency));
    PdfLineText(kPdfLeft, y, "Spend: ", n1); y -= 20;
    if (StatsAvgEco(gXStats)) {
        TextFixed(n1, (Int32)UnitEcoFuelToDisplay(StatsAvgEco(gXStats), gPrefs.economyUnit), 1, gPoint, 0); StrCat(n1, " "); StrCat(n1, UnitEcoLabel(gPrefs.economyUnit));
        PdfLineText(kPdfLeft, y, "Average economy: ", n1); y -= 20;
        TextFixed(n1, (Int32)UnitEcoFuelToDisplay(gXStats->bestEco, gPrefs.economyUnit), 1, gPoint, 0); StrCat(n1, " / ");
        TextFixed(n2, (Int32)UnitEcoFuelToDisplay(gXStats->worstEco, gPrefs.economyUnit), 1, gPoint, 0); StrCat(n1, n2);
        PdfLineText(kPdfLeft, y, "Best / worst: ", n1); y -= 20;
    }
    if (StatsAvgPrice(gXStats)) {
        TextPrice(n1, UnitPriceToDisplay(StatsAvgPrice(gXStats), gPrefs.priceUnit), gPrefs.priceFormat == unitPriceFormatCents, gPoint);
        StrCat(n1, gPrefs.priceFormat == unitPriceFormatCents ? " c" : " $"); StrCat(n1, UnitPriceLabel(gPrefs.priceUnit));
        PdfLineText(kPdfLeft, y, "Average price: ", n1); y -= 20;
    }
    if (gXStats->distance) { StrCopy(n1, "$"); TextFixed(n1 + 1, (Int32)StatsCostPer100km(gXStats), 2, gPoint, 0); PdfLineText(kPdfLeft, y, "Cost per 100 km: ", n1); y -= 20; }
    {
        UInt32 svc = SvcSpendCents(vehicle);
        if (svc) { StrCopy(n1, "$"); TextFixed(n1 + 1, (Int32)svc, 2, gPoint, gThousands); PdfLineText(kPdfLeft, y, "Service spend: ", n1); y -= 20; }
    }
    PdfText(&gPw, pdfFontRegular, 9, kPdfLeft, 40, "FuelLog for Pocket PC. Log pages follow, then the economy graph.");
    PdfPageEnd(&gPw);

    /* Log pages like the paper form. */
    for (i = 0; i < n && pages < pdfMaxPages - 2; i++) {
        Int16 ty;
        UInt8 k;
        if (DbGetFill(i, &gXFill) != errNone || !ExportWants(&gXFill, vehicle, year)) continue;
        if (rows == 0) {
            pages++;
            PdfPageBegin(&gPw);
            StrCopy(b, "Fuel Log, page "); StrIToA(b + StrLen(b), pages);
            PdfText(&gPw, pdfFontBold, 14, kPdfLeft, kPdfTop, b);
            PdfLineWidth(&gPw, 8);
            PdfRect(&gPw, cols[0], kPdfTop - 20 - kPdfRowH * (kPdfRows + 1), cols[8] - cols[0], kPdfRowH * (kPdfRows + 1));
            PdfLineWidth(&gPw, 4);
            for (k = 0; k <= kPdfRows; k++) PdfLine(&gPw, cols[0], kPdfTop - 20 - kPdfRowH * k, cols[8], kPdfTop - 20 - kPdfRowH * k);
            for (k = 1; k < 8; k++) PdfLine(&gPw, cols[k], kPdfTop - 20, cols[k], kPdfTop - 20 - kPdfRowH * (kPdfRows + 1));
            for (k = 0; k < 8; k++) PdfText(&gPw, pdfFontBold, 10, cols[k] + 4, kPdfTop - 20 - 12, heads[k]);
        }
        ty = kPdfTop - 20 - kPdfRowH * (rows + 1) - 12;
        if (gXFill.vehicle != lastVeh) { VehGet(gXFill.vehicle, &gXVeh); lastVeh = gXFill.vehicle; }
        DateShortText(b, gXFill.date); if (vehicle == 255) { UInt16 l = StrLen(b); b[l] = ' '; StrNCopy(b + l + 1, gXVeh.name, 8); b[l + 9] = 0; }
        PdfText(&gPw, pdfFontRegular, 10, cols[0] + 4, ty, b);
        TextFixed(b, (Int32)(UnitDistToDisplay(gXFill.odometer, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands); PdfText(&gPw, pdfFontRegular, 10, cols[1] + 4, ty, b);
        if (gXFill.trip) { TextFixed(b, (Int32)UnitDistToDisplay(gXFill.trip, gPrefs.distanceUnit), 1, gPoint, 0); PdfText(&gPw, pdfFontRegular, 10, cols[2] + 4, ty, b); }
        if (gXFill.flags & fillFlagCharge) TextFixed(b, (Int32)((gXFill.volume + 50) / 100), 1, gPoint, 0);
        else TextFixed(b, (Int32)UnitVolToDisplay(gXFill.volume, gPrefs.volumeUnit), UnitVolDecimals(gPrefs.volumeUnit), gPoint, 0);
        PdfText(&gPw, pdfFontRegular, 10, cols[3] + 4, ty, b);
        if (gXFill.price) { TextPrice(b, (gXFill.flags & fillFlagCharge) ? gXFill.price : UnitPriceToDisplay(gXFill.price, gPrefs.priceUnit), gPrefs.priceFormat == unitPriceFormatCents, gPoint); PdfText(&gPw, pdfFontRegular, 10, cols[4] + 4, ty, b); }
        TextFixed(b, (Int32)gXFill.total, 2, gPoint, 0); StrCat(b, " "); StrCat(b, CurrencyCode(gXFill.currency)); PdfText(&gPw, pdfFontRegular, 10, cols[5] + 4, ty, b);
        b[0] = 0;
        for (k = 0; k < gXFill.pointCount; k++) { if (k) StrCat(b, " "); StrIToA(b + StrLen(b), gXFill.points[k].earned); }
        PdfText(&gPw, pdfFontRegular, 10, cols[6] + 4, ty, b);
        StrNCopy(b, gXFill.notes, 44); b[44] = 0;
        PdfText(&gPw, pdfFontRegular, 10, cols[7] + 4, ty, b);
        if (++rows == kPdfRows) { rows = 0; PdfPageEnd(&gPw); }
    }
    if (rows) PdfPageEnd(&gPw);

    /* Economy graph: one point per full-to-full segment, fuel chain only. */
    {
        Int16 px[120], py[120]; UInt32 ev[120]; UInt16 ed[120];   /* on the stack: the data segment is nearly full */
        UInt16 pts = 0; UInt32 lo = 0, hi = 0, sum = 0;
        struct { UInt32 lastFullOdo, pending; UInt8 haveFull; } chain[statsMaxId];
        MemSet(chain, sizeof chain, 0);
        for (i = 0; i < n && pts < 120; i++) {
            UInt8 v;
            if (DbGetFill(i, &gXFill) != errNone || !ExportWants(&gXFill, vehicle, year) || (gXFill.flags & fillFlagCharge)) continue;
            v = gXFill.vehicle < statsMaxId ? gXFill.vehicle : 0;
            if (gXFill.flags & fillFlagMissedPrev) { chain[v].haveFull = 0; chain[v].pending = 0; }
            chain[v].pending += gXFill.volume;
            if (gXFill.flags & fillFlagFullTank) {
                if (chain[v].haveFull && gXFill.odometer > chain[v].lastFullOdo) {
                    UInt32 eco = UnitEcoFuelToDisplay(CalcL100kmX10(chain[v].pending, gXFill.odometer - chain[v].lastFullOdo), gPrefs.economyUnit);
                    if (eco) { ev[pts] = eco; ed[pts] = gXFill.date; if (!pts || eco < lo) lo = eco; if (eco > hi) hi = eco; sum += eco; pts++; }
                }
                chain[v].lastFullOdo = gXFill.odometer; chain[v].haveFull = 1; chain[v].pending = 0;
            }
        }
        PdfPageBegin(&gPw);
        StrCopy(b, "Economy, "); StrCat(b, UnitEcoLabel(gPrefs.economyUnit)); StrCat(b, ", per full-to-full segment");
        PdfText(&gPw, pdfFontBold, 14, kPdfLeft, kPdfTop, b);
        if (pts < 2) PdfText(&gPw, pdfFontRegular, 12, kPdfLeft, kPdfTop - 40, "Not enough full fills for a graph yet.");
        else {
            Int16 gx0 = 72, gx1 = 740, gy0 = 80, gy1 = 500;
            UInt32 span = hi - lo; UInt16 k;
            Int32 mean = (Int32)((sum + pts / 2) / pts);
            if (span == 0) span = 1;
            PdfLineWidth(&gPw, 6);
            PdfLine(&gPw, gx0, gy0, gx0, gy1); PdfLine(&gPw, gx0, gy0, gx1, gy0);
            for (k = 0; k < pts; k++) {
                px[k] = (Int16)(gx0 + (Int32)(gx1 - gx0) * (Int32)(ed[k] - ed[0]) / (Int32)((ed[pts - 1] - ed[0]) ? (ed[pts - 1] - ed[0]) : 1));
                py[k] = (Int16)(gy0 + 20 + (Int32)(gy1 - gy0 - 40) * (Int32)(ev[k] - lo) / (Int32)span);
            }
            PdfLineWidth(&gPw, 10);
            for (k = 1; k < pts; k++) PdfLine(&gPw, px[k - 1], py[k - 1], px[k], py[k]);
            for (k = 0; k < pts; k++) PdfRect(&gPw, px[k] - 2, py[k] - 2, 4, 4);
            PdfDash(&gPw, 6, 4); PdfGray(&gPw, 50); PdfLineWidth(&gPw, 6);
            { Int16 my = (Int16)(gy0 + 20 + (Int32)(gy1 - gy0 - 40) * (mean - (Int32)lo) / (Int32)span); PdfLine(&gPw, gx0, my, gx1, my); }
            PdfDash(&gPw, 0, 0); PdfGray(&gPw, 0);
            TextFixed(b, (Int32)hi, 1, gPoint, 0); PdfText(&gPw, pdfFontRegular, 10, 40, gy1 - 24, b);
            TextFixed(b, (Int32)lo, 1, gPoint, 0); PdfText(&gPw, pdfFontRegular, 10, 40, gy0 + 16, b);
            TextFixed(b, mean, 1, gPoint, 0); StrCat(b, " mean"); PdfText(&gPw, pdfFontBold, 10, gx1 - 60, (Int16)(gy0 + 20 + (Int32)(gy1 - gy0 - 40) * (mean - (Int32)lo) / (Int32)span + 4), b);
            DateShortText(b, ed[0]); PdfText(&gPw, pdfFontRegular, 10, gx0, gy0 - 14, b);
            DateShortText(b, ed[pts - 1]); PdfText(&gPw, pdfFontRegular, 10, gx1 - 40, gy0 - 14, b);
        }
        PdfPageEnd(&gPw);
    }
    PdfEnd(&gPw);
    MemPtrFree(gXStats); gXStats = NULL;
}


/* ---- the file ---- */

static Int32 FileWrite(OutStream *s, const void *buf, UInt32 len)
{
    return fwrite(buf, 1, len, (FILE *)s->ctx) == len ? 0 : -1;
}

static Int32 FileSeek(OutStream *s, UInt32 pos)
{
    return fseek((FILE *)s->ctx, (long)pos, SEEK_SET) == 0 ? 0 : -1;
}

void ExportSetup(const FuelPrefs *p, char point, char thousands, int time24)
{
    gPrefs = *p; gPoint = point; gThousands = thousands; gTime24 = time24;
}

void ExportName(char *buf, const char *ext, UInt16 today)
{
    UInt16 y; UInt8 m, d;
    DateDaysToYmd(today, &y, &m, &d);
    strcpy(buf, "FuelLog-");
    StrIToA(buf + strlen(buf), y);
    if (m < 10) strcat(buf, "0");
    StrIToA(buf + strlen(buf), m);
    if (d < 10) strcat(buf, "0");
    StrIToA(buf + strlen(buf), d);
    strcat(buf, ext);
}

Int32 ExportFile(const char *dir, UInt8 fmt, UInt8 vehicle, UInt16 year, UInt8 pgm, UInt16 today, char *nameOut, UInt32 *bytes)
{
    static const char *const ext[3] = { ".csv", ".xlsx", ".pdf" };
    char path[300];
    OutStream out;
    FILE *f;
    int closed;
    if (fmt > 2) fmt = 0;
    ExportName(nameOut, ext[fmt], today);
    if (!DirMake(dir)) return exportErrFolder;
    PathJoin(path, dir, nameOut);
    f = fopen(path, "wb");
    if (!f) return exportErrFolder;
    memset(&out, 0, sizeof out);
    out.write = FileWrite;
    out.seek = FileSeek;
    out.ctx = f;
    gExportPgm = pgm;
    ExportToStream(&out, fmt, vehicle, year);
    gExportPgm = 255;
    closed = fclose(f) == 0;
    *bytes = FileSize(path);
    if (out.err || !closed) return exportErrWrite;
    return exportOk;
}

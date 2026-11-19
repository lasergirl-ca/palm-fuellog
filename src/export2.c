/* XLSX and PDF export on the SP5 and SP7 writers. Its own code section. */
#include "export.h"
#include "card.h"
#include "xlsx.h"
#include "pdf.h"
#include "crc32.h"
#include "stats.h"
#include "report.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "datecalc.h"
#include "vehicles.h"
#include "programmes.h"
#include "service_form.h"
#include "resources.h"

static UInt32 *gCrc;              /* 1 KB, on the heap during an export */
static XlsxWriter gXw;
static PdfWriter gPw;
static CardOut gXOut;
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

static const char *kGrade[7] = {   /* non-const array: a const table in a sectioned file breaks on the device */ "Regular", "Mid", "Premium", "Diesel", "Home AC", "Public AC", "DC fast" };

/* Section placement: see sections.h. */
static UInt8 CurrencyStyle(UInt8 currency) XP_SEC;
static void LoadProgrammes(void) XP_SEC;
static void VolDecimalsCell(XlsxWriter *w, UInt32 ml) XP_SEC;
static void FillsSheet(UInt8 vehicle, UInt16 year) XP_SEC;
static void MonthlySheet(UInt8 vehicle, UInt16 year) XP_SEC;
static void VehiclesSheet(void) XP_SEC;
static void ProgrammesSheet(void) XP_SEC;
static void ServicesSheet(UInt8 vehicle) XP_SEC;
static void PdfLineText(Int16 x, Int16 y, const char *label, const char *value) XP_SEC;

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
            if (DbGetPart(dbServiceTypes, t, tp, sizeof tp, &len) != errNone || ServiceTypeUnpack(tp, len, &gXType) != 0) continue;
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

Err ExportXlsx(UInt16 vol, UInt8 vehicle, UInt16 year, char *nameOut, UInt32 *bytes)
{
    char path[80];
    Err err;
    CardExportName(nameOut, ".xlsx");
    StrCopy(path, cardExport); StrCat(path, "/"); StrCat(path, nameOut);
    err = CardEnsureDirs(vol);
    if (err) return err;
    err = CardOutOpen(&gXOut, vol, path);
    if (err) return err;
    ExportXlsxTo(&gXOut.out, vehicle, year);
    err = CardOutClose(&gXOut);
    *bytes = CardFileSize(vol, path);
    return err;
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
    PdfText(&gPw, pdfFontRegular, 9, kPdfLeft, 40, "FuelLog for Palm OS. Log pages follow, then the economy graph.");
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

/* ---- Service log PDF: one vehicle (or each in turn), its plate, VIN and
 * note, every service type with when it was last done and is next due, then
 * every visit with its note. The same code runs on the Pocket PC. ---- */

#define kSlBottom 44
#define kSlMaxPages (pdfMaxPages - 1)
#define kSlRight 756

typedef struct {
    UInt8 count;
    UInt8 id[svtMaxPerVehicle];
    char name[svtMaxPerVehicle][svtNameLen];
    UInt8 raw[dbMaxRecord];
    UInt8 vraw[dbMaxRecord];
    char note[fillMaxNotes];
    char title[48];
    Int16 y;
    UInt8 table;            /* 0 none, 1 services, 2 visits: column heads to repeat on a new page */
    Boolean open, full;
} SvcLog;

static void SlHeads(SvcLog *s) XP_SEC;
static void SlPage(SvcLog *s) XP_SEC;
static Boolean SlRoom(SvcLog *s, Int16 need) XP_SEC;
static void SlWrap(SvcLog *s, UInt8 font, UInt16 size, Int16 x, const char *text, UInt16 perLine) XP_SEC;
static void SlDist(char *b, UInt32 kmX10) XP_SEC;
static void SlDate(char *b, UInt16 days) XP_SEC;
static Boolean SlLast(SvcLog *s, UInt8 vehicle, UInt8 typeId, UInt16 *date, UInt32 *odo) XP_SEC;
static void SlVehicle(SvcLog *s, UInt16 vIndex) XP_SEC;

static void SlHeads(SvcLog *s)
{
    static Int16 svcX[4] = { kPdfLeft, 220, 360, 520 };
    static Int16 visX[6] = { kPdfLeft, 110, 200, 420, 570, 680 };
    static const char *svcH[4] = { "Service", "Every", "Last done", "Next due" };
    static const char *visH[6] = { "Date", "Odometer", "Services", "Detail", "Shop", "Cost" };
    UInt8 k;
    if (s->table == 1) for (k = 0; k < 4; k++) PdfText(&gPw, pdfFontBold, 10, svcX[k], s->y - 10, svcH[k]);
    else if (s->table == 2) for (k = 0; k < 6; k++) PdfText(&gPw, pdfFontBold, 10, visX[k], s->y - 10, visH[k]);
    else return;
    PdfLineWidth(&gPw, 5);
    PdfLine(&gPw, kPdfLeft, s->y - 14, kSlRight, s->y - 14);
    s->y -= 18;
}

static void SlPage(SvcLog *s)
{
    PdfPageBegin(&gPw);
    s->open = true;
    PdfText(&gPw, pdfFontBold, 16, kPdfLeft, kPdfTop, s->title);
    s->y = kPdfTop - 14;
    SlHeads(s);
}

/* Room for need points more on this page, else a new page (with the column
 * heads again); false once the PDF has as many pages as it can hold. */
static Boolean SlRoom(SvcLog *s, Int16 need)
{
    if (s->full) return false;
    if (s->y - need >= kSlBottom) return true;
    if (gPw.nPages >= kSlMaxPages) {
        PdfText(&gPw, pdfFontRegular, 9, kPdfLeft, 28, "The rest did not fit in one file: export one vehicle at a time.");
        s->full = true;
        return false;
    }
    PdfPageEnd(&gPw);
    SlPage(s);
    return true;
}

/* Text over as many lines as it needs, broken at spaces and at its own line breaks. */
static void SlWrap(SvcLog *s, UInt8 font, UInt16 size, Int16 x, const char *text, UInt16 perLine)
{
    char line[160];
    UInt16 n, cut, i;
    if (perLine > sizeof line - 1) perLine = sizeof line - 1;
    while (*text) {
        for (n = 0; text[n] && text[n] != '\n' && n <= perLine; n++) ;
        if (n > perLine) {
            for (cut = perLine; cut > 0 && text[cut] != ' '; cut--) ;
            if (cut == 0) cut = perLine;
        } else cut = n;
        for (i = 0; i < cut; i++) line[i] = text[i] == '\t' ? ' ' : text[i];
        line[cut] = 0;
        if (!SlRoom(s, (Int16)(size + 3))) return;
        if (cut) PdfText(&gPw, font, size, x, (Int16)(s->y - size), line);
        s->y -= (Int16)(size + 3);
        text += cut;
        if (*text == '\n') text++;
        else while (*text == ' ') text++;
    }
}

static void SlDist(char *b, UInt32 kmX10)
{
    TextFixed(b, (Int32)(UnitDistToDisplay(kmX10, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands);
    StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit));
}

/* "May 14 2026": a service log runs over years. */
static void SlDate(char *b, UInt16 days)
{
    UInt16 y; UInt8 m, d;
    DateShortText(b, days);
    DateDaysToYmd(days, &y, &m, &d);
    StrCat(b, " "); StrIToA(b + StrLen(b), y);
}

/* The latest visit of the vehicle that covered the type. */
static Boolean SlLast(SvcLog *s, UInt8 vehicle, UInt8 typeId, UInt16 *date, UInt32 *odo)
{
    UInt16 n = DbCount(dbServiceVisits), i;
    UInt32 len;
    Boolean found = false;
    for (i = 0; i < n; i++) {
        if (DbGet(dbServiceVisits, i, s->vraw, sizeof s->vraw, &len) != errNone || ServiceVisitUnpack(s->vraw, len, &gXVisit) != 0) continue;
        if (gXVisit.vehicle != vehicle || !(gXVisit.services & (1UL << typeId))) continue;
        if (!found || gXVisit.date >= *date) { *date = gXVisit.date; *odo = gXVisit.odometer; found = true; }
    }
    return found;
}

static void SlVehicle(SvcLog *s, UInt16 vIndex)
{
    char b[120], n[32];
    UInt16 nt = DbCount(dbServiceTypes), nv = DbCount(dbServiceVisits), nf = DbCount(dbFills), i, t, visits = 0;
    UInt32 len, cents = 0, odoNow = 0;
    UInt8 k;
    if (s->full || DbGetVehicle(vIndex, &gXVeh) != errNone) return;
    if (s->open) { PdfPageEnd(&gPw); s->open = false; }
    if (gPw.nPages >= kSlMaxPages) { s->full = true; return; }
    StrCopy(s->title, "Service log: "); StrCat(s->title, gXVeh.name);
    s->table = 0;
    SlPage(s);

    /* Plate, VIN, the odometer now, and the vehicle's note. */
    for (i = nf; i-- > 0; ) if (DbGetFill(i, &gXFill) == errNone && gXFill.vehicle == gXVeh.id) { odoNow = gXFill.odometer; break; }
    for (i = 0; i < nv; i++)
        if (DbGet(dbServiceVisits, i, s->vraw, sizeof s->vraw, &len) == errNone && ServiceVisitUnpack(s->vraw, len, &gXVisit) == 0
            && gXVisit.vehicle == gXVeh.id && gXVisit.odometer > odoNow) odoNow = gXVisit.odometer;
    if (DbGet(dbVehicles, vIndex, s->raw, sizeof s->raw, &len) != errNone) len = 0;
    b[0] = 0;
    RecExtGet(recKindVehicle, s->raw, len, recExtPlate, n, sizeof n);
    if (n[0]) { StrCat(b, "Plate "); StrCat(b, n); }
    RecExtGet(recKindVehicle, s->raw, len, recExtVin, n, sizeof n);
    if (n[0]) { if (b[0]) StrCat(b, "      "); StrCat(b, "VIN "); StrCat(b, n); }
    if (odoNow) { if (b[0]) StrCat(b, "      "); StrCat(b, "Odometer "); SlDist(n, odoNow); StrCat(b, n); }
    if (b[0]) { PdfText(&gPw, pdfFontRegular, 11, kPdfLeft, (Int16)(s->y - 11), b); s->y -= 16; }
    RecExtGet(recKindVehicle, s->raw, len, recExtVehNote, s->note, sizeof s->note);
    if (s->note[0]) SlWrap(s, pdfFontRegular, 10, kPdfLeft, s->note, 150);
    s->y -= 10;

    /* The service types, with when each was last done and is next due. */
    s->count = 0;
    if (SlRoom(s, 40)) {
        PdfText(&gPw, pdfFontBold, 13, kPdfLeft, (Int16)(s->y - 13), "Services");
        s->y -= 20;
        s->table = 1;
        SlHeads(s);
    }
    for (t = 0; t < nt; t++) {
        UInt16 last = 0;
        UInt32 lastOdo = 0;
        Boolean done;
        if (DbGet(dbServiceTypes, t, s->raw, sizeof s->raw, &len) != errNone || ServiceTypeUnpack(s->raw, len, &gXType) != 0) continue;
        if (gXType.vehicle != gXVeh.id) continue;
        if (s->count < svtMaxPerVehicle) { s->id[s->count] = gXType.id; StrCopy(s->name[s->count], gXType.name); s->count++; }
        if (gXType.archived) continue;
        RecExtGet(recKindServiceType, s->raw, len, recExtTypeNote, s->note, sizeof s->note);
        if (!SlRoom(s, 14)) break;
        PdfText(&gPw, pdfFontRegular, 10, kPdfLeft, (Int16)(s->y - 10), gXType.name);
        b[0] = 0;
        if (gXType.intervalDist) SlDist(b, gXType.intervalDist);
        if (gXType.intervalMonths) { if (b[0]) StrCat(b, " or "); StrIToA(b + StrLen(b), gXType.intervalMonths); StrCat(b, " months"); }
        if (!b[0]) StrCopy(b, "no interval");
        PdfText(&gPw, pdfFontRegular, 10, 220, (Int16)(s->y - 10), b);
        done = SlLast(s, gXVeh.id, gXType.id, &last, &lastOdo);
        if (done) { SlDate(b, last); StrCat(b, ", "); SlDist(n, lastOdo); StrCat(b, n); }
        else StrCopy(b, "not yet");
        PdfText(&gPw, pdfFontRegular, 10, 360, (Int16)(s->y - 10), b);
        b[0] = 0;
        if (done && gXType.intervalMonths) SlDate(b, DateAddMonths(last, gXType.intervalMonths));
        if (done && gXType.intervalDist) { if (b[0]) StrCat(b, " or "); SlDist(n, lastOdo + gXType.intervalDist); StrCat(b, n); }
        if (b[0]) PdfText(&gPw, pdfFontRegular, 10, 520, (Int16)(s->y - 10), b);
        s->y -= 14;
        if (s->note[0]) SlWrap(s, pdfFontRegular, 9, kPdfLeft + 14, s->note, 150);
    }
    if (s->count == 0 && SlRoom(s, 14)) { PdfText(&gPw, pdfFontRegular, 10, kPdfLeft, (Int16)(s->y - 10), "No services set up for this vehicle."); s->y -= 14; }
    s->table = 0;
    s->y -= 10;

    /* Every visit, oldest first, its note under it. */
    if (SlRoom(s, 40)) {
        PdfText(&gPw, pdfFontBold, 13, kPdfLeft, (Int16)(s->y - 13), "Visits");
        s->y -= 20;
        s->table = 2;
        SlHeads(s);
    }
    for (i = 0; i < nv && !s->full; i++) {
        if (DbGet(dbServiceVisits, i, s->vraw, sizeof s->vraw, &len) != errNone || ServiceVisitUnpack(s->vraw, len, &gXVisit) != 0) continue;
        if (gXVisit.vehicle != gXVeh.id) continue;
        if (!SlRoom(s, 14)) break;
        visits++;
        cents += CalcHomeCents(gXVisit.cost, gXVisit.fxRate ? gXVisit.fxRate : fxOne);
        SlDate(b, gXVisit.date);
        PdfText(&gPw, pdfFontRegular, 10, kPdfLeft, (Int16)(s->y - 10), b);
        SlDist(b, gXVisit.odometer);
        PdfText(&gPw, pdfFontRegular, 10, 110, (Int16)(s->y - 10), b);
        b[0] = 0;
        for (k = 0; k < s->count; k++) {
            if (!(gXVisit.services & (1UL << s->id[k]))) continue;
            if (StrLen(b) + StrLen(s->name[k]) + 2 >= 44) { StrCat(b, "..."); break; }
            if (b[0]) StrCat(b, ", ");
            StrCat(b, s->name[k]);
        }
        if ((gXVisit.services & svlOtherBit) && gXVisit.other[0] && StrLen(b) + StrLen(gXVisit.other) + 2 < 44) { if (b[0]) StrCat(b, ", "); StrCat(b, gXVisit.other); }
        PdfText(&gPw, pdfFontRegular, 10, 200, (Int16)(s->y - 10), b);
        PdfText(&gPw, pdfFontRegular, 10, 420, (Int16)(s->y - 10), gXVisit.detail);
        PdfText(&gPw, pdfFontRegular, 10, 570, (Int16)(s->y - 10), gXVisit.shop);
        if (gXVisit.cost) {
            TextFixed(b, (Int32)gXVisit.cost, 2, gPoint, gThousands); StrCat(b, " "); StrCat(b, CurrencyCode(gXVisit.currency));
            PdfText(&gPw, pdfFontRegular, 10, 680, (Int16)(s->y - 10), b);
        }
        s->y -= 14;
        if (gXVisit.notes[0]) SlWrap(s, pdfFontRegular, 9, kPdfLeft + 14, gXVisit.notes, 150);
    }
    s->table = 0;
    if (!s->full && SlRoom(s, 24)) {
        s->y -= 6;
        PdfLineWidth(&gPw, 5);
        PdfLine(&gPw, kPdfLeft, s->y, kSlRight, s->y);
        if (visits) {
            StrIToA(b, visits); StrCat(b, visits == 1 ? " visit, " : " visits, ");
            TextFixed(b + StrLen(b), (Int32)cents, 2, gPoint, gThousands); StrCat(b, " "); StrCat(b, CurrencyCode(gPrefs.homeCurrency)); StrCat(b, " in all");
        } else StrCopy(b, "No visits logged yet.");
        PdfText(&gPw, pdfFontBold, 10, kPdfLeft, (Int16)(s->y - 14), b);
        s->y -= 18;
    }
}

/* vehicle 255 = every vehicle, each from a new page. */
void ExportServicePdfTo(OutStream *out, UInt8 vehicle)
{
    SvcLog *s = MemPtrNew(sizeof(SvcLog));
    UInt16 n = DbCount(dbVehicles), i;
    if (!s) { out->err = -1; return; }
    MemSet(s, sizeof(SvcLog), 0);
    PdfBegin(&gPw, out);
    for (i = 0; i < n; i++) {
        if (DbGetVehicle(i, &gXVeh) != errNone) continue;
        if (vehicle != 255 ? gXVeh.id != vehicle : gXVeh.archived) continue;
        SlVehicle(s, i);
    }
    if (!s->open && gPw.nPages == 0) {
        StrCopy(s->title, "Service log");
        SlPage(s);
        PdfText(&gPw, pdfFontRegular, 12, kPdfLeft, (Int16)(s->y - 12), "No vehicle to show.");
    }
    if (s->open) PdfPageEnd(&gPw);
    PdfEnd(&gPw);
    MemPtrFree(s);
}

Err ExportServicePdf(UInt16 vol, UInt8 vehicle, char *nameOut, UInt32 *bytes)
{
    char path[80];
    Err err;
    CardExportName(nameOut, "-service.pdf");
    StrCopy(path, cardExport); StrCat(path, "/"); StrCat(path, nameOut);
    err = CardEnsureDirs(vol);
    if (err) return err;
    err = CardOutOpen(&gXOut, vol, path);
    if (err) return err;
    ExportServicePdfTo(&gXOut.out, vehicle);
    err = CardOutClose(&gXOut);
    *bytes = CardFileSize(vol, path);
    return err;
}

Err ExportPdf(UInt16 vol, UInt8 vehicle, UInt16 year, char *nameOut, UInt32 *bytes)
{
    char path[80];
    Err err;
    CardExportName(nameOut, ".pdf");
    StrCopy(path, cardExport); StrCat(path, "/"); StrCat(path, nameOut);
    err = CardEnsureDirs(vol);
    if (err) return err;
    err = CardOutOpen(&gXOut, vol, path);
    if (err) return err;
    ExportPdfTo(&gXOut.out, vehicle, year);
    err = CardOutClose(&gXOut);
    *bytes = CardFileSize(vol, path);
    return err;
}

#include "reports_form.h"
#include "report.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "datecalc.h"
#include "vehicles.h"
#include "service_form.h"
#include "resources.h"

#ifndef vchrRockerLeft
#define vchrRockerLeft  0x0134
#define vchrRockerRight 0x0135
#endif

static Report gRpt;                  /* 700 bytes: static */
static FillRecord gRptFill;
static char gRVNames[vehMaxVehicles + 1][vehNameLen];
static char *gRVPtrs[vehMaxVehicles + 1];
static UInt8 gRVIds[vehMaxVehicles + 1];
static UInt8 gRptMode;
static UInt16 gRptYear;              /* monthly: the year; yearly: the first year shown */
static RectangleType gRptArea;
static Int16 gRptScroll, gRptHeight;
static char gRptYearText[14];
static UInt16 gRptFrom, gRptTo;
static char gRptFromText[12], gRptToText[12];
static VehicleRecord gRptVeh;
#define kRptRow 11
#define kRptTop 29

/* Section placement: see sections.h. */
static void ComputeReport(void) RPT_SEC;
static void DrawRightAt(const char *s, Coord right, Coord y) RPT_SEC;
static const char *EcoHeader(void) RPT_SEC;
static void RowText(const ReportRow *row, const char *label, Coord y, Boolean bold) RPT_SEC;
static void DrawReport(void) RPT_SEC;
static void YearLabel(FormType *form) RPT_SEC;
static void LayoutReport(FormType *form) RPT_SEC;
static void BuildRptPopup(FormType *form) RPT_SEC;
static void StepYear(FormType *form, Int16 dir) RPT_SEC;

static void ComputeReport(void)
{
    UInt16 n = DbCount(dbFills), i;
    ReportBegin(&gRpt, gRptMode, gRptYear);
    if (gRptMode == rptRange) ReportSetRange(&gRpt, gRptFrom, gRptTo);
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gRptFill) != errNone) continue;
        if (gPrefs.currentVehicle != 255 && gRptFill.vehicle != gPrefs.currentVehicle) continue;
        ReportAdd(&gRpt, &gRptFill);
    }
}

static void DrawRightAt(const char *s, Coord right, Coord y) { WinDrawChars(s, StrLen(s), right - FntCharsWidth(s, StrLen(s)), y); }

static const char *EcoHeader(void)
{
    if (gPrefs.economyUnit == unitEcoMpgUS || gPrefs.economyUnit == unitEcoMpgImp) return "MPG";
    if (gPrefs.economyUnit == unitEcoKmPerL) return "KPL";
    return "L/100";
}

static void RowText(const ReportRow *row, const char *label, Coord y, Boolean bold)
{
    char b[20];
    UInt32 eco = ReportRowEco(row), shown, div = 1;
    UInt8 d = UnitVolDecimals(gPrefs.volumeUnit);
    FntSetFont(bold ? boldFont : stdFont);
    WinDrawChars(label, StrLen(label), 4, y);
    StrIToA(b, row->fills); DrawRightAt(b, 44, y);
    shown = UnitVolToDisplay(row->ml, gPrefs.volumeUnit);
    while (d-- > 1) div *= 10;
    TextFixed(b, (Int32)((shown + div / 2) / div), 1, gPoint, 0); DrawRightAt(b, 78, y);
    TextFixed(b, (Int32)row->homeCents, 2, gPoint, gThousands); DrawRightAt(b, 118, y);
    if (eco) { TextFixed(b, (Int32)UnitEcoFuelToDisplay(eco, gPrefs.economyUnit), 1, gPoint, 0); DrawRightAt(b, 150, y); }
}

static void DrawReport(void)
{
    char b[56];
    Coord y = (Coord)(kRptTop - gRptScroll);
    UInt8 i, shown = 0;

    WinEraseRectangle(&gRptArea, 0);
    WinSetClip(&gRptArea);
    FntSetFont(boldFont);
    if (gRptMode == rptRange) WinDrawChars("Range", 5, 4, y);
    else WinDrawChars(gRptMode == rptMonthly ? "Month" : "Year", gRptMode == rptMonthly ? 5 : 4, 4, y);
    DrawRightAt("#", 44, y);
    DrawRightAt(UnitVolLabel(gPrefs.volumeUnit), 78, y);
    DrawRightAt(CurrencySymbol(gPrefs.homeCurrency), 118, y);
    DrawRightAt(EcoHeader(), 150, y);
    y += kRptRow + 1;
    for (i = 0; i < rptMaxRows; i++) {
        if (gRpt.rows[i].fills == 0) continue;
        if (gRptMode == rptRange) StrCopy(b, "Period");
        else if (gRptMode == rptMonthly) StrCopy(b, DateMonthName((UInt8)(i + 1))); else StrIToA(b, gRptYear + i);
        RowText(&gRpt.rows[i], b, y, false);
        y += kRptRow; shown++;
    }
    if (shown == 0) { FntSetFont(stdFont); WinDrawChars("No fills in this period.", 24, 4, y); y += kRptRow; }
    WinDrawLine(4, (Coord)(y + 1), 150, (Coord)(y + 1)); y += 3;
    RowText(&gRpt.total, "Total", y, true); y += kRptRow + 4;
    if (gRpt.total.dkm) {
        FntSetFont(stdFont);
        StrCopy(b, "Distance "); TextFixed(b + 9, (Int32)(UnitDistToDisplay(gRpt.total.dkm, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands);
        StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit));
        if (ReportRowPrice(&gRpt.total)) {
            StrCat(b, ", avg ");
            TextPrice(b + StrLen(b), UnitPriceToDisplay(ReportRowPrice(&gRpt.total), gPrefs.priceUnit), gPrefs.priceFormat == unitPriceFormatCents, gPoint);
            StrCat(b, gPrefs.priceFormat == unitPriceFormatCents ? " c" : " $"); StrCat(b, UnitPriceLabel(gPrefs.priceUnit));
        }
        WinDrawChars(b, StrLen(b), 4, y); y += kRptRow + 2;
    }
    if (gRpt.total.points) {
        FntSetFont(stdFont);
        StrCopy(b, "Points net "); TextFixed(b + 11, gRpt.total.points, 0, gPoint, gThousands);
        WinDrawChars(b, StrLen(b), 4, y); y += kRptRow + 2;
    }
    if (gPrefs.currentVehicle == 255) {
        /* Per-vehicle breakdown: the same columns, one row per car with fills in the period. */
        UInt8 v; Boolean any = false;
        for (v = 1; v < rptMaxId; v++) if (gRpt.byVehicle[v].fills) any = true;
        if (any) {
            FntSetFont(boldFont); WinDrawChars("By vehicle", 10, 4, y); y += kRptRow;
            for (v = 1; v < rptMaxId; v++) {
                if (!gRpt.byVehicle[v].fills) continue;
                VehGet(v, &gRptVeh);
                StrNCopy(b, gRptVeh.name, 9); b[9] = 0;
                RowText(&gRpt.byVehicle[v], b, y, false);
                y += kRptRow;
            }
            y += 3;
        }
    }
    FntSetFont(boldFont); WinDrawChars("By payment", 10, 4, y); y += kRptRow;
    FntSetFont(stdFont);
    WinDrawChars("Cash", 4, 12, y); TextFixed(b, (Int32)gRpt.cashCents, 2, gPoint, gThousands); DrawRightAt(b, 118, y); y += kRptRow;
    WinDrawChars("Card", 4, 12, y); TextFixed(b, (Int32)gRpt.cardCents, 2, gPoint, gThousands); DrawRightAt(b, 118, y); y += kRptRow + 3;
    FntSetFont(boldFont); WinDrawChars("By currency", 11, 4, y); y += kRptRow;
    FntSetFont(stdFont);
    for (i = 0; i < 4; i++) {
        if (!gRpt.curCents[i]) continue;
        WinDrawChars(CurrencyCode(i), 3, 12, y);
        TextFixed(b, (Int32)gRpt.curCents[i], 2, gPoint, gThousands); DrawRightAt(b, 118, y);
        y += kRptRow;
    }
    WinResetClip();
    gRptHeight = (Int16)(y + gRptScroll - gRptArea.topLeft.y + 2);
    AppListScrollBar(FrmGetActiveForm(), RptScrollBar, gRptScroll, (Int16)(gRptHeight - gRptArea.extent.y), gRptArea.extent.y,
                     gRptArea.topLeft.y, (Coord)(gRptArea.topLeft.y + gRptArea.extent.y));
}

/* Section placement: see sections.h. */
static void ShowObj(FormType *form, UInt16 id, Boolean show) RPT_SEC;
static void PickRangeDate(FormType *form, Boolean from) RPT_SEC;

static void ShowObj(FormType *form, UInt16 id, Boolean show)
{
    UInt16 idx = FrmGetObjectIndex(form, id);
    if (show) FrmShowObject(form, idx); else FrmHideObject(form, idx);
}

static void YearLabel(FormType *form)
{
    Boolean range = gRptMode == rptRange;
    if (gRptMode == rptMonthly) StrIToA(gRptYearText, gRptYear);
    else if (gRptMode == rptYearly) { StrIToA(gRptYearText, gRptYear); StrCat(gRptYearText, "-"); StrIToA(gRptYearText + StrLen(gRptYearText), (gRptYear + rptMaxRows - 1) % 100); }
    else gRptYearText[0] = 0;
    AppSetLabel(form, RptYearLabel, gRptYearText);
    ShowObj(form, RptPrevButton, !range); ShowObj(form, RptNextButton, !range); ShowObj(form, RptYearLabel, !range);
    if (range) {
        DateShortText(gRptFromText, gRptFrom); DateShortText(gRptToText, gRptTo);
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, RptFromTrigger)), gRptFromText);
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, RptToTrigger)), gRptToText);
    }
    ShowObj(form, RptFromTrigger, range); ShowObj(form, RptToTrigger, range);
}

/* A date picker for one end of the range; keeps from <= to. */
static void PickRangeDate(FormType *form, Boolean from)
{
    UInt16 y; UInt8 m, d; Int16 yy, mm, dd;
    UInt16 *target = from ? &gRptFrom : &gRptTo;
    DateDaysToYmd(*target, &y, &m, &d);
    yy = (Int16)y; mm = m; dd = d;
    if (SelectDay(selectDayByDay, &mm, &dd, &yy, from ? "Range from" : "Range to")) {
        *target = DateYmdToDays((UInt16)yy, (UInt8)mm, (UInt8)dd);
        if (gRptFrom > gRptTo) { if (from) gRptTo = gRptFrom; else gRptFrom = gRptTo; }
        YearLabel(form);
        gRptScroll = 0; ComputeReport(); DrawReport();
    }
}

static void LayoutReport(FormType *form)
{
    static UInt16 bottom[] = { RptDoneButton, RptGraphsButton, RptExportButton };
    Coord w, h;
    AppFitForm(form, bottom, 3);
    WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
    gRptArea.topLeft.x = 0; gRptArea.topLeft.y = kRptTop; gRptArea.extent.x = w; gRptArea.extent.y = (Coord)(h - 17 - kRptTop);
}

static void BuildRptPopup(FormType *form)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, RptVehicleList));
    Int16 sel;
    VehBuildChoices(lst, gRVNames, gRVPtrs, gRVIds, true, 255, gPrefs.currentVehicle, &sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, RptVehicleTrigger)), gRVNames[sel]);
}

static void StepYear(FormType *form, Int16 dir)
{
    if (gRptMode == rptRange) {
        /* step the range by its own length */
        UInt16 len = (UInt16)(gRptTo - gRptFrom + 1);
        if (dir < 0 && gRptFrom >= len) { gRptFrom -= len; gRptTo -= len; }
        else if (dir > 0) { gRptFrom += len; gRptTo += len; }
        YearLabel(form); gRptScroll = 0; ComputeReport(); DrawReport();
        return;
    }
    gRptYear = (UInt16)(gRptYear + dir * (gRptMode == rptMonthly ? 1 : rptMaxRows));
    YearLabel(form);
    gRptScroll = 0;
    ComputeReport();
    DrawReport();
}

Boolean ReportsFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent: {
        UInt16 y; UInt8 m, d;
        DateDaysToYmd(AppToday(), &y, &m, &d);
        if (gRptYear == 0) { gRptMode = rptMonthly; gRptYear = y; }
        if (gRptTo == 0) { gRptTo = AppToday(); gRptFrom = (UInt16)(gRptTo - 90); }
        gRptScroll = 0;
        LayoutReport(form);
        BuildRptPopup(form);
        {
            ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, RptPeriodList));
            LstSetSelection(lst, gRptMode);
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, RptPeriodTrigger)), LstGetSelectionText(lst, gRptMode));
        }
        YearLabel(form);
        ComputeReport();
        FrmDrawForm(form);
        DrawReport();
        return true;
    }
    case frmUpdateEvent: FrmDrawForm(form); DrawReport(); return true;
    case winDisplayChangedEvent: LayoutReport(form); gRptScroll = 0; WinEraseWindow(); FrmDrawForm(form); DrawReport(); return true;
    case sclRepeatEvent: case sclExitEvent: {
        Int16 v;
        if (AppListScrollEvent(event, &v)) { gRptScroll = v < 0 ? 0 : v; DrawReport(); return true; }
        break;
    }
    case keyDownEvent: {
        WChar c = event->data.keyDown.chr;
        Boolean page;
        Int16 dir = AppScrollDirection(event, &page), step = page ? 60 : 22;
        Int16 room = gRptHeight - gRptArea.extent.y - gRptScroll;
        if (c == vchrRockerLeft) { StepYear(form, -1); return true; }
        if (c == vchrRockerRight) { StepYear(form, 1); return true; }
        if (dir > 0 && room > 0) { gRptScroll += room < step ? room : step; DrawReport(); return true; }
        if (dir < 0 && gRptScroll > 0) { gRptScroll -= gRptScroll < step ? gRptScroll : step; DrawReport(); return true; }
        break;
    }
    case popSelectEvent:
        if (event->data.popSelect.listID == RptVehicleList) {
            gPrefs.currentVehicle = gRVIds[event->data.popSelect.selection];
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, RptVehicleTrigger)), gRVNames[event->data.popSelect.selection]);
            gRptScroll = 0; ComputeReport(); DrawReport();
            return true;
        }
        if (event->data.popSelect.listID == RptPeriodList) {
            UInt16 y; UInt8 m, d;
            gRptMode = (UInt8)event->data.popSelect.selection;
            DateDaysToYmd(AppToday(), &y, &m, &d);
            if (gRptMode != rptRange) gRptYear = gRptMode == rptMonthly ? y : (UInt16)(y - rptMaxRows + 1);
            YearLabel(form);
            gRptScroll = 0; ComputeReport(); DrawReport();
            break;   /* let the trigger update its label */
        }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case RptPrevButton: StepYear(form, -1); return true;
        case RptNextButton: StepYear(form, 1); return true;
        case RptFromTrigger: PickRangeDate(form, true); return true;
        case RptToTrigger: PickRangeDate(form, false); return true;
        case RptDoneButton: FrmGotoForm(StatsForm); return true;
        case RptGraphsButton: FrmGotoForm(GraphsForm); return true;
        case RptExportButton: FrmGotoForm(ExportForm); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

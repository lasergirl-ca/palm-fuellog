/* The fill list: home base. One row per fill, newest first, drawn by hand
 * so numbers line up on the decimal and the economy column is bold. */
#include "fill_list.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "units.h"
#include "text.h"
#include "datecalc.h"
#include "resources.h"
#include "vehicles.h"
#include "service_form.h"

#define kRowH 11
#define kTop gTop        /* first row, below the title, the header and any due banner */
static Coord gTop = 26;
static char gBanner[44];      /* "Oil and filter due in 640 km", or empty */
static UInt8 gBannerStatus;
static UInt16 gFirstRow;  /* index into the newest-first order */
static Int16  gSel = -1;  /* 5-way highlighted row, or -1 */
static FillRecord gRowFill, gScanFill;   /* static: keeps the big records off the stack */
static char gVehNames[vehMaxVehicles + 1][vehNameLen];
static char *gVehNamePtrs[vehMaxVehicles + 1];
static UInt8 gVehIds[vehMaxVehicles + 1];
static UInt16 gVehCount;
static UInt16 gVisible[2048 / 2];   /* fill indexes shown, newest first (capped) */
static UInt16 gVisibleCount;
static RectangleType gRows;

/* Column right edges in standard coordinates. */
static const Coord kColDate = 4, kColOdo = 68, kColVol = 94, kColTotal = 126, kColEco = 150;   /* the scrollbar takes the last 8 px */
static VehicleRecord gCurVeh;   /* the selected vehicle, for the headers */

static void DrawRight(const char *s, Coord right, Coord y)
{
    WinDrawChars(s, StrLen(s), right - FntCharsWidth(s, StrLen(s)), y);
}

/* The list keeps economy in the "distance per unit" style, which fits a
 * three-letter header: MPG or KPL for fuel (the Prefs L/100 km choice shows
 * as km/L here), MPE or KPE for an electric car (miles or km per kWh). */
static Boolean EcoMiles(void) { return gPrefs.economyUnit == unitEcoMpgUS || gPrefs.economyUnit == unitEcoMpgImp; }

static const char *EcoHeader(Boolean electric)
{
    if (electric) return gPrefs.distanceUnit == unitDistMiles ? "MPE" : "KPE";
    return EcoMiles() ? "MPG" : "KPL";
}

/* eco is L/100 km x 10 for fuel, kWh/100 km x 10 for a charge; returns the
 * list figure x 10. */
static UInt32 EcoDisplay(UInt32 eco, Boolean charge)
{
    if (eco == 0) return 0;
    if (charge) {
        /* km per kWh x 10 = 10000 / (kWh/100 km x 10); miles divide by 1.609 */
        if (gPrefs.distanceUnit == unitDistMiles) return CalcMulDiv(10000UL, 1000UL, eco * 1609UL);
        return (10000UL + eco / 2) / eco;
    }
    return UnitEcoFuelToDisplay(eco, gPrefs.economyUnit == unitEcoL100km ? unitEcoKmPerL : gPrefs.economyUnit);
}

/* Economy for the fill at database index k (oldest first): volume since the
 * previous full fill over the distance, or 0 for a partial or a break. */
static UInt32 RowEconomy(UInt16 k, FillRecord *f)
{
    FillRecord *gp = &gScanFill;
    UInt32 volume = f->volume;
    Int32 j;
    if (!(f->flags & fillFlagFullTank) || (f->flags & fillFlagMissedPrev)) return 0;
    for (j = (Int32)k - 1; j >= 0; j--) {
        if (DbGetFill((UInt16)j, gp) != errNone) return 0;
        if (gp->vehicle != f->vehicle) continue;
        if (gp->flags & fillFlagFullTank) {
            if (gp->odometer >= f->odometer) return 0;
            if (f->flags & fillFlagCharge) return CalcKwh100kmX10(volume, f->odometer - gp->odometer);
            return CalcL100kmX10(volume, f->odometer - gp->odometer);
        }
        if (gp->flags & fillFlagMissedPrev) return 0;
        volume += gp->volume;
    }
    return 0;
}

/* Which fills show: all, or the current vehicle's. Newest first. */
static void Collect(void)
{
    UInt16 n = DbCount(dbFills), i;
    gVisibleCount = 0;
    for (i = n; i > 0 && gVisibleCount < 1024; i--) {
        if (gPrefs.currentVehicle != 255) {
            if (DbGetFill(i - 1, &gScanFill) != errNone || gScanFill.vehicle != gPrefs.currentVehicle) continue;
        }
        gVisible[gVisibleCount++] = i - 1;
    }
}

/* The vehicle popup: live vehicles then "All vehicles". */
static void BuildVehiclePopup(FormType *form)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillListVehicleList));
    Int16 sel;
    gVehCount = VehBuildChoices(lst, gVehNames, gVehNamePtrs, gVehIds, true, 255, gPrefs.currentVehicle, &sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillListVehicleTrigger)), gVehNames[sel]);
}

static void DrawRows(void)
{
    UInt16 count, i, rows;
    Coord y = kTop;
    char buf[24];
    FillRecord *fp = &gRowFill;

    Boolean electric = gPrefs.currentVehicle != 255 && VehGet(gPrefs.currentVehicle, &gCurVeh) && gCurVeh.powertrain == vehPowerElectric;
    Coord colVol = kColVol, colTotal = kColTotal;
    Collect();
    count = gVisibleCount;
    WinEraseRectangle(&gRows, 0);
    if (gTop == 37) {
        RectangleType br;
        br.topLeft.x = 0; br.topLeft.y = 15; br.extent.x = 160; br.extent.y = 11;
        WinEraseRectangle(&br, 0);
        FntSetFont(stdFont);
        WinDrawChars(gBanner, StrLen(gBanner), 4, 15);
    }
    FntSetFont(boldFont);
    WinDrawChars("Date", 4, kColDate, kTop - 11);
    {
        /* The odometer figures are wider than their title: centre "Odo" over a six-digit reading. */
        Coord figures, title;
        FntSetFont(stdFont); figures = FntCharsWidth("000,000", 7);
        FntSetFont(boldFont); title = FntCharsWidth("Odo", 3);
        WinDrawChars("Odo", 3, (Coord)(kColOdo - figures / 2 - title / 2), kTop - 11);
    }
    DrawRight(electric ? "kWh" : UnitVolLabel(gPrefs.volumeUnit), colVol, kTop - 11);
    DrawRight("Total", colTotal, kTop - 11);
    DrawRight(EcoHeader(electric), kColEco, kTop - 11);
    rows = (UInt16)((gRows.extent.y) / kRowH);
    if (gSel >= (Int16)count) gSel = (Int16)count - 1;
    if (count == 0) {
        FntSetFont(stdFont);
        WinDrawChars("No fills yet. Tap New to log one.", 33, 8, y + 20);
        AppListScrollBar(FrmGetActiveForm(), FillListScrollBar, 0, 0, 1, gRows.topLeft.y, (Coord)(gRows.topLeft.y + gRows.extent.y));
        return;
    }
    for (i = 0; i < rows && gFirstRow + i < count; i++, y += kRowH) {
        UInt16 k = gVisible[gFirstRow + i];   /* newest first */
        UInt32 eco;
        Boolean hi = (Int16)(gFirstRow + i) == gSel;
        if (DbGetFill(k, fp) != errNone) continue;
        if (hi) {
            RectangleType sr;
            sr.topLeft.x = 0; sr.topLeft.y = y; sr.extent.x = 160; sr.extent.y = kRowH;
            AppRowHighlight(&sr);
        } else if (i & 1) {
            RectangleType stripe;
            stripe.topLeft.x = 0; stripe.topLeft.y = y; stripe.extent.x = 160; stripe.extent.y = kRowH;
            WinSetBackColor(UIColorGetTableEntryIndex(UIFieldBackground));
            WinSetForeColor(UIColorGetTableEntryIndex(UIObjectFill));
            WinFillRectangle(&stripe, 0);
            WinSetForeColor(UIColorGetTableEntryIndex(UIObjectForeground));
        }
        FntSetFont(stdFont);
        DateShortText(buf, fp->date);
        WinDrawChars(buf, StrLen(buf), kColDate, y);
        TextFixed(buf, (Int32)(UnitDistToDisplay(fp->odometer, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands);
        DrawRight(buf, kColOdo, y);
        {
            /* The list shows one decimal of volume to save width; the form keeps them all. */
            UInt32 shown = UnitVolToDisplay(fp->volume, gPrefs.volumeUnit), div = 1;
            UInt8 d = UnitVolDecimals(gPrefs.volumeUnit);
            while (d-- > 1) div *= 10;
            TextFixed(buf, (Int32)((shown + div / 2) / div), 1, gPoint, 0);
        }
        if (fp->flags & fillFlagCharge) TextFixed(buf, (Int32)((fp->volume + 50) / 100), 1, gPoint, 0);
        DrawRight(buf, colVol, y);
        TextFixed(buf, (Int32)fp->total, 2, gPoint, 0);
        DrawRight(buf, colTotal, y);
        eco = RowEconomy(k, fp);
        FntSetFont(boldFont);
        if (eco) {
            TextFixed(buf, (Int32)EcoDisplay(eco, (fp->flags & fillFlagCharge) != 0), 1, gPoint, 0);
            DrawRight(buf, kColEco, y);
        } else {
            FntSetFont(stdFont);
            DrawRight((fp->flags & fillFlagFullTank) ? "" : "partial", kColEco, y);
        }
        if (hi) AppRowPlain();
    }
    AppListScrollBar(FrmGetActiveForm(), FillListScrollBar, (Int16)gFirstRow, (Int16)((Int16)count - (Int16)rows), (Int16)rows,
                     gRows.topLeft.y, (Coord)(gRows.topLeft.y + gRows.extent.y));
}


static void DrawFooter(void)
{
    UInt16 count = DbCount(dbFills), i;
    UInt32 fullVol = 0, firstFullOdo = 0, lastFullOdo = 0, monthCents = 0, eco;
    UInt16 y, today = AppToday();
    UInt8 m, d, tm, td;
    FillRecord *fp = &gRowFill;
    char buf[40], num[16];
    Coord fy = gRows.topLeft.y + gRows.extent.y + 1;
    RectangleType r;
    /* The average covers fuel, or charges when the selected car is electric; never both. */
    Boolean electric = gPrefs.currentVehicle != 255 && VehGet(gPrefs.currentVehicle, &gCurVeh) && gCurVeh.powertrain == vehPowerElectric;

    DateDaysToYmd(today, &y, &tm, &td);
    for (i = 0; i < count; i++) {
        UInt16 fy2;
        if (DbGetFill(i, fp) != errNone) continue;
        if (gPrefs.currentVehicle != 255 && fp->vehicle != gPrefs.currentVehicle) continue;
        DateDaysToYmd(fp->date, &fy2, &m, &d);
        if (fy2 == y && m == tm) monthCents += CalcHomeCents(fp->total, fp->fxRate);   /* fuel and electricity */
        if (((fp->flags & fillFlagCharge) != 0) != electric) continue;
        if (fp->flags & fillFlagFullTank) {
            if (firstFullOdo == 0) firstFullOdo = fp->odometer; else fullVol += fp->volume;
            lastFullOdo = fp->odometer;
        } else if (firstFullOdo) {
            fullVol += fp->volume;
        }
    }
    r.topLeft.x = 0; r.topLeft.y = fy; r.extent.x = 160; r.extent.y = 11;
    WinEraseRectangle(&r, 0);
    FntSetFont(stdFont);
    eco = (lastFullOdo > firstFullOdo) ? CalcL100kmX10(fullVol, lastFullOdo - firstFullOdo) : 0;
    if (eco) {
        StrCopy(buf, "Avg ");
        /* CalcL100kmX10 and CalcKwh100kmX10 scale alike: Wh over 0.1 km */
        TextFixed(num, (Int32)EcoDisplay(eco, electric), 1, gPoint, 0);
        StrCat(buf, num); StrCat(buf, " "); StrCat(buf, EcoHeader(electric));
        WinDrawChars(buf, StrLen(buf), 4, fy);
    }
    StrCopy(buf, DateMonthName(tm));
    StrCat(buf, " $");
    TextFixed(num, (Int32)monthCents, 2, gPoint, gThousands);
    StrCat(buf, num);
    DrawRight(buf, 150, fy);
}

static void Layout(FormType *form)
{
    static const UInt16 bottom[] = { FillListNewButton, FillListStatsButton, FillListServiceButton };
    Coord w, h;
    /* The most urgent service for the vehicle shown; Soon or Overdue earns a banner line. */
    gBannerStatus = SvcBanner(gPrefs.currentVehicle, gBanner);
    gTop = (gBannerStatus == svcStatusSoon || gBannerStatus == svcStatusOverdue) ? 37 : 26;
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillListServiceButton)), gTop == 37 ? "Service!" : "Service");
    AppFitForm(form, bottom, 3);
    WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
    gRows.topLeft.x = 0; gRows.topLeft.y = kTop;
    gRows.extent.x = w; gRows.extent.y = (Coord)(((h - 17 - 12 - kTop) / kRowH) * kRowH);
}

static void DrawAll(FormType *form)
{
    BuildVehiclePopup(form);
    FrmDrawForm(form);
    DrawRows();
    DrawFooter();
}

Boolean FillListHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        Layout(form);
        DrawAll(form);
        return true;
    case frmUpdateEvent:
        DrawAll(form);
        return true;
    case winDisplayChangedEvent:
        gDisplayChanges++;
        Layout(form);
        WinEraseWindow();
        DrawAll(form);
        return true;
    case penDownEvent:
        if (RctPtInRectangle(event->screenX, event->screenY, &gRows)) {
            UInt16 row = (UInt16)((event->screenY - gRows.topLeft.y) / kRowH) + gFirstRow;
            if (row < gVisibleCount) {
                gSel = (Int16)row;
                gEditIndex = gVisible[row];
                FrmGotoForm(FillForm);
                return true;
            }
        }
        break;
    case sclRepeatEvent: case sclExitEvent: {
        Int16 v;
        if (AppListScrollEvent(event, &v)) { gFirstRow = (UInt16)(v < 0 ? 0 : v); DrawRows(); return true; }
        break;
    }
    case keyDownEvent: {
        UInt16 rows = (UInt16)(gRows.extent.y / kRowH), count = gVisibleCount;
        {
            Int16 rk = AppRowKey(event, &gSel, &gFirstRow, count, rows);
            if (rk == 2) { gEditIndex = gVisible[gSel]; FrmGotoForm(FillForm); return true; }
            if (rk == 1) { DrawRows(); return true; }
        }
        {
            Boolean page;
            Int16 dir = AppScrollDirection(event, &page);
            UInt16 step = page ? rows : 1;
            if (dir > 0) { if (gFirstRow + rows < count) { gFirstRow = (UInt16)(gFirstRow + step + rows > count ? count - rows : gFirstRow + step); DrawRows(); } return true; }
            if (dir < 0) { gFirstRow = gFirstRow > step ? gFirstRow - step : 0; DrawRows(); return true; }
        }
        if (event->data.keyDown.chr == vchrPageDown) {
            if (gFirstRow + rows < count) { gFirstRow += rows; DrawRows(); }
            return true;
        }
        if (event->data.keyDown.chr == vchrPageUp) {
            gFirstRow = gFirstRow > rows ? gFirstRow - rows : 0;
            DrawRows();
            return true;
        }
        break;
    }
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case FillListNewButton:
            gEditIndex = dmMaxRecordIndex;
            FrmGotoForm(FillForm);
            return true;
        case FillListStatsButton:
            FrmGotoForm(StatsForm);
            return true;
        case FillListServiceButton:
            FrmGotoForm(ServiceForm);
            return true;
        default: break;
        }
        break;
    case popSelectEvent:
        if (event->data.popSelect.listID == FillListVehicleList) {
            gPrefs.currentVehicle = gVehIds[event->data.popSelect.selection];
            PrefsSave(&gPrefs);
            gFirstRow = 0; gSel = -1;
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillListVehicleTrigger)), gVehNames[event->data.popSelect.selection]);
            DrawRows(); DrawFooter();
            return true;
        }
        break;
    case menuEvent:
        if (event->data.menu.itemID == MenuPrefs) { FrmGotoForm(PrefsForm); return true; }
        if (event->data.menu.itemID == MenuVehicles) { FrmGotoForm(VehiclesForm); return true; }
        if (event->data.menu.itemID == MenuProgrammes) { FrmGotoForm(ProgrammesForm); return true; }
        if (event->data.menu.itemID == MenuExport) { FrmGotoForm(ExportForm); return true; }
        if (event->data.menu.itemID == MenuDiagnostics) { FrmGotoForm(DiagForm); return true; }
        if (event->data.menu.itemID == MenuAbout) {
            /* Diagnostics for the layout: display vs window height and DIA changes seen. */
            char diag[56];
            Coord w, h;
            WinGetDisplayExtent(&w, &h);
            StrCopy(diag, "display "); StrIToA(diag + StrLen(diag), h);
            StrCat(diag, ", window "); StrIToA(diag + StrLen(diag), AppFormHeight(form));
            StrCat(diag, ", changes "); StrIToA(diag + StrLen(diag), gDisplayChanges);
            if (AppClockShift()) { StrCat(diag, AppClockIsDateFix() ? ", DateFix +" : ", clock +"); StrIToA(diag + StrLen(diag), AppClockShift() / 365); StrCat(diag, "y"); }
            /* \353 is e-diaeresis in the Palm character set */
            FrmCustomAlert(AboutAlert, FUELLOG_BUILD, "Zo\353 Leto and Andrew Culwell", diag);
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

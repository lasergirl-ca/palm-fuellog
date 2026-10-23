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

#define kRowH 11
#define kTop 26          /* first row, below the title and header */
static UInt16 gFirstRow;  /* index into the newest-first order */
static FillRecord gRowFill, gScanFill;   /* static: keeps the big records off the stack */
static char gVehNames[vehMaxVehicles + 1][vehNameLen];
static char *gVehNamePtrs[vehMaxVehicles + 1];
static UInt8 gVehIds[vehMaxVehicles + 1];
static UInt16 gVehCount;
static UInt16 gVisible[2048 / 2];   /* fill indexes shown, newest first (capped) */
static UInt16 gVisibleCount;
static RectangleType gRows;

/* Column right edges in standard coordinates. */
static const Coord kColDate = 4, kColOdo = 68, kColVol = 94, kColTotal = 122, kColEco = 156;
static VehicleRecord gCurVeh;   /* the selected vehicle, for the headers */

static void DrawRight(const char *s, Coord right, Coord y)
{
    WinDrawChars(s, StrLen(s), right - FntCharsWidth(s, StrLen(s)), y);
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
    /* An electric car's kWh/100 header is wide: pull the kWh and Total columns in a little. */
    Coord colVol = electric ? kColVol - 4 : kColVol, colTotal = electric ? kColTotal - 6 : kColTotal;
    Collect();
    count = gVisibleCount;
    WinEraseRectangle(&gRows, 0);
    FntSetFont(boldFont);
    WinDrawChars("Date", 4, kColDate, kTop - 11);
    DrawRight("Odo", kColOdo, kTop - 11);
    DrawRight(electric ? "kWh" : UnitVolLabel(gPrefs.volumeUnit), colVol, kTop - 11);
    DrawRight("Total", colTotal, kTop - 11);
    /* The economy header sits in the last column; an electric car's is too long for bold. */
    if (electric) { FntSetFont(stdFont); DrawRight("kWh/100", kColEco, kTop - 11); FntSetFont(boldFont); }
    else DrawRight(gPrefs.economyUnit == unitEcoL100km ? "L/100" : UnitEcoLabel(gPrefs.economyUnit), kColEco, kTop - 11);
    rows = (UInt16)((gRows.extent.y) / kRowH);
    if (count == 0) {
        FntSetFont(stdFont);
        WinDrawChars("No fills yet. Tap New to log one.", 33, 8, y + 20);
        return;
    }
    for (i = 0; i < rows && gFirstRow + i < count; i++, y += kRowH) {
        UInt16 k = gVisible[gFirstRow + i];   /* newest first */
        UInt32 eco;
        if (DbGetFill(k, fp) != errNone) continue;
        if (i & 1) {
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
        TextFixed(buf, (Int32)UnitVolToDisplay(fp->volume, gPrefs.volumeUnit), UnitVolDecimals(gPrefs.volumeUnit), gPoint, 0);
        if (fp->flags & fillFlagCharge) TextFixed(buf, (Int32)(fp->volume / 100), 1, gPoint, 0);
        DrawRight(buf, colVol, y);
        TextFixed(buf, (Int32)fp->total, 2, gPoint, 0);
        DrawRight(buf, colTotal, y);
        eco = RowEconomy(k, fp);
        FntSetFont(boldFont);
        if (eco) {
            /* A charge's economy is kWh/100 km as it stands; fuel follows the economy unit. */
            if (fp->flags & fillFlagCharge) TextFixed(buf, (Int32)eco, 1, gPoint, 0);
            else TextFixed(buf, (Int32)UnitEcoFuelToDisplay(eco, gPrefs.economyUnit), 1, gPoint, 0);
            DrawRight(buf, kColEco, y);
        } else {
            FntSetFont(stdFont);
            DrawRight((fp->flags & fillFlagFullTank) ? "" : "partial", kColEco, y);
        }
    }
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
        if (electric) {   /* CalcL100kmX10 and CalcKwh100kmX10 scale alike: Wh over 0.1 km */
            TextFixed(num, (Int32)eco, 1, gPoint, 0);
            StrCat(buf, num); StrCat(buf, " kWh/100 km");
        } else {
            TextFixed(num, (Int32)UnitEcoFuelToDisplay(eco, gPrefs.economyUnit), 1, gPoint, 0);
            StrCat(buf, num); StrCat(buf, " "); StrCat(buf, UnitEcoLabel(gPrefs.economyUnit));
        }
        WinDrawChars(buf, StrLen(buf), 4, fy);
    }
    StrCopy(buf, DateMonthName(tm));
    StrCat(buf, " $");
    TextFixed(num, (Int32)monthCents, 2, gPoint, gThousands);
    StrCat(buf, num);
    DrawRight(buf, 156, fy);
}

static void Layout(FormType *form)
{
    static const UInt16 bottom[] = { FillListNewButton, FillListStatsButton, FillListServiceButton };
    Coord w, h;
    AppFitForm(form, bottom, 3);
    WinGetDisplayExtent(&w, &h);
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
        Layout(form);
        WinEraseWindow();
        DrawAll(form);
        return true;
    case penDownEvent:
        if (RctPtInRectangle(event->screenX, event->screenY, &gRows)) {
            UInt16 row = (UInt16)((event->screenY - gRows.topLeft.y) / kRowH) + gFirstRow;
            if (row < gVisibleCount) {
                gEditIndex = gVisible[row];
                FrmGotoForm(FillForm);
                return true;
            }
        }
        break;
    case keyDownEvent: {
        UInt16 rows = (UInt16)(gRows.extent.y / kRowH), count = gVisibleCount;
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
        case FillListServiceButton:
            FrmAlert(NotYetAlert);
            return true;
        default: break;
        }
        break;
    case popSelectEvent:
        if (event->data.popSelect.listID == FillListVehicleList) {
            gPrefs.currentVehicle = gVehIds[event->data.popSelect.selection];
            PrefsSave(&gPrefs);
            gFirstRow = 0;
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillListVehicleTrigger)), gVehNames[event->data.popSelect.selection]);
            DrawRows(); DrawFooter();
            return true;
        }
        break;
    case menuEvent:
        if (event->data.menu.itemID == MenuPrefs) { FrmGotoForm(PrefsForm); return true; }
        if (event->data.menu.itemID == MenuVehicles) { FrmGotoForm(VehiclesForm); return true; }
        if (event->data.menu.itemID == MenuDiagnostics) { FrmGotoForm(DiagForm); return true; }
        if (event->data.menu.itemID == MenuAbout) { FrmCustomAlert(AboutAlert, FUELLOG_BUILD, "", ""); return true; }
        break;
    default:
        break;
    }
    return false;
}

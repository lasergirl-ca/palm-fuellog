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

#define kRowH 11
#define kTop 26          /* first row, below the title and header */
static UInt16 gFirstRow;  /* index into the newest-first order */
static FillRecord gRowFill, gScanFill;   /* static: the 68k stack is 4 KB */
static RectangleType gRows;

/* Column right edges in standard coordinates. */
static const Coord kColDate = 4, kColOdo = 60, kColVol = 88, kColTotal = 120, kColEco = 156;

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
            return CalcL100kmX10(volume, f->odometer - gp->odometer);
        }
        if (gp->flags & fillFlagMissedPrev) return 0;
        volume += gp->volume;
    }
    return 0;
}

static void DrawRows(void)
{
    UInt16 count = DbCount(dbFills), i, rows;
    Coord y = kTop;
    char buf[24];
    FillRecord *fp = &gRowFill;

    WinEraseRectangle(&gRows, 0);
    FntSetFont(boldFont);
    WinDrawChars("Date", 4, kColDate, kTop - 11);
    DrawRight("Odo", kColOdo, kTop - 11);
    DrawRight(UnitVolLabel(gPrefs.volumeUnit), kColVol, kTop - 11);
    DrawRight("Total", kColTotal, kTop - 11);
    /* The economy header sits in the last column; everything else is right-aligned on its edge. */
    DrawRight(gPrefs.economyUnit == unitEcoL100km ? "L/100" : UnitEcoLabel(gPrefs.economyUnit), kColEco, kTop - 11);
    rows = (UInt16)((gRows.extent.y) / kRowH);
    if (count == 0) {
        FntSetFont(stdFont);
        WinDrawChars("No fills yet. Tap New to log one.", 33, 8, y + 20);
        return;
    }
    for (i = 0; i < rows && gFirstRow + i < count; i++, y += kRowH) {
        UInt16 k = count - 1 - (gFirstRow + i);   /* newest first */
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
        DrawRight(buf, kColVol, y);
        TextFixed(buf, (Int32)fp->total, 2, gPoint, 0);
        DrawRight(buf, kColTotal, y);
        eco = (fp->flags & fillFlagCharge) ? 0 : RowEconomy(k, fp);
        FntSetFont(boldFont);
        if (eco) {
            TextFixed(buf, (Int32)UnitEcoFuelToDisplay(eco, gPrefs.economyUnit), 1, gPoint, 0);
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

    DateDaysToYmd(today, &y, &tm, &td);
    for (i = 0; i < count; i++) {
        UInt16 fy2;
        if (DbGetFill(i, fp) != errNone) continue;
        if (fp->flags & fillFlagCharge) continue;
        if (fp->flags & fillFlagFullTank) {
            if (firstFullOdo == 0) firstFullOdo = fp->odometer; else fullVol += fp->volume;
            lastFullOdo = fp->odometer;
        } else if (firstFullOdo) {
            fullVol += fp->volume;
        }
        DateDaysToYmd(fp->date, &fy2, &m, &d);
        if (fy2 == y && m == tm) monthCents += CalcHomeCents(fp->total, fp->fxRate);
    }
    r.topLeft.x = 0; r.topLeft.y = fy; r.extent.x = 160; r.extent.y = 11;
    WinEraseRectangle(&r, 0);
    FntSetFont(stdFont);
    eco = (lastFullOdo > firstFullOdo) ? CalcL100kmX10(fullVol, lastFullOdo - firstFullOdo) : 0;
    if (eco) {
        StrCopy(buf, "Avg ");
        TextFixed(num, (Int32)UnitEcoFuelToDisplay(eco, gPrefs.economyUnit), 1, gPoint, 0);
        StrCat(buf, num); StrCat(buf, " "); StrCat(buf, UnitEcoLabel(gPrefs.economyUnit));
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
            UInt16 count = DbCount(dbFills);
            if (row < count) {
                gEditIndex = count - 1 - row;
                FrmGotoForm(FillForm);
                return true;
            }
        }
        break;
    case keyDownEvent: {
        UInt16 rows = (UInt16)(gRows.extent.y / kRowH), count = DbCount(dbFills);
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
    case menuEvent:
        if (event->data.menu.itemID == MenuDiagnostics) { FrmGotoForm(DiagForm); return true; }
        if (event->data.menu.itemID == MenuAbout) { FrmAlert(AboutAlert); return true; }
        break;
    default:
        break;
    }
    return false;
}

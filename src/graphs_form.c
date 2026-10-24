/* Graphs: economy per full-to-full segment, price per fill, or spend per
 * month, over time; one line per vehicle under All vehicles. Drawn at
 * double density where the screen allows; a tap names the nearest point. */
#include "reports_form.h"
#include "stats.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "datecalc.h"
#include "vehicles.h"
#include "device.h"
#include "resources.h"

#define grEconomy 0
#define grPrice   1
#define grSpend   2
#define kGrMax 256
#define kGrSeries 8

static FillRecord gGrFill;
static VehicleRecord gGrVeh;
static char gGVNames[vehMaxVehicles + 1][vehNameLen];
static char *gGVPtrs[vehMaxVehicles + 1];
static UInt8 gGVIds[vehMaxVehicles + 1];
static UInt8 gGrKind;
static Int32 gGrX[kGrMax], gGrY[kGrMax];     /* x: days or month index; y: display value x 10 (or cents) */
static UInt8 gGrS[kGrMax];                    /* series (vehicle slot) per point */
static UInt16 gGrN;
static UInt8 gGrSeriesId[kGrSeries], gGrSeriesCount;
static char gGrSeriesName[kGrSeries][vehNameLen];
static Int32 gGrMin, gGrMax, gGrMean, gGrXMin, gGrXMax;
static Int16 gGrSelected = -1;
static RectangleType gGrPlot;                 /* standard coordinates */
static struct { UInt32 lastFullOdo, pending; UInt8 haveFull; } gGrChain[statsMaxId][2];
static UInt32 gGrMonthCents[kGrMax];          /* spend per month, by month index from gGrFirstMonth */
static UInt16 gGrFirstMonth;

/* Section placement: see sections.h. */
static UInt8 SeriesFor(UInt8 vehicle) RPT_SEC;
static void AddPoint(Int32 x, Int32 y, UInt8 s) RPT_SEC;
static void Collect(void) RPT_SEC;
static void ValueText(char *b, Int32 v) RPT_SEC;
static Coord PY(Int32 y, Coord s) RPT_SEC;
static void SetRGB(UInt8 r, UInt8 g, UInt8 b) RPT_SEC;
static void SeriesColour(UInt8 s) RPT_SEC;
static void DrawGraph(void) RPT_SEC;
static void LayoutGraph(FormType *form) RPT_SEC;
static void BuildGrPopup(FormType *form) RPT_SEC;

static UInt8 SeriesFor(UInt8 vehicle)
{
    UInt8 i;
    for (i = 0; i < gGrSeriesCount; i++) if (gGrSeriesId[i] == vehicle) return i;
    if (gGrSeriesCount >= kGrSeries) return 0;
    gGrSeriesId[gGrSeriesCount] = vehicle;
    VehGet(vehicle, &gGrVeh);
    StrCopy(gGrSeriesName[gGrSeriesCount], gGrVeh.name);
    return gGrSeriesCount++;
}

static void AddPoint(Int32 x, Int32 y, UInt8 s)
{
    if (gGrN >= kGrMax) return;
    gGrX[gGrN] = x; gGrY[gGrN] = y; gGrS[gGrN] = s; gGrN++;
}

/* Economy and price values in the display unit x 10; spend in cents. */
static void Collect(void)
{
    UInt16 n = DbCount(dbFills), i;
    Boolean all = gPrefs.currentVehicle == 255;
    UInt16 months = 0;
    gGrN = 0; gGrSeriesCount = 0; gGrSelected = -1;
    MemSet(gGrChain, sizeof gGrChain, 0);
    if (gGrKind == grSpend) MemSet(gGrMonthCents, sizeof gGrMonthCents, 0);
    gGrFirstMonth = 0;
    for (i = 0; i < n; i++) {
        UInt8 v, c, s;
        if (DbGetFill(i, &gGrFill) != errNone) continue;
        if (!all && gGrFill.vehicle != gPrefs.currentVehicle) continue;
        v = gGrFill.vehicle < statsMaxId ? gGrFill.vehicle : 0;
        c = (gGrFill.flags & fillFlagCharge) ? 1 : 0;
        s = (all && gGrKind != grSpend) ? SeriesFor(gGrFill.vehicle) : 0;   /* spend pools the fleet */
        if (gGrKind == grSpend) {
            UInt16 y; UInt8 m, d; UInt16 idx;
            DateDaysToYmd(gGrFill.date, &y, &m, &d);
            idx = (UInt16)(y * 12 + m - 1);
            if (gGrFirstMonth == 0) gGrFirstMonth = idx;
            if (idx >= gGrFirstMonth && idx - gGrFirstMonth < kGrMax) {
                gGrMonthCents[idx - gGrFirstMonth] += CalcHomeCents(gGrFill.total, gGrFill.fxRate ? gGrFill.fxRate : fxOne);
                if (idx - gGrFirstMonth + 1 > months) months = (UInt16)(idx - gGrFirstMonth + 1);
            }
            continue;
        }
        if (gGrKind == grPrice) {
            if (!c && gGrFill.price)
                AddPoint(gGrFill.date, (Int32)UnitPriceToDisplay(gGrFill.price, gPrefs.priceUnit), s);
            continue;
        }
        /* economy: segments closing at a full fill */
        if (gGrFill.flags & fillFlagMissedPrev) { gGrChain[v][c].haveFull = 0; gGrChain[v][c].pending = 0; }
        gGrChain[v][c].pending += gGrFill.volume;
        if (gGrFill.flags & fillFlagFullTank) {
            if (gGrChain[v][c].haveFull && gGrFill.odometer > gGrChain[v][c].lastFullOdo) {
                UInt32 dist = gGrFill.odometer - gGrChain[v][c].lastFullOdo;
                UInt32 eco = c ? CalcKwh100kmX10(gGrChain[v][c].pending, dist) : CalcL100kmX10(gGrChain[v][c].pending, dist);
                if (eco) AddPoint(gGrFill.date, (Int32)(c ? eco : UnitEcoFuelToDisplay(eco, gPrefs.economyUnit)), s);
            }
            gGrChain[v][c].lastFullOdo = gGrFill.odometer; gGrChain[v][c].haveFull = 1; gGrChain[v][c].pending = 0;
        }
    }
    if (gGrKind == grSpend) { for (i = 0; i < months; i++) AddPoint(i, (Int32)gGrMonthCents[i], 0); }
    /* range and mean */
    if (gGrN) {
        Int32 sum = 0;
        gGrMin = gGrMax = gGrY[0]; gGrXMin = gGrXMax = gGrX[0];
        for (i = 0; i < gGrN; i++) {
            if (gGrY[i] < gGrMin) gGrMin = gGrY[i];
            if (gGrY[i] > gGrMax) gGrMax = gGrY[i];
            if (gGrX[i] < gGrXMin) gGrXMin = gGrX[i];
            if (gGrX[i] > gGrXMax) gGrXMax = gGrX[i];
            sum += gGrY[i];
        }
        gGrMean = (sum + (Int32)gGrN / 2) / (Int32)gGrN;
        if (gGrMax == gGrMin) { gGrMax += 1; gGrMin -= 1; }
        if (gGrXMax == gGrXMin) gGrXMax++;
    }
}

static void ValueText(char *b, Int32 v)
{
    if (gGrKind == grSpend) { StrCopy(b, "$"); TextFixed(b + 1, v, 2, gPoint, 0); return; }
    if (gGrKind == grPrice) { TextPrice(b, (UInt32)v, gPrefs.priceFormat == unitPriceFormatCents, gPoint); return; }
    TextFixed(b, v, 1, gPoint, 0);
}

static Coord PX(Int32 x, Coord s)   /* plot coordinates at scale s */
{
    Int32 w = (Int32)(gGrPlot.extent.x - 1) * s;
    return (Coord)(gGrPlot.topLeft.x * s + (Coord)((x - gGrXMin) * w / (gGrXMax - gGrXMin)));
}
static Coord PY(Int32 y, Coord s)
{
    Int32 h = (Int32)(gGrPlot.extent.y - 1) * s;
    Int32 lo = gGrMin - (gGrMax - gGrMin) / 10, hi = gGrMax + (gGrMax - gGrMin) / 10;
    if (hi == lo) hi = lo + 1;
    return (Coord)((gGrPlot.topLeft.y + gGrPlot.extent.y - 1) * s - (Coord)((y - lo) * h / (hi - lo)));
}

static void SetRGB(UInt8 r, UInt8 g, UInt8 b)
{
    RGBColorType c; c.index = 0; c.r = r; c.g = g; c.b = b;
    WinSetForeColor(WinRGBToIndex(&c));
}

static void SeriesColour(UInt8 s)
{
    static UInt8 pal[kGrSeries][3] = { {0x00,0x55,0xCC}, {0xCC,0x33,0x00}, {0x00,0x88,0x33}, {0x88,0x33,0xAA}, {0xCC,0x88,0x00}, {0x00,0x88,0x88}, {0x66,0x66,0x66}, {0x00,0x00,0x00} };
    SetRGB(pal[s][0], pal[s][1], pal[s][2]);
}

static void DrawGraph(void)
{
    char b[64];
    Coord s = DeviceHasHiRes() ? 2 : 1, y0, y1, x0, x1;
    UInt16 oldSys = 0, i;
    RectangleType r;
    Coord h = AppFormHeight(FrmGetActiveForm());

    r.topLeft.x = 0; r.topLeft.y = 28; r.extent.x = 160; r.extent.y = (Coord)(h - 17 - 28);
    WinEraseRectangle(&r, 0);
    gGrPlot.topLeft.x = 30; gGrPlot.topLeft.y = (Coord)(gGrSeriesCount > 1 ? 40 : 30);
    gGrPlot.extent.x = 118; gGrPlot.extent.y = (Coord)(h - 17 - 24 - gGrPlot.topLeft.y);
    FntSetFont(stdFont);
    if (gGrN < (gGrKind == grEconomy ? 1 : 1)) {
        WinDrawChars("Nothing to plot yet.", 20, 30, 60);
        if (gGrKind == grEconomy) WinDrawChars("Economy needs two full fills.", 29, 14, 72);
        return;
    }
    /* legend under All vehicles */
    if (gGrSeriesCount > 1) {
        Coord x = 4;
        for (i = 0; i < gGrSeriesCount && x < 150; i++) {
            RectangleType sw;
            WinPushDrawState(); SeriesColour((UInt8)i);
            sw.topLeft.x = x; sw.topLeft.y = 30; sw.extent.x = 6; sw.extent.y = 6; WinPaintRectangle(&sw, 0);
            WinPopDrawState();
            WinDrawChars(gGrSeriesName[i], StrLen(gGrSeriesName[i]) > 9 ? 9 : StrLen(gGrSeriesName[i]), (Coord)(x + 8), 28);
            x += 8 + FntCharsWidth(gGrSeriesName[i], StrLen(gGrSeriesName[i]) > 9 ? 9 : StrLen(gGrSeriesName[i])) + 6;
        }
    }
    WinPushDrawState();
    if (s == 2) oldSys = WinSetCoordinateSystem(kCoordinatesDouble);
    x0 = (Coord)(gGrPlot.topLeft.x * s); x1 = (Coord)((gGrPlot.topLeft.x + gGrPlot.extent.x - 1) * s);
    y0 = (Coord)(gGrPlot.topLeft.y * s); y1 = (Coord)((gGrPlot.topLeft.y + gGrPlot.extent.y - 1) * s);
    /* axes and the dashed mean */
    SetRGB(0x66, 0x66, 0x66);
    WinPaintLine(x0, y0, x0, y1); WinPaintLine(x0, y1, x1, y1);
    {
        Coord ym = PY(gGrMean, s), x;
        for (x = x0; x < x1; x += (Coord)(4 * s)) WinPaintLine(x, ym, (Coord)(x + 2 * s), ym);
    }
    /* lines and dots per series */
    for (i = 0; i < gGrSeriesCount || (i == 0 && gGrSeriesCount == 0); i++) {
        UInt16 k; Boolean have = false; Coord px = 0, py = 0;
        if (gGrSeriesCount > 1) SeriesColour((UInt8)i); else SetRGB(0x00, 0x55, 0xCC);
        for (k = 0; k < gGrN; k++) {
            Coord x, y;
            if (gGrS[k] != i) continue;
            x = PX(gGrX[k], s); y = PY(gGrY[k], s);
            if (have) { WinPaintLine(px, py, x, y); if (s == 2) WinPaintLine(px, (Coord)(py + 1), x, (Coord)(y + 1)); }
            r.topLeft.x = (Coord)(x - s); r.topLeft.y = (Coord)(y - s); r.extent.x = (Coord)(2 * s + 1); r.extent.y = (Coord)(2 * s + 1);
            WinPaintRectangle(&r, (UInt16)s);
            px = x; py = y; have = true;
        }
    }
    if (s == 2) WinSetCoordinateSystem(oldSys);
    WinPopDrawState();
    /* y labels: max, mean, min at the left */
    FntSetFont(stdFont);
    ValueText(b, gGrMax); WinDrawChars(b, StrLen(b), 2, (Coord)(PY(gGrMax, 1) - 5));
    ValueText(b, gGrMin); WinDrawChars(b, StrLen(b), 2, (Coord)(PY(gGrMin, 1) - 5));
    FntSetFont(boldFont);
    ValueText(b, gGrMean); WinDrawChars(b, StrLen(b), 2, (Coord)(PY(gGrMean, 1) - 5));
    FntSetFont(stdFont);
    /* x labels: month names along the bottom */
    {
        Coord ly = (Coord)(gGrPlot.topLeft.y + gGrPlot.extent.y + 2);
        if (gGrKind == grSpend) {
            UInt16 m, step = (UInt16)((gGrXMax - gGrXMin) / 5 + 1);
            for (m = 0; (Int32)m <= gGrXMax; m += step) {
                UInt16 abs = (UInt16)(gGrFirstMonth + m);
                WinDrawChars(DateMonthName((UInt8)(abs % 12 + 1)), 3, (Coord)(PX(m, 1) - 8), ly);
            }
        } else {
            UInt16 y, yy; UInt8 m, d, mm;
            Int32 days;
            UInt16 months, step, k = 0, drawn = 0;
            DateDaysToYmd((UInt16)gGrXMin, &y, &m, &d);
            DateDaysToYmd((UInt16)gGrXMax, &yy, &mm, &d);
            months = (UInt16)((yy * 12 + mm) - (y * 12 + m) + 1);
            step = (UInt16)(months / 5 + 1);
            for (; (y * 12 + m) <= (yy * 12 + mm); m++, k++) {
                if (m > 12) { m = 1; y++; }
                days = DateYmdToDays(y, m, 1);
                if (days < gGrXMin || days > gGrXMax || (k % step)) continue;
                WinDrawChars(DateMonthName(m), 3, (Coord)(PX(days, 1) - 8), ly);
                drawn++;
            }
            if (drawn < 2) {
                /* Under a month of data: name the first and last dates instead. */
                DateShortText(b, (UInt16)gGrXMin);
                WinDrawChars(b, StrLen(b), gGrPlot.topLeft.x, ly);
                DateShortText(b, (UInt16)gGrXMax);
                WinDrawChars(b, StrLen(b), (Coord)(gGrPlot.topLeft.x + gGrPlot.extent.x - FntCharsWidth(b, StrLen(b))), ly);
            }
        }
    }
    /* summary line and the selected point's callout */
    {
        char lo[16], hi[16], me[16];
        Coord sy = (Coord)(h - 17 - 11);
        ValueText(lo, gGrMin); ValueText(hi, gGrMax); ValueText(me, gGrMean);
        StrCopy(b, "Low "); StrCat(b, lo); StrCat(b, ", high "); StrCat(b, hi); StrCat(b, ", mean "); StrCat(b, me);
        WinDrawChars(b, StrLen(b), 4, sy);
        if (gGrSelected >= 0 && gGrSelected < (Int16)gGrN) {
            Coord cx = PX(gGrX[gGrSelected], 1), cy = PY(gGrY[gGrSelected], 1), w;
            if (gGrKind == grSpend) { UInt16 abs = (UInt16)(gGrFirstMonth + gGrX[gGrSelected]); StrCopy(b, DateMonthName((UInt8)(abs % 12 + 1))); StrCat(b, " "); StrIToA(b + StrLen(b), abs / 12); }
            else DateShortText(b, (UInt16)gGrX[gGrSelected]);
            StrCat(b, ": "); ValueText(b + StrLen(b), gGrY[gGrSelected]);
            w = (Coord)(FntCharsWidth(b, StrLen(b)) + 6);
            r.topLeft.x = (Coord)(cx - w / 2); if (r.topLeft.x < 2) r.topLeft.x = 2; if (r.topLeft.x + w > 158) r.topLeft.x = (Coord)(158 - w);
            r.topLeft.y = (Coord)(cy - 16); if (r.topLeft.y < gGrPlot.topLeft.y) r.topLeft.y = (Coord)(cy + 4);
            r.extent.x = w; r.extent.y = 12;
            WinEraseRectangle(&r, 2); WinDrawRectangleFrame(roundFrame, &r);
            WinDrawChars(b, StrLen(b), (Coord)(r.topLeft.x + 3), (Coord)(r.topLeft.y + 1));
        } else {
            WinDrawChars("Tap a point for its date and value", 34, 4, (Coord)(sy));
        }
    }
}

static void LayoutGraph(FormType *form)
{
    static UInt16 bottom[] = { GrDoneButton, GrReportsButton };
    AppFitForm(form, bottom, 2);
}

static void BuildGrPopup(FormType *form)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, GrVehicleList));
    Int16 sel;
    VehBuildChoices(lst, gGVNames, gGVPtrs, gGVIds, true, 255, gPrefs.currentVehicle, &sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, GrVehicleTrigger)), gGVNames[sel]);
}

Boolean GraphsFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        LayoutGraph(form);
        BuildGrPopup(form);
        {
            ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, GrKindList));
            LstSetSelection(lst, gGrKind);
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, GrKindTrigger)), LstGetSelectionText(lst, gGrKind));
        }
        Collect();
        FrmDrawForm(form);
        DrawGraph();
        return true;
    case frmUpdateEvent: FrmDrawForm(form); DrawGraph(); return true;
    case winDisplayChangedEvent: LayoutGraph(form); WinEraseWindow(); FrmDrawForm(form); DrawGraph(); return true;
    case penDownEvent:
        if (gGrN && RctPtInRectangle(event->screenX, event->screenY, &gGrPlot)) {
            UInt16 k; Int32 best = 0x7FFFFFFF; Int16 hit = -1;
            for (k = 0; k < gGrN; k++) {
                Int32 dx = PX(gGrX[k], 1) - event->screenX, dy = PY(gGrY[k], 1) - event->screenY, d2 = dx * dx + dy * dy;
                if (d2 < best) { best = d2; hit = (Int16)k; }
            }
            if (best > 400) hit = -1;                /* farther than 20 px: clear */
            gGrSelected = hit == gGrSelected ? -1 : hit;
            DrawGraph();
            return true;
        }
        break;
    case popSelectEvent:
        if (event->data.popSelect.listID == GrVehicleList) {
            gPrefs.currentVehicle = gGVIds[event->data.popSelect.selection];
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, GrVehicleTrigger)), gGVNames[event->data.popSelect.selection]);
            Collect(); DrawGraph();
            return true;
        }
        if (event->data.popSelect.listID == GrKindList) {
            gGrKind = (UInt8)event->data.popSelect.selection;
            Collect(); DrawGraph();
            break;
        }
        break;
    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == GrDoneButton) { FrmGotoForm(StatsForm); return true; }
        if (event->data.ctlSelect.controlID == GrReportsButton) { FrmGotoForm(ReportsForm); return true; }
        break;
    default: break;
    }
    return false;
}

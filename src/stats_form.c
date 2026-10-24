#include "stats_form.h"
#include "stats.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "datecalc.h"
#include "vehicles.h"
#include "programmes.h"
#include "resources.h"

static Stats gStats;                 /* 1 KB: static, off the stack */
static FillRecord gStatsFill;
static ProgrammeRecord gStatsPgm;
static char gSVNames[vehMaxVehicles + 1][vehNameLen];
static char *gSVPtrs[vehMaxVehicles + 1];
static UInt8 gSVIds[vehMaxVehicles + 1];
static RectangleType gStatsArea;     /* between the title and the buttons */
static Int16 gStatsScroll;           /* rows hidden above, in pixels */
static Int16 gStatsHeight;           /* content height drawn last time */
#define kStatsRow 11

/* Section placement: see sections.h. */
static void Compute(void) FORMS_SEC;
static void DrawLeft(const char *s, Coord y) FORMS_SEC;
static void DrawRightAt(const char *s, Coord right, Coord y) FORMS_SEC;
static void Row(const char *label, const char *value, Coord *y) FORMS_SEC;
static void EcoText(char *b, UInt32 l100x10) FORMS_SEC;
static void DrawStats(void) FORMS_SEC;
static void Layout(FormType *form) FORMS_SEC;
static void BuildPopup(FormType *form) FORMS_SEC;

static void Compute(void)
{
    UInt16 n = DbCount(dbFills), i;
    StatsBegin(&gStats);
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gStatsFill) != errNone) continue;
        if (gPrefs.currentVehicle != 255 && gStatsFill.vehicle != gPrefs.currentVehicle) continue;
        StatsAdd(&gStats, &gStatsFill);
    }
}

static void DrawLeft(const char *s, Coord y) { WinDrawChars(s, StrLen(s), 4, y); }
static void DrawRightAt(const char *s, Coord right, Coord y) { WinDrawChars(s, StrLen(s), right - FntCharsWidth(s, StrLen(s)), y); }

/* A label on the left, a value on the right. */
static void Row(const char *label, const char *value, Coord *y)
{
    FntSetFont(stdFont);
    DrawLeft(label, *y);
    DrawRightAt(value, 156, *y);
    *y += kStatsRow;
}

static void EcoText(char *b, UInt32 l100x10)
{
    TextFixed(b, (Int32)UnitEcoFuelToDisplay(l100x10, gPrefs.economyUnit), 1, gPoint, 0);
}

static void DrawStats(void)
{
    char b[40], n1[20], n2[20];
    Coord y = (Coord)(17 - gStatsScroll), top = gStatsArea.topLeft.y;
    Boolean electricOnly = gStats.fuelCount == 0 && gStats.chargeCount > 0;
    const Stats *s = &gStats;
    UInt8 id;

    WinEraseRectangle(&gStatsArea, 0);
    WinSetClip(&gStatsArea);
    if (s->count == 0) {
        FntSetFont(stdFont);
        DrawLeft("No fills yet for this choice.", y + 20);
        WinResetClip();
        gStatsHeight = 40;
        return;
    }
    /* "Since Jul 29, 8 fills" */
    FntSetFont(stdFont);
    StrCopy(b, "Since "); DateShortText(b + 6, s->firstDate);
    StrCat(b, ", "); StrIToA(n1, s->count); StrCat(b, n1); StrCat(b, s->count == 1 ? " fill" : " fills");
    DrawLeft(b, y); y += 13;
    /* Headlines: economy and spend in large type, captions under. */
    FntSetFont(largeBoldFont);
    if (electricOnly) TextFixed(n1, (Int32)StatsAvgEcoElectric(s), 1, gPoint, 0);
    else EcoText(n1, StatsAvgEco(s));
    if ((electricOnly ? StatsAvgEcoElectric(s) : StatsAvgEco(s)) == 0) StrCopy(n1, "--");
    DrawLeft(n1, y);
    StrCopy(n2, "$"); TextFixed(n2 + 1, (Int32)s->homeCents, 2, gPoint, gThousands);
    DrawRightAt(n2, 156, y);
    y += 18;
    FntSetFont(stdFont);
    StrCopy(b, electricOnly ? "kWh/100 km" : UnitEcoLabel(gPrefs.economyUnit)); StrCat(b, " average");
    DrawLeft(b, y);
    StrCopy(b, electricOnly ? "energy spend, " : (s->chargeCount ? "fuel and power, " : "fuel spend, ")); StrCat(b, CurrencyCode(gPrefs.homeCurrency));
    DrawRightAt(b, 156, y);
    y += 14;
    WinDrawLine(4, y, 156, y); y += 4;

    TextFixed(n1, (Int32)(UnitDistToDisplay(s->distance, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands);
    StrCopy(b, n1); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit));
    Row("Distance", b, &y);
    if (s->fuelCount) {
        TextFixed(n1, (Int32)UnitVolToDisplay(s->fuelMl, gPrefs.volumeUnit), 1, gPoint, gThousands);
        StrCopy(b, n1); StrCat(b, " "); StrCat(b, UnitVolLabel(gPrefs.volumeUnit));
        Row("Fuel used", b, &y);
    }
    if (s->chargeCount) {
        TextFixed(n1, (Int32)((s->wh + 50) / 100), 1, gPoint, gThousands);
        StrCopy(b, n1); StrCat(b, " kWh, $"); TextFixed(b + StrLen(b), (Int32)s->elecHomeCents, 2, gPoint, gThousands);
        Row(s->fuelCount ? "Electricity" : "Energy used", b, &y);
    }
    if (electricOnly ? s->bestEcoE : s->bestEco) {
        if (electricOnly) { TextFixed(n1, (Int32)s->bestEcoE, 1, gPoint, 0); TextFixed(n2, (Int32)s->worstEcoE, 1, gPoint, 0); }
        else { EcoText(n1, s->bestEco); EcoText(n2, s->worstEco); }   /* the unit conversion keeps best first */
        StrCopy(b, n1); StrCat(b, " / "); StrCat(b, n2);
        Row("Best / worst", b, &y);
    }
    if (s->fuelCount && StatsAvgPrice(s)) {
        Boolean cents = gPrefs.priceFormat == unitPriceFormatCents;
        TextPrice(n1, UnitPriceToDisplay(StatsAvgPrice(s), gPrefs.priceUnit), cents, gPoint);
        StrCopy(b, n1); StrCat(b, cents ? " c" : " $"); StrCat(b, UnitPriceLabel(gPrefs.priceUnit));
        Row("Average price", b, &y);
        TextPrice(n1, UnitPriceToDisplay(s->lowPrice, gPrefs.priceUnit), cents, gPoint);
        TextPrice(n2, UnitPriceToDisplay(s->highPrice, gPrefs.priceUnit), cents, gPoint);
        StrCopy(b, n1); StrCat(b, " / "); StrCat(b, n2);
        Row("Low / high price", b, &y);
    } else if (electricOnly && StatsAvgPriceElectric(s)) {
        TextPrice(n1, StatsAvgPriceElectric(s), gPrefs.priceFormat == unitPriceFormatCents, gPoint);
        StrCopy(b, n1); StrCat(b, gPrefs.priceFormat == unitPriceFormatCents ? " c/kWh" : " $/kWh");
        Row("Average price", b, &y);
    }
    if (s->distance) {
        StrCopy(b, "$"); TextFixed(b + 1, (Int32)StatsCostPer100km(s), 2, gPoint, 0);
        StrCopy(n1, "Cost per 100 "); StrCat(n1, UnitDistLabel(gPrefs.distanceUnit));
        Row(n1, b, &y);
    }
    if (s->fuelCount) {
        TextFixed(n1, (Int32)UnitVolToDisplay(StatsAvgFillMl(s), gPrefs.volumeUnit), 1, gPoint, 0);
        StrCopy(b, n1); StrCat(b, " "); StrCat(b, UnitVolLabel(gPrefs.volumeUnit));
        Row("Average fill", b, &y);
    }
    if (s->count > 1) {
        TextFixed(b, (Int32)StatsDaysBetweenX10(s), 1, gPoint, 0);
        Row("Days between fills", b, &y);
    }
    StrCopy(b, "$"); TextFixed(b + 1, (Int32)StatsSpendPerMonth(s), 2, gPoint, gThousands);
    Row("Spend per month", b, &y);
    if (s->cardCents && s->cashCents) {
        StrCopy(b, "$"); TextFixed(b + 1, (Int32)s->cashCents, 2, gPoint, gThousands);
        StrCat(b, " / $"); TextFixed(b + StrLen(b), (Int32)s->cardCents, 2, gPoint, gThousands);
        Row("Cash / card", b, &y);
    }
    if (s->discountCents) {
        StrCopy(b, "$"); TextFixed(b + 1, (Int32)s->discountCents, 2, gPoint, gThousands);
        Row("Saved by points", b, &y);
    }

    /* Points: one line per programme that has ever appeared on a fill. */
    {
        Boolean any = false;
        for (id = 1; id < statsMaxId; id++) if (s->pgmUsed[id]) any = true;
        if (any) {
            y += 3; WinDrawLine(4, y, 156, y); y += 4;
            FntSetFont(boldFont); DrawLeft("Points", y); y += kStatsRow;
            for (id = 1; id < statsMaxId; id++) {
                Int32 bal;
                if (!s->pgmUsed[id] || !PgmGet(id, &gStatsPgm)) continue;
                bal = PgmBalance(id, gStatsPgm.openingBalance);
                PgmValueText(n1, &gStatsPgm, bal);
                StrCopy(b, n1);
                if (!PgmIsMoney(&gStatsPgm) && gStatsPgm.valuePerPoint && bal > 0) {
                    StrCat(b, " (about $");
                    TextFixed(b + StrLen(b), (Int32)CalcMulDiv((UInt32)bal, gStatsPgm.valuePerPoint, 1000), 2, gPoint, gThousands);
                    StrCat(b, ")");
                }
                Row(gStatsPgm.name, b, &y);
            }
        }
    }
    WinResetClip();
    gStatsHeight = (Coord)(y + gStatsScroll - top);
    (void)top;
}

static void Layout(FormType *form)
{
    static UInt16 bottom[] = { StatsDoneButton, StatsReportsButton, StatsGraphsButton };
    Coord w, h;
    AppFitForm(form, bottom, 3);
    WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
    gStatsArea.topLeft.x = 0; gStatsArea.topLeft.y = 15; gStatsArea.extent.x = w; gStatsArea.extent.y = (Coord)(h - 17 - 15);
}

static void BuildPopup(FormType *form)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, StatsVehicleList));
    Int16 sel;
    VehBuildChoices(lst, gSVNames, gSVPtrs, gSVIds, true, 255, gPrefs.currentVehicle, &sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, StatsVehicleTrigger)), gSVNames[sel]);
}

Boolean StatsFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        gStatsScroll = 0;
        Layout(form);
        BuildPopup(form);
        Compute();
        FrmDrawForm(form);
        DrawStats();
        return true;
    case frmUpdateEvent: FrmDrawForm(form); DrawStats(); return true;
    case winDisplayChangedEvent: Layout(form); gStatsScroll = 0; WinEraseWindow(); FrmDrawForm(form); DrawStats(); return true;
    case keyDownEvent: {
        /* Page keys scroll when the content outruns the screen (input area open). */
        WChar c = event->data.keyDown.chr;
        Int16 room = gStatsHeight - gStatsArea.extent.y - gStatsScroll;
        if (c == vchrPageDown && room > 0) { gStatsScroll += room < 60 ? room : 60; DrawStats(); return true; }
        if (c == vchrPageUp && gStatsScroll > 0) { gStatsScroll -= gStatsScroll < 60 ? gStatsScroll : 60; DrawStats(); return true; }
        break;
    }
    case popSelectEvent:
        if (event->data.popSelect.listID == StatsVehicleList) {
            gPrefs.currentVehicle = gSVIds[event->data.popSelect.selection];
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, StatsVehicleTrigger)), gSVNames[event->data.popSelect.selection]);
            gStatsScroll = 0;
            Compute();
            DrawStats();
            return true;
        }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case StatsDoneButton: FrmGotoForm(FillListForm); return true;
        case StatsReportsButton: case StatsGraphsButton: FrmAlert(NotYetAlert); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

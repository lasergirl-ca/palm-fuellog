/* New Fill / edit fill. Any two of volume, price and total give the third;
 * the one FuelLog worked out is marked "calculated". */
#include "fill_form.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "datecalc.h"
#include "resources.h"

static FillRecord gFill;
static UInt16 gDate, gTime;
static UInt8 gTyped;          /* bits: 1 volume, 2 price, 4 total, in the order the user touched them */
static UInt8 gOrder[3];       /* last three fields typed, newest last */
static UInt32 gPrevOdo;       /* previous fill's odometer for the hint */
static FillRecord gPrevFill;  /* static: keeps the big record off the stack */
static char gDateText[12];    /* CtlSetLabel keeps the pointer, so the text must persist */
static char gTimeText[timeStringLength];

#define typedVolume 1
#define typedPrice  2
#define typedTotal  4

static void Touch(UInt8 which)
{
    UInt8 i, j = 0, tmp[3];
    for (i = 0; i < 3; i++) if (gOrder[i] && gOrder[i] != which) tmp[j++] = gOrder[i];
    while (j < 2) { UInt8 k; for (k = j; k < 2; k++) tmp[k] = 0; break; }
    gOrder[0] = j > 1 ? tmp[j - 2] : 0;
    gOrder[1] = j > 0 ? tmp[j - 1] : 0;
    gOrder[2] = which;
    gTyped |= which;
}

static Int32 ReadVolume(FormType *form, UInt32 *ml)
{
    UInt32 v;
    Int32 r = TextParseFixed(AppGetFieldText(form, FillVolumeField), UnitVolDecimals(gPrefs.volumeUnit), gPoint, gThousands, &v);
    if (r == 1) *ml = UnitVolFromDisplay(v, gPrefs.volumeUnit);
    return r;
}

static Int32 ReadPrice(FormType *form, UInt32 *price)
{
    UInt32 v;
    Int32 r = TextParsePrice(AppGetFieldText(form, FillPriceField), gPrefs.priceFormat == unitPriceFormatCents, gPoint, gThousands, &v);
    if (r == 1) *price = UnitPriceFromDisplay(v, gPrefs.priceUnit);
    return r;
}

static Int32 ReadTotal(FormType *form, UInt32 *cents)
{
    return TextParseFixed(AppGetFieldText(form, FillTotalField), 2, gPoint, gThousands, cents);
}

static void ShowVolume(FormType *form, UInt32 ml)
{
    char b[16]; TextFixed(b, (Int32)UnitVolToDisplay(ml, gPrefs.volumeUnit), UnitVolDecimals(gPrefs.volumeUnit), gPoint, 0);
    AppSetFieldText(form, FillVolumeField, b);
}
static void ShowPrice(FormType *form, UInt32 price)
{
    char b[16]; TextPrice(b, UnitPriceToDisplay(price, gPrefs.priceUnit), gPrefs.priceFormat == unitPriceFormatCents, gPoint);
    AppSetFieldText(form, FillPriceField, b);
}
static void ShowTotal(FormType *form, UInt32 cents)
{
    char b[16]; TextFixed(b, (Int32)cents, 2, gPoint, 0);
    AppSetFieldText(form, FillTotalField, b);
}

/* Recompute the field the user did not type from the two they did. */
static void Recompute(FormType *form)
{
    UInt32 ml = 0, price = 0, cents = 0;
    Int32 hv = ReadVolume(form, &ml), hp = ReadPrice(form, &price), ht = ReadTotal(form, &cents);
    UInt8 last = gOrder[2], prev = gOrder[1];
    UInt8 have = (hv == 1 ? typedVolume : 0) | (hp == 1 ? typedPrice : 0) | (ht == 1 ? typedTotal : 0);
    UInt8 target = 0;

    if (last && prev) target = (UInt8)(7 & ~(last | prev));
    else if (have == (typedVolume | typedPrice)) target = typedTotal;
    else if (have == (typedVolume | typedTotal)) target = typedPrice;
    else if (have == (typedPrice | typedTotal)) target = typedVolume;
    AppSetLabel(form, FillCalcHint, "            ");
    if (target == typedTotal && (have & typedVolume) && (have & typedPrice)) {
        ShowTotal(form, CalcTotalCents(price, ml));
        AppSetLabel(form, FillCalcHint, "calculated");
    } else if (target == typedPrice && (have & typedVolume) && (have & typedTotal) && ml) {
        ShowPrice(form, CalcMulDiv(cents, 100000UL, ml));
        AppSetLabel(form, FillCalcHint, "price calc.");
    } else if (target == typedVolume && (have & typedPrice) && (have & typedTotal) && price) {
        ShowVolume(form, CalcMulDiv(cents, 100000UL, price));
        AppSetLabel(form, FillCalcHint, "volume calc.");
    }
}

static void CheckTrip(FormType *form)
{
    UInt32 odo = 0, trip = 0;
    Int32 ho = TextParseFixed(AppGetFieldText(form, FillOdoField), 1, gPoint, gThousands, &odo);
    Int32 ht = TextParseFixed(AppGetFieldText(form, FillTripField), 1, gPoint, gThousands, &trip);
    char b[24];
    if (ho == 1) odo = UnitDistFromDisplay(odo, gPrefs.distanceUnit);
    if (ht == 1) trip = UnitDistFromDisplay(trip, gPrefs.distanceUnit);
    if (ho == 1 && ht == 1 && gPrevOdo && odo > gPrevOdo) {
        UInt32 moved = odo - gPrevOdo, gap = moved > trip ? moved - trip : trip - moved;
        UInt32 tol = moved / 50 > 50 ? moved / 50 : 50;   /* 2 % or 5.0 km, whichever is larger */
        AppSetLabel(form, FillTripHint, gap <= tol ? "= odo" : "check odo");
    } else if (ho != 1 && ht == 1 && gPrevOdo) {
        /* Blank odometer: it will be the previous one plus the trip. */
        StrCopy(b, "odo ");
        TextFixed(b + 4, (Int32)(UnitDistToDisplay(gPrevOdo + trip, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands);
        AppSetLabel(form, FillTripHint, b);
    } else {
        AppSetLabel(form, FillTripHint, "              ");
    }
}

/* A value field changed, by keystroke or otherwise: keep the linked fields
 * and hints in step. */
static void AfterEdit(FormType *form, UInt16 fieldId)
{
    switch (fieldId) {
    case FillVolumeField: Touch(typedVolume); Recompute(form); break;
    case FillPriceField:  Touch(typedPrice);  Recompute(form); break;
    case FillTotalField:  Touch(typedTotal);  Recompute(form); break;
    case FillOdoField: case FillTripField: CheckTrip(form); break;
    default: break;
    }
}

static void Load(FormType *form)
{
    char b[24];
    UInt16 count = DbCount(dbFills);
    FillRecord *prev = &gPrevFill;

    MemSet(&gFill, sizeof gFill, 0);
    gTyped = 0; gOrder[0] = gOrder[1] = gOrder[2] = 0;
    gPrevOdo = 0;
    if (gEditIndex != dmMaxRecordIndex && DbGetFill(gEditIndex, &gFill) == errNone) {
        if (gEditIndex > 0 && DbGetFill(gEditIndex - 1, prev) == errNone) gPrevOdo = prev->odometer;
    } else {
        gEditIndex = dmMaxRecordIndex;
        gFill.date = AppToday();
        gFill.time = AppNowMinutes();
        gFill.vehicle = gPrefs.currentVehicle == 255 ? 1 : gPrefs.currentVehicle;
        gFill.flags = fillFlagFullTank;
        gFill.currency = CurrencyForNewEntry(vehCurrencyUsePrefs, gPrefs.defaultCurrency, gPrefs.lastCurrency);
        gFill.fxRate = CurrencyDefaultRate(gFill.currency, gPrefs.homeCurrency, gPrefs.fxRate);
        gFill.payment = gPrefs.lastPayment;
        if (count && DbGetFill(count - 1, prev) == errNone) gPrevOdo = prev->odometer;
    }
    gDate = gFill.date;
    gTime = gFill.time;
    DateShortText(gDateText, gDate);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillDateTrigger)), gDateText);
    AppTimeText(gTimeText, gTime);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillTimeTrigger)), gTimeText);
    /* The odometer shows whole units like the dash; tenths are accepted when typed. */
    if (gFill.odometer) { TextFixed(b, (Int32)(UnitDistToDisplay(gFill.odometer, gPrefs.distanceUnit) / 10), 0, gPoint, 0); AppSetFieldText(form, FillOdoField, b); }
    if (gFill.trip) { TextFixed(b, (Int32)(UnitDistToDisplay(gFill.trip, gPrefs.distanceUnit)), 1, gPoint, 0); AppSetFieldText(form, FillTripField, b); }
    if (gFill.volume) ShowVolume(form, gFill.volume);
    if (gFill.price) ShowPrice(form, gFill.price);
    if (gFill.total) ShowTotal(form, gFill.total);
    if (gPrevOdo) {
        StrCopy(b, "p: ");
        TextFixed(b + 3, (Int32)(UnitDistToDisplay(gPrevOdo, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands);
        AppSetLabel(form, FillOdoHint, b);
    }
    /* Unit labels follow Prefs. */
    StrCopy(b, "Volume ("); StrCat(b, UnitVolLabel(gPrefs.volumeUnit)); StrCat(b, "):");
    AppSetLabel(form, FillVolumeLabel, b);
    StrCopy(b, gPrefs.priceFormat == unitPriceFormatCents ? "Price (c" : "Price ($");
    StrCat(b, UnitPriceLabel(gPrefs.priceUnit)); StrCat(b, "):");
    AppSetLabel(form, FillPriceLabel, b);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCurrencyTrigger)), CurrencyCode(gFill.currency));
    LstSetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCurrencyList)), gFill.currency);
    FrmSetControlGroupSelection(form, 1, gFill.payment == fillPaymentCard ? FillCardButton : FillCashButton);
    {
        static const char *const grades[] = { "Regular", "Mid", "Premium", "Diesel" };
        UInt8 g = gFill.grade < 4 ? gFill.grade : 0;
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillGradeTrigger)), grades[g]);
        LstSetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillGradeList)), g);
    }
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillFullCheckbox)), (gFill.flags & fillFlagFullTank) ? 1 : 0);
    FrmShowObject(form, FrmGetObjectIndex(form, FillDeleteButton));
    if (gEditIndex == dmMaxRecordIndex) FrmHideObject(form, FrmGetObjectIndex(form, FillDeleteButton));
    FrmSetTitle(form, gEditIndex == dmMaxRecordIndex ? "New Fill" : "Edit Fill");   /* literals persist */
    FrmSetFocus(form, FrmGetObjectIndex(form, FillOdoField));
}

static Boolean Save(FormType *form)
{
    UInt32 odo = 0, trip = 0, ml = 0, price = 0, cents = 0;
    Int32 ho = TextParseFixed(AppGetFieldText(form, FillOdoField), 1, gPoint, gThousands, &odo);
    Int32 ht = TextParseFixed(AppGetFieldText(form, FillTripField), 1, gPoint, gThousands, &trip);
    Int32 hv = ReadVolume(form, &ml), hp = ReadPrice(form, &price), hc = ReadTotal(form, &cents);
    Err err;

    if (ho == 1) odo = UnitDistFromDisplay(odo, gPrefs.distanceUnit);
    if (ht == 1) trip = UnitDistFromDisplay(trip, gPrefs.distanceUnit);
    if (ho != 1 && ht == 1 && gPrevOdo) odo = gPrevOdo + trip;     /* quick entry */
    if (ho < 0 || ht < 0 || hv < 0 || hp < 0 || hc < 0 || odo == 0) {
        FrmAlert(BadNumberAlert);
        return false;
    }
    if (hv != 1 && hc != 1) { FrmAlert(NeedVolumeAlert); return false; }
    if (hv == 1 && hp == 1 && hc != 1) cents = CalcTotalCents(price, ml);
    gFill.date = gDate;
    gFill.time = gTime;
    gFill.odometer = odo;
    gFill.trip = trip;
    gFill.volume = ml;
    gFill.price = price;
    gFill.total = cents;
    gFill.flags = (UInt16)((gFill.flags & ~(fillFlagFullTank | fillFlagTripEntered))
                 | (CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillFullCheckbox))) ? fillFlagFullTank : 0)
                 | (ht == 1 ? fillFlagTripEntered : 0));
    gFill.payment = FrmGetControlGroupSelection(form, 1) == FillCardButton ? fillPaymentCard : fillPaymentCash;
    gFill.currency = (UInt8)LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCurrencyList)));
    gFill.grade = (UInt8)LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillGradeList)));
    if (gEditIndex != dmMaxRecordIndex) DbRemove(dbFills, gEditIndex);
    err = DbAddFill(&gFill, NULL);
    if (err) { FrmAlert(SaveFailedAlert); return false; }
    gPrefs.lastPayment = gFill.payment;
    gPrefs.lastCurrency = gFill.currency;
    PrefsSave(&gPrefs);
    return true;
}

Boolean FillFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static const UInt16 bottom[] = { FillDoneButton, FillCancelButton, FillDeleteButton };

    switch (event->eType) {
    case frmOpenEvent:
        AppFitForm(form, bottom, 3);
        Load(form);
        FrmDrawForm(form);
        return true;
    case frmUpdateEvent:
        FrmDrawForm(form);
        return true;
    case winDisplayChangedEvent:
        AppFitForm(form, bottom, 3);
        WinEraseWindow();
        FrmDrawForm(form);
        return true;
    case fldChangedEvent:
        AfterEdit(form, event->data.fldChanged.fieldID);
        return true;

    case keyDownEvent: {
        /* Let the field take the character first, then recompute from what
         * it now holds. This does not rely on fldChangedEvent arriving. */
        UInt16 focus = FrmGetFocus(form);
        if (focus != noFocus) {
            UInt16 id = FrmGetObjectId(form, focus);
            if (id == FillVolumeField || id == FillPriceField || id == FillTotalField ||
                id == FillOdoField || id == FillTripField) {
                FrmHandleEvent(form, event);
                AfterEdit(form, id);
                return true;
            }
        }
        break;
    }
    case popSelectEvent:
        if (event->data.popSelect.listID == FillCurrencyList) {
            gFill.currency = (UInt8)event->data.popSelect.selection;
            gFill.fxRate = CurrencyDefaultRate(gFill.currency, gPrefs.homeCurrency, gPrefs.fxRate);
        }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case FillDateTrigger: {
            UInt16 y; UInt8 m, d; Int16 yy, mm, dd;
            DateDaysToYmd(gDate, &y, &m, &d);
            yy = (Int16)y; mm = m; dd = d;
            if (SelectDay(selectDayByDay, &mm, &dd, &yy, "Fill date")) {
                gDate = DateYmdToDays((UInt16)yy, (UInt8)mm, (UInt8)dd);
                DateShortText(gDateText, gDate);
                CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillDateTrigger)), gDateText);
            }
            return true;
        }
        case FillTimeTrigger: {
            Int16 hh = gTime == fillTimeUnknown ? 12 : gTime / 60, mm = gTime == fillTimeUnknown ? 0 : gTime % 60;
            if (SelectOneTime(&hh, &mm, "Fill time")) {
                gTime = (UInt16)(hh * 60 + mm);
                AppTimeText(gTimeText, gTime);
                CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillTimeTrigger)), gTimeText);
            }
            return true;
        }
        case FillDoneButton:
            if (Save(form)) FrmGotoForm(FillListForm);
            return true;
        case FillCancelButton:
            FrmGotoForm(FillListForm);
            return true;
        case FillDeleteButton:
            if (gEditIndex != dmMaxRecordIndex && FrmAlert(DeleteFillAlert) == 0) {
                DbRemove(dbFills, gEditIndex);
                FrmGotoForm(FillListForm);
            }
            return true;
        default: break;
        }
        break;
    default:
        break;
    }
    return false;
}

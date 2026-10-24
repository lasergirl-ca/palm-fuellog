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
#include "vehicles.h"
#include "programmes.h"

static FillRecord gFill;
static UInt16 gDate, gTime;
static UInt8 gTyped;          /* bits: 1 volume, 2 price, 4 total, in the order the user touched them */
static UInt8 gOrder[3];       /* last three fields typed, newest last */
static UInt32 gPrevOdo;       /* previous fill's odometer for the hint */
static FillRecord gPrevFill;  /* static: keeps the big record off the stack */
static VehicleRecord gFillVeh; /* the fill's vehicle: odometer unit, defaults */
static char gFVNames[vehMaxVehicles + 1][vehNameLen];
static char *gFVPtrs[vehMaxVehicles + 1];
static UInt8 gFVIds[vehMaxVehicles + 1];
static char gDateText[12];    /* CtlSetLabel keeps the pointer, so the text must persist */
static char gTimeText[timeStringLength];
/* Grade popup rows for this vehicle: fuel grades, charge kinds, or both. */
static const char *const kGradeNames[7] = { "Regular", "Mid", "Premium", "Diesel", "Home AC", "Public AC", "DC fast" };
static char *gGradePtrs[7];
static UInt8 gGradeMap[7];   /* popup row -> grade value */
static UInt16 gGradeRows;
static Boolean gCharge;      /* the fill is a charging session */

/* Points on this fill, edited in the Points dialog. An auto row's earned
 * value follows the volume and price; a typed one stays as typed. */
typedef struct { UInt8 pgm; Int32 earned; Int32 redeemed; Boolean autoEarned; } PointRow;
static PointRow gPts[fillMaxPoints];
static UInt8 gPtsCount;
static Boolean gPtsTouched;   /* the user opened the dialog, so stop re-applying presets */
static UInt32 gDiscount;      /* cents taken off by redemptions */
static ProgrammeRecord gPgm;  /* scratch */
static char gPointsText[40];  /* the summary trigger's label (CtlSetLabel keeps the pointer) */
static char gPgmNames[pgmMaxProgrammes + 1][pgmNameLen];
static char *gPgmPtrs[pgmMaxProgrammes + 1];
static UInt8 gPgmIds[pgmMaxProgrammes + 1];
static UInt16 gPgmChoices;

static void Recompute(FormType *form);
static Int32 ReadVolume(FormType *form, UInt32 *ml);
static Int32 ReadPrice(FormType *form, UInt32 *price);
static Int32 ReadTotal(FormType *form, UInt32 *cents);

/* ---- points ---- */

static Boolean PgmPaymentOk(const ProgrammeRecord *p)
{
    if (p->requiresPayment == pgmPaymentCash) return gFill.payment == fillPaymentCash;
    if (p->requiresPayment == pgmPaymentCard) return gFill.payment == fillPaymentCard;
    return true;
}

/* A new fill starts with every auto-apply programme that fits its payment. */
static void PointsAutoApply(void)
{
    UInt16 n = DbCount(dbProgrammes), i;
    gPtsCount = 0;
    for (i = 0; i < n && gPtsCount < fillMaxPoints; i++) {
        if (PgmGetAt(i, &gPgm) != errNone || gPgm.archived || !gPgm.autoApply || !PgmPaymentOk(&gPgm)) continue;
        gPts[gPtsCount].pgm = gPgm.id; gPts[gPtsCount].earned = 0; gPts[gPtsCount].redeemed = 0; gPts[gPtsCount].autoEarned = true;
        gPtsCount++;
    }
}

static void RuleOf(const ProgrammeRecord *p, CalcRedeemRule *r)
{
    r->kind = p->redeemKind; r->block = p->redeemBlock; r->value = p->redeemValue;
    r->volUnit = p->redeemVolUnit; r->maxVol = p->redeemMaxVol; r->maxPerUnit = p->redeemMaxPerUnit;
}

/* Auto-earned points and the discount for this volume (mL) and gross price
 * (cents before any discount); then the summary on the form. */
static void PointsRecompute(FormType *form, UInt32 ml, UInt32 gross)
{
    UInt8 i;
    gDiscount = 0;
    for (i = 0; i < gPtsCount; i++) {
        if (!PgmGet(gPts[i].pgm, &gPgm)) continue;
        if (gPts[i].autoEarned)
            gPts[i].earned = PgmPaymentOk(&gPgm)
                ? (Int32)CalcEarnedPoints(gPgm.earnRate, gPgm.earnBasis, ml, gross, gFill.grade == fillGradePremium ? gPgm.premiumMult : 100) : 0;
        if (gPts[i].redeemed > 0) {
            CalcRedeemRule rule;
            RuleOf(&gPgm, &rule);
            gDiscount += CalcDiscountCents(&rule, (UInt32)gPts[i].redeemed, ml, gross > gDiscount ? gross - gDiscount : 0);
        }
    }
    if (gPtsCount == 0) StrCopy(gPointsText, "none");
    else if (gPtsCount == 1 && PgmGet(gPts[0].pgm, &gPgm)) {
        char v[20];
        PgmValueText(v, &gPgm, gPts[0].earned - gPts[0].redeemed);
        StrNCopy(gPointsText, gPgm.name, 12); gPointsText[12] = 0;
        StrCat(gPointsText, " "); StrCat(gPointsText, v);
    } else { StrIToA(gPointsText, gPtsCount); StrCat(gPointsText, " programmes"); }
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillPointsTrigger)), gPointsText);
}

/* Parse a whole number with an optional leading minus. 0 empty, 1 ok, -1 bad. */
static Int32 ParseSigned(const char *t, Int32 *out)
{
    Boolean neg = false;
    UInt32 v;
    Int32 r;
    while (*t == ' ') t++;
    if (*t == '-') { neg = true; t++; }
    r = TextParseFixed(t, 0, gPoint, gThousands, &v);
    if (r == 1) *out = neg ? -(Int32)v : (Int32)v;
    return r;
}

/* The Points dialog rows: trigger, earned, redeemed, hint. */
static UInt32 gDlgMl, gDlgGross;

static void PtsHint(FormType *dlg, UInt8 i)
{
    char b[40], num[16];
    static const char *const unit[] = { "", "/L", "/gal", "/$" };
    if (!PgmGet(gPts[i].pgm, &gPgm)) return;
    if (gPts[i].redeemed > 0) {
        CalcRedeemRule rule;
        RuleOf(&gPgm, &rule);
        StrCopy(b, "redeem: $");
        TextFixed(b + StrLen(b), (Int32)CalcDiscountCents(&rule, (UInt32)gPts[i].redeemed, gDlgMl, gDlgGross), 2, gPoint, 0);
        StrCat(b, " off");
    } else if (!gPts[i].autoEarned) StrCopy(b, "typed");
    else if (!PgmPaymentOk(&gPgm)) StrCopy(b, gPgm.requiresPayment == pgmPaymentCard ? "Card fills only" : "Cash fills only");
    else if (gPgm.earnBasis == pgmEarnNone || gPgm.earnRate == 0) StrCopy(b, "no earn rule: type it");
    else {
        PgmRateText(num, gPgm.earnRate);
        StrCopy(b, "auto: "); StrCat(b, num); StrCat(b, PgmIsMoney(&gPgm) ? "c" : " pts");
        StrCat(b, unit[gPgm.earnBasis < 4 ? gPgm.earnBasis : 0]);
        if (gPgm.earnBasis == pgmEarnPerLitre) { StrCat(b, " x "); TextFixed(b + StrLen(b), (Int32)((gDlgMl + 5) / 10), 2, gPoint, 0); StrCat(b, " L"); }
        else if (gPgm.earnBasis == pgmEarnPerCurrency) { StrCat(b, " x $"); TextFixed(b + StrLen(b), (Int32)gDlgGross, 2, gPoint, 0); }
    }
    b[26] = 0;
    AppSetLabel(dlg, PtsHint0 + i, b);
}

static void PtsShowObj(FormType *dlg, UInt16 id, Boolean show)
{
    UInt16 idx = FrmGetObjectIndex(dlg, id);
    if (show) FrmShowObject(dlg, idx); else FrmHideObject(dlg, idx);
}

/* Lay the rows out: one per programme on the fill, then an Add row. */
static void PtsShowRows(FormType *dlg, Boolean fillFields)
{
    UInt8 i;
    char b[16];
    for (i = 0; i < fillMaxPoints; i++) {
        Boolean used = i < gPtsCount, add = i == gPtsCount;
        ListType *lst = FrmGetObjectPtr(dlg, FrmGetObjectIndex(dlg, PtsList0 + i));
        ControlType *trg = FrmGetObjectPtr(dlg, FrmGetObjectIndex(dlg, PtsTrigger0 + i));
        PtsShowObj(dlg, PtsTrigger0 + i, used || add);
        PtsShowObj(dlg, PtsEarned0 + i, used);
        PtsShowObj(dlg, PtsRedeemed0 + i, used);
        PtsShowObj(dlg, PtsHint0 + i, used);
        if (used) {
            UInt16 k;
            Int16 sel = 0;
            for (k = 0; k < gPgmChoices; k++) if (gPgmIds[k] == gPts[i].pgm) sel = (Int16)k;
            LstSetSelection(lst, sel);
            CtlSetLabel(trg, gPgmPtrs[sel]);
            if (fillFields) {
                TextFixed(b, gPts[i].earned, 0, gPoint, 0); AppSetFieldText(dlg, PtsEarned0 + i, b);
                if (gPts[i].redeemed) { TextFixed(b, gPts[i].redeemed, 0, gPoint, 0); AppSetFieldText(dlg, PtsRedeemed0 + i, b); }
                else AppSetFieldText(dlg, PtsRedeemed0 + i, "");
            }
            PtsHint(dlg, i);
        } else if (add) {
            LstSetSelection(lst, 0);
            CtlSetLabel(trg, "Add programme");
        }
    }
}

/* Recompute from the dialog's rows and refresh the hints and the total line. */
static void PtsRefresh(FormType *dlg, Boolean fillFields)
{
    char b[32];
    UInt8 i;
    gDiscount = 0;
    for (i = 0; i < gPtsCount; i++) {
        if (!PgmGet(gPts[i].pgm, &gPgm)) continue;
        if (gPts[i].autoEarned)
            gPts[i].earned = PgmPaymentOk(&gPgm)
                ? (Int32)CalcEarnedPoints(gPgm.earnRate, gPgm.earnBasis, gDlgMl, gDlgGross, gFill.grade == fillGradePremium ? gPgm.premiumMult : 100) : 0;
        if (gPts[i].redeemed > 0) {
            CalcRedeemRule rule;
            RuleOf(&gPgm, &rule);
            gDiscount += CalcDiscountCents(&rule, (UInt32)gPts[i].redeemed, gDlgMl, gDlgGross > gDiscount ? gDlgGross - gDiscount : 0);
        }
    }
    PtsShowRows(dlg, fillFields);
    if (gDiscount) { StrCopy(b, "Total less $"); TextFixed(b + StrLen(b), (Int32)gDiscount, 2, gPoint, 0); StrCat(b, " in points"); }
    else b[0] = 0;
    AppSetLabel(dlg, 2160, b);
}

static Boolean PointsDialogHandleEvent(EventType *e)
{
    FormType *dlg = FrmGetActiveForm();
    switch (e->eType) {
    case popSelectEvent: {
        UInt16 row = e->data.popSelect.listID - PtsList0;
        Int16 sel = e->data.popSelect.selection;
        if (row < fillMaxPoints) {
            UInt8 id = gPgmIds[sel], k;
            gPtsTouched = true;
            if (sel == 0 || id == 0) {
                if (row < gPtsCount) { for (k = row; k + 1 < gPtsCount; k++) gPts[k] = gPts[k + 1]; gPtsCount--; }
            } else {
                Boolean dup = false;
                for (k = 0; k < gPtsCount; k++) if (k != row && gPts[k].pgm == id) dup = true;
                if (!dup) {
                    if (row == gPtsCount) { gPts[row].earned = 0; gPts[row].redeemed = 0; gPtsCount++; }
                    gPts[row].pgm = id; gPts[row].autoEarned = true;
                }
            }
            PtsRefresh(dlg, true);
            return true;
        }
        break;
    }
    case keyDownEvent: {
        UInt16 focus = FrmGetFocus(dlg);
        if (focus != noFocus) {
            UInt16 id = FrmGetObjectId(dlg, focus);
            Int32 v = 0;
            FrmHandleEvent(dlg, e);
            if (id >= PtsEarned0 && id < PtsEarned0 + fillMaxPoints) {
                UInt8 i = (UInt8)(id - PtsEarned0);
                if (ParseSigned(AppGetFieldText(dlg, id), &v) >= 0) { gPts[i].earned = v; gPts[i].autoEarned = false; gPtsTouched = true; }
            } else if (id >= PtsRedeemed0 && id < PtsRedeemed0 + fillMaxPoints) {
                UInt8 i = (UInt8)(id - PtsRedeemed0);
                if (ParseSigned(AppGetFieldText(dlg, id), &v) >= 0) { gPts[i].redeemed = v; gPtsTouched = true; }
            } else break;
            PtsRefresh(dlg, false);
            return true;
        }
        break;
    }
    default: break;
    }
    return false;
}

/* Open the modal Points dialog over the fill form; OK keeps its rows. */
static void PointsDialog(FormType *form)
{
    FormType *dlg = FrmInitForm(PointsForm);
    PointRow saved[fillMaxPoints];
    UInt8 savedCount = gPtsCount, i;
    UInt32 ml = 0, price = 0, cents = 0;
    Int16 sel;
    UInt16 hit;

    for (i = 0; i < fillMaxPoints; i++) saved[i] = gPts[i];
    ReadVolume(form, &ml);
    if (ReadPrice(form, &price) == 1 && ml) gDlgGross = CalcTotalCents(price, ml);
    else gDlgGross = (ReadTotal(form, &cents) == 1 ? cents : 0) + gDiscount;
    gDlgMl = ml;
    FrmSetEventHandler(dlg, PointsDialogHandleEvent);
    gPgmChoices = PgmBuildChoices(FrmGetObjectPtr(dlg, FrmGetObjectIndex(dlg, PtsList0)), gPgmNames, gPgmPtrs, gPgmIds, true, 0, 0, &sel);
    for (i = 1; i < fillMaxPoints; i++) {
        ListType *lst = FrmGetObjectPtr(dlg, FrmGetObjectIndex(dlg, PtsList0 + i));
        LstSetListChoices(lst, gPgmPtrs, (Int16)gPgmChoices);
        LstSetHeight(lst, (Int16)(gPgmChoices < 6 ? gPgmChoices : 6));
    }
    PtsRefresh(dlg, true);
    hit = FrmDoDialog(dlg);
    FrmDeleteForm(dlg);
    if (hit != PtsOkButton) { for (i = 0; i < fillMaxPoints; i++) gPts[i] = saved[i]; gPtsCount = savedCount; }
    else gPtsTouched = true;
    Recompute(form);
}

/* ---- note ---- */

static void NoteCount(FormType *dlg)
{
    char b[20];
    FieldType *fld = FrmGetObjectPtr(dlg, FrmGetObjectIndex(dlg, FillNoteField));
    StrIToA(b, FldGetTextLength(fld));
    StrCat(b, " of 255 bytes");
    AppSetLabel(dlg, NoteCountLabel, b);
}

static Boolean NoteDialogHandleEvent(EventType *e)
{
    FormType *dlg = FrmGetActiveForm();
    if (e->eType == keyDownEvent) {
        FrmHandleEvent(dlg, e);
        NoteCount(dlg);
        return true;
    }
    return false;
}

static void NoteDialog(FormType *form)
{
    FormType *dlg = FrmInitForm(NoteForm);
    UInt16 hit;
    FrmSetEventHandler(dlg, NoteDialogHandleEvent);
    AppSetFieldText(dlg, FillNoteField, gFill.notes);
    NoteCount(dlg);
    FrmSetFocus(dlg, FrmGetObjectIndex(dlg, FillNoteField));
    hit = FrmDoDialog(dlg);
    if (hit == NoteOkButton) { StrNCopy(gFill.notes, AppGetFieldText(dlg, FillNoteField), fillMaxNotes - 1); gFill.notes[fillMaxNotes - 1] = 0; }
    else if (hit == NoteClearButton) gFill.notes[0] = 0;
    FrmDeleteForm(dlg);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillNoteButton)), gFill.notes[0] ? "Note*" : "Note");
}

/* ---- exchange rate ---- */

/* The rate field shows only when the fill is not in the home currency. */
static void ShowRate(FormType *form)
{
    Boolean foreign = gFill.currency != gPrefs.homeCurrency;
    UInt16 li = FrmGetObjectIndex(form, FillRateLabel), fi = FrmGetObjectIndex(form, FillRateField);
    if (foreign) {
        char b[16];
        TextFixed(b, (Int32)(gFill.fxRate / 100), 4, gPoint, 0);
        AppSetFieldText(form, FillRateField, b);
        FrmShowObject(form, li); FrmShowObject(form, fi);
    } else {
        FrmHideObject(form, li); FrmHideObject(form, fi);
    }
}

static Boolean IsChargeGrade(UInt8 grade) { return grade >= fillGradeHomeAC; }

/* Labels and title follow fuel or charge mode. */
static void SetMode(FormType *form, Boolean charge)
{
    char b[24];
    gCharge = charge;
    if (charge) {
        AppSetLabel(form, FillVolumeLabel, "Energy kWh:");
        StrCopy(b, gPrefs.priceFormat == unitPriceFormatCents ? "Price c/kWh:" : "Price $/kWh:");
        AppSetLabel(form, FillPriceLabel, b);
        AppSetLabel(form, FillGradeLabel, "Where:");
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillFullCheckbox)), "To full");
        /* Home charging is paid on the hydro bill; payment 0 means that for a charge. */
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCashButton)), "Bill");
    } else {
        StrCopy(b, "Volume ("); StrCat(b, UnitVolLabel(gPrefs.volumeUnit)); StrCat(b, "):");
        AppSetLabel(form, FillVolumeLabel, b);
        StrCopy(b, gPrefs.priceFormat == unitPriceFormatCents ? "Price (c" : "Price ($");
        StrCat(b, UnitPriceLabel(gPrefs.priceUnit)); StrCat(b, "):");
        AppSetLabel(form, FillPriceLabel, b);
        AppSetLabel(form, FillGradeLabel, "Grade:");
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillFullCheckbox)), "Full tank");
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCashButton)), "Cash");
    }
    FrmSetTitle(form, gEditIndex == dmMaxRecordIndex ? (charge ? "New Charge" : "New Fill") : (charge ? "Edit Charge" : "Edit Fill"));
}

/* Build the grade popup for the vehicle and select the fill's grade. */
static void BuildGradeList(FormType *form, UInt8 powertrain, UInt8 grade)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillGradeList));
    UInt8 from = powertrain == vehPowerElectric ? 4 : 0, to = powertrain == vehPowerFuel || powertrain == vehPowerHybrid ? 4 : 7, g;
    Int16 sel = 0;
    gGradeRows = 0;
    for (g = from; g < to; g++) {
        gGradePtrs[gGradeRows] = (char *)kGradeNames[g];
        gGradeMap[gGradeRows] = g;
        if (g == grade) sel = (Int16)gGradeRows;
        gGradeRows++;
    }
    LstSetListChoices(lst, gGradePtrs, (Int16)gGradeRows);
    LstSetHeight(lst, (Int16)gGradeRows);
    LstSetSelection(lst, sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillGradeTrigger)), gGradePtrs[sel]);
}

static UInt8 SelectedGrade(FormType *form)
{
    Int16 sel = LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillGradeList)));
    return (sel >= 0 && sel < (Int16)gGradeRows) ? gGradeMap[sel] : 0;
}

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

/* Volume is mL for fuel and Wh for a charge (typed as kWh to one decimal). */
static Int32 ReadVolume(FormType *form, UInt32 *ml)
{
    UInt32 v;
    Int32 r;
    if (gCharge) {
        r = TextParseFixed(AppGetFieldText(form, FillVolumeField), 1, gPoint, gThousands, &v);
        if (r == 1) *ml = v * 100;
        return r;
    }
    r = TextParseFixed(AppGetFieldText(form, FillVolumeField), UnitVolDecimals(gPrefs.volumeUnit), gPoint, gThousands, &v);
    if (r == 1) *ml = UnitVolFromDisplay(v, gPrefs.volumeUnit);
    return r;
}

static Int32 ReadPrice(FormType *form, UInt32 *price)
{
    UInt32 v;
    Int32 r = TextParsePrice(AppGetFieldText(form, FillPriceField), gPrefs.priceFormat == unitPriceFormatCents, gPoint, gThousands, &v);
    if (r == 1) *price = gCharge ? v : UnitPriceFromDisplay(v, gPrefs.priceUnit);
    return r;
}

static Int32 ReadTotal(FormType *form, UInt32 *cents)
{
    return TextParseFixed(AppGetFieldText(form, FillTotalField), 2, gPoint, gThousands, cents);
}

static void ShowVolume(FormType *form, UInt32 ml)
{
    char b[16];
    if (gCharge) TextFixed(b, (Int32)((ml + 50) / 100), 1, gPoint, 0);
    else TextFixed(b, (Int32)UnitVolToDisplay(ml, gPrefs.volumeUnit), UnitVolDecimals(gPrefs.volumeUnit), gPoint, 0);
    AppSetFieldText(form, FillVolumeField, b);
}
static void ShowPrice(FormType *form, UInt32 price)
{
    char b[16]; TextPrice(b, gCharge ? price : UnitPriceToDisplay(price, gPrefs.priceUnit), gPrefs.priceFormat == unitPriceFormatCents, gPoint);
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
    UInt32 gross;

    /* Points first: auto-earned points follow the volume and gross price,
     * and redemptions take a discount off the total. */
    if ((have & typedVolume) && (have & typedPrice)) gross = CalcTotalCents(price, ml);
    else gross = cents + gDiscount;
    PointsRecompute(form, ml, gross);

    if (last && prev) target = (UInt8)(7 & ~(last | prev));
    else if (have == (typedVolume | typedPrice)) target = typedTotal;
    else if (have == (typedVolume | typedTotal)) target = typedPrice;
    else if (have == (typedPrice | typedTotal)) target = typedVolume;
    AppSetLabel(form, FillCalcHint, "            ");
    if (target == typedTotal && (have & typedVolume) && (have & typedPrice)) {
        ShowTotal(form, gross > gDiscount ? gross - gDiscount : 0);
        AppSetLabel(form, FillCalcHint, gDiscount ? "less points" : "calculated");
    } else if (target == typedPrice && (have & typedVolume) && (have & typedTotal) && ml) {
        ShowPrice(form, CalcMulDiv(cents + gDiscount, 100000UL, ml));
        AppSetLabel(form, FillCalcHint, "price calc.");
    } else if (target == typedVolume && (have & typedPrice) && (have & typedTotal) && price) {
        ShowVolume(form, CalcMulDiv(cents + gDiscount, 100000UL, price));
        AppSetLabel(form, FillCalcHint, "volume calc.");
    }
}

static void CheckTrip(FormType *form)
{
    UInt32 odo = 0, trip = 0;
    Int32 ho = TextParseFixed(AppGetFieldText(form, FillOdoField), 1, gPoint, gThousands, &odo);
    Int32 ht = TextParseFixed(AppGetFieldText(form, FillTripField), 1, gPoint, gThousands, &trip);
    char b[24];
    if (ho == 1) odo = UnitDistFromDisplay(odo, gFillVeh.odoUnit);
    if (ht == 1) trip = UnitDistFromDisplay(trip, gFillVeh.odoUnit);
    if (ht == 1 && trip == 0) ht = 0;          /* a trip of 0 means not entered */
    if (ho == 1 && ht == 1 && gPrevOdo && odo > gPrevOdo) {
        UInt32 moved = odo - gPrevOdo, gap = moved > trip ? moved - trip : trip - moved;
        UInt32 tol = moved / 50 > 50 ? moved / 50 : 50;   /* 2 % or 5.0 km, whichever is larger */
        AppSetLabel(form, FillTripHint, gap <= tol ? "= odo" : "check odo");
    } else if (ho != 1 && ht == 1 && gPrevOdo) {
        /* Blank odometer: it will be the previous one plus the trip. */
        StrCopy(b, "odo ");
        TextFixed(b + 4, (Int32)(UnitDistToDisplay(gPrevOdo + trip, gFillVeh.odoUnit) / 10), 0, gPoint, gThousands);
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
    FillRecord *prev = &gPrevFill;

    UInt8 i;

    MemSet(&gFill, sizeof gFill, 0);
    gTyped = 0; gOrder[0] = gOrder[1] = gOrder[2] = 0;
    gPrevOdo = 0;
    gPtsCount = 0; gPtsTouched = false; gDiscount = 0;
    if (gEditIndex != dmMaxRecordIndex && DbGetFill(gEditIndex, &gFill) == errNone) {
        if (gEditIndex > 0 && DbGetFill(gEditIndex - 1, prev) == errNone) gPrevOdo = prev->odometer;
        /* Stored points are kept as typed; the dialog can put a row back on auto. */
        for (i = 0; i < gFill.pointCount && i < fillMaxPoints; i++) {
            gPts[i].pgm = gFill.points[i].programme; gPts[i].earned = gFill.points[i].earned;
            gPts[i].redeemed = gFill.points[i].redeemed; gPts[i].autoEarned = false;
        }
        gPtsCount = (UInt8)(gFill.pointCount < fillMaxPoints ? gFill.pointCount : fillMaxPoints);
        gPtsTouched = true;
        gDiscount = gFill.discount;
    } else {
        gEditIndex = dmMaxRecordIndex;
        gFill.date = AppToday();
        gFill.time = AppNowMinutes();
        gFill.vehicle = VehDefaultForNewFill();
        gFill.flags = fillFlagFullTank;
        VehGet(gFill.vehicle, &gFillVeh);
        gFill.grade = gFillVeh.fuelDefault;
        gFill.currency = CurrencyForNewEntry(gFillVeh.defaultCurrency, gPrefs.defaultCurrency, gPrefs.lastCurrency);
        gFill.fxRate = CurrencyDefaultRate(gFill.currency, gPrefs.homeCurrency, gPrefs.fxRate);
        gFill.payment = gPrefs.lastPayment;
        gPrevOdo = VehLastOdometer(gFill.vehicle);
        PointsAutoApply();
    }
    VehGet(gFill.vehicle, &gFillVeh);
    {
        ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillVehicleList));
        Int16 sel;
        VehBuildChoices(lst, gFVNames, gFVPtrs, gFVIds, false, gFill.vehicle, gFill.vehicle, &sel);
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillVehicleTrigger)), gFVNames[sel]);
    }
    gDate = gFill.date;
    gTime = gFill.time;
    DateShortText(gDateText, gDate);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillDateTrigger)), gDateText);
    AppTimeText(gTimeText, gTime);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillTimeTrigger)), gTimeText);
    /* The odometer shows whole units like the dash; tenths are accepted when typed. */
    if (gFill.odometer) { TextFixed(b, (Int32)(UnitDistToDisplay(gFill.odometer, gFillVeh.odoUnit) / 10), 0, gPoint, 0); AppSetFieldText(form, FillOdoField, b); }
    if (gFill.trip) { TextFixed(b, (Int32)(UnitDistToDisplay(gFill.trip, gFillVeh.odoUnit)), 1, gPoint, 0); AppSetFieldText(form, FillTripField, b); }
    if (gFill.volume) ShowVolume(form, gFill.volume);
    if (gFill.price) ShowPrice(form, gFill.price);
    if (gFill.total) ShowTotal(form, gFill.total);
    if (gPrevOdo) {
        StrCopy(b, "p: ");
        TextFixed(b + 3, (Int32)(UnitDistToDisplay(gPrevOdo, gFillVeh.odoUnit) / 10), 0, gPoint, gThousands);
        AppSetLabel(form, FillOdoHint, b);
    }
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCurrencyTrigger)), CurrencyCode(gFill.currency));
    LstSetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCurrencyList)), gFill.currency);
    FrmSetControlGroupSelection(form, 1, gFill.payment == fillPaymentCard ? FillCardButton : FillCashButton);
    /* An electric car charges; a plug-in may do either; the grade list says which. */
    if (gEditIndex == dmMaxRecordIndex && gFillVeh.powertrain == vehPowerElectric && !IsChargeGrade(gFill.grade))
        gFill.grade = fillGradeHomeAC;
    BuildGradeList(form, gFillVeh.powertrain, gFill.grade);
    SetMode(form, IsChargeGrade(gFill.grade));
    /* Show the stored volume and price in the mode's units (fuel values were shown above in litres). */
    if (gCharge) {
        if (gFill.volume) ShowVolume(form, gFill.volume);
        if (gFill.price) ShowPrice(form, gFill.price);
        else if (gEditIndex == dmMaxRecordIndex && gFill.grade == fillGradeHomeAC && gFillVeh.homeRate) ShowPrice(form, gFillVeh.homeRate);
    }
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillFullCheckbox)), (gFill.flags & fillFlagFullTank) ? 1 : 0);
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillMissedCheckbox)), (gFill.flags & fillFlagMissedPrev) ? 1 : 0);
    ShowRate(form);
    PointsRecompute(form, gFill.volume, gFill.total + gFill.discount);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillNoteButton)), gFill.notes[0] ? "Note*" : "Note");
    FrmShowObject(form, FrmGetObjectIndex(form, FillDeleteButton));
    if (gEditIndex == dmMaxRecordIndex) FrmHideObject(form, FrmGetObjectIndex(form, FillDeleteButton));
    FrmSetFocus(form, FrmGetObjectIndex(form, FillOdoField));
}

static Boolean Save(FormType *form)
{
    UInt32 odo = 0, trip = 0, ml = 0, price = 0, cents = 0;
    Int32 ho = TextParseFixed(AppGetFieldText(form, FillOdoField), 1, gPoint, gThousands, &odo);
    Int32 ht = TextParseFixed(AppGetFieldText(form, FillTripField), 1, gPoint, gThousands, &trip);
    Int32 hv = ReadVolume(form, &ml), hp = ReadPrice(form, &price), hc = ReadTotal(form, &cents);
    Err err;

    if (ho == 1) odo = UnitDistFromDisplay(odo, gFillVeh.odoUnit);
    if (ht == 1) trip = UnitDistFromDisplay(trip, gFillVeh.odoUnit);
    if (ht == 1 && trip == 0) ht = 0;
    if (ho != 1 && ht == 1 && gPrevOdo) odo = gPrevOdo + trip;     /* quick entry */
    if (ho < 0 || ht < 0 || hv < 0 || hp < 0 || hc < 0 || odo == 0) {
        FrmAlert(BadNumberAlert);
        return false;
    }
    if (ho == 1 && ht == 1 && gPrevOdo && odo > gPrevOdo) {
        /* The trip meter and the odometer disagree: offer the corrected reading. */
        UInt32 moved = odo - gPrevOdo, gap = moved > trip ? moved - trip : trip - moved;
        UInt32 tol = moved / 50 > 50 ? moved / 50 : 50;
        if (gap > tol) {
            char tripTxt[20], movedTxt[20], useTxt[20];
            UInt16 hit;
            TextFixed(tripTxt, (Int32)UnitDistToDisplay(trip, gFillVeh.odoUnit), 1, gPoint, gThousands);
            StrCat(tripTxt, " "); StrCat(tripTxt, UnitDistLabel(gFillVeh.odoUnit));
            TextFixed(movedTxt, (Int32)UnitDistToDisplay(moved, gFillVeh.odoUnit), 1, gPoint, gThousands);
            StrCat(movedTxt, " "); StrCat(movedTxt, UnitDistLabel(gFillVeh.odoUnit));
            TextFixed(useTxt, (Int32)(UnitDistToDisplay(gPrevOdo + trip, gFillVeh.odoUnit) / 10), 0, gPoint, gThousands);
            hit = FrmCustomAlert(OdometerAlert, tripTxt, movedTxt, useTxt);
            if (hit == 0) odo = gPrevOdo + trip;
            else if (hit == 1) return false;
        }
    }
    if (hv != 1 && hc != 1) { FrmAlert(NeedVolumeAlert); return false; }
    if (hv == 1 && hp == 1 && hc != 1) { UInt32 gross = CalcTotalCents(price, ml); cents = gross > gDiscount ? gross - gDiscount : 0; }
    if (gFill.currency != gPrefs.homeCurrency) {
        UInt32 rate;
        Int32 hr = TextParseFixed(AppGetFieldText(form, FillRateField), 4, gPoint, gThousands, &rate);
        if (hr < 0) { FrmAlert(BadNumberAlert); return false; }
        gFill.fxRate = hr == 1 && rate ? rate * 100 : CurrencyDefaultRate(gFill.currency, gPrefs.homeCurrency, gPrefs.fxRate);
        gPrefs.fxRate[gFill.currency] = gFill.fxRate;
    } else gFill.fxRate = fxOne;
    gFill.date = gDate;
    gFill.time = gTime;
    gFill.odometer = odo;
    gFill.trip = trip;
    gFill.volume = ml;
    gFill.price = price;
    gFill.total = cents;
    gFill.grade = SelectedGrade(form);
    gFill.flags = (UInt16)((gFill.flags & ~(fillFlagFullTank | fillFlagTripEntered | fillFlagCharge | fillFlagMissedPrev))
                 | (CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillFullCheckbox))) ? fillFlagFullTank : 0)
                 | (CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillMissedCheckbox))) ? fillFlagMissedPrev : 0)
                 | (ht == 1 ? fillFlagTripEntered : 0)
                 | (IsChargeGrade(gFill.grade) ? fillFlagCharge : 0));
    gFill.payment = FrmGetControlGroupSelection(form, 1) == FillCardButton ? fillPaymentCard : fillPaymentCash;
    gFill.currency = (UInt8)LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillCurrencyList)));
    gFill.discount = gDiscount;
    gFill.pointCount = 0;
    {
        UInt8 i;
        for (i = 0; i < gPtsCount; i++) {
            if (gPts[i].earned == 0 && gPts[i].redeemed == 0) continue;   /* an empty row is no entry */
            gFill.points[gFill.pointCount].programme = gPts[i].pgm;
            gFill.points[gFill.pointCount].earned = gPts[i].earned;
            gFill.points[gFill.pointCount].redeemed = gPts[i].redeemed;
            gFill.pointCount++;
        }
    }
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
    static const UInt16 bottom[] = { FillDoneButton, FillCancelButton, FillNoteButton, FillDeleteButton };
    static const UInt16 pinned[] = { FillDoneButton, FillCancelButton, FillNoteButton, FillDeleteButton, FillVehicleTrigger, FillVehicleList };

    switch (event->eType) {
    case frmOpenEvent:
        AppScrollReset();
        AppFitForm(form, bottom, 4);
        Load(form);
        FrmDrawForm(form);
        return true;
    case fldEnterEvent:
        AppScrollToField(form, event->data.fldEnter.fieldID, pinned, 6);
        break;
    case frmUpdateEvent:
        FrmDrawForm(form);
        return true;
    case winDisplayChangedEvent:
        if (gScrollYPublic()) AppScrollBy(form, gScrollYPublic(), pinned, 6);   /* back to the top before refitting */
        AppFitForm(form, bottom, 4);
        WinEraseWindow();
        FrmDrawForm(form);
        return true;
    case fldChangedEvent:
        AfterEdit(form, event->data.fldChanged.fieldID);
        return true;

    case keyDownEvent: {
        /* Let the field take the character first, then recompute from what
         * it now holds. This does not rely on fldChangedEvent arriving. */
        UInt16 focus;
        if (AppScrollKey(form, event, pinned, 6)) return true;
        focus = FrmGetFocus(form);
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
        if (event->data.popSelect.listID == FillVehicleList) {
            UInt8 id = gFVIds[event->data.popSelect.selection];
            if (id != gFill.vehicle) {
                char b[24];
                gFill.vehicle = id;
                VehGet(id, &gFillVeh);
                gPrevOdo = (gEditIndex == dmMaxRecordIndex) ? VehLastOdometer(id) : gPrevOdo;
                if (gPrevOdo) {
                    StrCopy(b, "p: ");
                    TextFixed(b + 3, (Int32)(UnitDistToDisplay(gPrevOdo, gFillVeh.odoUnit) / 10), 0, gPoint, gThousands);
                } else StrCopy(b, "               ");
                AppSetLabel(form, FillOdoHint, b);
                CheckTrip(form);
                if (gFillVeh.powertrain == vehPowerElectric && !IsChargeGrade(gFill.grade)) gFill.grade = fillGradeHomeAC;
                if (gFillVeh.powertrain == vehPowerFuel && IsChargeGrade(gFill.grade)) gFill.grade = gFillVeh.fuelDefault;
                BuildGradeList(form, gFillVeh.powertrain, gFill.grade);
                if (IsChargeGrade(gFill.grade) != gCharge) {
                    SetMode(form, IsChargeGrade(gFill.grade));
                    AppSetFieldText(form, FillVolumeField, "");
                    AppSetFieldText(form, FillPriceField, "");
                    AppSetFieldText(form, FillTotalField, "");
                    gOrder[0] = gOrder[1] = gOrder[2] = 0;
                }
            }
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, FillVehicleTrigger)), gFVNames[event->data.popSelect.selection]);
            return true;
        }
        if (event->data.popSelect.listID == FillCurrencyList) {
            gFill.currency = (UInt8)event->data.popSelect.selection;
            gFill.fxRate = CurrencyDefaultRate(gFill.currency, gPrefs.homeCurrency, gPrefs.fxRate);
            ShowRate(form);
        }
        if (event->data.popSelect.listID == FillGradeList) {
            UInt8 g = gGradeMap[event->data.popSelect.selection];
            Boolean charge = IsChargeGrade(g);
            UInt32 ml = 0, price = 0;
            Boolean hadVol = ReadVolume(form, &ml) == 1, hadPrice = ReadPrice(form, &price) == 1;
            if (charge != gCharge) {
                /* Switching between fuel and charge: clear the typed quantities,
                 * since litres are not kWh, and offer the home rate for a Home charge. */
                SetMode(form, charge);
                AppSetFieldText(form, FillVolumeField, "");
                AppSetFieldText(form, FillPriceField, "");
                AppSetFieldText(form, FillTotalField, "");
                AppSetLabel(form, FillCalcHint, "            ");
                gOrder[0] = gOrder[1] = gOrder[2] = 0;
                (void)hadVol; (void)hadPrice;
            }
            if (charge && g == fillGradeHomeAC && gFillVeh.homeRate && AppGetFieldText(form, FillPriceField)[0] == 0) {
                ShowPrice(form, gFillVeh.homeRate);
                Touch(typedPrice);
                Recompute(form);
            }
            gFill.grade = g;
            Recompute(form);   /* premium changes auto points */
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
        case FillCashButton: case FillCardButton:
            /* Card-only programmes come and go with the payment. */
            gFill.payment = event->data.ctlSelect.controlID == FillCardButton ? fillPaymentCard : fillPaymentCash;
            if (gEditIndex == dmMaxRecordIndex && !gPtsTouched) PointsAutoApply();
            Recompute(form);
            break;
        case FillPointsTrigger:
            PointsDialog(form);
            return true;
        case FillNoteButton:
            NoteDialog(form);
            return true;
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

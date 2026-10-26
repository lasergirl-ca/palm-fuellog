/* Rewards programmes: the list (most used first) and the Edit Programme
 * form, whose rules read as sentences: "Earn 10 pts per litre",
 * "1,000 pts = $1.00". Balances are worked out from the fills each time. */
#include "programmes.h"
#include "presets.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "currency.h"
#include "text.h"
#include "resources.h"
#include "points.h"
#include "datecalc.h"

UInt16 gEditProgrammeIndex = dmMaxRecordIndex;
static ProgrammeRecord gPgm;            /* the one being edited */
static ProgrammeRecord gScanPgm;
static FillRecord gScanFillP;
static UInt8 gPacked[pgmFixedSize + pgmAliasLen + 4];
static RectangleType gPgmRows;
static UInt16 gOrder[pgmMaxProgrammes];  /* list rows -> database index */
static UInt16 gOrderCount;
static Int16  gPgmSel = -1;   /* 5-way highlighted row */
#define kPgmRowH 20
static UInt16 gPgmFirst;   /* first row shown; page keys move it */
#define kPgmTop 18

/* ---- lookups ---- */

Err PgmGetAt(UInt16 index, ProgrammeRecord *p)
{
    UInt32 len;
    Err err = DbGet(dbProgrammes, index, gPacked, sizeof gPacked, &len);
    if (err) return err;
    return ProgrammeUnpack(gPacked, len, p) == 0 ? errNone : dbErrCorrupt;
}

Int16 PgmFindById(UInt8 id)
{
    UInt16 n = DbCount(dbProgrammes), i;
    for (i = 0; i < n; i++)
        if (PgmGetAt(i, &gScanPgm) == errNone && gScanPgm.id == id) return (Int16)i;
    return -1;
}

Boolean PgmGet(UInt8 id, ProgrammeRecord *p)
{
    Int16 i = PgmFindById(id);
    if (i >= 0 && PgmGetAt((UInt16)i, p) == errNone) return true;
    MemSet(p, sizeof *p, 0);
    p->id = id; StrCopy(p->name, "Programme"); p->premiumMult = 100;
    return false;
}

UInt8 PgmNextId(void)
{
    UInt16 used = 0, n = DbCount(dbProgrammes), i;
    UInt8 id;
    for (i = 0; i < n; i++)
        if (PgmGetAt(i, &gScanPgm) == errNone && gScanPgm.id >= 1 && gScanPgm.id <= 16) used |= (UInt16)(1 << (gScanPgm.id - 1));
    for (id = 1; id <= pgmMaxProgrammes; id++) if (!(used & (1 << (id - 1)))) return id;
    return 0;
}

UInt16 PgmUseCount(UInt8 id)
{
    UInt16 n = DbCount(dbFills), i, count = 0;
    UInt8 k;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanFillP) != errNone) continue;
        for (k = 0; k < gScanFillP.pointCount; k++) if (gScanFillP.points[k].programme == id) { count++; break; }
    }
    return count;
}

Int32 PgmBalance(UInt8 id, Int32 opening)
{
    UInt16 n = DbCount(dbFills), i;
    UInt8 k;
    Int32 bal = opening;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanFillP) != errNone) continue;
        for (k = 0; k < gScanFillP.pointCount; k++)
            if (gScanFillP.points[k].programme == id) bal += gScanFillP.points[k].earned - gScanFillP.points[k].redeemed;
    }
    return bal;
}

Int32 PgmExpiring(const ProgrammeRecord *p, UInt16 *when)
{
    UInt16 n, i, today, until, y; UInt8 m, d, k; Int32 out;
    PointsAge *age;   /* ~600 bytes: too big for the data segment, so it lives on the heap for the call */
    *when = 0;
    if (p->expiryRule == pgmExpiryNone) return 0;
    age = MemPtrNew(sizeof *age);
    if (!age) return 0;
    n = DbCount(dbFills);
    PointsBegin(age, p->expiryRule, p->expiryMonths);
    if (p->openingBalance > 0) PointsAdd(age, 0, p->openingBalance, 0);   /* the opening balance ages like an old lot */
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanFillP) != errNone) continue;
        for (k = 0; k < gScanFillP.pointCount; k++)
            if (gScanFillP.points[k].programme == p->id) PointsAdd(age, gScanFillP.date, gScanFillP.points[k].earned, gScanFillP.points[k].redeemed);
    }
    today = AppToday();
    DateDaysToYmd(DateAddMonths(today, 1), &y, &m, &d);
    until = DateYmdToDays(y, m, DateDaysInMonth(y, m));   /* end of next month */
    out = PointsExpiringBy(age, today, until, when);
    MemPtrFree(age);
    return out;
}

Boolean PgmIsMoney(const ProgrammeRecord *p) { return p->valuePerPoint == 1000; }

void PgmValueText(char *buf, const ProgrammeRecord *p, Int32 points)
{
    if (PgmIsMoney(p)) {
        StrCopy(buf, points < 0 ? "-$" : "$");
        TextFixed(buf + StrLen(buf), points < 0 ? -points : points, 2, gPoint, gThousands);
    } else {
        TextFixed(buf, points, 0, gPoint, gThousands);
        StrCat(buf, " pts");
    }
}

void PgmRateText(char *buf, UInt32 earnRate)
{
    if (earnRate % 100 == 0) TextFixed(buf, (Int32)(earnRate / 100), 0, gPoint, 0);
    else if (earnRate % 10 == 0) TextFixed(buf, (Int32)(earnRate / 10), 1, gPoint, 0);
    else TextFixed(buf, (Int32)earnRate, 2, gPoint, 0);
}

void PgmInstallPresets(void)
{
    UInt8 i;
    UInt16 n = DbCount(dbFills), k;
    if (DbCount(dbProgrammes) != 0) return;
    for (k = 0; k < n; k++)
        if (DbGetFill(k, &gScanFillP) == errNone && gScanFillP.pointCount) return;   /* points exist: the user removed them on purpose */
    for (i = 0; i < pgmPresetCount; i++) {
        UInt32 n;
        PgmPreset(i, &gScanPgm);
        if (i == pgmPresetCount - 1) gScanPgm.currency = gPrefs.homeCurrency;   /* card points: any currency */
        n = ProgrammePack(&gScanPgm, gPacked);
        DbAdd(dbProgrammes, gPacked, n, NULL);
    }
}

UInt16 PgmBuildChoices(ListType *lst, char names[][pgmNameLen], char *ptrs[], UInt8 ids[],
                       Boolean includeNone, UInt8 alwaysId, UInt8 selectedId, Int16 *selOut)
{
    UInt16 n = DbCount(dbProgrammes), i, count = 0;
    Int16 sel = 0;
    if (includeNone) { StrCopy(names[0], "(none)"); ids[0] = 0; count = 1; }
    for (i = 0; i < n && count < pgmMaxProgrammes + 1; i++) {
        if (PgmGetAt(i, &gScanPgm) != errNone) continue;
        if (gScanPgm.archived && gScanPgm.id != alwaysId) continue;
        StrCopy(names[count], gScanPgm.name);
        ids[count] = gScanPgm.id;
        if (gScanPgm.id == selectedId) sel = (Int16)count;
        count++;
    }
    if (count == 0) { StrCopy(names[0], "No programmes"); ids[0] = 0; count = 1; }
    for (i = 0; i < count; i++) ptrs[i] = names[i];
    LstSetListChoices(lst, ptrs, (Int16)count);
    LstSetHeight(lst, (Int16)(count < 6 ? count : 6));
    LstSetSelection(lst, sel);
    *selOut = sel;
    return count;
}

/* ---- Programmes list ---- */

/* The earn rule and flags in one line: "10 pts/L; Card only; auto". */
/* Section placement: see sections.h. */
static void RuleText(char *buf, const ProgrammeRecord *p) FORMS_SEC;
static void SortProgrammes(void) FORMS_SEC;
static void DrawProgrammes(void) FORMS_SEC;
static void LayoutProgrammes(FormType *form) FORMS_SEC;
static void SetPopup(FormType *form, UInt16 triggerId, UInt16 listId, Int16 sel) FORMS_SEC;
static Int16 GetPopup(FormType *form, UInt16 listId) FORMS_SEC;
static void ShowObj(FormType *form, UInt16 id, Boolean show) FORMS_SEC;
static void SetMoneyLabels(FormType *form, Boolean money) FORMS_SEC;
static void ShowRedeemRows(FormType *form, UInt8 kind) FORMS_SEC;
static void ShowExpiryRows(FormType *form, UInt8 rule) FORMS_SEC;
static Int32 ParseSigned(const char *t, UInt8 decimals, Int32 *out) FORMS_SEC;
static void LoadProgramme(FormType *form) FORMS_SEC;
static Boolean SaveProgramme(FormType *form) FORMS_SEC;

static void RuleText(char *buf, const ProgrammeRecord *p)
{
    char num[16];
    static const char *unit[] = { "", "L", "gal", "$" };
    if (p->earnBasis == pgmEarnNone || p->earnRate == 0) StrCopy(buf, "no earn rule");
    else {
        PgmRateText(num, p->earnRate);
        StrCopy(buf, num);
        StrCat(buf, PgmIsMoney(p) ? "c/" : " pts/");
        StrCat(buf, unit[p->earnBasis < 4 ? p->earnBasis : 0]);
    }
    if (p->requiresPayment == pgmPaymentCard) StrCat(buf, "; Card only");
    if (p->requiresPayment == pgmPaymentCash) StrCat(buf, "; Cash only");
    if (p->autoApply) StrCat(buf, "; auto");
    if (p->archived) StrCat(buf, "; archived");
}

/* Most used first, archived last. */
static void SortProgrammes(void)
{
    UInt16 n = DbCount(dbProgrammes), i, j;
    static UInt16 key[pgmMaxProgrammes];
    gOrderCount = 0;
    for (i = 0; i < n && gOrderCount < pgmMaxProgrammes; i++) {
        UInt16 k;
        if (PgmGetAt(i, &gScanPgm) != errNone) continue;
        k = (UInt16)(PgmUseCount(gScanPgm.id) + 1);
        PgmGetAt(i, &gScanPgm);
        if (gScanPgm.archived) k = 0;
        for (j = gOrderCount; j > 0 && key[j - 1] < k; j--) { key[j] = key[j - 1]; gOrder[j] = gOrder[j - 1]; }
        key[j] = k; gOrder[j] = i;
        gOrderCount++;
    }
}

static void DrawProgrammes(void)
{
    UInt16 row;
    Coord y = kPgmTop;
    char buf[48], val[20];

    WinEraseRectangle(&gPgmRows, 0);
    if (gOrderCount == 0) {
        FntSetFont(stdFont);
        WinDrawChars("No programmes. Tap New to add one.", 34, 6, y + 20);
        return;
    }
    if (gPgmFirst >= gOrderCount) gPgmFirst = 0;
    if (gPgmSel >= (Int16)gOrderCount) gPgmSel = -1;
    for (row = gPgmFirst; row < gOrderCount && y + kPgmRowH <= gPgmRows.topLeft.y + gPgmRows.extent.y; row++, y += kPgmRowH) {
        Int32 bal;
        Boolean hi = (Int16)row == gPgmSel;
        if (PgmGetAt(gOrder[row], &gScanPgm) != errNone) continue;
        if (hi) { RectangleType sr; sr.topLeft.x = 0; sr.topLeft.y = y; sr.extent.x = 160; sr.extent.y = kPgmRowH; AppRowHighlight(&sr); }
        FntSetFont(gScanPgm.archived ? stdFont : boldFont);
        WinDrawChars(gScanPgm.name, StrLen(gScanPgm.name), 4, y);
        bal = PgmBalance(gScanPgm.id, gScanPgm.openingBalance);
        PgmGetAt(gOrder[row], &gScanPgm);          /* the balance scan reuses the scratch buffers */
        PgmValueText(val, &gScanPgm, bal);
        WinDrawChars(val, StrLen(val), 150 - FntCharsWidth(val, StrLen(val)), y);
        FntSetFont(stdFont);
        {
            UInt16 when; Int32 exp = PgmExpiring(&gScanPgm, &when);
            PgmGetAt(gOrder[row], &gScanPgm);
            if (exp > 0) {
                /* the warning that costs money comes first */
                TextFixed(buf, exp, 0, gPoint, gThousands); StrCat(buf, " expire "); DateShortText(buf + StrLen(buf), when);
                WinDrawChars(buf, StrLen(buf), 10, y + 10);
                if (hi) AppRowPlain();
                continue;
            }
        }
        RuleText(buf, &gScanPgm);
        if (!PgmIsMoney(&gScanPgm) && gScanPgm.valuePerPoint && bal > 0) {
            /* "about $3.40": points x thousandths of a cent / 100000 = dollars */
            UInt32 cents = CalcMulDiv((UInt32)bal, gScanPgm.valuePerPoint, 1000);
            StrCat(buf, "; about $");
            TextFixed(buf + StrLen(buf), (Int32)cents, 2, gPoint, gThousands);
        }
        WinDrawChars(buf, StrLen(buf), 10, y + 10);
        if (hi) AppRowPlain();
    }
    {
        UInt16 visible = (UInt16)(gPgmRows.extent.y / kPgmRowH);
        AppListScrollBar(FrmGetActiveForm(), PgmsScrollBar, (Int16)gPgmFirst, (Int16)((Int16)gOrderCount - (Int16)visible), (Int16)visible,
                         gPgmRows.topLeft.y, (Coord)(gPgmRows.topLeft.y + gPgmRows.extent.y));
    }
}

static void LayoutProgrammes(FormType *form)
{
    static UInt16 bottom[] = { PgmsNewButton, PgmsDoneButton };
    Coord w, h;
    AppFitForm(form, bottom, 2);
    WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
    gPgmRows.topLeft.x = 0; gPgmRows.topLeft.y = kPgmTop; gPgmRows.extent.x = w; gPgmRows.extent.y = h - 17 - kPgmTop;
}

Boolean ProgrammesFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        LayoutProgrammes(form);
        SortProgrammes();
        FrmDrawForm(form);
        DrawProgrammes();
        return true;
    case frmUpdateEvent:
        FrmDrawForm(form); DrawProgrammes(); return true;
    case winDisplayChangedEvent:
        LayoutProgrammes(form); WinEraseWindow(); FrmDrawForm(form); DrawProgrammes(); return true;
    case penDownEvent:
        if (RctPtInRectangle(event->screenX, event->screenY, &gPgmRows)) {
            UInt16 row = (UInt16)((event->screenY - gPgmRows.topLeft.y) / kPgmRowH + gPgmFirst);
            if (row < gOrderCount) { gPgmSel = (Int16)row; gEditProgrammeIndex = gOrder[row]; FrmGotoForm(EditProgrammeForm); return true; }
        }
        break;
    case sclRepeatEvent: case sclExitEvent: {
        Int16 v;
        if (AppListScrollEvent(event, &v)) { gPgmFirst = (UInt16)(v < 0 ? 0 : v); DrawProgrammes(); return true; }
        break;
    }
    case keyDownEvent: {
        /* Page keys move through a long list by a screenful. */
        UInt16 visible = (UInt16)(gPgmRows.extent.y / kPgmRowH);
        WChar c = event->data.keyDown.chr;
        Boolean page;
        Int16 dir = AppScrollDirection(event, &page);
        UInt16 step = page ? visible : 1;
        Int16 rk = AppRowKey(event, &gPgmSel, &gPgmFirst, gOrderCount, visible);
        (void)c;
        if (rk == 2) { gEditProgrammeIndex = gOrder[gPgmSel]; FrmGotoForm(EditProgrammeForm); return true; }
        if (rk == 1) { DrawProgrammes(); return true; }
        if (dir > 0 && gPgmFirst + visible < gOrderCount) { gPgmFirst += step; DrawProgrammes(); return true; }
        if (dir < 0 && gPgmFirst > 0) { gPgmFirst = gPgmFirst > step ? gPgmFirst - step : 0; DrawProgrammes(); return true; }
        break;
    }
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case PgmsNewButton:
            if (PgmNextId() == 0) { FrmAlert(TooManyProgrammesAlert); return true; }
            gEditProgrammeIndex = dmMaxRecordIndex; FrmGotoForm(EditProgrammeForm); return true;
        case PgmsDoneButton: FrmGotoForm(FillListForm); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- Edit programme ---- */

static void SetPopup(FormType *form, UInt16 triggerId, UInt16 listId, Int16 sel)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, listId));
    LstSetSelection(lst, sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, triggerId)), LstGetSelectionText(lst, sel));
}

static Int16 GetPopup(FormType *form, UInt16 listId)
{
    return LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, listId)));
}

static void ShowObj(FormType *form, UInt16 id, Boolean show)
{
    UInt16 idx = FrmGetObjectIndex(form, id);
    if (show) FrmShowObject(form, idx); else FrmHideObject(form, idx);
}

/* Labels that depend on the Points/Money choice. */
static void SetMoneyLabels(FormType *form, Boolean money)
{
    AppSetLabel(form, PgmEarnUnitLabel, money ? "c per" : "pts per");
    AppSetLabel(form, PgmOpeningUnitLabel, money ? "$" : "pts");
}

/* The redemption rows follow the kind: none, money off, per volume. */
static void ShowRedeemRows(FormType *form, UInt8 kind)
{
    Boolean any = kind != pgmRedeemNone, vol = kind == pgmRedeemPerVolume;
    ShowObj(form, PgmBlockField, any);
    ShowObj(form, PgmEqLabel, any);
    ShowObj(form, PgmValueField, any);
    AppSetLabel(form, PgmEqLabel, vol ? "pts = c" : "pts = $");
    ShowObj(form, PgmPerLabel, vol);
    ShowObj(form, PgmVolUnitTrigger, vol);
    ShowObj(form, PgmUpToLabel, vol);
    ShowObj(form, PgmMaxVolField, vol);
    ShowObj(form, PgmCapLabel, vol);
    ShowObj(form, PgmMaxPerUnitField, vol);
    ShowObj(form, PgmCapUnitLabel, vol);
}

static void ShowExpiryRows(FormType *form, UInt8 rule)
{
    ShowObj(form, PgmMonthsField, rule == pgmExpiryMonths);
}

/* The lower rows scroll under the pinned buttons when the input area is
 * open: show only those inside the scrolling area. */
/* Section placement: see sections.h. */
static void ClampRows(FormType *form) FORMS_SEC;

static void ClampRows(FormType *form)
{
    static UInt16 ids[12];
    UInt16 n = 0;
    UInt8 kind = (UInt8)GetPopup(form, PgmRedeemList), rule = (UInt8)GetPopup(form, PgmExpiryList);
    if (kind == pgmRedeemPerVolume) { ids[n++] = PgmCapLabel; ids[n++] = PgmMaxPerUnitField; ids[n++] = PgmCapUnitLabel; }
    ids[n++] = PgmExpiresLabel; ids[n++] = PgmExpiryTrigger;
    if (rule == pgmExpiryMonths) ids[n++] = PgmMonthsField;
    ids[n++] = PgmOpeningLabel; ids[n++] = PgmOpeningField; ids[n++] = PgmOpeningUnitLabel;
    ids[n++] = PgmAliasesLabel; ids[n++] = PgmAliasField; ids[n++] = PgmBalanceLabel;
    AppShowByPosition(form, ids, n);
}

/* Parse a whole number with an optional leading minus. 0 empty, 1 ok, -1 bad. */
static Int32 ParseSigned(const char *t, UInt8 decimals, Int32 *out)
{
    Boolean neg = false;
    UInt32 v;
    Int32 r;
    while (*t == ' ') t++;
    if (*t == '-') { neg = true; t++; }
    r = TextParseFixed(t, decimals, gPoint, gThousands, &v);
    if (r == 1) *out = neg ? -(Int32)v : (Int32)v;
    return r;
}

static void LoadProgramme(FormType *form)
{
    char b[24];
    Boolean money, isNew = gEditProgrammeIndex == dmMaxRecordIndex;
    UInt16 uses = 0;
    if (isNew || PgmGetAt(gEditProgrammeIndex, &gPgm) != errNone) {
        gEditProgrammeIndex = dmMaxRecordIndex; isNew = true;
        MemSet(&gPgm, sizeof gPgm, 0);
        gPgm.id = PgmNextId();
        gPgm.premiumMult = 100;
        gPgm.currency = gPrefs.homeCurrency;
        gPgm.earnBasis = pgmEarnPerLitre;
        gPgm.autoApply = 1;
    }
    money = PgmIsMoney(&gPgm);
    FrmSetTitle(form, isNew ? "New Programme" : "Edit Programme");
    AppSetFieldText(form, PgmNameField, gPgm.name);
    SetPopup(form, PgmCurrencyTrigger, PgmCurrencyList, gPgm.currency < curCount ? gPgm.currency : 0);
    FrmSetControlGroupSelection(form, 1, money ? PgmMoneyButton : PgmPointsButton);
    SetMoneyLabels(form, money);
    if (gPgm.earnRate) { PgmRateText(b, gPgm.earnRate); AppSetFieldText(form, PgmEarnField, b); }
    else AppSetFieldText(form, PgmEarnField, "");
    SetPopup(form, PgmBasisTrigger, PgmBasisList, gPgm.earnBasis < 4 ? gPgm.earnBasis : 0);
    StrIToA(b, gPgm.premiumMult ? gPgm.premiumMult : 100); AppSetFieldText(form, PgmPremiumField, b);
    SetPopup(form, PgmPayTrigger, PgmPayList, gPgm.requiresPayment < 3 ? gPgm.requiresPayment : 0);
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, PgmAutoCheckbox)), gPgm.autoApply);
    SetPopup(form, PgmRedeemTrigger, PgmRedeemList, gPgm.redeemKind < 3 ? gPgm.redeemKind : 0);
    if (gPgm.redeemBlock) { TextFixed(b, (Int32)gPgm.redeemBlock, 0, gPoint, gThousands); AppSetFieldText(form, PgmBlockField, b); }
    else AppSetFieldText(form, PgmBlockField, "");
    if (gPgm.redeemValue) {
        /* Money off: thousandths of a dollar per step shown as dollars; per volume: shown as cents per unit. */
        if (gPgm.redeemKind == pgmRedeemPerVolume) TextFixed(b, (Int32)gPgm.redeemValue, 1, gPoint, 0);
        else TextFixed(b, (Int32)(gPgm.redeemValue / 10), 2, gPoint, 0);
        AppSetFieldText(form, PgmValueField, b);
    } else AppSetFieldText(form, PgmValueField, "");
    SetPopup(form, PgmVolUnitTrigger, PgmVolUnitList, gPgm.redeemVolUnit == pgmVolUSGallon ? 1 : 0);
    if (gPgm.redeemMaxVol) {
        UInt32 units = gPgm.redeemVolUnit == pgmVolUSGallon ? CalcMulDiv(gPgm.redeemMaxVol, 1000000UL, 3785411784UL) : (gPgm.redeemMaxVol + 500) / 1000;
        TextFixed(b, (Int32)units, 0, gPoint, 0); AppSetFieldText(form, PgmMaxVolField, b);
    } else AppSetFieldText(form, PgmMaxVolField, "");
    if (gPgm.redeemMaxPerUnit) { TextFixed(b, (Int32)gPgm.redeemMaxPerUnit, 1, gPoint, 0); AppSetFieldText(form, PgmMaxPerUnitField, b); }
    else AppSetFieldText(form, PgmMaxPerUnitField, "");
    SetPopup(form, PgmExpiryTrigger, PgmExpiryList, gPgm.expiryRule < 3 ? gPgm.expiryRule : 0);
    if (gPgm.expiryMonths) { StrIToA(b, gPgm.expiryMonths); AppSetFieldText(form, PgmMonthsField, b); }
    else AppSetFieldText(form, PgmMonthsField, "");
    if (gPgm.openingBalance || !isNew) { TextFixed(b, gPgm.openingBalance, money ? 2 : 0, gPoint, 0); AppSetFieldText(form, PgmOpeningField, b); }
    else AppSetFieldText(form, PgmOpeningField, "");
    AppSetFieldText(form, PgmAliasField, gPgm.aliases);
    ShowRedeemRows(form, gPgm.redeemKind);
    ShowExpiryRows(form, gPgm.expiryRule);
    if (isNew) {
        AppSetLabel(form, PgmBalanceLabel, "");
        FrmHideObject(form, FrmGetObjectIndex(form, PgmArchiveButton));
    } else {
        char msg[48], val[20];
        Int32 bal = PgmBalance(gPgm.id, gPgm.openingBalance);
        uses = PgmUseCount(gPgm.id);
        PgmValueText(val, &gPgm, bal);
        StrCopy(msg, "Balance now "); StrCat(msg, val);
        if (!money && gPgm.valuePerPoint && bal > 0) {
            StrCat(msg, ", about $");
            TextFixed(msg + StrLen(msg), (Int32)CalcMulDiv((UInt32)bal, gPgm.valuePerPoint, 1000), 2, gPoint, gThousands);
        }
        AppSetLabel(form, PgmBalanceLabel, msg);
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, PgmArchiveButton)),
                    gPgm.archived ? "Restore" : (uses ? "Archive" : "Delete"));
        FrmShowObject(form, FrmGetObjectIndex(form, PgmArchiveButton));
    }
    FrmSetFocus(form, FrmGetObjectIndex(form, PgmNameField));
}

static Boolean SaveProgramme(FormType *form)
{
    const char *name = AppGetFieldText(form, PgmNameField);
    Boolean money = AppGroupSelection(form, 1) == PgmMoneyButton;
    UInt32 v, n;
    Int32 r, s;
    Err err;

    if (!name[0]) { FrmAlert(NeedPgmNameAlert); return false; }
    StrNCopy(gPgm.name, name, pgmNameLen - 1); gPgm.name[pgmNameLen - 1] = 0;
    gPgm.currency = (UInt8)GetPopup(form, PgmCurrencyList);
    gPgm.earnBasis = (UInt8)GetPopup(form, PgmBasisList);
    r = TextParseFixed(AppGetFieldText(form, PgmEarnField), 2, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.earnRate = r == 1 ? v : 0;
    r = TextParseFixed(AppGetFieldText(form, PgmPremiumField), 0, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.premiumMult = (UInt16)(r == 1 && v ? v : 100);
    gPgm.requiresPayment = (UInt8)GetPopup(form, PgmPayList);
    gPgm.autoApply = (UInt8)CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, PgmAutoCheckbox)));
    gPgm.redeemKind = (UInt8)GetPopup(form, PgmRedeemList);
    r = TextParseFixed(AppGetFieldText(form, PgmBlockField), 0, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.redeemBlock = r == 1 ? v : 0;
    r = TextParseFixed(AppGetFieldText(form, PgmValueField), gPgm.redeemKind == pgmRedeemPerVolume ? 1 : 2, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.redeemValue = r == 1 ? (gPgm.redeemKind == pgmRedeemPerVolume ? v : v * 10) : 0;
    gPgm.redeemVolUnit = GetPopup(form, PgmVolUnitList) == 1 ? pgmVolUSGallon : pgmVolLitre;
    r = TextParseFixed(AppGetFieldText(form, PgmMaxVolField), 0, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.redeemMaxVol = r == 1 ? (gPgm.redeemVolUnit == pgmVolUSGallon ? CalcMulDiv(v, 3785411784UL, 1000000UL) : v * 1000) : 0;
    r = TextParseFixed(AppGetFieldText(form, PgmMaxPerUnitField), 1, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.redeemMaxPerUnit = r == 1 ? v : 0;
    gPgm.expiryRule = (UInt8)GetPopup(form, PgmExpiryList);
    r = TextParseFixed(AppGetFieldText(form, PgmMonthsField), 0, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.expiryMonths = (UInt8)(r == 1 ? v : 0);
    r = ParseSigned(AppGetFieldText(form, PgmOpeningField), money ? 2 : 0, &s);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gPgm.openingBalance = r == 1 ? s : 0;
    StrNCopy(gPgm.aliases, AppGetFieldText(form, PgmAliasField), pgmAliasLen - 1); gPgm.aliases[pgmAliasLen - 1] = 0;
    /* What a point is worth: a cent for money, the money-off rate for points, else unknown. */
    if (money) gPgm.valuePerPoint = 1000;
    else if (gPgm.redeemKind == pgmRedeemMoneyOff && gPgm.redeemBlock) gPgm.valuePerPoint = CalcMulDiv(gPgm.redeemValue, 100, gPgm.redeemBlock);
    else if (gPgm.valuePerPoint == 1000) gPgm.valuePerPoint = 0;
    n = ProgrammePack(&gPgm, gPacked);
    if (gEditProgrammeIndex == dmMaxRecordIndex) err = DbAdd(dbProgrammes, gPacked, n, NULL);
    else err = DbReplace(dbProgrammes, gEditProgrammeIndex, gPacked, n);
    if (err) { FrmAlert(SaveFailedAlert); return false; }
    return true;
}

Boolean EditProgrammeFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 bottom[] = { PgmDoneButton, PgmCancelButton, PgmArchiveButton };
    static UInt16 pinned[] = { PgmDoneButton, PgmCancelButton, PgmArchiveButton, PgmScrollBar };
    switch (event->eType) {
    case frmOpenEvent:
        AppScrollReset();
        AppFitForm(form, bottom, 3);
        LoadProgramme(form);
        ClampRows(form);
        AppScrollBarUpdate(form, PgmScrollBar, pinned, 4);
        FrmDrawForm(form);
        return true;
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case fldEnterEvent: AppScrollToField(form, event->data.fldEnter.fieldID, pinned, 4); ClampRows(form); AppScrollBarUpdate(form, PgmScrollBar, pinned, 4); break;
    case keyDownEvent: case sclRepeatEvent: case sclExitEvent:
        if (AppScrollEvent(form, event, pinned, 4, PgmScrollBar)) { ClampRows(form); FrmDrawForm(form); return true; }
        break;
    case winDisplayChangedEvent:
        if (gScrollYPublic()) AppScrollBy(form, gScrollYPublic(), pinned, 4);
        AppFitForm(form, bottom, 3); ClampRows(form); AppScrollBarUpdate(form, PgmScrollBar, pinned, 4); WinEraseWindow(); FrmDrawForm(form); return true;
    case popSelectEvent:
        if (event->data.popSelect.listID == PgmRedeemList) { ShowRedeemRows(form, (UInt8)event->data.popSelect.selection); ClampRows(form); }
        if (event->data.popSelect.listID == PgmExpiryList) { ShowExpiryRows(form, (UInt8)event->data.popSelect.selection); ClampRows(form); }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case PgmPointsButton: SetMoneyLabels(form, false); return true;
        case PgmMoneyButton: SetMoneyLabels(form, true); return true;
        case PgmDoneButton: if (SaveProgramme(form)) FrmGotoForm(ProgrammesForm); return true;
        case PgmCancelButton: FrmGotoForm(ProgrammesForm); return true;
        case PgmArchiveButton:
            if (gEditProgrammeIndex == dmMaxRecordIndex) return true;
            if (gPgm.archived) { gPgm.archived = 0; }
            else if (PgmUseCount(gPgm.id)) { gPgm.archived = 1; }
            else {
                if (FrmAlert(DeleteProgrammeAlert) != 0) return true;
                DbRemove(dbProgrammes, gEditProgrammeIndex);
                FrmGotoForm(ProgrammesForm);
                return true;
            }
            {
                UInt32 n = ProgrammePack(&gPgm, gPacked);
                if (DbReplace(dbProgrammes, gEditProgrammeIndex, gPacked, n)) FrmAlert(SaveFailedAlert);
            }
            FrmGotoForm(ProgrammesForm);
            return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

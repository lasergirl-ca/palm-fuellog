/* Welcome: first launch never shows an empty table. Step 1 makes the first
 * vehicle, step 2 is the Preferences form under a setup title, step 3 points
 * at the Service screen (M3). Skip is always available. The Rates form edits
 * the Prefs table of last-used exchange rates. */
#include "setup.h"
#include "app.h"
#include "db.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "vehicles.h"
#include "resources.h"

UInt8 gSetupStep;

/* Section placement: see sections.h. */
static void ShowObj(FormType *form, UInt16 id, Boolean show) FORMS_SEC;
static void SetPopup(FormType *form, UInt16 triggerId, UInt16 listId, Int16 sel) FORMS_SEC;
static void DrawDots(void) FORMS_SEC;
static void LayoutWelcome(FormType *form) FORMS_SEC;
static void LoadWelcome(FormType *form) FORMS_SEC;
static Boolean SaveFirstVehicle(FormType *form) FORMS_SEC;
static void LoadRates(FormType *form) FORMS_SEC;
static Boolean SaveRates(FormType *form) FORMS_SEC;

static void ShowObj(FormType *form, UInt16 id, Boolean show)
{
    UInt16 idx = FrmGetObjectIndex(form, id);
    if (show) FrmShowObject(form, idx); else FrmHideObject(form, idx);
}

static void SetPopup(FormType *form, UInt16 triggerId, UInt16 listId, Int16 sel)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, listId));
    LstSetSelection(lst, sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, triggerId)), LstGetSelectionText(lst, sel));
}

/* ---- Welcome ---- */

static void DrawDots(void)
{
    RectangleType r;
    UInt8 i;
    for (i = 0; i < 3; i++) {
        r.topLeft.x = (Coord)(4 + i * 10); r.topLeft.y = 42; r.extent.x = 6; r.extent.y = 6;
        if (i + 1 == gSetupStep || (gSetupStep == 3 && i == 2)) WinDrawRectangle(&r, 3);
        else WinDrawRectangleFrame(roundFrame, &r);
    }
}

static void LayoutWelcome(FormType *form)
{
    static UInt16 bottom[] = { WelcomeNextButton, WelcomeSkipButton };
    static UInt16 step1[] = { WelcomeNameLabel, WelcomeNameField, WelcomeOdoLabel, WelcomeOdoField, WelcomeUnitLabel,
                                    WelcomeKmButton, WelcomeMiButton, WelcomePowerLabel, WelcomePowerTrigger, WelcomeTripLabel, WelcomeTripTrigger };
    Boolean first = gSetupStep != 3;
    UInt8 i;
    AppFitForm(form, bottom, 2);
    for (i = 0; i < 11; i++) ShowObj(form, step1[i], first);
    ShowObj(form, WelcomeSkipButton, first);
    if (first) {
        AppSetLabel(form, WelcomeStepLabel, "Step 1 of 3: your first vehicle");
        AppSetLabel(form, WelcomeNote1, "Next: units and currency, then");
        AppSetLabel(form, WelcomeNote2, "your last service.");
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, WelcomeNextButton)), "Next");
    } else {
        AppSetLabel(form, WelcomeStepLabel, "Step 3 of 3: your last service");
        AppSetLabel(form, WelcomeNote1, "Service reminders come with the");
        AppSetLabel(form, WelcomeNote2, "Service screen. Nothing to do now.");
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, WelcomeNextButton)), "Finish");
    }
}

static void LoadWelcome(FormType *form)
{
    if (gSetupStep == 0) gSetupStep = 1;
    LayoutWelcome(form);
    if (gSetupStep == 1) {
        AppSetFieldText(form, WelcomeNameField, "");
        AppSetFieldText(form, WelcomeOdoField, "");
        FrmSetControlGroupSelection(form, 1, gPrefs.distanceUnit == unitDistMiles ? WelcomeMiButton : WelcomeKmButton);
        SetPopup(form, WelcomePowerTrigger, WelcomePowerList, 0);
        SetPopup(form, WelcomeTripTrigger, WelcomeTripList, 0);
        FrmSetFocus(form, FrmGetObjectIndex(form, WelcomeNameField));
    }
}

/* Step 1's Next: make the vehicle and move on to units and money. */
static Boolean SaveFirstVehicle(FormType *form)
{
    VehicleRecord v;
    UInt8 buf[vehRecordSize];
    UInt32 odo;
    Int32 r;
    const char *name = AppGetFieldText(form, WelcomeNameField);
    if (!name[0]) { FrmAlert(NeedNameAlert); return false; }
    MemSet(&v, sizeof v, 0);
    v.id = VehNextId();
    if (v.id == 0) { FrmAlert(TooManyVehiclesAlert); return false; }
    StrNCopy(v.name, name, vehNameLen - 1); v.name[vehNameLen - 1] = 0;
    v.powertrain = (UInt8)LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, WelcomePowerList)));
    v.odoUnit = FrmGetControlGroupSelection(form, 1) == WelcomeMiButton ? vehOdoMiles : vehOdoKm;
    r = TextParseFixed(AppGetFieldText(form, WelcomeOdoField), 1, gPoint, gThousands, &odo);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    v.startOdo = r == 1 ? UnitDistFromDisplay(odo, v.odoUnit) : 0;
    v.tripMode = (UInt8)LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, WelcomeTripList)));
    v.defaultCurrency = vehCurrencyUsePrefs;
    v.tankSize = 60000;
    if (v.powertrain == vehPowerElectric) v.batteryWh = 60000;
    VehiclePack(&v, buf);
    if (DbAdd(dbVehicles, buf, vehRecordSize, NULL)) { FrmAlert(SaveFailedAlert); return false; }
    gPrefs.currentVehicle = v.id;
    /* The odometer unit chosen here is the most likely display unit too. */
    gPrefs.distanceUnit = v.odoUnit == vehOdoMiles ? unitDistMiles : unitDistKm;
    PrefsSave(&gPrefs);
    return true;
}

Boolean WelcomeFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        LoadWelcome(form);
        FrmDrawForm(form);
        DrawDots();
        return true;
    case frmUpdateEvent: FrmDrawForm(form); DrawDots(); return true;
    case winDisplayChangedEvent: LayoutWelcome(form); WinEraseWindow(); FrmDrawForm(form); DrawDots(); return true;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case WelcomeNextButton:
            if (gSetupStep == 3) { gSetupStep = 0; FrmGotoForm(FillListForm); return true; }
            if (SaveFirstVehicle(form)) { gSetupStep = 2; FrmGotoForm(PrefsForm); }
            return true;
        case WelcomeSkipButton:
            gSetupStep = 0; FrmGotoForm(FillListForm); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- Exchange rates ---- */

static char gRateLabels[curCount][16];   /* FrmCopyLabel targets need no persistence, but the text is built here */

static void LoadRates(FormType *form)
{
    UInt8 c;
    char b[16], note[40];
    StrCopy(note, "Home units (");
    StrCat(note, CurrencyCode(gPrefs.homeCurrency));
    StrCat(note, ") per one unit of:");
    AppSetLabel(form, RatesNoteLabel, note);
    for (c = 0; c < curCount; c++) {
        Boolean show = c != gPrefs.homeCurrency;
        ShowObj(form, RatesLabel0 + c, show);
        ShowObj(form, RatesField0 + c, show);
        if (!show) continue;
        StrCopy(gRateLabels[c], "1 "); StrCat(gRateLabels[c], CurrencyCode(c)); StrCat(gRateLabels[c], " =");
        AppSetLabel(form, RatesLabel0 + c, gRateLabels[c]);
        TextFixed(b, (Int32)((gPrefs.fxRate[c] ? gPrefs.fxRate[c] : fxOne) / 100), 4, gPoint, 0);
        AppSetFieldText(form, RatesField0 + c, b);
    }
}

static Boolean SaveRates(FormType *form)
{
    UInt8 c;
    UInt32 v;
    for (c = 0; c < curCount; c++) {
        Int32 r;
        if (c == gPrefs.homeCurrency) continue;
        r = TextParseFixed(AppGetFieldText(form, RatesField0 + c), 4, gPoint, gThousands, &v);
        if (r < 0) { FrmAlert(BadNumberAlert); return false; }
        if (r == 1 && v) gPrefs.fxRate[c] = v * 100;
    }
    PrefsSave(&gPrefs);
    return true;
}

Boolean RatesFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 bottom[] = { RatesDoneButton, RatesCancelButton };
    switch (event->eType) {
    case frmOpenEvent:
        AppFitForm(form, bottom, 2);
        LoadRates(form);
        FrmDrawForm(form);
        return true;
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case winDisplayChangedEvent: AppFitForm(form, bottom, 2); WinEraseWindow(); FrmDrawForm(form); return true;
    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == RatesDoneButton) { if (SaveRates(form)) FrmGotoForm(PrefsForm); return true; }
        if (event->data.ctlSelect.controlID == RatesCancelButton) { FrmGotoForm(PrefsForm); return true; }
        break;
    default: break;
    }
    return false;
}

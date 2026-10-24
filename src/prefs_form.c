#include "prefs_form.h"
#include "app.h"
#include "units.h"
#include "currency.h"
#include "resources.h"
#include "setup.h"

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

static void LoadPrefs(FormType *form)
{
    SetPopup(form, PrefsDistTrigger, PrefsDistList, gPrefs.distanceUnit);
    SetPopup(form, PrefsVolTrigger, PrefsVolList, gPrefs.volumeUnit);
    SetPopup(form, PrefsEcoTrigger, PrefsEcoList, gPrefs.economyUnit < 4 ? gPrefs.economyUnit : 0);
    SetPopup(form, PrefsPriceTrigger, PrefsPriceList, gPrefs.priceUnit < 3 ? gPrefs.priceUnit : 0);
    FrmSetControlGroupSelection(form, 1, gPrefs.priceFormat == unitPriceFormatCents ? PrefsCentsButton : PrefsDollarsButton);
    SetPopup(form, PrefsHomeTrigger, PrefsHomeList, gPrefs.homeCurrency);
    SetPopup(form, PrefsNewTrigger, PrefsNewList, gPrefs.defaultCurrency == curLastUsed ? 0 : gPrefs.defaultCurrency + 1);
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, PrefsBackupCheckbox)), gPrefs.autoBackup);
    SetPopup(form, PrefsKeepTrigger, PrefsKeepList, gPrefs.backupSetsKept <= 3 ? 0 : gPrefs.backupSetsKept <= 5 ? 1 : 2);
}

static void SavePrefs(FormType *form)
{
    static const UInt8 keep[] = { 3, 5, 10 };
    Int16 sel;
    gPrefs.distanceUnit = (UInt8)GetPopup(form, PrefsDistList);
    gPrefs.volumeUnit = (UInt8)GetPopup(form, PrefsVolList);
    gPrefs.economyUnit = (UInt8)GetPopup(form, PrefsEcoList);
    gPrefs.priceUnit = (UInt8)GetPopup(form, PrefsPriceList);
    gPrefs.priceFormat = AppGroupSelection(form, 1) == PrefsCentsButton ? unitPriceFormatCents : unitPriceFormatDollars;
    gPrefs.homeCurrency = (UInt8)GetPopup(form, PrefsHomeList);
    sel = GetPopup(form, PrefsNewList);
    gPrefs.defaultCurrency = sel == 0 ? curLastUsed : (UInt8)(sel - 1);
    gPrefs.autoBackup = (UInt8)CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, PrefsBackupCheckbox)));
    gPrefs.backupSetsKept = keep[GetPopup(form, PrefsKeepList)];
    PrefsSave(&gPrefs);
}

Boolean PrefsFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static const UInt16 bottom[] = { PrefsDoneButton, PrefsCancelButton, PrefsRatesButton };
    static const UInt16 pinned[] = { PrefsDoneButton, PrefsCancelButton, PrefsRatesButton, PrefsScrollBar };
    switch (event->eType) {
    case frmOpenEvent:
        AppScrollReset();
        AppFitForm(form, bottom, 3);
        if (gSetupStep == 2) FrmSetTitle(form, "Setup 2 of 3: units");
        LoadPrefs(form);
        AppScrollBarUpdate(form, PrefsScrollBar, pinned, 4);
        FrmDrawForm(form);
        return true;
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case keyDownEvent: case sclRepeatEvent: case sclExitEvent:
        if (AppScrollEvent(form, event, pinned, 4, PrefsScrollBar)) return true;
        break;
    case winDisplayChangedEvent:
        if (gScrollYPublic()) AppScrollBy(form, gScrollYPublic(), pinned, 4);
        AppFitForm(form, bottom, 3); AppScrollBarUpdate(form, PrefsScrollBar, pinned, 4); WinEraseWindow(); FrmDrawForm(form); return true;
    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == PrefsDoneButton) {
            SavePrefs(form);
            if (gSetupStep == 2) { gSetupStep = 3; FrmGotoForm(WelcomeForm); }
            else FrmGotoForm(FillListForm);
            return true;
        }
        if (event->data.ctlSelect.controlID == PrefsCancelButton) { gSetupStep = 0; FrmGotoForm(FillListForm); return true; }
        if (event->data.ctlSelect.controlID == PrefsRatesButton) { SavePrefs(form); FrmGotoForm(RatesForm); return true; }
        break;
    default: break;
    }
    return false;
}

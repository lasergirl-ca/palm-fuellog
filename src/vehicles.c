#include "vehicles.h"
#include "app.h"
#include "db.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "calc.h"
#include "resources.h"

UInt16 gEditVehicleIndex = dmMaxRecordIndex;
static VehicleRecord gVeh;          /* the one being edited */
static VehicleRecord gScanVeh;
static FillRecord gScanFillV;
static Boolean gShowArchived;
static RectangleType gVehRows;
#define kVehRowH 22
#define kVehTop 18

Int16 VehFindById(UInt8 id)
{
    UInt16 n = DbCount(dbVehicles), i;
    for (i = 0; i < n; i++)
        if (DbGetVehicle(i, &gScanVeh) == errNone && gScanVeh.id == id) return (Int16)i;
    return -1;
}

Boolean VehGet(UInt8 id, VehicleRecord *v)
{
    Int16 i = VehFindById(id);
    if (i >= 0 && DbGetVehicle((UInt16)i, v) == errNone) return true;
    MemSet(v, sizeof *v, 0);
    v->id = id; StrCopy(v->name, "Vehicle"); v->defaultCurrency = vehCurrencyUsePrefs;
    return false;
}

UInt8 VehNextId(void)
{
    UInt16 used = 0, n = DbCount(dbVehicles), i;
    UInt8 id;
    for (i = 0; i < n; i++)
        if (DbGetVehicle(i, &gScanVeh) == errNone && gScanVeh.id >= 1 && gScanVeh.id <= 16) used |= (UInt16)(1 << (gScanVeh.id - 1));
    for (id = 1; id <= vehMaxVehicles; id++) if (!(used & (1 << (id - 1)))) return id;
    return 0;
}

UInt16 VehFillCount(UInt8 id)
{
    UInt16 n = DbCount(dbFills), i, count = 0;
    for (i = 0; i < n; i++) if (DbGetFill(i, &gScanFillV) == errNone && gScanFillV.vehicle == id) count++;
    return count;
}

UInt32 VehLastOdometer(UInt8 id)
{
    UInt16 n = DbCount(dbFills);
    while (n--) if (DbGetFill(n, &gScanFillV) == errNone && gScanFillV.vehicle == id) return gScanFillV.odometer;
    /* No fills yet: the reading tracking began at is the previous reading. */
    return VehFindById(id) >= 0 ? gScanVeh.startOdo : 0;
}

UInt8 VehDefaultForNewFill(void)
{
    UInt16 n = DbCount(dbVehicles), i;
    if (gPrefs.currentVehicle != 255) return gPrefs.currentVehicle;
    for (i = 0; i < n; i++) if (DbGetVehicle(i, &gScanVeh) == errNone && !gScanVeh.archived) return gScanVeh.id;
    return 1;
}

UInt16 VehBuildChoices(ListType *lst, char names[][vehNameLen], char *ptrs[], UInt8 ids[],
                       Boolean includeAll, UInt8 alwaysId, UInt8 selectedId, Int16 *selOut)
{
    UInt16 n = DbCount(dbVehicles), i, count = 0;
    Int16 sel = 0;
    for (i = 0; i < n && count < vehMaxVehicles; i++) {
        if (DbGetVehicle(i, &gScanVeh) != errNone) continue;
        if (gScanVeh.archived && gScanVeh.id != alwaysId) continue;
        StrCopy(names[count], gScanVeh.name);
        ids[count] = gScanVeh.id;
        if (gScanVeh.id == selectedId) sel = (Int16)count;
        count++;
    }
    if (includeAll) {
        StrCopy(names[count], "All vehicles");
        ids[count] = 255;
        if (selectedId == 255) sel = (Int16)count;
        count++;
    }
    if (count == 0) { StrCopy(names[0], "No vehicle"); ids[0] = selectedId; count = 1; }
    for (i = 0; i < count; i++) ptrs[i] = names[i];
    LstSetListChoices(lst, ptrs, (Int16)count);
    LstSetHeight(lst, (Int16)(count < 6 ? count : 6));
    LstSetSelection(lst, sel);
    *selOut = sel;
    return count;
}

/* ---- Vehicles list ---- */

static UInt16 VisibleAt(UInt16 row, UInt16 *dbIndex)
{
    UInt16 n = DbCount(dbVehicles), i, seen = 0;
    for (i = 0; i < n; i++) {
        if (DbGetVehicle(i, &gScanVeh) != errNone) continue;
        if (gScanVeh.archived && !gShowArchived) continue;
        if (seen == row) { *dbIndex = i; return 1; }
        seen++;
    }
    return 0;
}

static void DrawVehicles(void)
{
    UInt16 row = 0, idx;
    Coord y = kVehTop;
    char buf[40], num[16];

    WinEraseRectangle(&gVehRows, 0);
    if (!VisibleAt(0, &idx)) {
        FntSetFont(stdFont);
        WinDrawChars("No vehicles yet. Tap New to add one.", 36, 6, y + 20);
        return;
    }
    while (VisibleAt(row, &idx) && y + kVehRowH <= gVehRows.topLeft.y + gVehRows.extent.y) {
        UInt32 odo;
        DbGetVehicle(idx, &gScanVeh);
        FntSetFont(gScanVeh.archived ? stdFont : boldFont);
        WinDrawChars(gScanVeh.name, StrLen(gScanVeh.name), 4, y);
        FntSetFont(stdFont);
        odo = VehLastOdometer(gScanVeh.id);
        DbGetVehicle(idx, &gScanVeh);        /* the lookup above reuses the scan record */
        if (odo) {
            TextFixed(num, (Int32)(UnitDistToDisplay(odo, gPrefs.distanceUnit) / 10), 0, gPoint, gThousands);
            StrCopy(buf, num); StrCat(buf, " "); StrCat(buf, UnitDistLabel(gPrefs.distanceUnit));
            WinDrawChars(buf, StrLen(buf), 156 - FntCharsWidth(buf, StrLen(buf)), y);
        }
        {
            static const char *const power[] = { "Fuel", "Hybrid", "Plug-in hybrid", "Electric" };
            StrCopy(buf, power[gScanVeh.powertrain < 4 ? gScanVeh.powertrain : 0]);
            StrCat(buf, gScanVeh.archived ? "; archived" : "");
            StrCat(buf, "; "); StrIToA(num, VehFillCount(gScanVeh.id)); StrCat(buf, num); StrCat(buf, " fills");
            WinDrawChars(buf, StrLen(buf), 10, y + 10);
        }
        y += kVehRowH; row++;
    }
}

static void LayoutVehicles(FormType *form)
{
    static const UInt16 bottom[] = { VehiclesNewButton, VehiclesDoneButton, VehiclesArchivedCheckbox };
    Coord w, h;
    RectangleType r;
    UInt16 idx;
    AppFitForm(form, bottom, 2);
    WinGetDisplayExtent(&w, &h);
    idx = FrmGetObjectIndex(form, VehiclesArchivedCheckbox);
    FrmGetObjectBounds(form, idx, &r); r.topLeft.y = h - 29; FrmSetObjectBounds(form, idx, &r);
    gVehRows.topLeft.x = 0; gVehRows.topLeft.y = kVehTop; gVehRows.extent.x = w; gVehRows.extent.y = h - 31 - kVehTop;
}

Boolean VehiclesFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        LayoutVehicles(form);
        CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, VehiclesArchivedCheckbox)), gShowArchived);
        FrmDrawForm(form);
        DrawVehicles();
        return true;
    case frmUpdateEvent:
        FrmDrawForm(form); DrawVehicles(); return true;
    case winDisplayChangedEvent:
        LayoutVehicles(form); WinEraseWindow(); FrmDrawForm(form); DrawVehicles(); return true;
    case penDownEvent:
        if (RctPtInRectangle(event->screenX, event->screenY, &gVehRows)) {
            UInt16 row = (UInt16)((event->screenY - gVehRows.topLeft.y) / kVehRowH), idx;
            if (VisibleAt(row, &idx)) { gEditVehicleIndex = idx; FrmGotoForm(EditVehicleForm); return true; }
        }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case VehiclesNewButton:
            if (VehNextId() == 0) { FrmAlert(TooManyVehiclesAlert); return true; }
            gEditVehicleIndex = dmMaxRecordIndex; FrmGotoForm(EditVehicleForm); return true;
        case VehiclesDoneButton: FrmGotoForm(FillListForm); return true;
        case VehiclesArchivedCheckbox: gShowArchived = CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, VehiclesArchivedCheckbox))); DrawVehicles(); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- Edit vehicle ---- */

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

static void ShowElectricFields(FormType *form, UInt8 powertrain)
{
    Boolean electric = powertrain == vehPowerElectric || powertrain == vehPowerPlugIn;
    AppSetLabel(form, VehTankLabel, powertrain == vehPowerElectric ? "Battery kWh:" : "Tank (L):");
    if (powertrain == vehPowerElectric) {
        FrmHideObject(form, FrmGetObjectIndex(form, VehGradeLabel));
        FrmHideObject(form, FrmGetObjectIndex(form, VehGradeTrigger));
    } else {
        FrmShowObject(form, FrmGetObjectIndex(form, VehGradeLabel));
        FrmShowObject(form, FrmGetObjectIndex(form, VehGradeTrigger));
    }
    if (electric) {
        FrmShowObject(form, FrmGetObjectIndex(form, VehHomeRateLabel));
        FrmShowObject(form, FrmGetObjectIndex(form, VehHomeRateField));
    } else {
        FrmHideObject(form, FrmGetObjectIndex(form, VehHomeRateLabel));
        FrmHideObject(form, FrmGetObjectIndex(form, VehHomeRateField));
    }
}

static void LoadVehicle(FormType *form)
{
    char b[24];
    UInt16 fills;
    if (gEditVehicleIndex == dmMaxRecordIndex || DbGetVehicle(gEditVehicleIndex, &gVeh) != errNone) {
        gEditVehicleIndex = dmMaxRecordIndex;
        MemSet(&gVeh, sizeof gVeh, 0);
        gVeh.id = VehNextId();
        gVeh.defaultCurrency = vehCurrencyUsePrefs;
        gVeh.tripMode = vehTripReset;
        gVeh.tankSize = 60000;
    }
    FrmSetTitle(form, gEditVehicleIndex == dmMaxRecordIndex ? "New Vehicle" : "Edit Vehicle");
    AppSetFieldText(form, VehNameField, gVeh.name);
    SetPopup(form, VehPowerTrigger, VehPowerList, gVeh.powertrain < 4 ? gVeh.powertrain : 0);
    FrmSetControlGroupSelection(form, 1, gVeh.odoUnit == vehOdoMiles ? VehMiButton : VehKmButton);
    if (gVeh.startOdo) { TextFixed(b, (Int32)(UnitDistToDisplay(gVeh.startOdo, gVeh.odoUnit) / 10), 0, gPoint, 0); AppSetFieldText(form, VehStartField, b); }
    else AppSetFieldText(form, VehStartField, "");
    if (gVeh.powertrain == vehPowerElectric) { TextFixed(b, (Int32)(gVeh.batteryWh / 100), 1, gPoint, 0); }
    else { TextFixed(b, (Int32)(gVeh.tankSize / 1000), 0, gPoint, 0); }
    AppSetFieldText(form, VehTankField, b);
    TextPrice(b, gVeh.homeRate, 1, gPoint);
    AppSetFieldText(form, VehHomeRateField, gVeh.homeRate ? b : "");
    SetPopup(form, VehGradeTrigger, VehGradeList, gVeh.fuelDefault < 4 ? gVeh.fuelDefault : 0);
    SetPopup(form, VehCurrencyTrigger, VehCurrencyList, gVeh.defaultCurrency == vehCurrencyUsePrefs ? 0 : gVeh.defaultCurrency + 1);
    SetPopup(form, VehTripTrigger, VehTripList, gVeh.tripMode < 3 ? gVeh.tripMode : 0);
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, VehArchivedCheckbox)), gVeh.archived);
    ShowElectricFields(form, gVeh.powertrain);
    fills = gEditVehicleIndex == dmMaxRecordIndex ? 0 : VehFillCount(gVeh.id);
    if (gEditVehicleIndex == dmMaxRecordIndex) {
        FrmHideObject(form, FrmGetObjectIndex(form, VehDeleteButton));
        AppSetLabel(form, VehStatusLabel, "");
    } else if (fills) {
        /* Delete gives way to the reason it is unavailable. */
        char msg[24];
        StrIToA(b, fills);
        StrCopy(msg, b); StrCat(msg, fills == 1 ? " fill: archive" : " fills: archive");
        FrmHideObject(form, FrmGetObjectIndex(form, VehDeleteButton));
        AppSetLabel(form, VehStatusLabel, msg);
    } else {
        FrmShowObject(form, FrmGetObjectIndex(form, VehDeleteButton));
        AppSetLabel(form, VehStatusLabel, "");
    }
    FrmSetFocus(form, FrmGetObjectIndex(form, VehNameField));
}

static Boolean SaveVehicle(FormType *form)
{
    UInt32 v;
    Int32 r;
    const char *name = AppGetFieldText(form, VehNameField);
    UInt8 buf[vehRecordSize];
    Int16 sel;
    Err err;

    if (!name[0]) { FrmAlert(NeedNameAlert); return false; }
    StrNCopy(gVeh.name, name, vehNameLen - 1); gVeh.name[vehNameLen - 1] = 0;
    gVeh.powertrain = (UInt8)GetPopup(form, VehPowerList);
    gVeh.odoUnit = FrmGetControlGroupSelection(form, 1) == VehMiButton ? vehOdoMiles : vehOdoKm;
    r = TextParseFixed(AppGetFieldText(form, VehStartField), 1, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gVeh.startOdo = r == 1 ? UnitDistFromDisplay(v, gVeh.odoUnit) : 0;
    if (gVeh.powertrain == vehPowerElectric) {
        r = TextParseFixed(AppGetFieldText(form, VehTankField), 1, gPoint, gThousands, &v);
        if (r < 0) { FrmAlert(BadNumberAlert); return false; }
        gVeh.batteryWh = r == 1 ? v * 100 : 0;
    } else {
        r = TextParseFixed(AppGetFieldText(form, VehTankField), 0, gPoint, gThousands, &v);
        if (r < 0) { FrmAlert(BadNumberAlert); return false; }
        gVeh.tankSize = r == 1 ? v * 1000 : 0;
    }
    r = TextParsePrice(AppGetFieldText(form, VehHomeRateField), 1, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gVeh.homeRate = r == 1 ? v : 0;
    gVeh.fuelDefault = (UInt8)GetPopup(form, VehGradeList);
    sel = GetPopup(form, VehCurrencyList);
    gVeh.defaultCurrency = sel == 0 ? vehCurrencyUsePrefs : (UInt8)(sel - 1);
    gVeh.tripMode = (UInt8)GetPopup(form, VehTripList);
    gVeh.archived = (UInt8)CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, VehArchivedCheckbox)));
    VehiclePack(&gVeh, buf);
    if (gEditVehicleIndex == dmMaxRecordIndex) err = DbAdd(dbVehicles, buf, vehRecordSize, NULL);
    else err = DbReplace(dbVehicles, gEditVehicleIndex, buf, vehRecordSize);
    if (err) { FrmAlert(SaveFailedAlert); return false; }
    if (gPrefs.currentVehicle == 255 && DbCount(dbVehicles) == 1) { gPrefs.currentVehicle = gVeh.id; PrefsSave(&gPrefs); }
    return true;
}

Boolean EditVehicleFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static const UInt16 bottom[] = { VehDoneButton, VehCancelButton, VehDeleteButton, VehStatusLabel };
    switch (event->eType) {
    case frmOpenEvent:
        AppScrollReset();
        AppFitForm(form, bottom, 4);
        LoadVehicle(form);
        FrmDrawForm(form);
        return true;
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case fldEnterEvent: AppScrollToField(form, event->data.fldEnter.fieldID, bottom, 4); break;
    case keyDownEvent: if (AppScrollKey(form, event, bottom, 4)) return true; break;
    case winDisplayChangedEvent:
        if (gScrollYPublic()) AppScrollBy(form, gScrollYPublic(), bottom, 4);
        AppFitForm(form, bottom, 4); WinEraseWindow(); FrmDrawForm(form); return true;
    case popSelectEvent:
        if (event->data.popSelect.listID == VehPowerList) {
            ShowElectricFields(form, (UInt8)event->data.popSelect.selection);
        }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case VehDoneButton: if (SaveVehicle(form)) FrmGotoForm(VehiclesForm); return true;
        case VehCancelButton: FrmGotoForm(VehiclesForm); return true;
        case VehDeleteButton:
            if (gEditVehicleIndex != dmMaxRecordIndex && VehFillCount(gVeh.id) == 0 && FrmAlert(DeleteVehicleAlert) == 0) {
                DbRemove(dbVehicles, gEditVehicleIndex);
                if (gPrefs.currentVehicle == gVeh.id) { gPrefs.currentVehicle = 255; PrefsSave(&gPrefs); }
                FrmGotoForm(VehiclesForm);
            }
            return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

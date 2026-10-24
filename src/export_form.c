/* Export, backup and import on one form: the file name is shown before
 * you tap, and nothing leaves or lands without a message saying so. */
#include "export_form.h"
#include "export.h"
#include "backup.h"
#include "card.h"
#include "app.h"
#include "db.h"
#include "datecalc.h"
#include "vehicles.h"
#include "resources.h"

static char gEVNames[vehMaxVehicles + 1][vehNameLen];
static char *gEVPtrs[vehMaxVehicles + 1];
static UInt8 gEVIds[vehMaxVehicles + 1];
static UInt8 gExpVehicle = 255;
static char gYearNames[13][10];
static char *gYearPtrs[13];
static UInt16 gYears[13];
static UInt16 gYearCount, gExpYear;
static FillRecord gExpScan;
static char gSetNames[16][cardNameLen];
static char *gSetPtrs[16];
static UInt16 gSetCount;

/* Section placement: see sections.h. */
static void FileLabel(FormType *form) IO_SEC;
static void BackupLabel(FormType *form) IO_SEC;
static void BuildYears(FormType *form) IO_SEC;
static void DoExport(FormType *form) IO_SEC;
static void DoBackup(FormType *form) IO_SEC;

static void FileLabel(FormType *form)
{
    char b[44], name[40];
    UInt8 fmt = AppGroupSelection(form, 1) == ExpXlsxButton ? 1 : AppGroupSelection(form, 1) == ExpPdfButton ? 2 : 0;
    CardExportName(name, fmt == 1 ? ".xlsx" : fmt == 2 ? ".pdf" : ".csv");
    StrCopy(b, AppGroupSelection(form, 2) == ExpPalmButton ? "Sends " : "Saves Export/"); StrCat(b, name);
    AppSetLabel(form, ExpFileLabel, b);
}

static void BackupLabel(FormType *form)
{
    char b[44], n[8];
    UInt16 vol;
    if (gPrefs.lastBackupDate) { StrCopy(b, "Last backup "); DateShortText(b + StrLen(b), gPrefs.lastBackupDate); }
    else StrCopy(b, "No backup yet");
    if (CardFind(&vol) == errNone) {
        UInt16 sets = BackupList(vol, gSetNames, 16);
        StrCat(b, "; "); StrIToA(n, sets); StrCat(b, n); StrCat(b, sets == 1 ? " set on the card" : " sets on the card");
    } else StrCat(b, "; no card");
    AppSetLabel(form, ExpBackupLabel, b);
}

static void BuildYears(FormType *form)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, ExpRangeList));
    UInt16 n = DbCount(dbFills), y, ty; UInt8 m, d, i;
    UInt16 first = 0;
    DateDaysToYmd(AppToday(), &ty, &m, &d);
    if (n && DbGetFill(0, &gExpScan) == errNone) DateDaysToYmd(gExpScan.date, &first, &m, &d);
    if (first == 0 || first > ty) first = ty;
    if (ty - first > 11) first = (UInt16)(ty - 11);
    gYearCount = 0;
    StrCopy(gYearNames[0], "All years"); gYears[0] = 0; gYearCount++;
    for (y = ty; y >= first && gYearCount < 13; y--) { StrIToA(gYearNames[gYearCount], y); gYears[gYearCount] = y; gYearCount++; }
    for (i = 0; i < gYearCount; i++) gYearPtrs[i] = gYearNames[i];
    LstSetListChoices(lst, gYearPtrs, (Int16)gYearCount);
    LstSetHeight(lst, (Int16)(gYearCount < 6 ? gYearCount : 6));
    LstSetSelection(lst, 0);
    gExpYear = 0;
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, ExpRangeTrigger)), gYearNames[0]);
}

static void DoExport(FormType *form)
{
    UInt16 vol;
    char name[40], kb[12];
    UInt32 bytes = 0;
    Err err;
    if (AppGroupSelection(form, 2) == ExpPalmButton) { FrmAlert(NotYetAlert); return; }
    if (AppGroupSelection(form, 1) != ExpCsvButton) { FrmAlert(NotYetAlert); return; }
    if (CardFind(&vol) != errNone) { FrmAlert(NoCardAlert); return; }
    AppSetLabel(form, ExpFileLabel, "Writing...");
    err = ExportCsv(vol, gExpVehicle, gExpYear, name, &bytes);
    if (err) { StrIToA(kb, err); FrmCustomAlert(ExportFailedAlert, name, kb, ""); FileLabel(form); return; }
    StrIToA(kb, (bytes + 512) / 1024);
    FrmCustomAlert(ExportDoneAlert, name, kb, "");
    FileLabel(form);
}

static void DoBackup(FormType *form)
{
    UInt16 vol;
    char set[cardNameLen], n[8];
    Err err;
    if (CardFind(&vol) != errNone) { FrmAlert(NoCardAlert); return; }
    AppSetLabel(form, ExpBackupLabel, "Backing up...");
    err = BackupNow(vol, set);
    if (err) { StrIToA(n, err); FrmCustomAlert(ExportFailedAlert, "Backup", n, ""); }
    else FrmCustomAlert(BackupDoneAlert, set, "", "");
    BackupLabel(form);
}

Boolean ExportFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 bottom[] = { ExpDoneButton };
    static UInt16 pinned[] = { ExpDoneButton, ExpScrollBar };
    switch (event->eType) {
    case frmOpenEvent: {
        ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, ExpVehicleList));
        Int16 sel;
        AppScrollReset();
        AppFitForm(form, bottom, 1);
        gExpVehicle = gPrefs.currentVehicle;
        VehBuildChoices(lst, gEVNames, gEVPtrs, gEVIds, true, 255, gExpVehicle, &sel);
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, ExpVehicleTrigger)), gEVNames[sel]);
        FrmSetControlGroupSelection(form, 1, ExpCsvButton);
        FrmSetControlGroupSelection(form, 2, ExpCardButton);
        BuildYears(form);
        FileLabel(form);
        BackupLabel(form);
        AppScrollBarUpdate(form, ExpScrollBar, pinned, 2);
        FrmDrawForm(form);
        return true;
    }
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case keyDownEvent: case sclRepeatEvent: case sclExitEvent:
        if (AppScrollEvent(form, event, pinned, 2, ExpScrollBar)) return true;
        break;
    case winDisplayChangedEvent:
        if (gScrollYPublic()) AppScrollBy(form, gScrollYPublic(), pinned, 2);
        AppFitForm(form, bottom, 1); AppScrollBarUpdate(form, ExpScrollBar, pinned, 2); WinEraseWindow(); FrmDrawForm(form); return true;
    case popSelectEvent:
        if (event->data.popSelect.listID == ExpVehicleList) {
            gExpVehicle = gEVIds[event->data.popSelect.selection];
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, ExpVehicleTrigger)), gEVNames[event->data.popSelect.selection]);
            return true;
        }
        if (event->data.popSelect.listID == ExpRangeList) {
            gExpYear = gYears[event->data.popSelect.selection];
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, ExpRangeTrigger)), gYearNames[event->data.popSelect.selection]);
            return true;
        }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case ExpCsvButton: case ExpXlsxButton: case ExpPdfButton: case ExpCardButton: case ExpPalmButton: FileLabel(form); break;
        case ExpExportButton: DoExport(form); return true;
        case ExpBackupButton: DoBackup(form); return true;
        case ExpRestoreButton: FrmGotoForm(RestoreForm); return true;
        case ExpImportButton: FrmGotoForm(ChooseFileForm); return true;
        case ExpDoneButton: FrmGotoForm(FillListForm); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- Restore ---- */

Boolean RestoreFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 bottom[] = { RstRestoreButton, RstCancelButton };
    switch (event->eType) {
    case frmOpenEvent: {
        ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, RstList));
        UInt16 vol, i;
        AppFitForm(form, bottom, 2);
        gSetCount = CardFind(&vol) == errNone ? BackupList(vol, gSetNames, 16) : 0;
        if (gSetCount == 0) { StrCopy(gSetNames[0], "(no backup sets)"); gSetCount = 1; }
        for (i = 0; i < gSetCount; i++) gSetPtrs[i] = gSetNames[i];
        LstSetListChoices(lst, gSetPtrs, (Int16)gSetCount);
        LstSetSelection(lst, 0);
        FrmDrawForm(form);
        return true;
    }
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case winDisplayChangedEvent: AppFitForm(form, bottom, 2); WinEraseWindow(); FrmDrawForm(form); return true;
    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == RstCancelButton) { FrmGotoForm(ExportForm); return true; }
        if (event->data.ctlSelect.controlID == RstRestoreButton) {
            ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, RstList));
            Int16 sel = LstGetSelection(lst);
            UInt16 vol;
            Err err;
            if (sel < 0 || gSetNames[sel][0] == '(') return true;
            if (CardFind(&vol) != errNone) { FrmAlert(NoCardAlert); return true; }
            if (FrmCustomAlert(RestoreConfirmAlert, gSetNames[sel], "", "") != 0) return true;
            err = BackupRestore(vol, gSetNames[sel]);
            if (err == dmErrAlreadyExists) FrmAlert(NewerBackupAlert);
            else if (err) { char n[8]; StrIToA(n, err); FrmCustomAlert(ExportFailedAlert, "Restore", n, ""); }
            else FrmCustomAlert(RestoreDoneAlert, gSetNames[sel], "", "");
            FrmGotoForm(FillListForm);
            return true;
        }
        break;
    default: break;
    }
    return false;
}

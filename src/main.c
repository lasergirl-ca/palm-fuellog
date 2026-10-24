/* FuelLog for Palm OS: M0 sample form.
 * SP1: a label filled from a second code section.
 * SP2: a label computed with 64-bit arithmetic.
 * SP3: the form follows the input area (PIN) and draws a graph at double
 *      density, with a tap-to-read callout. */
#include <PalmOS.h>
#include <PenInputMgr.h>
#include "resources.h"
#include "hello2.h"
#include "calc.h"
#include "device.h"
#include "graph.h"
#include "vfstest.h"
#include "sp6test.h"
#include "m1test.h"
#include "app.h"
#include "db.h"
#include "fill_list.h"
#include "fill_form.h"
#include "vehicles.h"
#include "prefs_form.h"
#include "programmes.h"
#include "setup.h"
#include "stats_form.h"
#include "service_form.h"
#include "reports_form.h"
#include "export_form.h"
#include "import.h"
#include "backup.h"
#include "beam.h"
#include <ExgMgr.h>
#include <VFSMgr.h>

#define kMinRomVersion sysMakeROMVersion(3, 0, 0, sysROMStageRelease, 0)

static RectangleType gPlot;      /* graph area, standard coordinates */
static Int16 gSelected = -1;
static char gSp4Lines[vfsTestMaxLines][vfsTestLineLen];
static UInt16 gSp4Count;

/* Fit the form to the display and keep Done at the bottom. */
static void LayoutDiagForm(FormType *form)
{
    RectangleType bounds, r;
    Coord w, h;
    UInt16 idx;

    WinGetDisplayExtent(&w, &h);
    FrmGetFormBounds(form, &bounds);
    bounds.topLeft.x = 0;
    bounds.topLeft.y = 0;
    bounds.extent.x = w;
    bounds.extent.y = h;
    WinSetBounds(FrmGetWindowHandle(form), &bounds);

    idx = FrmGetObjectIndex(form, MainDoneButton);
    FrmGetObjectBounds(form, idx, &r);
    r.topLeft.y = h - r.extent.y - 3;
    FrmSetObjectBounds(form, idx, &r);

    idx = FrmGetObjectIndex(form, MainSp4Button);
    FrmGetObjectBounds(form, idx, &r);
    r.topLeft.y = h - r.extent.y - 3;
    FrmSetObjectBounds(form, idx, &r);

    idx = FrmGetObjectIndex(form, MainSp6Button);
    FrmGetObjectBounds(form, idx, &r);
    r.topLeft.y = h - r.extent.y - 3;
    FrmSetObjectBounds(form, idx, &r);

    idx = FrmGetObjectIndex(form, MainM1Button);
    FrmGetObjectBounds(form, idx, &r);
    r.topLeft.y = h - r.extent.y - 3;
    FrmSetObjectBounds(form, idx, &r);

    idx = FrmGetObjectIndex(form, MainSetupButton);
    FrmGetObjectBounds(form, idx, &r);
    r.topLeft.y = h - r.extent.y - 19;
    FrmSetObjectBounds(form, idx, &r);

    gPlot.topLeft.x = 2;
    gPlot.topLeft.y = 50;
    gPlot.extent.x = w - 4;
    gPlot.extent.y = r.topLeft.y - 2 - gPlot.topLeft.y;
}

static void DrawStatus(void)
{
    Coord w, h;
    char text[40];

    WinGetDisplayExtent(&w, &h);
    StrCopy(text, "Display ");
    StrIToA(text + StrLen(text), w);
    StrCat(text, "x");
    StrIToA(text + StrLen(text), h);
    StrCat(text, DeviceHasPIN() ? ", PIN" : ", no PIN");
    StrCat(text, ", density ");
    StrIToA(text + StrLen(text), DeviceDensity());
    FntSetFont(stdFont);
    WinDrawChars(text, StrLen(text), 4, 39);
}

static void DrawAll(FormType *form)
{
    FrmDrawForm(form);
    DrawStatus();
    GraphDraw(&gPlot, gSelected);
}

static void FillLabels(FormType *form)
{
    char text[32];

    FrmCopyLabel(form, MainSectionLabel, HelloFromSection2());
    /* SP2: $84.62 at 1.36 home units per fill unit is $115.08. */
    StrCopy(text, "64-bit: 8462 x 1.36 = ");
    StrIToA(text + StrLen(text), CalcHomeCents(8462, 1360000));
    FrmCopyLabel(form, MainMathLabel, text);
}

static Boolean DiagFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();

    switch (event->eType) {
    case frmOpenEvent:
        FillLabels(form);
        LayoutDiagForm(form);
        DrawAll(form);
        return true;

    case frmUpdateEvent:
        DrawAll(form);
        return true;

    case winDisplayChangedEvent:
        /* The input area opened or collapsed: refit and redraw everything. */
        LayoutDiagForm(form);
        WinEraseWindow();
        DrawAll(form);
        return true;

    case penDownEvent: {
        Int16 hit = GraphHitTest(&gPlot, event->screenX, event->screenY);
        if (hit >= 0) {
            gSelected = (hit == gSelected) ? -1 : hit;
            GraphDraw(&gPlot, gSelected);
            return true;
        }
        break;
    }

    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == MainDoneButton) {
            FrmGotoForm(FillListForm);
            return true;
        }
        if (event->data.ctlSelect.controlID == MainSp4Button) {
            gSp4Count = VfsTestRun(gSp4Lines);
            FrmGotoForm(ResultsForm);
            return true;
        }
        if (event->data.ctlSelect.controlID == MainSp6Button) {
            gSp4Count = Sp6TestRun(gSp4Lines);
            FrmGotoForm(ResultsForm);
            return true;
        }
        if (event->data.ctlSelect.controlID == MainSetupButton) {
            gSetupStep = 1;
            FrmGotoForm(WelcomeForm);
            return true;
        }
        if (event->data.ctlSelect.controlID == MainM1Button) {
            gSp4Count = M1TestRun(gSp4Lines);
            FrmGotoForm(ResultsForm);
            return true;
        }
        break;

    default:
        break;
    }
    return false;
}

static void DrawSp4Lines(void)
{
    UInt16 i;
    FntSetFont(stdFont);
    for (i = 0; i < gSp4Count; i++)
        WinDrawChars(gSp4Lines[i], StrLen(gSp4Lines[i]), 4, 16 + i * 10);
}

static Boolean Sp4FormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();

    switch (event->eType) {
    case frmOpenEvent:
    case frmUpdateEvent:
        FrmDrawForm(form);
        DrawSp4Lines();
        return true;
    case ctlSelectEvent:
        if (event->data.ctlSelect.controlID == ResultsDoneButton) {
            FrmGotoForm(DiagForm);
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

static Boolean AppHandleEvent(EventType *event)
{
    if (event->eType == frmLoadEvent) {
        UInt16 formId = event->data.frmLoad.formID;
        FormType *form = FrmInitForm(formId);
        FrmSetActiveForm(form);
        switch (formId) {
        case FillListForm:
            FrmSetEventHandler(form, FillListHandleEvent);
            AppSetupDIA(form);
            break;
        case FillForm:
            FrmSetEventHandler(form, FillFormHandleEvent);
            AppSetupDIA(form);
            break;
        case PrefsForm:
            FrmSetEventHandler(form, PrefsFormHandleEvent);
            AppSetupDIA(form);
            break;
        case VehiclesForm:
            FrmSetEventHandler(form, VehiclesFormHandleEvent);
            AppSetupDIA(form);
            break;
        case EditVehicleForm:
            FrmSetEventHandler(form, EditVehicleFormHandleEvent);
            AppSetupDIA(form);
            break;
        case ProgrammesForm:
            FrmSetEventHandler(form, ProgrammesFormHandleEvent);
            AppSetupDIA(form);
            break;
        case EditProgrammeForm:
            FrmSetEventHandler(form, EditProgrammeFormHandleEvent);
            AppSetupDIA(form);
            break;
        case WelcomeForm:
            FrmSetEventHandler(form, WelcomeFormHandleEvent);
            AppSetupDIA(form);
            break;
        case RatesForm:
            FrmSetEventHandler(form, RatesFormHandleEvent);
            AppSetupDIA(form);
            break;
        case StatsForm:
            FrmSetEventHandler(form, StatsFormHandleEvent);
            AppSetupDIA(form);
            break;
        case ServiceForm:
            FrmSetEventHandler(form, ServiceFormHandleEvent);
            AppSetupDIA(form);
            break;
        case EditServiceForm:
            FrmSetEventHandler(form, EditServiceFormHandleEvent);
            AppSetupDIA(form);
            break;
        case LogServiceForm:
            FrmSetEventHandler(form, LogServiceFormHandleEvent);
            AppSetupDIA(form);
            break;
        case HistoryForm:
            FrmSetEventHandler(form, HistoryFormHandleEvent);
            AppSetupDIA(form);
            break;
        case ReportsForm:
            FrmSetEventHandler(form, ReportsFormHandleEvent);
            AppSetupDIA(form);
            break;
        case GraphsForm:
            FrmSetEventHandler(form, GraphsFormHandleEvent);
            AppSetupDIA(form);
            break;
        case ExportForm:
            FrmSetEventHandler(form, ExportFormHandleEvent);
            AppSetupDIA(form);
            break;
        case RestoreForm:
            FrmSetEventHandler(form, RestoreFormHandleEvent);
            AppSetupDIA(form);
            break;
        case ChooseFileForm:
            FrmSetEventHandler(form, ChooseFileFormHandleEvent);
            AppSetupDIA(form);
            break;
        case ImportForm:
            FrmSetEventHandler(form, ImportFormHandleEvent);
            AppSetupDIA(form);
            break;
        case DiagForm:
            FrmSetEventHandler(form, DiagFormHandleEvent);
            AppSetupDIA(form);
            break;
        case ResultsForm:
            FrmSetEventHandler(form, Sp4FormHandleEvent);
            break;
        default:
            break;
        }
        return true;
    }
    return false;
}

static void AppEventLoop(void)
{
    EventType event;
    Err error;

    do {
        EvtGetEvent(&event, evtWaitForever);
        if (!SysHandleEvent(&event))
            if (!MenuHandleEvent(0, &event, &error))
                if (!AppHandleEvent(&event))
                    FrmDispatchEvent(&event);
    } while (event.eType != appStopEvent);
}

static Err RomVersionCompatible(UInt32 required, UInt16 launchFlags)
{
    UInt32 romVersion;

    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &romVersion);
    if (romVersion < required) {
        if ((launchFlags & (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp))
            == (sysAppLaunchFlagNewGlobals | sysAppLaunchFlagUIApp)) {
            FrmAlert(0);
        }
        return sysErrRomIncompatible;
    }
    return errNone;
}

/* Without globals (alarm and reset launches) nothing here may touch gPrefs,
 * the databases or any other code section: system traps and locals only. */
static void RestoreAlarmAfterReset(void)
{
    FuelPrefs p;
    UInt16 size = sizeof p, card;
    LocalID dbId;
    Int16 version = PrefGetAppPreferences('ZFlg', prefsAppPrefId, &p, &size, true);
    if (version == noPreferenceFound || version < 2 || size < sizeof p || p.nextAlarm == 0) return;
    if (SysCurAppDatabase(&card, &dbId) == errNone) AlmSetAlarm(card, dbId, 0, p.nextAlarm, true);
}

/* Receives a .fuel transfer with system traps only, so it works without
 * globals: a records bundle goes into the inbox database for the next
 * launch (or right away when the app is running); a file envelope is
 * written to the card. */
static void ReceiveBeam(ExgSocketType *sock, Boolean running)
{
    UInt8 head[10], chunk[1024];
    UInt32 got, total = 0;
    Err err;
    UInt16 kind;
    if (ExgAccept(sock) != errNone) return;
    got = ExgReceive(sock, head, 8, &err);
    if (err || got < 8 || head[0] != 'F' || head[1] != 'U' || head[2] != 'E' || head[3] != 'L') { ExgDisconnect(sock, err ? err : exgErrBadData); return; }
    kind = (UInt16)((head[6] << 8) | head[7]);
    if (kind == beamKindFile) {
        UInt8 nameLen, fmt; char name[48], path[96]; UInt16 vol; FileRef f = 0;
        UInt32 iter = vfsIteratorStart;
        got = ExgReceive(sock, head, 2, &err);
        if (err || got < 2) { ExgDisconnect(sock, exgErrBadData); return; }
        nameLen = head[0]; fmt = head[1];
        if (nameLen > 40) nameLen = 40;
        got = ExgReceive(sock, name, nameLen, &err);
        name[got] = 0;
        if (VFSVolumeEnumerate(&vol, &iter) != errNone) { ExgDisconnect(sock, vfsErrVolumeBadRef); return; }
        StrCopy(path, fmt == 2 ? "/PALM/Programs/FuelLog/Export/" : "/PALM/Programs/FuelLog/Import/");
        StrCat(path, name);
        VFSFileDelete(vol, path);
        if (VFSFileOpen(vol, path, vfsModeWrite | vfsModeCreate | vfsModeTruncate, &f) != errNone) { ExgDisconnect(sock, vfsErrFileGeneric); return; }
        for (;;) {
            UInt32 wrote;
            got = ExgReceive(sock, chunk, sizeof chunk, &err);
            if (err || got == 0) break;
            VFSFileWrite(f, got, chunk, &wrote);
            total += got;
        }
        VFSFileClose(f);
        ExgDisconnect(sock, err);
        if (running) FrmCustomAlert(ReceivedFileAlert, name, fmt == 2 ? "Export" : "Import", "");
        return;
    }
    {
        /* records: gather into memory, then one inbox record */
        MemHandle h = MemHandleNew(8);
        UInt8 *p;
        if (!h) { ExgDisconnect(sock, memErrNotEnoughSpace); return; }
        p = MemHandleLock(h); MemMove(p, head, 8); MemHandleUnlock(h); total = 8;
        for (;;) {
            got = ExgReceive(sock, chunk, sizeof chunk, &err);
            if (err || got == 0) break;
            if (MemHandleResize(h, total + got) != errNone) { err = memErrNotEnoughSpace; break; }
            p = MemHandleLock(h); MemMove(p + total, chunk, got); MemHandleUnlock(h);
            total += got;
        }
        ExgDisconnect(sock, err);
        if (!err) {
            LocalID id = DmFindDatabase(0, beamInboxName);
            DmOpenRef inbox;
            if (!id) { DmCreateDatabase(0, beamInboxName, 'ZFlg', 'Inbx', false); id = DmFindDatabase(0, beamInboxName); }
            inbox = id ? DmOpenDatabase(0, id, dmModeReadWrite) : 0;
            if (inbox) {
                UInt16 at = dmMaxRecordIndex;
                MemHandle rec = DmNewRecord(inbox, &at, total);
                if (rec) {
                    p = MemHandleLock(h);
                    DmWrite(MemHandleLock(rec), 0, p, total);
                    MemHandleUnlock(rec); MemHandleUnlock(h);
                    DmReleaseRecord(inbox, at, true);
                }
                DmCloseDatabase(inbox);
            }
        }
        MemHandleFree(h);
        if (running) { if (BeamProcessInbox()) FrmUpdateForm(FrmGetActiveFormID(), frmRedrawUpdateCode); }
    }
}

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    if (cmd == sysAppLaunchCmdExgReceiveData) {
        ReceiveBeam((ExgSocketType *)cmdPBP, (launchFlags & sysAppLaunchFlagSubCall) != 0);
        return errNone;
    }
    if (cmd == sysAppLaunchCmdExgAskUser) return errNone;   /* the system's own dialog asks */
    if (cmd == sysAppLaunchCmdSystemReset) {
        RestoreAlarmAfterReset();
        return errNone;
    }
    if (cmd == sysAppLaunchCmdDisplayAlarm) {
        FrmAlert(ServiceAlarmAlert);
        return errNone;
    }
    if (cmd == sysAppLaunchCmdAlarmTriggered) return errNone;   /* the system plays the alarm sound */
    if (cmd == sysAppLaunchCmdNormalLaunch) {
        if (RomVersionCompatible(kMinRomVersion, launchFlags) != errNone)
            return sysErrRomIncompatible;
        DeviceInit();
        PrefsLoad(&gPrefs);
        LocGetNumberSeparators((NumberFormatType)PrefGetPreference(prefNumberFormat), &gThousands, &gPoint);
        if (DbOpenAll() != errNone) {
            FrmAlert(SaveFailedAlert);
            return errNone;
        }
        PgmInstallPresets();
        BeamRegister();
        BeamProcessInbox();
        SvcScheduleAlarm();
        /* First launch: no vehicle yet means the Welcome steps, never an empty table. */
        if (DbCount(dbVehicles) == 0) { gSetupStep = 1; FrmGotoForm(WelcomeForm); }
        else FrmGotoForm(FillListForm);
        AppEventLoop();
        FrmCloseAllForms();
        BackupAuto();           /* weekly card backup on exit, when due */
        PrefsSave(&gPrefs);
        DbCloseAll();
    }
    return errNone;
}

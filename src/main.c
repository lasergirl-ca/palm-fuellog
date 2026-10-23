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

UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
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
        FrmGotoForm(FillListForm);
        AppEventLoop();
        FrmCloseAllForms();
        PrefsSave(&gPrefs);
        DbCloseAll();
    }
    return errNone;
}

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

#define kMinRomVersion sysMakeROMVersion(3, 0, 0, sysROMStageRelease, 0)

static RectangleType gPlot;      /* graph area, standard coordinates */
static Int16 gSelected = -1;

/* Fit the form to the display and keep Done at the bottom. */
static void LayoutMainForm(FormType *form)
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

static Boolean MainFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();

    switch (event->eType) {
    case frmOpenEvent:
        FillLabels(form);
        LayoutMainForm(form);
        DrawAll(form);
        return true;

    case frmUpdateEvent:
        DrawAll(form);
        return true;

    case winDisplayChangedEvent:
        /* The input area opened or collapsed: refit and redraw everything. */
        LayoutMainForm(form);
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
            EventType stop;
            MemSet(&stop, sizeof(stop), 0);
            stop.eType = appStopEvent;
            EvtAddEventToQueue(&stop);
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
        if (formId == MainForm) {
            FrmSetEventHandler(form, MainFormHandleEvent);
            if (DeviceHasPIN()) {
                /* Let the user collapse the input area; the form may grow
                 * from 160 to 225 rows of standard coordinates. */
                FrmSetDIAPolicyAttr(form, frmDIAPolicyCustom);
                PINSetInputTriggerState(pinInputTriggerEnabled);
                WinSetConstraintsSize(FrmGetWindowHandle(form),
                                      160, 225, 225, 160, 160, 160);
                PINSetInputAreaState(pinInputAreaUser);
            }
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
        FrmGotoForm(MainForm);
        AppEventLoop();
        FrmCloseAllForms();
    }
    return errNone;
}

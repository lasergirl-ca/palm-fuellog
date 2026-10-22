/* FuelLog for Palm OS: M0 hello world.
 * One form, one label filled from a function in a second code section. */
#include <PalmOS.h>
#include "resources.h"
#include "hello2.h"
#include "calc.h"

#define kMinRomVersion sysMakeROMVersion(3, 0, 0, sysROMStageRelease, 0)

static Boolean MainFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();

    switch (event->eType) {
    case frmOpenEvent:
        FrmCopyLabel(form, MainSectionLabel, HelloFromSection2());
        {
            /* SP2: exercise the 64-bit libgcc helpers on the device.
             * $84.62 at 1.36 home units per fill unit is $115.08. */
            Char text[32];
            StrCopy(text, "8462 x 1.36 = ");
            StrIToA(text + StrLen(text), CalcHomeCents(8462, 1360000));
            FrmCopyLabel(form, MainMathLabel, text);
        }
        FrmDrawForm(form);
        return true;

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
        if (formId == MainForm)
            FrmSetEventHandler(form, MainFormHandleEvent);
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
        FrmGotoForm(MainForm);
        AppEventLoop();
        FrmCloseAllForms();
    }
    return errNone;
}

#include "app.h"
#include "device.h"
#include "datecalc.h"
#include <PenInputMgr.h>

FuelPrefs gPrefs;
char gThousands = ',', gPoint = '.';
UInt16 gEditIndex = dmMaxRecordIndex;

void AppSetFieldText(FormType *form, UInt16 fieldId, const char *text)
{
    FieldType *fld = FrmGetObjectPtr(form, FrmGetObjectIndex(form, fieldId));
    MemHandle old = FldGetTextHandle(fld);
    MemHandle h = MemHandleNew(StrLen(text) + 1);
    if (!h) return;
    StrCopy(MemHandleLock(h), text);
    MemHandleUnlock(h);
    FldSetTextHandle(fld, h);
    if (old) MemHandleFree(old);
    if (FrmVisible(form)) FldDrawField(fld);
}

const char *AppGetFieldText(FormType *form, UInt16 fieldId)
{
    FieldType *fld = FrmGetObjectPtr(form, FrmGetObjectIndex(form, fieldId));
    const char *t = FldGetTextPtr(fld);
    return t ? t : "";
}

void AppSetLabel(FormType *form, UInt16 labelId, const char *text)
{
    FrmCopyLabel(form, labelId, text);
}

UInt16 AppToday(void)
{
    DateTimeType now;
    TimSecondsToDateTime(TimGetSeconds(), &now);
    return DateYmdToDays((UInt16)now.year, (UInt8)now.month, (UInt8)now.day);
}

UInt16 AppNowMinutes(void)
{
    DateTimeType now;
    TimSecondsToDateTime(TimGetSeconds(), &now);
    return (UInt16)(now.hour * 60 + now.minute);
}

void AppTimeText(char *buf, UInt16 minutes)
{
    if (minutes == fillTimeUnknown) { StrCopy(buf, "--:--"); return; }
    TimeToAscii((UInt8)(minutes / 60), (UInt8)(minutes % 60),
                (TimeFormatType)PrefGetPreference(prefTimeFormat), buf);
}

void AppSetupDIA(FormType *form)
{
    if (DeviceHasPIN()) {
        FrmSetDIAPolicyAttr(form, frmDIAPolicyCustom);
        PINSetInputTriggerState(pinInputTriggerEnabled);
        WinSetConstraintsSize(FrmGetWindowHandle(form), 160, 225, 225, 160, 160, 160);
        PINSetInputAreaState(pinInputAreaUser);
    }
}

void AppFitForm(FormType *form, const UInt16 *bottomIds, UInt16 n)
{
    RectangleType bounds, r;
    Coord w, h;
    UInt16 i, idx;

    WinGetDisplayExtent(&w, &h);
    FrmGetFormBounds(form, &bounds);
    bounds.topLeft.x = 0; bounds.topLeft.y = 0;
    bounds.extent.x = w; bounds.extent.y = h;
    WinSetBounds(FrmGetWindowHandle(form), &bounds);
    for (i = 0; i < n; i++) {
        idx = FrmGetObjectIndex(form, bottomIds[i]);
        FrmGetObjectBounds(form, idx, &r);
        r.topLeft.y = h - r.extent.y - 3;
        FrmSetObjectBounds(form, idx, &r);
    }
}

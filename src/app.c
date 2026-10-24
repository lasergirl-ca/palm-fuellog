#include "app.h"
#include "device.h"
#include "datecalc.h"
#include <PenInputMgr.h>

FuelPrefs gPrefs;
char gThousands = ',', gPoint = '.';
UInt16 gEditIndex = dmMaxRecordIndex;
UInt16 gDisplayChanges;

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

static Int16 gScrollY;          /* how far the content has moved up */

void AppScrollReset(void) { gScrollY = 0; }
Int16 gScrollYPublic(void) { return gScrollY; }

static Boolean IsPinned(UInt16 id, const UInt16 *pinnedIds, UInt16 n)
{
    UInt16 i;
    for (i = 0; i < n; i++) if (pinnedIds[i] == id) return true;
    return false;
}

/* Bottom edge of the lowest unpinned object, in window coordinates now. */
static Coord ContentBottom(FormType *form, const UInt16 *pinnedIds, UInt16 n)
{
    UInt16 count = FrmGetNumberOfObjects(form), i;
    Coord bottom = 0;
    RectangleType r;
    for (i = 0; i < count; i++) {
        FormObjectKind kind = FrmGetObjectType(form, i);
        if (kind == frmListObj) continue;                 /* popup lists are hidden and tall */
        if (IsPinned(FrmGetObjectId(form, i), pinnedIds, n)) continue;
        FrmGetObjectBounds(form, i, &r);
        if (r.topLeft.y + r.extent.y > bottom) bottom = r.topLeft.y + r.extent.y;
    }
    return bottom;
}

void AppScrollBy(FormType *form, Int16 dy, const UInt16 *pinnedIds, UInt16 n)
{
    UInt16 count = FrmGetNumberOfObjects(form), i;
    Coord x, y;
    if (dy == 0) return;
    for (i = 0; i < count; i++) {
        if (IsPinned(FrmGetObjectId(form, i), pinnedIds, n)) continue;
        FrmGetObjectPosition(form, i, &x, &y);
        FrmSetObjectPosition(form, i, x, (Coord)(y + dy));
    }
    gScrollY -= dy;
    WinEraseWindow();
    FrmDrawForm(form);
}

/* The content area runs from under the title bar to above the pinned buttons. */
static void VisibleArea(Coord *top, Coord *bottom)
{
    *top = 15;
    *bottom = AppFormHeight(FrmGetActiveForm()) - 16;
}

Boolean AppScrollKey(FormType *form, const EventType *event, const UInt16 *pinnedIds, UInt16 n)
{
    Coord top, bottom, content;
    WChar c = event->data.keyDown.chr;
    if (c != vchrPageDown && c != vchrPageUp) return false;
    VisibleArea(&top, &bottom);
    content = ContentBottom(form, pinnedIds, n);
    if (c == vchrPageDown) {
        Int16 room = content - bottom;              /* how much is hidden below */
        if (room <= 0) return true;
        AppScrollBy(form, (Int16)-(room < 60 ? room : 60), pinnedIds, n);
    } else {
        if (gScrollY <= 0) return true;
        AppScrollBy(form, (Int16)(gScrollY < 60 ? gScrollY : 60), pinnedIds, n);
    }
    return true;
}

void AppScrollToField(FormType *form, UInt16 fieldId, const UInt16 *pinnedIds, UInt16 n)
{
    RectangleType r;
    Coord top, bottom;
    UInt16 idx = FrmGetObjectIndex(form, fieldId);
    if (idx == frmInvalidObjectId) return;
    VisibleArea(&top, &bottom);
    FrmGetObjectBounds(form, idx, &r);
    if (r.topLeft.y + r.extent.y > bottom)
        AppScrollBy(form, (Int16)(bottom - (r.topLeft.y + r.extent.y)), pinnedIds, n);
    else if (r.topLeft.y < top)
        AppScrollBy(form, (Int16)(top - r.topLeft.y), pinnedIds, n);
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
    /* Pin to the height the window really has: if the resize did not take,
     * the buttons stay on screen instead of moving below the window. */
    h = AppFormHeight(form);
    for (i = 0; i < n; i++) {
        idx = FrmGetObjectIndex(form, bottomIds[i]);
        FrmGetObjectBounds(form, idx, &r);
        r.topLeft.y = h - r.extent.y - 3;
        FrmSetObjectBounds(form, idx, &r);
    }
}

UInt16 AppGroupSelection(FormType *form, UInt8 group)
{
    UInt16 idx = FrmGetControlGroupSelection(form, group);
    return idx == frmNoSelectedControl ? 0 : FrmGetObjectId(form, idx);
}

void AppShowByPosition(FormType *form, const UInt16 *ids, UInt16 n)
{
    Coord top = 15, bottom = (Coord)(AppFormHeight(form) - 16);
    UInt16 i;
    for (i = 0; i < n; i++) {
        UInt16 idx = FrmGetObjectIndex(form, ids[i]);
        RectangleType r;
        if (idx == frmInvalidObjectId) continue;
        FrmGetObjectBounds(form, idx, &r);
        if (r.topLeft.y >= top && r.topLeft.y + r.extent.y <= bottom) FrmShowObject(form, idx);
        else FrmHideObject(form, idx);
    }
}

Coord AppFormHeight(FormType *form)
{
    RectangleType b;
    FrmGetFormBounds(form, &b);
    return b.extent.y;
}

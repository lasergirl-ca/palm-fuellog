#include "app.h"
#include "device.h"
#include "datecalc.h"
#include "resources.h"
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

UInt16 AppDeviceToday(void)
{
    DateTimeType now;
    TimSecondsToDateTime(TimGetSeconds(), &now);
    return DateYmdToDays((UInt16)now.year, (UInt8)now.month, (UInt8)now.day);
}

/* The clock may run decades behind: DateFix moves the epoch from 1904 to a
 * start year S and publishes S - 1904 as feature 1 of creator 'DtFx'
 * (TimSecondsToDateTime then returns internal years, S..S+127 in reality).
 * Without DateFix, a clock more than 14 years behind the build year is taken
 * as set back 28 years by hand. Either way the app works in real dates. */
static UInt16  gShiftDays;
static UInt16  gShiftYears;
static Boolean gDateFix;

void AppClockInit(void)
{
    UInt32 years;
    DateTimeType now;
    if (FtrGet('DtFx', 1, &years) == errNone && years >= 4 && years <= 68 && (years % 4) == 0) {
        gDateFix = true;
        gShiftYears = (UInt16)years;
        gShiftDays = (UInt16)DateEpochOffsetDays((UInt16)years);
        return;
    }
    gDateFix = false;
    TimSecondsToDateTime(TimGetSeconds(), &now);
    gShiftDays = DateClockShift((UInt16)now.year, FUELLOG_BUILD_YEAR);
    gShiftYears = gShiftDays ? dateClockShiftYears : 0;
}

UInt16 AppClockShift(void) { return gShiftDays; }
Boolean AppClockIsDateFix(void) { return gDateFix; }

UInt16 AppToday(void)
{
    return (UInt16)(AppDeviceToday() + gShiftDays);
}

Boolean AppSelectDay(UInt16 *days, const char *title)
{
    UInt16 y; UInt8 m, d; Int16 yy, mm, dd;
    DateDaysToYmd(*days, &y, &m, &d);
    /* DateFix's picker takes and returns internal years and shows the real
     * ones; the plain system picker shows what it is given. */
    if (gDateFix) y = y >= 1904 + gShiftYears ? (UInt16)(y - gShiftYears) : 1904;
    yy = (Int16)y; mm = m; dd = d;
    if (!SelectDay(selectDayByDay, &mm, &dd, &yy, title)) return false;
    if (gDateFix) yy = (Int16)(yy + gShiftYears);
    /* the picker's Today is the clock's own date: moved like any clock date */
    *days = DateUnshift(DateYmdToDays((UInt16)yy, (UInt8)mm, (UInt8)dd), AppDeviceToday(), gShiftDays);
    return true;
}

UInt32 AppAlarmSeconds(UInt16 days, UInt8 hour)
{
    DateTimeType dt;
    UInt16 y; UInt8 m, d;
    DateDaysToYmd((UInt16)(days - gShiftDays), &y, &m, &d);
    MemSet(&dt, sizeof dt, 0);
    dt.year = (Int16)y; dt.month = m; dt.day = d; dt.hour = hour;
    return TimDateTimeToSeconds(&dt);
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

#ifndef vchrRockerUp
#define vchrRockerUp   0x0132
#define vchrRockerDown 0x0133
#endif
#ifndef vchrRockerCenter
#define vchrRockerCenter 0x0136
#endif
#ifndef vchrNavChange
#define vchrNavChange 0x0507
#define navBitSelect 0x0010
#define navChangeSelect 0x1000
#endif

Int16 AppScrollDirection(const EventType *event, Boolean *page)
{
    WChar c;
    *page = false;
    if (event->eType != keyDownEvent) return 0;
    c = event->data.keyDown.chr;
    if (c == vchrPageDown) { *page = true; return 1; }
    if (c == vchrPageUp) { *page = true; return -1; }
    if (c == vchrRockerDown) return 1;
    if (c == vchrRockerUp) return -1;
    return 0;
}

Int16 AppRowKey(const EventType *event, Int16 *sel, UInt16 *first, UInt16 count, UInt16 visible)
{
    WChar c;
    UInt8 down, up, centre;
    if (event->eType != keyDownEvent || count == 0) return 0;
    c = event->data.keyDown.chr;
    if (*sel >= (Int16)count) *sel = (Int16)count - 1;
    /* A Treo rocker sends its own codes; a Palm 5-way (the T|X) sends page keys for up and
     * down and vchrNavChange with the select bit for the centre. Palm's convention: page
     * keys page until a row is highlighted, then they move the highlight; the first centre
     * press highlights the top row, the next opens it. */
    centre = c == vchrRockerCenter ||
             (c == vchrNavChange && (event->data.keyDown.keyCode & (navBitSelect | navChangeSelect)) == (navBitSelect | navChangeSelect));
    if (centre) {
        if (*sel >= 0) return 2;
        *sel = (Int16)*first;
        return 1;
    }
    if (c == vchrNavChange) return 0;
    down = c == vchrRockerDown || (c == vchrPageDown && *sel >= 0);
    up = c == vchrRockerUp || (c == vchrPageUp && *sel >= 0);
    if (!down && !up) return 0;
    if (visible == 0) visible = 1;
    if (*sel < 0) {
        /* nothing picked yet: start at the top or bottom of what is on screen */
        UInt16 last = (UInt16)(*first + visible - 1);
        *sel = down ? (Int16)*first : (Int16)(last < count ? last : count - 1);
    } else if (down) { if (*sel + 1 < (Int16)count) (*sel)++; }
    else if (*sel > 0) (*sel)--;
    if (*sel < (Int16)*first) *first = (UInt16)*sel;
    else if (*sel >= (Int16)(*first + visible)) *first = (UInt16)(*sel - visible + 1);
    return 1;
}

void AppRowHighlight(const RectangleType *r)
{
    WinSetBackColor(UIColorGetTableEntryIndex(UIObjectSelectedFill));
    WinSetForeColor(UIColorGetTableEntryIndex(UIObjectSelectedFill));
    WinFillRectangle(r, 0);
    WinSetForeColor(UIColorGetTableEntryIndex(UIObjectSelectedForeground));
    WinSetTextColor(UIColorGetTableEntryIndex(UIObjectSelectedForeground));
}

void AppRowPlain(void)
{
    WinSetBackColor(UIColorGetTableEntryIndex(UIFormFill));
    WinSetForeColor(UIColorGetTableEntryIndex(UIObjectForeground));
    WinSetTextColor(UIColorGetTableEntryIndex(UIObjectForeground));
}

Boolean AppScrollKey(FormType *form, const EventType *event, const UInt16 *pinnedIds, UInt16 n)
{
    Coord top, bottom, content;
    Boolean page;
    Int16 dir = AppScrollDirection(event, &page), step;
    if (dir == 0) return false;
    step = page ? 60 : 26;                          /* a screenful or two rows */
    VisibleArea(&top, &bottom);
    content = ContentBottom(form, pinnedIds, n);
    if (dir > 0) {
        Int16 room = content - bottom;              /* how much is hidden below */
        if (room <= 0) return true;
        AppScrollBy(form, (Int16)-(room < step ? room : step), pinnedIds, n);
    } else {
        if (gScrollY <= 0) return true;
        AppScrollBy(form, (Int16)(gScrollY < step ? gScrollY : step), pinnedIds, n);
    }
    return true;
}

void AppScrollBarUpdate(FormType *form, UInt16 sbId, const UInt16 *pinnedIds, UInt16 n)
{
    UInt16 idx = FrmGetObjectIndex(form, sbId);
    RectangleType r;
    Coord top, bottom, content, h = AppFormHeight(form);
    Int16 overflow;
    if (idx == frmInvalidObjectId) return;
    FrmGetObjectBounds(form, idx, &r);
    r.topLeft.y = 15; r.extent.y = (Coord)(h - 16 - 15);
    FrmSetObjectBounds(form, idx, &r);
    VisibleArea(&top, &bottom);
    content = (Coord)(ContentBottom(form, pinnedIds, n) + gScrollY);   /* height before any scrolling */
    overflow = (Int16)(content - bottom);
    if (overflow <= 0) { FrmHideObject(form, idx); return; }
    SclSetScrollBar(FrmGetObjectPtr(form, idx), gScrollY, 0, overflow, (Int16)(bottom - top));
    FrmShowObject(form, idx);
}

Boolean AppScrollEvent(FormType *form, const EventType *event, const UInt16 *pinnedIds, UInt16 n, UInt16 sbId)
{
    Boolean handled;
    if (event->eType == keyDownEvent) handled = AppScrollKey(form, event, pinnedIds, n);
    else if (event->eType == sclRepeatEvent || event->eType == sclExitEvent) {
        Int16 value = event->eType == sclRepeatEvent ? event->data.sclRepeat.newValue : event->data.sclExit.newValue;
        if (value != gScrollY) AppScrollBy(form, (Int16)(gScrollY - value), pinnedIds, n);
        handled = true;
    } else return false;
    if (handled) AppScrollBarUpdate(form, sbId, pinnedIds, n);
    return handled;
}

void AppListScrollBar(FormType *form, UInt16 sbId, Int16 value, Int16 max, Int16 page, Coord top, Coord bottom)
{
    UInt16 idx = FrmGetObjectIndex(form, sbId);
    RectangleType r;
    if (idx == frmInvalidObjectId) return;
    FrmGetObjectBounds(form, idx, &r);
    r.topLeft.y = top; r.extent.y = (Coord)(bottom - top);
    FrmSetObjectBounds(form, idx, &r);
    if (max <= 0) { FrmHideObject(form, idx); return; }
    if (value > max) value = max;
    SclSetScrollBar(FrmGetObjectPtr(form, idx), value, 0, max, page);
    FrmShowObject(form, idx);
}

Boolean AppListScrollEvent(const EventType *event, Int16 *value)
{
    if (event->eType == sclRepeatEvent) { *value = event->data.sclRepeat.newValue; return true; }
    if (event->eType == sclExitEvent) { *value = event->data.sclExit.newValue; return true; }
    return false;
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

/* ---- note dialog ---- */

static void NoteCount(FormType *dlg)
{
    char b[20];
    FieldType *fld = FrmGetObjectPtr(dlg, FrmGetObjectIndex(dlg, FillNoteField));
    StrIToA(b, FldGetTextLength(fld));
    StrCat(b, " of 255 bytes");
    AppSetLabel(dlg, NoteCountLabel, b);
}

static Boolean NoteDialogHandleEvent(EventType *e)
{
    FormType *dlg = FrmGetActiveForm();
    if (e->eType == keyDownEvent) {
        FrmHandleEvent(dlg, e);
        NoteCount(dlg);
        return true;
    }
    return false;
}

void AppNoteDialog(char *notes)
{
    FormType *dlg = FrmInitForm(NoteForm);
    UInt16 hit;
    FrmSetEventHandler(dlg, NoteDialogHandleEvent);
    AppSetFieldText(dlg, FillNoteField, notes);
    NoteCount(dlg);
    FrmSetFocus(dlg, FrmGetObjectIndex(dlg, FillNoteField));
    hit = FrmDoDialog(dlg);
    if (hit == NoteOkButton) { StrNCopy(notes, AppGetFieldText(dlg, FillNoteField), fillMaxNotes - 1); notes[fillMaxNotes - 1] = 0; }
    else if (hit == NoteClearButton) notes[0] = 0;
    FrmDeleteForm(dlg);
}

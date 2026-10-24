#include "service_form.h"
#include "app.h"
#include "db.h"
#include "calc.h"
#include "units.h"
#include "currency.h"
#include "text.h"
#include "datecalc.h"
#include "vehicles.h"
#include "resources.h"
#include "device.h"

static ServiceTypeRecord gSvt, gScanSvt;
static ServiceVisitRecord gSvl, gScanSvl;      /* 400 bytes each: static, off the stack */
static FillRecord gScanFillS;
static VehicleRecord gSvcVeh;
static UInt8 gSvcPacked[svlFixedSize + fillMaxNotes + 8];
static UInt16 gEditSvtIndex = dmMaxRecordIndex;
static UInt16 gEditSvlIndex = dmMaxRecordIndex;   /* visit being edited, or new */
static UInt16 gHistType = dmMaxRecordIndex;        /* service type the History shows; all visits when dmMaxRecordIndex */
static UInt16 gSvlReturn = ServiceForm;            /* where Log Service goes back to */
static UInt8 gSvcVehicle;                      /* the vehicle the service screens work on */
static char gSVNames[vehMaxVehicles + 1][vehNameLen];
static char *gSVPtrs[vehMaxVehicles + 1];
static UInt8 gSVIds[vehMaxVehicles + 1];

static void DueDateText(char *b, UInt16 days) SVC_SEC;

/* ---- lookups ---- */

/* Section placement: see sections.h. */
static Err SvtGetAt(UInt16 i, ServiceTypeRecord *t) SVC_SEC;
static Err SvlGetAt(UInt16 i, ServiceVisitRecord *v) SVC_SEC;
static UInt8 SvcVehicleNow(void) SVC_SEC;
static Boolean LastVisit(UInt8 vehicle, UInt8 typeId, UInt16 *date, UInt32 *odo) SVC_SEC;
static UInt32 Pace(UInt8 vehicle) SVC_SEC;
static void InstallPresets(UInt8 vehicle) SVC_SEC;
static UInt8 NextTypeId(UInt8 vehicle) SVC_SEC;
static UInt16 VisitsCovering(UInt8 vehicle, UInt8 typeId) SVC_SEC;
static void DueFor(const ServiceTypeRecord *t, UInt32 odoNow, UInt32 pace, SvcDue *d) SVC_SEC;
static void DistText(char *b, UInt32 dkm, char thousands) SVC_SEC;
static void BannerText(const ServiceTypeRecord *t, const SvcDue *d, char *buf) SVC_SEC;
static void Collect(void) SVC_SEC;
static void DrawRightAt(const char *s, Coord right, Coord y) SVC_SEC;
static void DrawService(void) SVC_SEC;
static void LayoutService(FormType *form) SVC_SEC;
static void BuildServicePopup(FormType *form) SVC_SEC;
static void Summary(FormType *form) SVC_SEC;
static void LoadSvt(FormType *form) SVC_SEC;
static Boolean SaveSvt(FormType *form) SVC_SEC;
static void PlaceSvl(FormType *form) SVC_SEC;
static void NextLine(FormType *form) SVC_SEC;
static void LoadSvl(FormType *form) SVC_SEC;
static Boolean SaveSvl(FormType *form) SVC_SEC;

static Err SvtGetAt(UInt16 i, ServiceTypeRecord *t)
{
    UInt32 len;
    Err err = DbGet(dbServiceTypes, i, gSvcPacked, sizeof gSvcPacked, &len);
    if (err) return err;
    return ServiceTypeUnpack(gSvcPacked, len, t) == 0 ? errNone : dbErrCorrupt;
}

static Err SvlGetAt(UInt16 i, ServiceVisitRecord *v)
{
    UInt32 len;
    Err err = DbGet(dbServiceVisits, i, gSvcPacked, sizeof gSvcPacked, &len);
    if (err) return err;
    return ServiceVisitUnpack(gSvcPacked, len, v) == 0 ? errNone : dbErrCorrupt;
}

static UInt8 SvcVehicleNow(void)
{
    return gPrefs.currentVehicle != 255 ? gPrefs.currentVehicle : VehDefaultForNewFill();
}

/* Visits are sorted by date, so the latest covering a service is found from the end. */
static Boolean LastVisit(UInt8 vehicle, UInt8 typeId, UInt16 *date, UInt32 *odo)
{
    UInt16 n = DbCount(dbServiceVisits);
    while (n--) {
        if (SvlGetAt(n, &gScanSvl) != errNone || gScanSvl.vehicle != vehicle) continue;
        if (typeId != 255 && !(gScanSvl.services & (1UL << typeId))) continue;
        *date = gScanSvl.date; *odo = gScanSvl.odometer;
        return true;
    }
    return false;
}

/* km x 10 per day over the last 90 days of fills (at least 3), else over all fills. */
static UInt32 Pace(UInt8 vehicle)
{
    UInt16 n = DbCount(dbFills), i, count = 0, allCount = 0;
    UInt16 today = AppToday(), firstDate = 0, lastDate = 0, allFirstDate = 0, allLastDate = 0;
    UInt32 firstOdo = 0, lastOdo = 0, allFirstOdo = 0, allLastOdo = 0;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanFillS) != errNone || gScanFillS.vehicle != vehicle || gScanFillS.odometer == 0) continue;
        if (allCount++ == 0) { allFirstDate = gScanFillS.date; allFirstOdo = gScanFillS.odometer; }
        allLastDate = gScanFillS.date; allLastOdo = gScanFillS.odometer;
        if (gScanFillS.date + 90 < today) continue;
        if (count++ == 0) { firstDate = gScanFillS.date; firstOdo = gScanFillS.odometer; }
        lastDate = gScanFillS.date; lastOdo = gScanFillS.odometer;
    }
    if (count >= 3) return SvcPaceX10(firstOdo, lastOdo, firstDate, lastDate);
    if (allCount >= 2) return SvcPaceX10(allFirstOdo, allLastOdo, allFirstDate, allLastDate);
    return 0;
}

static void InstallPresets(UInt8 vehicle)
{
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt8 k, count;
    for (i = 0; i < n; i++) if (SvtGetAt(i, &gScanSvt) == errNone && gScanSvt.vehicle == vehicle) return;
    VehGet(vehicle, &gSvcVeh);
    count = SvcPresetCount(gSvcVeh.powertrain);
    for (k = 0; k < count; k++) {
        SvcPreset(gSvcVeh.powertrain, k, &gScanSvt);
        gScanSvt.vehicle = vehicle;
        DbAdd(dbServiceTypes, gSvcPacked, ServiceTypePack(&gScanSvt, gSvcPacked), NULL);
    }
}

static UInt8 NextTypeId(UInt8 vehicle)
{
    UInt32 used = 0;
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt8 id;
    for (i = 0; i < n; i++) if (SvtGetAt(i, &gScanSvt) == errNone && gScanSvt.vehicle == vehicle && gScanSvt.id < 31) used |= 1UL << gScanSvt.id;
    for (id = 0; id < 31; id++) if (!(used & (1UL << id))) return id;
    return 255;
}

static UInt16 VisitsCovering(UInt8 vehicle, UInt8 typeId)
{
    UInt16 n = DbCount(dbServiceVisits), i, count = 0;
    for (i = 0; i < n; i++)
        if (SvlGetAt(i, &gScanSvl) == errNone && gScanSvl.vehicle == vehicle && (gScanSvl.services & (1UL << typeId))) count++;
    return count;
}

static void DueFor(const ServiceTypeRecord *t, UInt32 odoNow, UInt32 pace, SvcDue *d)
{
    UInt16 date = 0;
    UInt32 odo = 0;
    Boolean have = LastVisit(t->vehicle, t->id, &date, &odo);
    SvcCompute(t, have ? 1 : 0, date, odo, odoNow, AppToday(), pace, d);
}

static void DistText(char *b, UInt32 dkm, char thousands)
{
    TextFixed(b, (Int32)(UnitDistToDisplay(dkm, gPrefs.distanceUnit) / 10), 0, gPoint, thousands);
}

/* "Oil and filter due in 640 km" / "Wiper blades overdue" / "Oil and filter due Nov 6". */
static void BannerText(const ServiceTypeRecord *t, const SvcDue *d, char *buf)
{
    char n[16];
    StrNCopy(buf, t->name, 20); buf[20] = 0;
    if (d->status == svcStatusOverdue) { StrCat(buf, " overdue"); return; }
    if (d->hasDist && d->kmLeftX10 >= 0 && (!d->hasTime || d->distDue == 0 || d->distDue <= d->timeDue)) {
        DistText(n, (UInt32)d->kmLeftX10, gThousands);
        StrCat(buf, " due in "); StrCat(buf, n); StrCat(buf, " "); StrCat(buf, UnitDistLabel(gPrefs.distanceUnit));
    } else {
        StrCat(buf, " due "); DueDateText(buf + StrLen(buf), d->due);
    }
}

UInt8 SvcBanner(UInt8 vehicle, char *buf)
{
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt8 worst = svcStatusOK;
    Int32 worstKey = 0x7FFFFFFF;
    UInt8 lastVehicle = 255;
    UInt32 odoNow = 0, pace = 0;
    buf[0] = 0;
    for (i = 0; i < n; i++) {
        SvcDue d;
        Int32 key;
        if (SvtGetAt(i, &gSvt) != errNone || gSvt.archived) continue;
        if (vehicle != 255 && gSvt.vehicle != vehicle) continue;
        if (gSvt.vehicle != lastVehicle) {
            if (!VehGet(gSvt.vehicle, &gSvcVeh) || gSvcVeh.archived) continue;
            lastVehicle = gSvt.vehicle; odoNow = VehLastOdometer(gSvt.vehicle); pace = Pace(gSvt.vehicle);
        }
        DueFor(&gSvt, odoNow, pace, &d);
        if (d.status != svcStatusSoon && d.status != svcStatusOverdue) continue;
        key = (d.status == svcStatusOverdue ? -1000000L : 0) + (d.due ? (Int32)d.due : 0);
        if (key < worstKey) { worstKey = key; worst = d.status; BannerText(&gSvt, &d, buf); }
    }
    return worst;
}

/* ---- Service list ---- */

#define kSvcRowH 20
#define kSvcTop 27
static UInt16 gSvcRows[svtMaxPerVehicle];
static UInt8 gSvcStatus[svtMaxPerVehicle];
static UInt8 gSvcRowCount, gSvcFirst;
static RectangleType gSvcArea;
static UInt32 gSvcOdoNow, gSvcPace;

static void Collect(void)
{
    UInt16 n = DbCount(dbServiceTypes), i;
    static Int32 key[svtMaxPerVehicle];
    gSvcRowCount = 0;
    gSvcOdoNow = VehLastOdometer(gSvcVehicle);
    gSvcPace = Pace(gSvcVehicle);
    for (i = 0; i < n && gSvcRowCount < svtMaxPerVehicle; i++) {
        SvcDue d;
        Int32 k;
        UInt8 j;
        if (SvtGetAt(i, &gSvt) != errNone || gSvt.vehicle != gSvcVehicle || gSvt.archived) continue;
        DueFor(&gSvt, gSvcOdoNow, gSvcPace, &d);
        /* Urgency order: overdue, soon, OK by date, no visit, no interval. */
        switch (d.status) {
        case svcStatusOverdue: k = 0 + (Int32)d.due; break;
        case svcStatusSoon:    k = 100000L + (Int32)d.due; break;
        case svcStatusOK:      k = 200000L + (Int32)(d.due ? d.due : 65535); break;
        case svcStatusNoVisit: k = 400000L; break;
        default:               k = 500000L; break;
        }
        for (j = gSvcRowCount; j > 0 && key[j - 1] > k; j--) { key[j] = key[j - 1]; gSvcRows[j] = gSvcRows[j - 1]; gSvcStatus[j] = gSvcStatus[j - 1]; }
        key[j] = k; gSvcRows[j] = i; gSvcStatus[j] = d.status;
        gSvcRowCount++;
    }
}

static void DrawRightAt(const char *s, Coord right, Coord y) { WinDrawChars(s, StrLen(s), right - FntCharsWidth(s, StrLen(s)), y); }

/* A date with its year when it is not this year's: "Oct 23 2027". */
/* Section placement: see sections.h. */
static void DueDateText(char *b, UInt16 days) SVC_SEC;
static void SetRGB(UInt8 r, UInt8 g, UInt8 b) SVC_SEC;
static void DrawStatusBadge(UInt8 status, Coord x, Coord y) SVC_SEC;

static void DueDateText(char *b, UInt16 days)
{
    UInt16 y, ty; UInt8 m, d;
    DateDaysToYmd(days, &y, &m, &d);
    DateDaysToYmd(AppToday(), &ty, &m, &d);
    DateShortText(b, days);
    if (y != ty) { StrCat(b, " "); StrIToA(b + StrLen(b), y); }
}

/* Status as a 10-pixel badge, drawn at double density where the screen
 * allows: a green disc with a white tick, a yellow disc with a black
 * exclamation mark, a red disc with a white bar. x, y are standard
 * coordinates of the badge's top-left corner. */
static void SetRGB(UInt8 r, UInt8 g, UInt8 b)
{
    RGBColorType c;
    c.index = 0; c.r = r; c.g = g; c.b = b;
    WinSetForeColor(WinRGBToIndex(&c));
}

/* A filled disc from scanlines: the rounded-rectangle call clamps its corners. */
/* Section placement: see sections.h. */
static void PaintDisc(Coord cx, Coord cy, Coord radius) SVC_SEC;

static void PaintDisc(Coord cx, Coord cy, Coord radius)
{
    Coord dy;
    for (dy = (Coord)-radius; dy <= radius; dy++) {
        Int32 rem = (Int32)radius * radius - (Int32)dy * dy;
        Coord dx = 0;
        while ((Int32)(dx + 1) * (dx + 1) <= rem) dx++;
        WinPaintLine((Coord)(cx - dx), (Coord)(cy + dy), (Coord)(cx + dx), (Coord)(cy + dy));
    }
}

static void DrawStatusBadge(UInt8 status, Coord x, Coord y)
{
    RectangleType r;
    Coord s = DeviceHasHiRes() ? 2 : 1;
    UInt16 oldSys = 0;
    WinPushDrawState();
    if (s == 2) oldSys = WinSetCoordinateSystem(kCoordinatesDouble);
    if (status == svcStatusOK) SetRGB(0x00, 0x99, 0x33);
    else if (status == svcStatusSoon) SetRGB(0xFF, 0xCC, 0x00);
    else SetRGB(0xDD, 0x22, 0x22);
    PaintDisc((Coord)((x + 5) * s), (Coord)((y + 5) * s), (Coord)(5 * s - 1));
    if (status == svcStatusOK) {
        SetRGB(0xFF, 0xFF, 0xFF);
        WinPaintLine((Coord)((x + 2) * s), (Coord)((y + 5) * s), (Coord)((x + 4) * s), (Coord)((y + 7) * s));
        WinPaintLine((Coord)((x + 4) * s), (Coord)((y + 7) * s), (Coord)((x + 8) * s), (Coord)((y + 3) * s));
        if (s == 2) {
            WinPaintLine((Coord)((x + 2) * s), (Coord)((y + 5) * s + 1), (Coord)((x + 4) * s), (Coord)((y + 7) * s + 1));
            WinPaintLine((Coord)((x + 4) * s), (Coord)((y + 7) * s + 1), (Coord)((x + 8) * s), (Coord)((y + 3) * s + 1));
        }
    } else if (status == svcStatusSoon) {
        SetRGB(0x00, 0x00, 0x00);
        r.topLeft.x = (Coord)((x + 4) * s + (s == 2 ? 1 : 0)); r.topLeft.y = (Coord)((y + 2) * s); r.extent.x = (Coord)(s); r.extent.y = (Coord)(4 * s);
        WinPaintRectangle(&r, 0);
        r.topLeft.y = (Coord)((y + 7) * s); r.extent.y = (Coord)(s);
        WinPaintRectangle(&r, 0);
    } else {
        SetRGB(0xFF, 0xFF, 0xFF);
        r.topLeft.x = (Coord)((x + 2) * s); r.topLeft.y = (Coord)((y + 4) * s); r.extent.x = (Coord)(6 * s); r.extent.y = (Coord)(2 * s);
        WinPaintRectangle(&r, 0);
    }
    if (s == 2) WinSetCoordinateSystem(oldSys);
    WinPopDrawState();
}

static void DrawService(void)
{
    char b[48], n[20];
    Coord y = kSvcTop;
    UInt8 row;
    UInt16 date;
    UInt32 odo;
    RectangleType r;

    /* Header line and the area between it and the footer. */
    r.topLeft.x = 0; r.topLeft.y = 15; r.extent.x = 160; r.extent.y = (Coord)(gSvcArea.topLeft.y + gSvcArea.extent.y + 12 - 15);
    WinEraseRectangle(&r, 0);
    FntSetFont(stdFont);
    StrCopy(b, "Odometer "); DistText(n, gSvcOdoNow, gThousands); StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit));
    if (gSvcPace) { StrCat(b, ", pace "); DistText(n, gSvcPace, 0); StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); StrCat(b, "/day"); }
    WinDrawChars(b, StrLen(b), 4, 15);
    if (gSvcRowCount == 0) {
        WinDrawChars("No services for this vehicle.", 29, 6, y + 20);
    }
    if (gSvcFirst >= gSvcRowCount) gSvcFirst = 0;
    for (row = gSvcFirst; row < gSvcRowCount && y + kSvcRowH <= gSvcArea.topLeft.y + gSvcArea.extent.y; row++, y += kSvcRowH) {
        SvcDue d;
        const char *status;
        if (SvtGetAt(gSvcRows[row], &gSvt) != errNone) continue;
        DueFor(&gSvt, gSvcOdoNow, gSvcPace, &d);
        FntSetFont(boldFont);
        WinDrawChars(gSvt.name, StrLen(gSvt.name), 4, y);
        if (d.status == svcStatusOK || d.status == svcStatusSoon || d.status == svcStatusOverdue) {
            DrawStatusBadge(d.status, 140, y);
            FntSetFont(stdFont);
        } else {
            status = d.status == svcStatusNoVisit ? "no visit" : "set up";
            FntSetFont(stdFont);
            DrawRightAt(status, 150, y);
        }
        if (d.due) {
            if (d.distDue && (!d.hasTime || d.distDue <= d.timeDue)) { StrCopy(b, "about "); DueDateText(b + 6, d.due); }
            else DueDateText(b, d.due);
            DrawRightAt(b, 136, y);
        }
        /* The second line says why. */
        b[0] = 0;
        if (d.status == svcStatusNoInterval) StrCopy(b, "tap to set the interval");
        else if (d.status == svcStatusNoVisit) StrCopy(b, "no visit logged yet");
        else if (d.hasDist) {
            if (d.kmLeftX10 >= 0) { DistText(n, (UInt32)d.kmLeftX10, gThousands); StrCopy(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); StrCat(b, " left of "); }
            else { DistText(n, (UInt32)(-d.kmLeftX10), gThousands); StrCopy(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); StrCat(b, " over "); }
            DistText(n, gSvt.intervalDist, gThousands); StrCat(b, n);
            if (d.hasTime) { StrCat(b, " or "); StrIToA(n, gSvt.intervalMonths); StrCat(b, n); StrCat(b, " mo"); }
        } else {
            StrCopy(b, "every "); StrIToA(n, gSvt.intervalMonths); StrCat(b, n); StrCat(b, " months, last ");
            if (LastVisit(gSvt.vehicle, gSvt.id, &date, &odo)) DateShortText(b + StrLen(b), date);
        }
        WinDrawChars(b, StrLen(b), 10, y + 10);
    }
    /* Footer: the latest visit of any kind. */
    FntSetFont(stdFont);
    y = (Coord)(gSvcArea.topLeft.y + gSvcArea.extent.y + 1);
    if (LastVisit(gSvcVehicle, 255, &date, &odo)) {
        StrCopy(b, "Last visit "); DateShortText(b + StrLen(b), date); StrCat(b, " at ");
        DistText(n, odo, gThousands); StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit));
    } else StrCopy(b, "No visits logged yet");
    WinDrawChars(b, StrLen(b), 4, y);
    {
        UInt16 visible = (UInt16)(gSvcArea.extent.y / kSvcRowH);
        AppListScrollBar(FrmGetActiveForm(), ServiceScrollBar, (Int16)gSvcFirst, (Int16)((Int16)gSvcRowCount - (Int16)visible), (Int16)visible,
                         gSvcArea.topLeft.y, (Coord)(gSvcArea.topLeft.y + gSvcArea.extent.y));
    }
}

static void LayoutService(FormType *form)
{
    static UInt16 bottom[] = { ServiceLogButton, ServiceNewButton, ServiceHistoryButton, ServiceDoneButton };
    Coord w, h;
    AppFitForm(form, bottom, 4);
    WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
    gSvcArea.topLeft.x = 0; gSvcArea.topLeft.y = kSvcTop; gSvcArea.extent.x = w;
    gSvcArea.extent.y = (Coord)(((h - 17 - 12 - kSvcTop) / kSvcRowH) * kSvcRowH);
}

static void BuildServicePopup(FormType *form)
{
    ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, ServiceVehicleList));
    Int16 sel;
    VehBuildChoices(lst, gSVNames, gSVPtrs, gSVIds, false, gSvcVehicle, gSvcVehicle, &sel);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, ServiceVehicleTrigger)), gSVNames[sel]);
}

Boolean ServiceFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        gSvcVehicle = SvcVehicleNow();
        InstallPresets(gSvcVehicle);
        gSvcFirst = 0;
        LayoutService(form);
        BuildServicePopup(form);
        Collect();
        FrmDrawForm(form);
        DrawService();
        return true;
    case frmUpdateEvent: FrmDrawForm(form); DrawService(); return true;
    case winDisplayChangedEvent: LayoutService(form); WinEraseWindow(); FrmDrawForm(form); DrawService(); return true;
    case penDownEvent:
        if (RctPtInRectangle(event->screenX, event->screenY, &gSvcArea)) {
            UInt16 row = (UInt16)((event->screenY - gSvcArea.topLeft.y) / kSvcRowH + gSvcFirst);
            if (row < gSvcRowCount) {
                UInt16 date; UInt32 odo;
                gEditSvtIndex = gSvcRows[row];
                if (SvtGetAt(gEditSvtIndex, &gSvt) == errNone && LastVisit(gSvt.vehicle, gSvt.id, &date, &odo)) { gHistType = gEditSvtIndex; FrmGotoForm(HistoryForm); }
                else FrmGotoForm(EditServiceForm);
                return true;
            }
        }
        break;
    case sclRepeatEvent: case sclExitEvent: {
        Int16 v;
        if (AppListScrollEvent(event, &v)) { gSvcFirst = (UInt8)(v < 0 ? 0 : v); DrawService(); return true; }
        break;
    }
    case keyDownEvent: {
        UInt16 visible = (UInt16)(gSvcArea.extent.y / kSvcRowH);
        WChar c = event->data.keyDown.chr;
        Boolean page;
        Int16 dir = AppScrollDirection(event, &page);
        UInt16 step = page ? visible : 1;
        (void)c;
        if (dir > 0 && gSvcFirst + visible < gSvcRowCount) { gSvcFirst = (UInt8)(gSvcFirst + step); DrawService(); return true; }
        if (dir < 0 && gSvcFirst > 0) { gSvcFirst = (UInt8)(gSvcFirst > step ? gSvcFirst - step : 0); DrawService(); return true; }
        break;
    }
    case popSelectEvent:
        if (event->data.popSelect.listID == ServiceVehicleList) {
            gSvcVehicle = gSVIds[event->data.popSelect.selection];
            gPrefs.currentVehicle = gSvcVehicle;
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, ServiceVehicleTrigger)), gSVNames[event->data.popSelect.selection]);
            InstallPresets(gSvcVehicle);
            gSvcFirst = 0; Collect(); DrawService();
            return true;
        }
        break;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case ServiceLogButton: gEditSvlIndex = dmMaxRecordIndex; gSvlReturn = ServiceForm; FrmGotoForm(LogServiceForm); return true;
        case ServiceHistoryButton: gHistType = dmMaxRecordIndex; FrmGotoForm(HistoryForm); return true;
        case ServiceNewButton:
            if (NextTypeId(gSvcVehicle) == 255) { FrmAlert(TooManyServicesAlert); return true; }
            gEditSvtIndex = dmMaxRecordIndex; FrmGotoForm(EditServiceForm); return true;
        case ServiceDoneButton: FrmGotoForm(FillListForm); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- Service type ---- */

static void Summary(FormType *form)
{
    char b[44], n[20];
    SvcDue d;
    UInt32 odoNow = VehLastOdometer(gSvcVehicle), pace = Pace(gSvcVehicle);
    UInt16 date = 0; UInt32 odo = 0;
    Boolean have = LastVisit(gSvcVehicle, gSvt.id, &date, &odo);
    if (gEditSvtIndex == dmMaxRecordIndex || (gSvt.intervalDist == 0 && gSvt.intervalMonths == 0)) {
        AppSetLabel(form, SvtSummary1, "Set a distance, a time, or both;");
        AppSetLabel(form, SvtSummary2, "whichever comes first is due.");
        AppSetLabel(form, SvtSummary3, "Blank means no limit of that kind.");
        return;
    }
    if (!have) {
        AppSetLabel(form, SvtSummary1, "No visit logged yet: tap Log on");
        AppSetLabel(form, SvtSummary2, "the Service screen with the last");
        AppSetLabel(form, SvtSummary3, "date and odometer to start.");
        return;
    }
    SvcCompute(&gSvt, 1, date, odo, odoNow, AppToday(), pace, &d);
    StrCopy(b, "Due");
    if (d.hasDist) { StrCat(b, " at "); DistText(n, odo + gSvt.intervalDist, gThousands); StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); }
    if (d.hasTime) { StrCat(b, d.hasDist ? " or " : " "); DateShortText(b + StrLen(b), d.timeDue); }
    AppSetLabel(form, SvtSummary1, b);
    if (d.hasDist && pace) {
        StrCopy(b, "At "); DistText(n, pace, 0); StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); StrCat(b, "/day that is about ");
        DateShortText(b + StrLen(b), d.distDue);
    } else if (d.hasDist) StrCopy(b, "No pace yet: log a few fills.");
    else b[0] = 0;
    AppSetLabel(form, SvtSummary2, b);
    if (d.hasDist) {
        { UInt32 dueOdo = odo + gSvt.intervalDist; StrCopy(b, "Warning starts at "); DistText(n, dueOdo > gSvt.warnDist ? dueOdo - gSvt.warnDist : 0, gThousands); } StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit));
    } else { StrCopy(b, "Warning starts "); StrIToA(n, gSvt.warnDays); StrCat(b, n); StrCat(b, " days ahead"); }
    AppSetLabel(form, SvtSummary3, b);
}

static void LoadSvt(FormType *form)
{
    char b[20];
    Boolean isNew = gEditSvtIndex == dmMaxRecordIndex;
    if (isNew || SvtGetAt(gEditSvtIndex, &gSvt) != errNone) {
        gEditSvtIndex = dmMaxRecordIndex; isNew = true;
        MemSet(&gSvt, sizeof gSvt, 0);
        gSvt.vehicle = gSvcVehicle; gSvt.id = NextTypeId(gSvcVehicle); gSvt.warnDays = 14;
    }
    FrmSetTitle(form, isNew ? "New Service" : "Service Type");
    AppSetFieldText(form, SvtNameField, gSvt.name);
    if (gSvt.intervalDist) { DistText(b, gSvt.intervalDist, 0); AppSetFieldText(form, SvtDistField, b); } else AppSetFieldText(form, SvtDistField, "");
    AppSetLabel(form, SvtDistUnitLabel, UnitDistLabel(gPrefs.distanceUnit));
    if (gSvt.intervalMonths) { StrIToA(b, gSvt.intervalMonths); AppSetFieldText(form, SvtMonthsField, b); } else AppSetFieldText(form, SvtMonthsField, "");
    if (gSvt.warnDist) { DistText(b, gSvt.warnDist, 0); AppSetFieldText(form, SvtWarnDistField, b); } else AppSetFieldText(form, SvtWarnDistField, "");
    StrCopy(b, UnitDistLabel(gPrefs.distanceUnit)); StrCat(b, " before"); AppSetLabel(form, SvtWarnUnitLabel, b);
    StrIToA(b, gSvt.warnDays); AppSetFieldText(form, SvtWarnDaysField, b);
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvtAlarmCheckbox)), gSvt.alarm);
    AppSetFieldText(form, SvtDetailField, gSvt.defaultDetail);
    Summary(form);
    if (isNew) FrmHideObject(form, FrmGetObjectIndex(form, SvtArchiveButton));
    else {
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvtArchiveButton)),
                    gSvt.archived ? "Restore" : (VisitsCovering(gSvt.vehicle, gSvt.id) ? "Archive" : "Delete"));
        FrmShowObject(form, FrmGetObjectIndex(form, SvtArchiveButton));
    }
    FrmSetFocus(form, FrmGetObjectIndex(form, SvtNameField));
}

static Boolean SaveSvt(FormType *form)
{
    UInt32 v;
    Int32 r;
    Err err;
    const char *name = AppGetFieldText(form, SvtNameField);
    if (!name[0]) { FrmAlert(NeedServiceNameAlert); return false; }
    StrNCopy(gSvt.name, name, svtNameLen - 1); gSvt.name[svtNameLen - 1] = 0;
    r = TextParseFixed(AppGetFieldText(form, SvtDistField), 1, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gSvt.intervalDist = r == 1 ? UnitDistFromDisplay(v, gPrefs.distanceUnit) : 0;
    r = TextParseFixed(AppGetFieldText(form, SvtMonthsField), 0, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    if (r == 1 && v > 255) { FrmAlert(BadNumberAlert); return false; }
    gSvt.intervalMonths = (UInt8)(r == 1 ? v : 0);
    r = TextParseFixed(AppGetFieldText(form, SvtWarnDistField), 1, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gSvt.warnDist = r == 1 ? UnitDistFromDisplay(v, gPrefs.distanceUnit) : SvcDefaultWarnDist(gSvt.intervalDist);
    r = TextParseFixed(AppGetFieldText(form, SvtWarnDaysField), 0, gPoint, gThousands, &v);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gSvt.warnDays = (UInt16)(r == 1 ? v : 14);
    gSvt.alarm = (UInt8)CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvtAlarmCheckbox)));
    StrNCopy(gSvt.defaultDetail, AppGetFieldText(form, SvtDetailField), svtDetailLen - 1); gSvt.defaultDetail[svtDetailLen - 1] = 0;
    if (gEditSvtIndex == dmMaxRecordIndex) err = DbAdd(dbServiceTypes, gSvcPacked, ServiceTypePack(&gSvt, gSvcPacked), NULL);
    else err = DbReplace(dbServiceTypes, gEditSvtIndex, gSvcPacked, ServiceTypePack(&gSvt, gSvcPacked));
    if (err) { FrmAlert(SaveFailedAlert); return false; }
    SvcScheduleAlarm();
    return true;
}

Boolean EditServiceFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 bottom[] = { SvtDoneButton, SvtCancelButton, SvtArchiveButton };
    static UInt16 pinned[] = { SvtDoneButton, SvtCancelButton, SvtArchiveButton, SvtScrollBar };
    switch (event->eType) {
    case frmOpenEvent:
        AppScrollReset();
        AppFitForm(form, bottom, 3);
        LoadSvt(form);
        AppScrollBarUpdate(form, SvtScrollBar, pinned, 4);
        FrmDrawForm(form);
        return true;
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case fldEnterEvent: AppScrollToField(form, event->data.fldEnter.fieldID, pinned, 4); AppScrollBarUpdate(form, SvtScrollBar, pinned, 4); break;
    case keyDownEvent: case sclRepeatEvent: case sclExitEvent:
        if (AppScrollEvent(form, event, pinned, 4, SvtScrollBar)) return true;
        break;
    case winDisplayChangedEvent:
        if (gScrollYPublic()) AppScrollBy(form, gScrollYPublic(), pinned, 4);
        AppFitForm(form, bottom, 3); AppScrollBarUpdate(form, SvtScrollBar, pinned, 4); WinEraseWindow(); FrmDrawForm(form); return true;
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case SvtDoneButton: if (SaveSvt(form)) FrmGotoForm(ServiceForm); return true;
        case SvtCancelButton: FrmGotoForm(ServiceForm); return true;
        case SvtArchiveButton:
            if (gEditSvtIndex == dmMaxRecordIndex) return true;
            if (gSvt.archived) gSvt.archived = 0;
            else if (VisitsCovering(gSvt.vehicle, gSvt.id)) gSvt.archived = 1;
            else {
                if (FrmAlert(DeleteServiceAlert) != 0) return true;
                DbRemove(dbServiceTypes, gEditSvtIndex);
                FrmGotoForm(ServiceForm);
                return true;
            }
            if (DbReplace(dbServiceTypes, gEditSvtIndex, gSvcPacked, ServiceTypePack(&gSvt, gSvcPacked))) FrmAlert(SaveFailedAlert);
            FrmGotoForm(ServiceForm);
            return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- Log service ---- */

#define kSvlChecks 12
static UInt16 gSvlTypeIndex[kSvlChecks];   /* database index behind each checkbox */
static UInt8 gSvlTypeId[kSvlChecks];
static char gSvlNames[kSvlChecks][svtNameLen];
static UInt8 gSvlCount;
static UInt16 gSvlDate;
static char gSvlDateText[12];
static UInt16 gSvlClamp[kSvlChecks + 13];
static UInt16 gSvlClampCount;

static void PlaceSvl(FormType *form)
{
    static UInt16 bottom[] = { SvlDoneButton, SvlCancelButton, SvlNoteButton, SvlDeleteButton };
    Coord y = 56;
    UInt8 i;
    Coord x;
    AppFitForm(form, bottom, 4);
    gSvlClampCount = 0;
    for (i = 0; i < kSvlChecks; i++) {
        UInt16 idx = FrmGetObjectIndex(form, SvlCheck0 + i);
        if (i < gSvlCount) { FrmGetObjectPosition(form, idx, &x, &y); FrmSetObjectPosition(form, idx, x, (Coord)(56 + i * 12)); FrmShowObject(form, idx); gSvlClamp[gSvlClampCount++] = SvlCheck0 + i; }
        else { FrmGetObjectPosition(form, idx, &x, &y); FrmSetObjectPosition(form, idx, x, 56); FrmHideObject(form, idx); }   /* parked, so it adds no scroll room */
    }
    y = (Coord)(56 + gSvlCount * 12);
    {
        static UInt16 rows[][3] = {
            { SvlOtherCheckbox, SvlOtherField, 0 }, { SvlCostLabel, SvlCostField, SvlCurrencyTrigger },
            { SvlPaidLabel, SvlCashButton, SvlCardButton }, { SvlShopLabel, SvlShopField, 0 },
            { SvlDetailLabel, SvlDetailField, 0 }, { SvlNextLabel, 0, 0 } };
        UInt8 r, k;
        for (r = 0; r < 6; r++, y += 13) {
            for (k = 0; k < 3; k++) {
                UInt16 idx;
                Coord ox, oy;
                if (!rows[r][k]) continue;
                idx = FrmGetObjectIndex(form, rows[r][k]);
                FrmGetObjectPosition(form, idx, &ox, &oy);
                FrmSetObjectPosition(form, idx, ox, (Coord)(y + (rows[r][k] == SvlCurrencyTrigger ? 0 : 0)));
                gSvlClamp[gSvlClampCount++] = rows[r][k];
            }
            if (r == 1) { UInt16 li = FrmGetObjectIndex(form, SvlCurrencyList); Coord lx, ly; FrmGetObjectPosition(form, li, &lx, &ly); FrmSetObjectPosition(form, li, lx, y); }
        }
    }
    AppShowByPosition(form, gSvlClamp, gSvlClampCount);
    {
        static UInt16 pinned[] = { SvlDoneButton, SvlCancelButton, SvlNoteButton, SvlDeleteButton, SvlScrollBar };
        AppScrollBarUpdate(form, SvlScrollBar, pinned, 5);
    }
}

static void NextLine(FormType *form)
{
    char b[44], n[20];
    UInt8 i;
    UInt32 odo = 0;
    b[0] = 0;
    if (TextParseFixed(AppGetFieldText(form, SvlOdoField), 1, gPoint, gThousands, &odo) == 1) odo = UnitDistFromDisplay(odo, gPrefs.distanceUnit);   /* DistText shows it in this unit */
    for (i = 0; i < gSvlCount; i++) {
        if (!CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlCheck0 + i)))) continue;
        if (SvtGetAt(gSvlTypeIndex[i], &gScanSvt) != errNone || (gScanSvt.intervalDist == 0 && gScanSvt.intervalMonths == 0)) continue;
        StrCopy(b, "Next "); StrNCopy(n, gScanSvt.name, 12); n[12] = 0; StrCat(b, n); StrCat(b, ":");
        if (gScanSvt.intervalDist && odo) { StrCat(b, " "); DistText(n, odo + gScanSvt.intervalDist, gThousands); StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); }
        if (gScanSvt.intervalMonths) { StrCat(b, gScanSvt.intervalDist && odo ? " or " : " "); DateShortText(b + StrLen(b), DateAddMonths(gSvlDate, gScanSvt.intervalMonths)); }
        break;
    }
    AppSetLabel(form, SvlNextLabel, b);
}

static void LoadSvl(FormType *form)
{
    char b[20];
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt32 odo;
    Boolean editing = gEditSvlIndex != dmMaxRecordIndex && SvlGetAt(gEditSvlIndex, &gSvl) == errNone;
    if (editing) {
        gSvcVehicle = gSvl.vehicle;
        VehGet(gSvcVehicle, &gSvcVeh);
        gSvlDate = gSvl.date;
        odo = gSvl.odometer;
    } else {
        gEditSvlIndex = dmMaxRecordIndex;
        gSvcVehicle = SvcVehicleNow();
        VehGet(gSvcVehicle, &gSvcVeh);
        MemSet(&gSvl, sizeof gSvl, 0);
        gSvl.vehicle = gSvcVehicle;
        gSvl.currency = CurrencyForNewEntry(gSvcVeh.defaultCurrency, gPrefs.defaultCurrency, gPrefs.lastCurrency);
        gSvl.fxRate = CurrencyDefaultRate(gSvl.currency, gPrefs.homeCurrency, gPrefs.fxRate);
        gSvl.payment = gPrefs.lastPayment;
        gSvlDate = AppToday();
        odo = VehLastOdometer(gSvcVehicle);
    }
    FrmSetTitle(form, editing ? "Edit Visit" : "Log Service");
    DateShortText(gSvlDateText, gSvlDate);
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlDateTrigger)), gSvlDateText);
    if (odo) { DistText(b, odo, 0); AppSetFieldText(form, SvlOdoField, b); } else AppSetFieldText(form, SvlOdoField, "");
    AppSetLabel(form, SvlOdoHint, editing ? "" : (odo ? "from last fill" : ""));
    if (editing) FrmShowObject(form, FrmGetObjectIndex(form, SvlDeleteButton));
    else FrmHideObject(form, FrmGetObjectIndex(form, SvlDeleteButton));
    gSvlCount = 0;
    for (i = 0; i < n && gSvlCount < kSvlChecks; i++) {
        ControlType *c;
        if (SvtGetAt(i, &gScanSvt) != errNone || gScanSvt.vehicle != gSvcVehicle || gScanSvt.archived) continue;
        gSvlTypeIndex[gSvlCount] = i; gSvlTypeId[gSvlCount] = gScanSvt.id;
        StrCopy(gSvlNames[gSvlCount], gScanSvt.name);
        c = FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlCheck0 + gSvlCount));
        CtlSetLabel(c, gSvlNames[gSvlCount]);
        CtlSetValue(c, editing && (gSvl.services & (1UL << gScanSvt.id)) ? 1 : 0);
        gSvlCount++;
    }
    CtlSetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlOtherCheckbox)), editing && (gSvl.services & svlOtherBit) ? 1 : 0);
    AppSetFieldText(form, SvlOtherField, editing ? gSvl.other : "");
    if (editing && gSvl.cost) { TextFixed(b, (Int32)gSvl.cost, 2, gPoint, 0); AppSetFieldText(form, SvlCostField, b); }
    else AppSetFieldText(form, SvlCostField, "");
    {
        ListType *lst = FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlCurrencyList));
        LstSetSelection(lst, gSvl.currency);
        CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlCurrencyTrigger)), CurrencyCode(gSvl.currency));
    }
    FrmSetControlGroupSelection(form, 1, gSvl.payment == fillPaymentCard ? SvlCardButton : SvlCashButton);
    AppSetFieldText(form, SvlShopField, editing ? gSvl.shop : "");
    AppSetFieldText(form, SvlDetailField, editing ? gSvl.detail : "");
    CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlNoteButton)), gSvl.notes[0] ? "Note*" : "Note");
    AppSetLabel(form, SvlNextLabel, "");
    PlaceSvl(form);
    if (editing) NextLine(form);
}

static Boolean SaveSvl(FormType *form)
{
    UInt32 odo = 0, cents = 0;
    Int32 r;
    UInt8 i;
    Boolean other = CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlOtherCheckbox))) != 0;
    r = TextParseFixed(AppGetFieldText(form, SvlOdoField), 1, gPoint, gThousands, &odo);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gSvl.odometer = r == 1 ? UnitDistFromDisplay(odo, gPrefs.distanceUnit) : 0;
    /* Clear only the bits this form showed: archived or overflow types stay as they were. */
    for (i = 0; i < gSvlCount; i++) gSvl.services &= ~(1UL << gSvlTypeId[i]);
    gSvl.services &= ~svlOtherBit;
    for (i = 0; i < gSvlCount; i++)
        if (CtlGetValue(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlCheck0 + i)))) gSvl.services |= 1UL << gSvlTypeId[i];
    StrNCopy(gSvl.other, AppGetFieldText(form, SvlOtherField), svlOtherLen - 1); gSvl.other[svlOtherLen - 1] = 0;
    if (other && gSvl.other[0]) gSvl.services |= svlOtherBit; else gSvl.other[0] = 0;
    if (gSvl.services == 0) { FrmAlert(NothingDoneAlert); return false; }
    r = TextParseFixed(AppGetFieldText(form, SvlCostField), 2, gPoint, gThousands, &cents);
    if (r < 0) { FrmAlert(BadNumberAlert); return false; }
    gSvl.cost = r == 1 ? cents : 0;
    gSvl.date = gSvlDate;
    gSvl.currency = (UInt8)LstGetSelection(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlCurrencyList)));
    gSvl.fxRate = CurrencyDefaultRate(gSvl.currency, gPrefs.homeCurrency, gPrefs.fxRate);
    gSvl.payment = AppGroupSelection(form, 1) == SvlCardButton ? fillPaymentCard : fillPaymentCash;
    StrNCopy(gSvl.shop, AppGetFieldText(form, SvlShopField), svlShopLen - 1); gSvl.shop[svlShopLen - 1] = 0;
    StrNCopy(gSvl.detail, AppGetFieldText(form, SvlDetailField), svlDetailLen - 1); gSvl.detail[svlDetailLen - 1] = 0;
    if (gEditSvlIndex != dmMaxRecordIndex) { DbRemove(dbServiceVisits, gEditSvlIndex); gEditSvlIndex = dmMaxRecordIndex; }
    if (DbAdd(dbServiceVisits, gSvcPacked, ServiceVisitPack(&gSvl, gSvcPacked), NULL)) { FrmAlert(SaveFailedAlert); return false; }
    gPrefs.lastPayment = gSvl.payment;
    PrefsSave(&gPrefs);
    SvcScheduleAlarm();
    return true;
}

Boolean LogServiceFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    static UInt16 pinned[] = { SvlDoneButton, SvlCancelButton, SvlNoteButton, SvlDeleteButton, SvlScrollBar };
    switch (event->eType) {
    case frmOpenEvent:
        AppScrollReset();
        LoadSvl(form);
        FrmDrawForm(form);
        return true;
    case frmUpdateEvent: FrmDrawForm(form); return true;
    case fldEnterEvent:
        AppScrollToField(form, event->data.fldEnter.fieldID, pinned, 5);
        AppShowByPosition(form, gSvlClamp, gSvlClampCount);
        AppScrollBarUpdate(form, SvlScrollBar, pinned, 5);
        break;
    case keyDownEvent: case sclRepeatEvent: case sclExitEvent:
        if (AppScrollEvent(form, event, pinned, 5, SvlScrollBar)) { AppShowByPosition(form, gSvlClamp, gSvlClampCount); FrmDrawForm(form); return true; }
        break;
    case winDisplayChangedEvent:
        if (gScrollYPublic()) AppScrollBy(form, gScrollYPublic(), pinned, 5);
        PlaceSvl(form); WinEraseWindow(); FrmDrawForm(form); return true;
    case popSelectEvent:
        if (event->data.popSelect.listID == SvlCurrencyList) {
            gSvl.currency = (UInt8)event->data.popSelect.selection;
        }
        break;
    case ctlSelectEvent: {
        UInt16 id = event->data.ctlSelect.controlID;
        if (id >= SvlCheck0 && id < SvlCheck0 + kSvlChecks) { NextLine(form); break; }
        switch (id) {
        case SvlDateTrigger: {
            UInt16 y; UInt8 m, d; Int16 yy, mm, dd;
            DateDaysToYmd(gSvlDate, &y, &m, &d);
            yy = (Int16)y; mm = m; dd = d;
            if (SelectDay(selectDayByDay, &mm, &dd, &yy, "Service date")) {
                gSvlDate = DateYmdToDays((UInt16)yy, (UInt8)mm, (UInt8)dd);
                DateShortText(gSvlDateText, gSvlDate);
                CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlDateTrigger)), gSvlDateText);
                NextLine(form);
            }
            return true;
        }
        case SvlDoneButton: if (SaveSvl(form)) FrmGotoForm(gSvlReturn); return true;
        case SvlCancelButton: FrmGotoForm(gSvlReturn); return true;
        case SvlDeleteButton:
            if (gEditSvlIndex != dmMaxRecordIndex && FrmAlert(DeleteVisitAlert) == 0) {
                DbRemove(dbServiceVisits, gEditSvlIndex);
                SvcScheduleAlarm();
                FrmGotoForm(gSvlReturn);
            }
            return true;
        case SvlNoteButton:
            AppNoteDialog(gSvl.notes);
            CtlSetLabel(FrmGetObjectPtr(form, FrmGetObjectIndex(form, SvlNoteButton)), gSvl.notes[0] ? "Note*" : "Note");
            return true;
        default: break;
        }
        break;
    }
    default: break;
    }
    return false;
}

/* ---- History ---- */

#define kHistMax 64
#define kHistRowH 20
#define kHistTop 63
static UInt16 gHistIdx[kHistMax];       /* visit database indexes, newest first */
static UInt32 gHistGapDist[kHistMax];   /* km x 10 since the visit before, 0 for the first */
static UInt16 gHistGapDays[kHistMax];
static UInt8  gHistOver[kHistMax];      /* 1 distance, 2 time, 3 both past the interval */
static UInt8  gHistCount, gHistFirst;
static UInt32 gHistLongest, gHistAvg, gHistCents;
static RectangleType gHistArea;

UInt32 SvcSpendCents(UInt8 vehicle)
{
    UInt16 n = DbCount(dbServiceVisits), i;
    UInt32 cents = 0;
    for (i = 0; i < n; i++) {
        if (SvlGetAt(i, &gScanSvl) != errNone) continue;
        if (vehicle != 255 && gScanSvl.vehicle != vehicle) continue;
        cents += CalcHomeCents(gScanSvl.cost, gScanSvl.fxRate ? gScanSvl.fxRate : fxOne);
    }
    return cents;
}

/* Visits of the vehicle (covering the chosen service) oldest first, with the gap to each
 * from the one before; stored newest first for drawing. */
/* Section placement: see sections.h. */
static void CollectHistory(void) SVC_SEC;
static void DrawHistory(void) SVC_SEC;
static void LayoutHistory(FormType *form) SVC_SEC;

static void CollectHistory(void)
{
    UInt16 n = DbCount(dbServiceVisits), i;
    UInt16 prevDate = 0; UInt32 prevOdo = 0, sumDist = 0; UInt8 gaps = 0;
    Boolean perType = gHistType != dmMaxRecordIndex && SvtGetAt(gHistType, &gSvt) == errNone;
    gHistCount = 0; gHistLongest = 0; gHistAvg = 0; gHistCents = 0;
    for (i = 0; i < n && gHistCount < kHistMax; i++) {
        UInt8 k, over = 0;
        if (SvlGetAt(i, &gScanSvl) != errNone || gScanSvl.vehicle != gSvcVehicle) continue;
        if (perType && !(gScanSvl.services & (1UL << gSvt.id))) continue;
        /* shift down: newest first */
        for (k = gHistCount; k > 0; k--) { gHistIdx[k] = gHistIdx[k - 1]; gHistGapDist[k] = gHistGapDist[k - 1]; gHistGapDays[k] = gHistGapDays[k - 1]; gHistOver[k] = gHistOver[k - 1]; }
        gHistIdx[0] = i;
        if (gHistCount > 0 && gScanSvl.odometer > prevOdo) {
            gHistGapDist[0] = gScanSvl.odometer - prevOdo;
            gHistGapDays[0] = (UInt16)(gScanSvl.date > prevDate ? gScanSvl.date - prevDate : 0);
            if (perType && gSvt.intervalDist && gHistGapDist[0] > gSvt.intervalDist) over |= 1;
            if (perType && gSvt.intervalMonths && SvcMonthsX10(gHistGapDays[0]) > (UInt16)gSvt.intervalMonths * 10) over |= 2;
            if (gHistGapDist[0] > gHistLongest) gHistLongest = gHistGapDist[0];
            sumDist += gHistGapDist[0]; gaps++;
        } else { gHistGapDist[0] = 0; gHistGapDays[0] = 0; }
        gHistOver[0] = over;
        gHistCents += CalcHomeCents(gScanSvl.cost, gScanSvl.fxRate ? gScanSvl.fxRate : fxOne);
        prevDate = gScanSvl.date; prevOdo = gScanSvl.odometer;
        gHistCount++;
    }
    if (gaps) gHistAvg = (sumDist + gaps / 2) / gaps;
}

static void DrawHistory(void)
{
    char b[48], n[20];
    Coord y;
    UInt8 row;
    RectangleType r;
    Boolean perType = gHistType != dmMaxRecordIndex && SvtGetAt(gHistType, &gSvt) == errNone;

    r.topLeft.x = 0; r.topLeft.y = 15; r.extent.x = 160; r.extent.y = (Coord)(gHistArea.topLeft.y + gHistArea.extent.y - 15);
    WinEraseRectangle(&r, 0);
    FntSetFont(boldFont);
    WinDrawChars(perType ? gSvt.name : "All services", perType ? StrLen(gSvt.name) : 12, 4, 15);
    FntSetFont(stdFont);
    if (perType && (gSvt.intervalDist || gSvt.intervalMonths)) {
        StrCopy(b, "every ");
        if (gSvt.intervalDist) { DistText(n, gSvt.intervalDist, gThousands); StrCat(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); }
        if (gSvt.intervalMonths) { StrCat(b, gSvt.intervalDist ? " or " : ""); StrIToA(n, gSvt.intervalMonths); StrCat(b, n); StrCat(b, " months"); }
    } else StrCopy(b, perType ? "no interval set" : "every visit for this vehicle");
    WinDrawChars(b, StrLen(b), 4, 26);
    /* Headlines: longest gap, average gap, spend over N visits. */
    FntSetFont(boldFont);
    if (gHistLongest) { DistText(b, gHistLongest, gThousands); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); } else StrCopy(b, "--");
    WinDrawChars(b, StrLen(b), 4, 38);
    if (gHistAvg) { DistText(b, gHistAvg, gThousands); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit)); } else StrCopy(b, "--");
    WinDrawChars(b, StrLen(b), 58, 38);
    StrCopy(b, "$"); TextFixed(b + 1, (Int32)gHistCents, 2, gPoint, gThousands);
    DrawRightAt(b, 150, 38);
    FntSetFont(stdFont);
    WinDrawChars("longest gap", 11, 4, 48);
    WinDrawChars("average", 7, 58, 48);
    StrIToA(b, gHistCount); StrCat(b, gHistCount == 1 ? " visit" : " visits");
    DrawRightAt(b, 150, 48);
    y = kHistTop;
    if (gHistCount == 0) { WinDrawChars("No visits yet. Tap Log on Service.", 34, 6, y + 10); }
    if (gHistFirst >= gHistCount) gHistFirst = 0;
    for (row = gHistFirst; row < gHistCount && y + kHistRowH <= gHistArea.topLeft.y + gHistArea.extent.y; row++, y += kHistRowH) {
        if (SvlGetAt(gHistIdx[row], &gScanSvl) != errNone) continue;
        FntSetFont(stdFont);
        DueDateText(b, gScanSvl.date);
        WinDrawChars(b, StrLen(b), 4, y);
        DistText(b, gScanSvl.odometer, gThousands);
        DrawRightAt(b, 96, y);
        if (gScanSvl.cost) { TextFixed(b, (Int32)gScanSvl.cost, 2, gPoint, gThousands); DrawRightAt(b, 150, y); }
        /* second line: the gap, flagged when it ran past the interval */
        if (gHistGapDist[row]) {
            FntSetFont(gHistOver[row] ? boldFont : stdFont);
            DistText(n, gHistGapDist[row], gThousands);
            StrCopy(b, n); StrCat(b, " "); StrCat(b, UnitDistLabel(gPrefs.distanceUnit));
            if (gHistGapDays[row]) { StrCat(b, ", "); TextFixed(n, (Int32)SvcMonthsX10(gHistGapDays[row]), 1, gPoint, 0); StrCat(b, n); StrCat(b, " months"); }
            if (gHistOver[row] == 3) StrCat(b, "; over both");
            else if (gHistOver[row] == 1) StrCat(b, "; over distance");
            else if (gHistOver[row] == 2) StrCat(b, "; over time");
        } else { FntSetFont(stdFont); StrCopy(b, row == gHistCount - 1 ? "first record" : ""); }
        WinDrawChars(b, StrLen(b), 10, y + 10);
        /* services covered, when showing every visit */
        if (!perType) {
            UInt8 cnt = 0, id;
            for (id = 0; id < 31; id++) if (gScanSvl.services & (1UL << id)) cnt++;
            if (gScanSvl.services & svlOtherBit) cnt++;
            StrIToA(n, cnt); StrCat(n, cnt == 1 ? " item" : " items");
            FntSetFont(stdFont);
            DrawRightAt(n, 150, y + 10);
        }
    }
    {
        UInt16 visible = (UInt16)(gHistArea.extent.y / kHistRowH);
        AppListScrollBar(FrmGetActiveForm(), HistScrollBar, (Int16)gHistFirst, (Int16)((Int16)gHistCount - (Int16)visible), (Int16)visible,
                         gHistArea.topLeft.y, (Coord)(gHistArea.topLeft.y + gHistArea.extent.y));
    }
}

static void LayoutHistory(FormType *form)
{
    static UInt16 bottom[] = { HistDoneButton, HistIntervalsButton };
    Coord w, h;
    AppFitForm(form, bottom, 2);
    WinGetDisplayExtent(&w, &h); h = AppFormHeight(form);
    gHistArea.topLeft.x = 0; gHistArea.topLeft.y = kHistTop; gHistArea.extent.x = w;
    gHistArea.extent.y = (Coord)(((h - 17 - kHistTop) / kHistRowH) * kHistRowH);
    if (gHistType == dmMaxRecordIndex) FrmHideObject(form, FrmGetObjectIndex(form, HistIntervalsButton));
    else FrmShowObject(form, FrmGetObjectIndex(form, HistIntervalsButton));
}

Boolean HistoryFormHandleEvent(EventType *event)
{
    FormType *form = FrmGetActiveForm();
    switch (event->eType) {
    case frmOpenEvent:
        gSvcVehicle = SvcVehicleNow();
        gHistFirst = 0;
        LayoutHistory(form);
        CollectHistory();
        FrmDrawForm(form);
        DrawHistory();
        return true;
    case frmUpdateEvent: FrmDrawForm(form); DrawHistory(); return true;
    case winDisplayChangedEvent: LayoutHistory(form); WinEraseWindow(); FrmDrawForm(form); DrawHistory(); return true;
    case penDownEvent:
        if (RctPtInRectangle(event->screenX, event->screenY, &gHistArea)) {
            UInt16 row = (UInt16)((event->screenY - gHistArea.topLeft.y) / kHistRowH + gHistFirst);
            if (row < gHistCount) { gEditSvlIndex = gHistIdx[row]; gSvlReturn = HistoryForm; FrmGotoForm(LogServiceForm); return true; }
        }
        break;
    case sclRepeatEvent: case sclExitEvent: {
        Int16 v;
        if (AppListScrollEvent(event, &v)) { gHistFirst = (UInt8)(v < 0 ? 0 : v); DrawHistory(); return true; }
        break;
    }
    case keyDownEvent: {
        UInt16 visible = (UInt16)(gHistArea.extent.y / kHistRowH);
        Boolean page;
        Int16 dir = AppScrollDirection(event, &page);
        UInt16 step = page ? visible : 1;
        if (dir > 0 && gHistFirst + visible < gHistCount) { gHistFirst = (UInt8)(gHistFirst + step); DrawHistory(); return true; }
        if (dir < 0 && gHistFirst > 0) { gHistFirst = (UInt8)(gHistFirst > step ? gHistFirst - step : 0); DrawHistory(); return true; }
        break;
    }
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case HistDoneButton: FrmGotoForm(ServiceForm); return true;
        case HistIntervalsButton: gEditSvtIndex = gHistType; FrmGotoForm(EditServiceForm); return true;
        default: break;
        }
        break;
    default: break;
    }
    return false;
}

/* ---- alarm ---- */

void SvcScheduleAlarm(void)
{
    UInt16 n = DbCount(dbServiceTypes), i, today = AppToday(), best = 0;
    UInt8 lastVehicle = 255;
    UInt32 odoNow = 0, pace = 0, seconds = 0;
    UInt16 card; LocalID dbId;
    for (i = 0; i < n; i++) {
        SvcDue d;
        if (SvtGetAt(i, &gSvt) != errNone || gSvt.archived || !gSvt.alarm) continue;
        if (gSvt.vehicle != lastVehicle) {
            if (!VehGet(gSvt.vehicle, &gSvcVeh) || gSvcVeh.archived) continue;
            lastVehicle = gSvt.vehicle; odoNow = VehLastOdometer(gSvt.vehicle); pace = Pace(gSvt.vehicle);
        }
        DueFor(&gSvt, odoNow, pace, &d);
        if (!d.due || d.due <= today) continue;      /* today or past: the list already says so */
        if (!best || d.due < best) best = d.due;
    }
    if (best) {
        DateTimeType dt;
        UInt16 y; UInt8 m, dd;
        DateDaysToYmd(best, &y, &m, &dd);
        MemSet(&dt, sizeof dt, 0);
        dt.year = (Int16)y; dt.month = m; dt.day = dd; dt.hour = 9;
        seconds = TimDateTimeToSeconds(&dt);
    }
    if (SysCurAppDatabase(&card, &dbId) == errNone) AlmSetAlarm(card, dbId, 0, seconds, true);
    if (gPrefs.nextAlarm != seconds) { gPrefs.nextAlarm = seconds; PrefsSave(&gPrefs); }
}

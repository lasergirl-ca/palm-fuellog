/* Service without the window (svcview.h), after src/service_form.c. */
#include "svcview.h"
#include "vehedit.h"
#include "../store/db.h"
#include "../../src/calc.h"
#include "../../src/units.h"
#include "../../src/currency.h"
#include "../../src/text.h"
#include "../../src/datecalc.h"
#include <string.h>

static ServiceTypeRecord gScanT, gT;
static ServiceVisitRecord gScanV;
static FillRecord gScanF;
static UInt8 gPacked[dbMaxRecord + 16];

static int GetType(UInt16 i, ServiceTypeRecord *t)
{
    UInt32 len;
    return DbGet(dbServiceTypes, i, gPacked, sizeof gPacked, &len) == dbOk && ServiceTypeUnpack(gPacked, len, t) == 0;
}

static int GetVisit(UInt16 i, ServiceVisitRecord *v)
{
    UInt32 len;
    return DbGet(dbServiceVisits, i, gPacked, sizeof gPacked, &len) == dbOk && ServiceVisitUnpack(gPacked, len, v) == 0;
}

static void Cp(char *d, const char *s, int n)
{
    int i;
    for (i = 0; i < n - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}

static int SameName(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z') x = (char)(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = (char)(y - 'A' + 'a');
        if (x != y) return 0;
    }
    return *a == *b;
}

/* Distance in the Prefs unit, whole units. */
static void DistText(char *b, UInt32 dkm, const FuelPrefs *p, char point, char thousands)
{
    TextFixed(b, (Int32)(UnitDistToDisplay(dkm, p->distanceUnit) / 10), 0, point, thousands);
}

static void DistUnit(char *b, UInt32 dkm, const FuelPrefs *p, char point, char thousands)
{
    DistText(b, dkm, p, point, thousands);
    strcat(b, " ");
    strcat(b, UnitDistLabel(p->distanceUnit));
}

/* ---- what is due ---- */

int SvcViewLastVisit(UInt8 vehicle, UInt8 typeId, UInt16 *date, UInt32 *odo)
{
    UInt16 n = DbCount(dbServiceVisits);
    /* Visits are sorted by date, so the latest is found from the end. */
    while (n--) {
        if (!GetVisit(n, &gScanV) || gScanV.vehicle != vehicle) continue;
        if (typeId != 255 && !(gScanV.services & (1UL << typeId))) continue;
        *date = gScanV.date; *odo = gScanV.odometer;
        return 1;
    }
    return 0;
}

UInt32 SvcViewPace(UInt8 vehicle, UInt16 today)
{
    UInt16 n = DbCount(dbFills), i, count = 0, allCount = 0;
    UInt16 firstDate = 0, lastDate = 0, allFirstDate = 0, allLastDate = 0;
    UInt32 firstOdo = 0, lastOdo = 0, allFirstOdo = 0, allLastOdo = 0;
    for (i = 0; i < n; i++) {
        if (DbGetFill(i, &gScanF) != dbOk || gScanF.vehicle != vehicle || gScanF.odometer == 0) continue;
        if (allCount++ == 0) { allFirstDate = gScanF.date; allFirstOdo = gScanF.odometer; }
        allLastDate = gScanF.date; allLastOdo = gScanF.odometer;
        if ((UInt32)gScanF.date + 90 < today) continue;
        if (count++ == 0) { firstDate = gScanF.date; firstOdo = gScanF.odometer; }
        lastDate = gScanF.date; lastOdo = gScanF.odometer;
    }
    /* A pace needs at least a week of driving behind it, or a weekend of fills reads as 1,000 km/day. */
    if (count >= 3 && lastDate >= firstDate + 7) return SvcPaceX10(firstOdo, lastOdo, firstDate, lastDate);
    if (allCount >= 2 && allLastDate >= allFirstDate + 7) return SvcPaceX10(allFirstOdo, allLastOdo, allFirstDate, allLastDate);
    return 0;
}

UInt16 SvcViewVisitsCovering(UInt8 vehicle, UInt8 typeId)
{
    UInt16 n = DbCount(dbServiceVisits), i, count = 0;
    for (i = 0; i < n; i++)
        if (GetVisit(i, &gScanV) && gScanV.vehicle == vehicle && (gScanV.services & (1UL << typeId))) count++;
    return count;
}

UInt8 SvcViewInstallPresets(UInt8 vehicle)
{
    VehicleRecord v;
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt8 k, count, made = 0;
    for (i = 0; i < n; i++) if (GetType(i, &gScanT) && gScanT.vehicle == vehicle) return 0;
    if (!DbVehicleById(vehicle, &v)) return 0;
    count = SvcPresetCount(v.powertrain);
    for (k = 0; k < count; k++) {
        SvcPreset(v.powertrain, k, &gScanT);
        gScanT.vehicle = vehicle;
        if (DbAppend(dbServiceTypes, gPacked, ServiceTypePack(&gScanT, gPacked)) == dbOk) made++;
    }
    return made;
}

UInt8 SvcViewNextTypeId(UInt8 vehicle)
{
    UInt32 used = 0;
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt8 id;
    for (i = 0; i < n; i++) if (GetType(i, &gScanT) && gScanT.vehicle == vehicle && gScanT.id < 31) used |= 1UL << gScanT.id;
    for (id = 0; id < 31; id++) if (!(used & (1UL << id))) return id;
    return 255;
}

UInt8 SvcViewVehicle(const FuelPrefs *p)
{
    VehicleRecord v;
    UInt16 n = DbCount(dbVehicles), i;
    if (p->currentVehicle != 255 && DbVehicleById(p->currentVehicle, &v)) return p->currentVehicle;
    for (i = 0; i < n; i++) if (DbGetVehicle(i, &v) == dbOk && !v.archived) return v.id;
    return 1;
}

static void DueFor(const ServiceTypeRecord *t, UInt32 odoNow, UInt32 pace, UInt16 today, SvcDue *d)
{
    UInt16 date = 0;
    UInt32 odo = 0;
    int have = SvcViewLastVisit(t->vehicle, t->id, &date, &odo);
    SvcCompute(t, (UInt8)have, date, odo, odoNow, today, pace, d);
}

void SvcViewDueDate(char *buf, UInt16 days, UInt16 today)
{
    UInt16 y, ty; UInt8 m, d;
    DateDaysToYmd(days, &y, &m, &d);
    DateDaysToYmd(today, &ty, &m, &d);
    DateShortText(buf, days);
    if (y != ty) { strcat(buf, " "); TextFixed(buf + strlen(buf), y, 0, '.', 0); }
}

/* "Oil and filter due in 640 km" / "Wiper blades overdue" / "Oil and filter due Nov 6". */
static void BannerText(const ServiceTypeRecord *t, const SvcDue *d, UInt16 today, const FuelPrefs *p, char point, char thousands, char *buf)
{
    char n[24];
    Cp(buf, t->name, 21);
    if (d->status == svcStatusOverdue) { strcat(buf, " overdue"); return; }
    if (d->hasDist && d->kmLeftX10 >= 0 && (!d->hasTime || d->distDue == 0 || d->distDue <= d->timeDue)) {
        DistUnit(n, (UInt32)d->kmLeftX10, p, point, thousands);
        strcat(buf, " due in "); strcat(buf, n);
    } else {
        strcat(buf, " due "); SvcViewDueDate(buf + strlen(buf), d->due, today);
    }
}

UInt8 SvcViewBanner(UInt8 vehicle, UInt16 today, const FuelPrefs *p, char point, char thousands, char *buf)
{
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt8 worst = svcStatusOK, lastVehicle = 255;
    Int32 worstKey = 0x7FFFFFFF;
    UInt32 odoNow = 0, pace = 0;
    buf[0] = 0;
    for (i = 0; i < n; i++) {
        VehicleRecord v;
        SvcDue d;
        Int32 key;
        if (!GetType(i, &gT) || gT.archived) continue;
        if (vehicle != 255 && gT.vehicle != vehicle) continue;
        if (gT.vehicle != lastVehicle) {
            if (!DbVehicleById(gT.vehicle, &v) || v.archived) continue;
            lastVehicle = gT.vehicle; odoNow = VehLastOdometer(gT.vehicle); pace = SvcViewPace(gT.vehicle, today);
        }
        DueFor(&gT, odoNow, pace, today, &d);
        if (d.status != svcStatusSoon && d.status != svcStatusOverdue) continue;
        key = (d.status == svcStatusOverdue ? -1000000L : 0) + (d.due ? (Int32)d.due : 0);
        if (key < worstKey) { worstKey = key; worst = d.status; BannerText(&gT, &d, today, p, point, thousands, buf); }
    }
    return worst;
}

UInt16 SvcViewNextAlarm(UInt16 today)
{
    UInt16 n = DbCount(dbServiceTypes), i, best = 0;
    UInt8 lastVehicle = 255;
    UInt32 odoNow = 0, pace = 0;
    for (i = 0; i < n; i++) {
        VehicleRecord v;
        SvcDue d;
        if (!GetType(i, &gT) || gT.archived || !gT.alarm) continue;
        if (gT.vehicle != lastVehicle) {
            if (!DbVehicleById(gT.vehicle, &v) || v.archived) continue;
            lastVehicle = gT.vehicle; odoNow = VehLastOdometer(gT.vehicle); pace = SvcViewPace(gT.vehicle, today);
        }
        DueFor(&gT, odoNow, pace, today, &d);
        if (!d.due || d.due <= today) continue;      /* today or past: the list already says so */
        if (!best || d.due < best) best = d.due;
    }
    return best;
}

/* ---- the Service list ---- */

UInt8 SvcViewList(UInt8 vehicle, UInt16 today, const FuelPrefs *p, char point, char thousands,
                  SvcRow *rows, UInt8 max, char *head, char *foot)
{
    UInt16 n = DbCount(dbServiceTypes), i;
    Int32 key[svcMaxRows];
    UInt32 odoNow = VehLastOdometer(vehicle), pace = SvcViewPace(vehicle, today), odo;
    UInt16 date;
    UInt8 count = 0, j;
    char b[64], num[24];

    strcpy(head, "Odo "); DistUnit(head + 4, odoNow, p, point, thousands);
    if (pace) { strcat(head, ", pace "); DistUnit(num, pace, p, point, 0); strcat(head, num); strcat(head, "/day"); }
    else strcat(head, ", pace after a week");
    if (SvcViewLastVisit(vehicle, 255, &date, &odo)) {
        strcpy(foot, "Last visit "); DateShortText(foot + strlen(foot), date); strcat(foot, " at ");
        DistUnit(foot + strlen(foot), odo, p, point, thousands);
    } else strcpy(foot, "No visits logged yet");

    if (max > svcMaxRows) max = svcMaxRows;
    for (i = 0; i < n && count < max; i++) {
        SvcDue d;
        SvcRow r;
        Int32 k;
        if (!GetType(i, &gT) || gT.vehicle != vehicle || gT.archived) continue;
        DueFor(&gT, odoNow, pace, today, &d);
        memset(&r, 0, sizeof r);
        r.dbIndex = i;
        r.hasVisit = (UInt8)SvcViewLastVisit(vehicle, gT.id, &date, &odo);
        Cp(r.left, gT.name, sizeof r.left);
        if (d.status == svcStatusOK || d.status == svcStatusSoon || d.status == svcStatusOverdue) r.badge = (UInt8)(1 + d.status);
        else strcpy(r.right, d.status == svcStatusNoVisit ? "no visit" : "set up");
        if (d.due) {
            if (d.distDue && (!d.hasTime || d.distDue <= d.timeDue)) { strcpy(r.mid, "about "); SvcViewDueDate(r.mid + 6, d.due, today); }
            else SvcViewDueDate(r.mid, d.due, today);
        }
        /* The second line says why. */
        if (d.status == svcStatusNoInterval) strcpy(b, "tap to set the interval");
        else if (d.status == svcStatusNoVisit) strcpy(b, "no visit logged yet");
        else if (d.hasDist) {
            if (d.kmLeftX10 >= 0) { DistUnit(b, (UInt32)d.kmLeftX10, p, point, thousands); strcat(b, " left of "); }
            else { DistUnit(b, (UInt32)(-d.kmLeftX10), p, point, thousands); strcat(b, " over "); }
            DistText(num, gT.intervalDist, p, point, thousands); strcat(b, num);
            if (d.hasTime) { strcat(b, " or "); TextFixed(num, gT.intervalMonths, 0, point, 0); strcat(b, num); strcat(b, " mo"); }
        } else {
            strcpy(b, "every "); TextFixed(num, gT.intervalMonths, 0, point, 0); strcat(b, num); strcat(b, " months, last ");
            if (r.hasVisit) DateShortText(b + strlen(b), date);
        }
        Cp(r.line2, b, sizeof r.line2);
        /* Urgency order: overdue, soon, OK by date, no visit, no interval. */
        switch (d.status) {
        case svcStatusOverdue: k = (Int32)d.due; break;
        case svcStatusSoon:    k = 100000L + (Int32)d.due; break;
        case svcStatusOK:      k = 200000L + (Int32)(d.due ? d.due : 65535); break;
        case svcStatusNoVisit: k = 400000L; break;
        default:               k = 500000L; break;
        }
        for (j = count; j > 0 && key[j - 1] > k; j--) { key[j] = key[j - 1]; rows[j] = rows[j - 1]; }
        key[j] = k; rows[j] = r;
        count++;
    }
    return count;
}

UInt8 SvcViewHistory(UInt8 vehicle, UInt16 typeIndex, UInt16 today, const FuelPrefs *p, char point, char thousands,
                     SvcRow *rows, UInt8 max, TextLine head[4], UInt8 *headCount)
{
    UInt16 n = DbCount(dbServiceVisits), i, prevDate = 0;
    UInt32 prevOdo = 0, sumDist = 0, longest = 0, cents = 0, avg = 0;
    UInt8 count = 0, gaps = 0, k;
    int perType = typeIndex != 0xFFFF && GetType(typeIndex, &gT) && gT.vehicle == vehicle;
    char b[64], num[24];

    if (max > svcMaxRows) max = svcMaxRows;
    for (i = 0; i < n && count < max; i++) {
        SvcRow r;
        UInt32 gapDist = 0;
        UInt16 gapDays = 0;
        UInt8 over = 0, items = 0, id;
        if (!GetVisit(i, &gScanV) || gScanV.vehicle != vehicle) continue;
        if (perType && !(gScanV.services & (1UL << gT.id))) continue;
        memset(&r, 0, sizeof r);
        r.dbIndex = i;
        SvcViewDueDate(r.left, gScanV.date, today);
        DistText(r.mid, gScanV.odometer, p, point, thousands);
        if (gScanV.cost) TextFixed(r.right, (Int32)gScanV.cost, 2, point, thousands);
        if (count > 0 && gScanV.odometer > prevOdo) {
            gapDist = gScanV.odometer - prevOdo;
            gapDays = (UInt16)(gScanV.date > prevDate ? gScanV.date - prevDate : 0);
            if (perType && gT.intervalDist && gapDist > gT.intervalDist) over |= 1;
            if (perType && gT.intervalMonths && SvcMonthsX10(gapDays) > (UInt16)gT.intervalMonths * 10) over |= 2;
            if (gapDist > longest) longest = gapDist;
            sumDist += gapDist; gaps++;
        }
        /* the second line: the gap, flagged when it ran past the interval */
        if (gapDist) {
            DistUnit(b, gapDist, p, point, thousands);
            if (gapDays) { strcat(b, ", "); TextFixed(num, (Int32)SvcMonthsX10(gapDays), 1, point, 0); strcat(b, num); strcat(b, " months"); }
            if (over == 3) strcat(b, "; over both");
            else if (over == 1) strcat(b, "; over distance");
            else if (over == 2) strcat(b, "; over time");
            Cp(r.line2, b, sizeof r.line2);
            r.bold2 = (UInt8)(over != 0);
        }
        if (!perType) {
            for (id = 0; id < 31; id++) if (gScanV.services & (1UL << id)) items++;
            if (gScanV.services & svlOtherBit) items++;
            TextFixed(r.right2, items, 0, point, 0); strcat(r.right2, items == 1 ? " item" : " items");
        }
        cents += CalcHomeCents(gScanV.cost, gScanV.fxRate ? gScanV.fxRate : fxOne);
        prevDate = gScanV.date; prevOdo = gScanV.odometer;
        /* newest first */
        for (k = count; k > 0; k--) rows[k] = rows[k - 1];
        rows[0] = r;
        count++;
    }
    if (count) strcpy(rows[count - 1].line2, "first record");
    if (gaps) avg = (sumDist + gaps / 2) / gaps;

    memset(head, 0, 4 * sizeof *head);
    head[0].style = tvHeading;
    Cp(head[0].cell[0], perType ? gT.name : "All services", tvText);
    head[1].style = tvNote;
    if (perType && (gT.intervalDist || gT.intervalMonths)) {
        strcpy(b, "every ");
        if (gT.intervalDist) DistUnit(b + strlen(b), gT.intervalDist, p, point, thousands);
        if (gT.intervalMonths) { strcat(b, gT.intervalDist ? " or " : ""); TextFixed(num, gT.intervalMonths, 0, point, 0); strcat(b, num); strcat(b, " months"); }
    } else strcpy(b, perType ? "no interval set" : "every visit for this vehicle");
    Cp(head[1].cell[0], b, tvText);
    /* Headlines: longest gap, average gap, spend over N visits. */
    head[2].style = tvTripleBold;
    if (longest) DistUnit(head[2].cell[0], longest, p, point, thousands); else strcpy(head[2].cell[0], "--");
    if (avg) DistUnit(head[2].cell[1], avg, p, point, thousands); else strcpy(head[2].cell[1], "--");
    strcpy(head[2].cell[2], "$"); TextFixed(head[2].cell[2] + 1, (Int32)cents, 2, point, thousands);
    head[3].style = tvTriple;
    strcpy(head[3].cell[0], "longest gap");
    strcpy(head[3].cell[1], "average");
    TextFixed(head[3].cell[2], count, 0, point, 0); strcat(head[3].cell[2], count == 1 ? " visit" : " visits");
    *headCount = 4;
    return count;
}

/* ---- Service Type ---- */

int SvtEditBegin(SvtEdit *e, UInt16 index, UInt8 vehicle, const FuelPrefs *p, char point, SvtForm *f)
{
    memset(e, 0, sizeof *e);
    memset(f, 0, sizeof *f);
    e->index = svtEditNew;
    if (index != svtEditNew && GetType(index, &e->t)) {
        e->index = index;
    } else {
        e->t.vehicle = vehicle;
        e->t.id = SvcViewNextTypeId(vehicle);
        if (e->t.id == 255) return 0;
        e->t.warnDays = 14;
    }
    strcpy(f->name, e->t.name);
    if (e->t.intervalDist) DistText(f->dist, e->t.intervalDist, p, point, 0);
    if (e->t.intervalMonths) TextFixed(f->months, e->t.intervalMonths, 0, point, 0);
    if (e->t.warnDist) DistText(f->warnDist, e->t.warnDist, p, point, 0);
    TextFixed(f->warnDays, e->t.warnDays, 0, point, 0);
    f->alarm = e->t.alarm ? 1 : 0;
    strcpy(f->detail, e->t.defaultDetail);
    return 1;
}

void SvtEditSummary(const SvtEdit *e, UInt16 today, const FuelPrefs *p, char point, char thousands, char lines[3][64])
{
    const ServiceTypeRecord *t = &e->t;
    char num[24];
    SvcDue d;
    UInt32 odoNow = VehLastOdometer(t->vehicle), pace = SvcViewPace(t->vehicle, today), odo = 0;
    UInt16 date = 0;
    int have = SvcViewLastVisit(t->vehicle, t->id, &date, &odo);
    if (e->index == svtEditNew || (t->intervalDist == 0 && t->intervalMonths == 0)) {
        strcpy(lines[0], "Set a distance, a time, or both;");
        strcpy(lines[1], "whichever comes first is due.");
        strcpy(lines[2], "Blank means no limit of that kind.");
        return;
    }
    if (!have) {
        strcpy(lines[0], "No visit logged yet: choose Log on");
        strcpy(lines[1], "the Service page with the last");
        strcpy(lines[2], "date and odometer to start.");
        return;
    }
    SvcCompute(t, 1, date, odo, odoNow, today, pace, &d);
    strcpy(lines[0], "Due");
    if (d.hasDist) { strcat(lines[0], " at "); DistUnit(num, odo + t->intervalDist, p, point, thousands); strcat(lines[0], num); }
    if (d.hasTime) { strcat(lines[0], d.hasDist ? " or " : " "); DateShortText(lines[0] + strlen(lines[0]), d.timeDue); }
    if (d.hasDist && pace) {
        strcpy(lines[1], "At "); DistUnit(num, pace, p, point, 0); strcat(lines[1], num); strcat(lines[1], "/day that is about ");
        DateShortText(lines[1] + strlen(lines[1]), d.distDue);
    } else if (d.hasDist) strcpy(lines[1], "No pace yet: log a few fills.");
    else lines[1][0] = 0;
    if (d.hasDist) {
        UInt32 dueOdo = odo + t->intervalDist;
        strcpy(lines[2], "Warning starts at ");
        DistUnit(num, dueOdo > t->warnDist ? dueOdo - t->warnDist : 0, p, point, thousands);
        strcat(lines[2], num);
    } else {
        strcpy(lines[2], "Warning starts "); TextFixed(num, t->warnDays, 0, point, 0); strcat(lines[2], num); strcat(lines[2], " days ahead");
    }
}

const char *SvtEditArchiveLabel(const SvtEdit *e)
{
    if (e->index == svtEditNew) return 0;
    if (e->t.archived) return "Restore";
    return SvcViewVisitsCovering(e->t.vehicle, e->t.id) ? "Archive" : "Delete";
}

Int32 SvtEditSave(SvtEdit *e, const SvtForm *f, const FuelPrefs *p, char point, char thousands)
{
    ServiceTypeRecord t = e->t;
    UInt32 v, n;
    Int32 r, err;
    const char *name = f->name;
    while (*name == ' ') name++;
    if (!name[0]) return seNeedName;
    Cp(t.name, name, svtNameLen);
    r = TextParseFixed(f->dist, 1, point, thousands, &v);
    if (r < 0) return seBadNumber;
    t.intervalDist = r == 1 ? UnitDistFromDisplay(v, p->distanceUnit) : 0;
    r = TextParseFixed(f->months, 0, point, thousands, &v);
    if (r < 0 || (r == 1 && v > 255)) return seBadNumber;
    t.intervalMonths = (UInt8)(r == 1 ? v : 0);
    r = TextParseFixed(f->warnDist, 1, point, thousands, &v);
    if (r < 0) return seBadNumber;
    t.warnDist = r == 1 ? UnitDistFromDisplay(v, p->distanceUnit) : SvcDefaultWarnDist(t.intervalDist);
    r = TextParseFixed(f->warnDays, 0, point, thousands, &v);
    if (r < 0 || (r == 1 && v > 65535)) return seBadNumber;
    t.warnDays = (UInt16)(r == 1 ? v : 14);
    t.alarm = f->alarm ? 1 : 0;
    Cp(t.defaultDetail, f->detail, svtDetailLen);
    n = ServiceTypePack(&t, gPacked);
    if (e->index == svtEditNew) {
        err = DbAppend(dbServiceTypes, gPacked, n);
        if (err == dbOk) e->index = (UInt16)(DbCount(dbServiceTypes) - 1);
    } else err = DbReplace(dbServiceTypes, e->index, gPacked, n);
    if (err != dbOk) return seSaveFailed;
    e->t = t;
    return seSaved;
}

Int32 SvtEditArchive(SvtEdit *e)
{
    ServiceTypeRecord t;
    Int32 err;
    if (e->index == svtEditNew || !GetType(e->index, &t)) return dbErrRange;
    if (!t.archived && !SvcViewVisitsCovering(t.vehicle, t.id)) {
        err = DbDelete(dbServiceTypes, e->index);
        if (err == dbOk) e->index = svtEditNew;
        return err;
    }
    t.archived = t.archived ? 0 : 1;
    err = DbReplace(dbServiceTypes, e->index, gPacked, ServiceTypePack(&t, gPacked));
    if (err == dbOk) e->t = t;
    return err;
}

/* ---- Log Service and Edit Visit ---- */

void SvlEditBegin(SvlEdit *e, UInt16 index, UInt8 vehicle, UInt16 today, const FuelPrefs *p, char point, SvlForm *f)
{
    VehicleRecord veh;
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt32 odo;
    UInt8 k;
    int editing = index != svtEditNew && GetVisit(index, &e->v);
    memset(f, 0, sizeof *f);
    if (editing) {
        e->index = index;
        e->vehicle = e->v.vehicle;
        f->date = e->v.date;
        odo = e->v.odometer;
        e->prefilledOdo = 0;
    } else {
        memset(&e->v, 0, sizeof e->v);
        e->index = svtEditNew;
        e->vehicle = vehicle;
        if (!DbVehicleById(vehicle, &veh)) { memset(&veh, 0, sizeof veh); veh.defaultCurrency = vehCurrencyUsePrefs; }
        e->v.vehicle = vehicle;
        e->v.currency = CurrencyForNewEntry(veh.defaultCurrency, p->defaultCurrency, p->lastCurrency);
        e->v.fxRate = CurrencyDefaultRate(e->v.currency, p->homeCurrency, p->fxRate);
        e->v.payment = p->lastPayment;
        f->date = today;
        odo = VehLastOdometer(vehicle);
        e->prefilledOdo = odo != 0;
    }
    if (odo) DistText(f->odo, odo, p, point, 0);
    e->count = 0;
    for (i = 0; i < n && e->count < svlChecks; i++) {
        if (!GetType(i, &gScanT) || gScanT.vehicle != e->vehicle || gScanT.archived) continue;
        e->typeId[e->count] = gScanT.id;
        Cp(e->names[e->count], gScanT.name, svtNameLen);
        f->checked[e->count] = (UInt8)(editing && (e->v.services & (1UL << gScanT.id)));
        e->count++;
    }
    for (k = e->count; k < svlChecks; k++) f->checked[k] = 0;
    f->otherOn = (UInt8)(editing && (e->v.services & svlOtherBit));
    if (editing) {
        Cp(f->other, e->v.other, svlOtherLen);
        if (e->v.cost) TextFixed(f->cost, (Int32)e->v.cost, 2, point, 0);
        Cp(f->shop, e->v.shop, svlShopLen);
        Cp(f->detail, e->v.detail, svlDetailLen);
        Cp(f->notes, e->v.notes, fillMaxNotes);
    }
    f->currency = e->v.currency < curCount ? e->v.currency : 0;
    f->payment = e->v.payment == fillPaymentCard ? fillPaymentCard : fillPaymentCash;
}

void SvlEditNextLine(const SvlEdit *e, const SvlForm *f, const FuelPrefs *p, char point, char thousands, char *buf)
{
    UInt16 n = DbCount(dbServiceTypes), i;
    UInt32 odo = 0;
    UInt8 k;
    char num[24];
    buf[0] = 0;
    if (TextParseFixed(f->odo, 1, point, thousands, &odo) == 1) odo = UnitDistFromDisplay(odo, p->distanceUnit);
    else odo = 0;
    for (k = 0; k < e->count; k++) {
        if (!f->checked[k]) continue;
        for (i = 0; i < n; i++) if (GetType(i, &gScanT) && gScanT.vehicle == e->vehicle && gScanT.id == e->typeId[k]) break;
        if (i == n || (gScanT.intervalDist == 0 && gScanT.intervalMonths == 0)) continue;
        strcpy(buf, "Next "); Cp(num, gScanT.name, 13); strcat(buf, num); strcat(buf, ":");
        if (gScanT.intervalDist && odo) { strcat(buf, " "); DistUnit(num, odo + gScanT.intervalDist, p, point, thousands); strcat(buf, num); }
        if (gScanT.intervalMonths) { strcat(buf, gScanT.intervalDist && odo ? " or " : " "); DateShortText(buf + strlen(buf), DateAddMonths(f->date, gScanT.intervalMonths)); }
        return;
    }
}

Int32 SvlEditSave(SvlEdit *e, const SvlForm *f, UInt8 otherChoice, FuelPrefs *p, char point, char thousands)
{
    ServiceVisitRecord v = e->v;
    UInt32 odo = 0, cents = 0;
    Int32 r;
    UInt8 k, other = f->otherOn, foundId = 255;
    UInt16 n = DbCount(dbServiceTypes), i, at;

    r = TextParseFixed(f->odo, 1, point, thousands, &odo);
    if (r < 0) return svBadNumber;
    v.odometer = r == 1 ? UnitDistFromDisplay(odo, p->distanceUnit) : 0;
    r = TextParseFixed(f->cost, 2, point, thousands, &cents);
    if (r < 0) return svBadNumber;
    v.cost = r == 1 ? cents : 0;
    /* Clear only the bits this form showed: archived or overflow types stay as they were. */
    for (k = 0; k < e->count; k++) v.services &= ~(1UL << e->typeId[k]);
    v.services &= ~svlOtherBit;
    for (k = 0; k < e->count; k++) if (f->checked[k]) v.services |= 1UL << e->typeId[k];
    Cp(v.other, f->other, svlOtherLen);
    if (other && v.other[0]) {
        /* One tap turns a one-off item into a tracked service type. */
        for (i = 0; i < n; i++) {
            if (!GetType(i, &gScanT) || gScanT.vehicle != e->vehicle) continue;
            if (SameName(gScanT.name, v.other)) foundId = gScanT.id;
        }
        if (foundId != 255) {                       /* it already is a type: tick that instead */
            v.services |= 1UL << foundId; v.other[0] = 0;
        } else {
            UInt8 nid = SvcViewNextTypeId(e->vehicle);
            if (nid != 255 && otherChoice == svOtherAsk) return svAskTrack;
            if (nid != 255 && otherChoice == svOtherTrack) {
                memset(&gScanT, 0, sizeof gScanT);
                gScanT.vehicle = e->vehicle; gScanT.id = nid; gScanT.warnDays = 14;
                Cp(gScanT.name, v.other, svtNameLen);
                if (DbAppend(dbServiceTypes, gPacked, ServiceTypePack(&gScanT, gPacked)) != dbOk) return svSaveFailed;
                v.services |= 1UL << nid; v.other[0] = 0;
            } else v.services |= svlOtherBit;
        }
    } else v.other[0] = 0;
    if (v.services == 0) return svNothing;
    v.date = f->date;
    v.clockShift = 0;           /* a Windows Mobile clock runs past 2031: established, no shift */
    v.currency = f->currency < curCount ? f->currency : 0;
    v.fxRate = CurrencyDefaultRate(v.currency, p->homeCurrency, p->fxRate);
    v.payment = f->payment == fillPaymentCard ? fillPaymentCard : fillPaymentCash;
    Cp(v.shop, f->shop, svlShopLen);
    Cp(v.detail, f->detail, svlDetailLen);
    Cp(v.notes, f->notes, fillMaxNotes);
    /* The edited visit goes in before the old one comes out, so a failed add loses nothing. */
    if (DbAdd(dbServiceVisits, gPacked, ServiceVisitPack(&v, gPacked), &at) != dbOk) return svSaveFailed;
    if (e->index != svtEditNew) DbRemove(dbServiceVisits, (UInt16)(at <= e->index ? e->index + 1 : e->index));
    e->v = v;
    e->index = svtEditNew;
    p->lastPayment = v.payment;
    return svSaved;
}

Int32 SvlEditDelete(SvlEdit *e)
{
    Int32 err;
    if (e->index == svtEditNew) return dbErrRange;
    err = DbDelete(dbServiceVisits, e->index);
    if (err == dbOk) e->index = svtEditNew;
    return err;
}

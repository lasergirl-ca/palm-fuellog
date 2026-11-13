/* Host tests for the Pocket PC build's service model (app/svcview.c),
 * against a working copy of the fixture set tools/mkpdb writes (argv[1]).
 * The fixture: an Outback with fills on Jul 29 (80,650 km) and Aug 10
 * (81,190 km); its one service type, Oil and filter, every 8,000 km or 6
 * months, warn 800 km or 14 days, alarm on; and one visit for it on May 14
 * at 76,852 km for $119.95 at Corner Garage. "Today" is Aug 20, 2026. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../store/db.h"
#include "../app/svcview.h"
#include "../app/vehedit.h"
#include "../../src/datecalc.h"
#include "../../src/units.h"
#include "../../src/currency.h"

static int failures;

static void check(const char *name, long got, long want)
{
    if (got == want) printf("ok   %s = %ld\n", name, got);
    else { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
}
static void checkStr(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) printf("ok   %s = \"%s\"\n", name, got);
    else { printf("FAIL %s: got \"%s\" want \"%s\"\n", name, got, want); failures++; }
}

static UInt32 Now(void) { return 3877000000UL; }

static SvcRow rows[svcMaxRows];

int main(int argc, char **argv)
{
    const char *work = argc > 1 ? argv[1] : "build/host/work6";
    DbClock clock;
    FuelPrefs p;
    TextLine head[4];
    UInt8 hc, n;
    UInt16 today = DateYmdToDays(2026, 8, 20), date;
    UInt32 odo;
    char h[64], f[64], buf[64], lines[3][64];
    SvtEdit te;
    SvtForm tf;
    SvlEdit ve;
    SvlForm vf;
    ServiceTypeRecord t;

    clock.now = Now; clock.clockShift = 0;
    check("open", DbOpenAll(work, &clock), dbOk);
    DbEnsureUids();
    PrefsSetDefaults(&p);

    /* What is due. */
    check("pace: 540 km over 12 days", (long)SvcViewPace(1, today), 450);
    check("last oil change", SvcViewLastVisit(1, 0, &date, &odo), 1);
    check("  May 14", date, DateYmdToDays(2026, 5, 14));
    check("  at 76,852 km", (long)odo, 768520);
    check("no visit of another type", SvcViewLastVisit(1, 3, &date, &odo), 0);
    check("visits covering oil", SvcViewVisitsCovering(1, 0), 1);
    check("the Outback has types: no presets", SvcViewInstallPresets(1), 0);
    check("next type id", SvcViewNextTypeId(1), 1);
    check("vehicle for All", SvcViewVehicle(&p), 1);
    check("nothing due yet", SvcViewBanner(255, today, &p, '.', ',', buf), svcStatusOK);
    checkStr("  no banner", buf, "");
    check("the alarm: about Nov 9", SvcViewNextAlarm(today), DateYmdToDays(2026, 11, 9));
    check("Oct 30: fifteen days to the six months", SvcViewBanner(1, DateYmdToDays(2026, 10, 30), &p, '.', ',', buf), svcStatusOK);
    check("Nov 1: soon", SvcViewBanner(1, DateYmdToDays(2026, 11, 1), &p, '.', ',', buf), svcStatusSoon);
    checkStr("  the date comes first", buf, "Oil and filter due Nov 14");
    check("by Nov 15 it is overdue", SvcViewBanner(1, DateYmdToDays(2026, 11, 15), &p, '.', ',', buf), svcStatusOverdue);
    checkStr("  the banner", buf, "Oil and filter overdue");

    /* The Service list. */
    n = SvcViewList(1, today, &p, '.', ',', rows, svcMaxRows, h, f);
    check("one service", n, 1);
    checkStr("  header", h, "Odo 81,190 km, pace 45 km/day");
    checkStr("  footer", f, "Last visit May 14 at 76,852 km");
    checkStr("  name", rows[0].left, "Oil and filter");
    check("  green badge", rows[0].badge, 1 + svcStatusOK);
    checkStr("  due by the pace", rows[0].mid, "about Nov 9");
    checkStr("  why", rows[0].line2, "3,662 km left of 8,000 or 6 mo");
    check("  opens its history", rows[0].hasVisit, 1);

    /* Presets for the Ioniq, then its list. */
    check("presets for the Ioniq", SvcViewInstallPresets(2), 5);
    check("  once", SvcViewInstallPresets(2), 0);
    n = SvcViewList(2, today, &p, '.', ',', rows, svcMaxRows, h, f);
    check("  five rows", n, 5);
    checkStr("  first", rows[0].left, "Tyre rotation");
    checkStr("  set up", rows[0].right, "set up");
    checkStr("  says why", rows[0].line2, "tap to set the interval");
    checkStr("  no pace", h, "Odo 18,420 km, pace after a week");
    checkStr("  no visits", f, "No visits logged yet");

    /* History. */
    n = SvcViewHistory(1, 0xFFFF, today, &p, '.', ',', rows, svcMaxRows, head, &hc);
    check("history: one visit", n, 1);
    checkStr("  all services", head[0].cell[0], "All services");
    checkStr("  spend", head[2].cell[2], "$119.95");
    checkStr("  visits", head[3].cell[2], "1 visit");
    checkStr("  date", rows[0].left, "May 14");
    checkStr("  odometer", rows[0].mid, "76,852");
    checkStr("  cost", rows[0].right, "119.95");
    checkStr("  first record", rows[0].line2, "first record");
    checkStr("  items", rows[0].right2, "1 item");
    n = SvcViewHistory(1, 0, today, &p, '.', ',', rows, svcMaxRows, head, &hc);
    checkStr("history of oil: name", head[0].cell[0], "Oil and filter");
    checkStr("  interval", head[1].cell[0], "every 8,000 km or 6 months");
    checkStr("  no gap yet", head[2].cell[0], "--");

    /* Service Type. */
    check("open oil", SvtEditBegin(&te, 0, 1, &p, '.', &tf), 1);
    checkStr("  name", tf.name, "Oil and filter");
    checkStr("  every", tf.dist, "8000");
    checkStr("  months", tf.months, "6");
    checkStr("  warn", tf.warnDist, "800");
    checkStr("  days", tf.warnDays, "14");
    check("  alarm", tf.alarm, 1);
    checkStr("  covered by a visit: archive", SvtEditArchiveLabel(&te), "Archive");
    SvtEditSummary(&te, today, &p, '.', ',', lines);
    checkStr("  summary", lines[0], "Due at 84,852 km or Nov 14");
    checkStr("  pace line", lines[1], "At 45 km/day that is about Nov 9");
    checkStr("  warning", lines[2], "Warning starts at 84,052 km");
    strcpy(tf.dist, "10,000"); tf.warnDist[0] = 0;
    check("save every 10,000 km", SvtEditSave(&te, &tf, &p, '.', ','), seSaved);
    {
        UInt8 pk[dbMaxRecord]; UInt32 len;
        DbGet(dbServiceTypes, 0, pk, sizeof pk, &len);
        ServiceTypeUnpack(pk, len, &t);
    }
    check("  interval", (long)t.intervalDist, 100000);
    check("  warning a tenth by default", (long)t.warnDist, 10000);
    strcpy(tf.months, "300");
    check("  months over 255 refused", SvtEditSave(&te, &tf, &p, '.', ','), seBadNumber);
    check("new type", SvtEditBegin(&te, svtEditNew, 1, &p, '.', &tf), 1);
    check("  id 1", te.t.id, 1);
    checkStr("  fourteen days", tf.warnDays, "14");
    SvtEditSummary(&te, today, &p, '.', ',', lines);
    checkStr("  explains", lines[0], "Set a distance, a time, or both;");
    tf.name[0] = 0;
    check("  needs a name", SvtEditSave(&te, &tf, &p, '.', ','), seNeedName);
    strcpy(tf.name, "Wiper blades"); strcpy(tf.months, "12");
    check("  saves", SvtEditSave(&te, &tf, &p, '.', ','), seSaved);
    checkStr("  no visits: delete", SvtEditArchiveLabel(&te), "Delete");

    /* Log a visit. */
    SvlEditBegin(&ve, svtEditNew, 1, today, &p, '.', &vf);
    check("log: today", vf.date, today);
    checkStr("  odometer from the last fill", vf.odo, "81190");
    check("  prefilled", ve.prefilledOdo, 1);
    check("  two types to tick", ve.count, 2);
    checkStr("  first", ve.names[0], "Oil and filter");
    check("  CAD", vf.currency, curCAD);
    check("nothing ticked", SvlEditSave(&ve, &vf, svOtherAsk, &p, '.', ','), svNothing);
    vf.checked[0] = 1;
    SvlEditNextLine(&ve, &vf, &p, '.', ',', buf);
    checkStr("  next line, the name cut to 12 as on the Palm", buf, "Next Oil and filt: 91,190 km or Feb 20");
    strcpy(vf.cost, "89.50"); strcpy(vf.shop, "Corner Garage");
    vf.otherOn = 1; strcpy(vf.other, "Battery");
    check("Other not yet a type: ask", SvlEditSave(&ve, &vf, svOtherAsk, &p, '.', ','), svAskTrack);
    check("  track it", SvlEditSave(&ve, &vf, svOtherTrack, &p, '.', ','), svSaved);
    check("  two visits", DbCount(dbServiceVisits), 2);
    check("  three Outback types", SvcViewNextTypeId(1), 3);
    check("  Battery is type 2 and ticked", SvcViewVisitsCovering(1, 2), 1);
    check("  oil covered twice", SvcViewVisitsCovering(1, 0), 2);
    n = SvcViewHistory(1, 0, today, &p, '.', ',', rows, svcMaxRows, head, &hc);
    check("oil history: two visits", n, 2);
    checkStr("  newest first", rows[0].left, "Aug 20");
    checkStr("  the gap", rows[0].line2, "4,338 km, 3.2 months");
    checkStr("  longest gap", head[2].cell[0], "4,338 km");
    checkStr("  spend", head[2].cell[2], "$209.45");
    /* Edit the new visit: Other once, kept as Other. */
    SvlEditBegin(&ve, 1, 1, today, &p, '.', &vf);
    check("edit: the visit's own date", vf.date, today);
    check("  three types now", ve.count, 3);
    check("  oil ticked", vf.checked[0], 1);
    checkStr("  cost", vf.cost, "89.50");
    vf.otherOn = 1; strcpy(vf.other, "Wash");
    check("  Other once", SvlEditSave(&ve, &vf, svOtherOnce, &p, '.', ','), svSaved);
    check("  still two visits", DbCount(dbServiceVisits), 2);
    n = SvcViewHistory(1, 0xFFFF, today, &p, '.', ',', rows, svcMaxRows, head, &hc);
    checkStr("  oil, battery and wash", rows[0].right2, "3 items");
    vf.otherOn = 1; strcpy(vf.other, "battery");
    SvlEditBegin(&ve, 1, 1, today, &p, '.', &vf);
    vf.otherOn = 1; strcpy(vf.other, "BATTERY");
    check("Other naming a type ticks it, no question", SvlEditSave(&ve, &vf, svOtherAsk, &p, '.', ','), svSaved);
    SvlEditBegin(&ve, 1, 1, today, &p, '.', &vf);
    check("  no Other left", vf.otherOn, 0);
    check("delete the visit", SvlEditDelete(&ve), dbOk);
    check("  one visit", DbCount(dbServiceVisits), 1);
    check("  a tombstone", DbCount(dbDeleted) >= 1, 1);
    check("save", DbSave(), dbOk);
    DbCloseAll();

    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

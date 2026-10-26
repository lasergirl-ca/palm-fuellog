/* Prefs defaults and the version upgrade path. */
#include <stdio.h>
#include <string.h>
#include "../src/prefs.h"
#include "../src/units.h"
#include "../src/currency.h"

static int failures;
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}

int main(void)
{
    FuelPrefs p;
    PrefsSetDefaults(&p);
    check("default version", p.version, prefsVersion);
    check("default home currency CAD", p.homeCurrency, curCAD);
    check("default new-entry currency follows last used", p.defaultCurrency, curLastUsed);
    check("default price format cents", p.priceFormat, unitPriceFormatCents);
    check("default rates are the built-in table (0)", p.fxRate[curUSD], 0);
    check("default sets kept", p.backupSetsKept, 5);
    p.lastScrollRow = 42;
    check("upgrade same version keeps fields", PrefsUpgrade(&p, 3, sizeof p), 1);
    check("field kept", p.lastScrollRow, 42);
    check("newer version refused", PrefsUpgrade(&p, 4, sizeof p), 0);
    p.nextAlarm = 99;
    {
        /* a v2 image: four rate slots, USD typed, AUD left at the old 1.0 default, CAD home */
        FuelPrefsV2 v2;
        FuelPrefs q;
        unsigned k;
        memset(&v2, 0, sizeof v2);
        v2.version = 2; v2.homeCurrency = curCAD; v2.distanceUnit = 1; v2.backupSetsKept = 7; v2.lastScrollRow = 9; v2.nextAlarm = 12345;
        v2.fxRate[curUSD] = 1360000; v2.fxRate[curCAD] = 1000000; v2.fxRate[curEUR] = 1000000; v2.fxRate[curAUD] = 1000000;
        memset(&q, 0xAA, sizeof q);
        for (k = 0; k < sizeof v2; k++) ((unsigned char *)&q)[k] = ((unsigned char *)&v2)[k];
        check("v2 image upgrades", PrefsUpgrade(&q, 2, sizeof v2), 1);
        check("v2 fields moved past the longer rate table", q.lastScrollRow, 9);
        check("v2 nextAlarm kept", (long)q.nextAlarm, 12345);
        check("v2 sets kept", q.backupSetsKept, 7);
        check("typed USD rate kept", (long)q.fxRate[curUSD], 1360000);
        check("untouched AUD 1.0 becomes built-in", (long)q.fxRate[curAUD], 0);
        check("home currency slot kept", (long)q.fxRate[curCAD], 1000000);
        check("new NZD slot is built-in", (long)q.fxRate[curNZD], 0);
        v2.nextAlarm = 0;
        for (k = 0; k < sizeof v2; k++) ((unsigned char *)&q)[k] = ((unsigned char *)&v2)[k];
        check("v1 image upgrades", PrefsUpgrade(&q, 1, sizeof v2 - sizeof(UInt32)), 1);
        check("v1 upgrade clears nextAlarm", (long)q.nextAlarm, 0);
    }
    check("short stored struct reset to defaults", PrefsUpgrade(&p, 1, 4), 1);
    check("field reset", p.lastScrollRow, 0);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

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
    check("default USD rate 1.0", p.fxRate[curUSD], 1000000);
    check("default sets kept", p.backupSetsKept, 5);
    p.lastScrollRow = 42;
    check("upgrade same version keeps fields", PrefsUpgrade(&p, 2, sizeof p), 1);
    check("field kept", p.lastScrollRow, 42);
    check("newer version refused", PrefsUpgrade(&p, 3, sizeof p), 0);
    p.nextAlarm = 99;
    check("v1 blob upgrades in place", PrefsUpgrade(&p, 1, sizeof p - sizeof(UInt32)), 1);
    check("v1 upgrade clears nextAlarm", p.nextAlarm, 0);
    check("short stored struct reset to defaults", PrefsUpgrade(&p, 1, 4), 1);
    check("field reset", p.lastScrollRow, 0);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}

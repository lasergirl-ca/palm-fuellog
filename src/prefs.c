#include "prefs.h"
#include "units.h"
#include "currency.h"
#include "records.h"

void PrefsSetDefaults(FuelPrefs *p)
{
    UInt8 i;
    p->version = prefsVersion;
    p->distanceUnit = unitDistKm;
    p->volumeUnit = unitVolLitres;
    p->economyUnit = unitEcoL100km;
    p->priceUnit = unitPricePerLitre;
    p->priceFormat = unitPriceFormatCents;
    p->homeCurrency = curCAD;
    p->defaultCurrency = curLastUsed;
    p->lastCurrency = curCAD;
    p->lastPayment = fillPaymentCash;
    p->autoBackup = 1;
    for (i = 0; i < prefsRateSlots; i++) p->fxRate[i] = 0;   /* the built-in rates until one is typed */
    p->lastBackupDate = 0;
    p->backupSetsKept = 5;
    p->currentVehicle = 255;
    p->lastFormId = 0;
    p->lastScrollRow = 0;
    p->nextAlarm = 0;
}

Int32 PrefsUpgrade(FuelPrefs *p, UInt16 storedVersion, UInt16 storedSize)
{
    if (storedVersion > prefsVersion) return 0;
    if ((storedVersion == 1 && storedSize >= sizeof(FuelPrefsV2) - sizeof(UInt32)) || (storedVersion == 2 && storedSize >= sizeof(FuelPrefsV2))) {
        /* v1 and v2 had four rate slots (v1 also no nextAlarm): rebuild from the old layout */
        FuelPrefsV2 old;
        UInt8 c;
        const UInt8 *s = (const UInt8 *)p; UInt8 *d = (UInt8 *)&old;
        UInt16 k;
        for (k = 0; k < sizeof old; k++) d[k] = s[k];
        if (storedVersion == 1) old.nextAlarm = 0;
        PrefsSetDefaults(p);
        p->distanceUnit = old.distanceUnit; p->volumeUnit = old.volumeUnit; p->economyUnit = old.economyUnit;
        p->priceUnit = old.priceUnit; p->priceFormat = old.priceFormat; p->homeCurrency = old.homeCurrency;
        p->defaultCurrency = old.defaultCurrency; p->lastCurrency = old.lastCurrency; p->lastPayment = old.lastPayment;
        p->autoBackup = old.autoBackup; p->lastBackupDate = old.lastBackupDate; p->backupSetsKept = old.backupSetsKept;
        p->currentVehicle = old.currentVehicle; p->lastFormId = old.lastFormId; p->lastScrollRow = old.lastScrollRow;
        p->nextAlarm = old.nextAlarm;
        /* a rate left at the old default of 1.0 for a foreign currency was never typed: the built-in rate takes over */
        for (c = 0; c < 4; c++) p->fxRate[c] = (old.fxRate[c] == fxOne && c != old.homeCurrency) ? 0 : old.fxRate[c];
        return 1;
    }
    if (storedVersion < 1 || storedSize < sizeof(FuelPrefs)) {
        PrefsSetDefaults(p);
        return 1;
    }
    p->version = prefsVersion;
    return 1;
}

#ifdef __palmos__
#include <PalmOS.h>
#define kCreator 'ZFlg'

void PrefsLoad(FuelPrefs *p)
{
    UInt16 size = sizeof(FuelPrefs);
    Int16 version = PrefGetAppPreferences(kCreator, prefsAppPrefId, p, &size, true);
    if (version == noPreferenceFound) {
        PrefsSetDefaults(p);
        return;
    }
    if (!PrefsUpgrade(p, (UInt16)version, size))
        PrefsSetDefaults(p);
}

void PrefsSave(const FuelPrefs *p)
{
    PrefSetAppPreferences(kCreator, prefsAppPrefId, prefsVersion, p, sizeof(FuelPrefs), true);
}
#endif

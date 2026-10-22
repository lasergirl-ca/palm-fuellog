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
    for (i = 0; i < 4; i++) p->fxRate[i] = fxOne;
    p->lastBackupDate = 0;
    p->backupSetsKept = 5;
    p->currentVehicle = 255;
    p->lastFormId = 0;
    p->lastScrollRow = 0;
}

Int32 PrefsUpgrade(FuelPrefs *p, UInt16 storedVersion, UInt16 storedSize)
{
    if (storedVersion > prefsVersion) return 0;
    if (storedVersion < 1 || storedSize < sizeof(FuelPrefs)) {
        PrefsSetDefaults(p);
        return 1;
    }
    /* Version 1 is current; later versions add their field fixes here. */
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

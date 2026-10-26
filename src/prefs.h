/* Application preferences: a versioned struct saved with
 * PrefSetAppPreferences. Everything here is a display or entry choice;
 * stored data never depends on it. The struct and its defaults are plain
 * C so they are host-tested; only the load and save live on the device. */
#ifndef PREFS_H
#define PREFS_H
#include "ptypes.h"
#include "sections.h"

#define prefsVersion 3
#define prefsRateSlots 8     /* fxRate entries: room for currencies beyond curCount without another version */
#define prefsAppPrefId 0

typedef struct {
    UInt16 version;
    UInt8  distanceUnit;      /* unitDist* */
    UInt8  volumeUnit;        /* unitVol* */
    UInt8  economyUnit;       /* unitEco* for fuel */
    UInt8  priceUnit;         /* unitPrice* */
    UInt8  priceFormat;       /* unitPriceFormat* */
    UInt8  homeCurrency;      /* cur* */
    UInt8  defaultCurrency;   /* cur* or curLastUsed */
    UInt8  lastCurrency;
    UInt8  lastPayment;       /* fillPayment* */
    UInt8  autoBackup;
    UInt32 fxRate[prefsRateSlots];   /* last rate per currency, home units per unit x 1e6; 0 = use the built-in rate (v3) */
    UInt16 lastBackupDate;    /* days since 1904, 0 = never */
    UInt8  backupSetsKept;
    UInt8  currentVehicle;    /* vehicle id, 255 = all */
    UInt16 lastFormId;
    UInt16 lastScrollRow;
    UInt32 nextAlarm;         /* v2: seconds of the service alarm set, 0 = none; re-set after a reset */
} FuelPrefs;

/* The v1/v2 layout (v1 ends before nextAlarm), for the upgrade. */
typedef struct {
    UInt16 version;
    UInt8  distanceUnit, volumeUnit, economyUnit, priceUnit, priceFormat, homeCurrency, defaultCurrency, lastCurrency, lastPayment, autoBackup;
    UInt32 fxRate[4];
    UInt16 lastBackupDate;
    UInt8  backupSetsKept, currentVehicle;
    UInt16 lastFormId, lastScrollRow;
    UInt32 nextAlarm;
} FuelPrefsV2;

void PrefsSetDefaults(FuelPrefs *p) EXPORT_SEC;
/* Bring a struct of an older version up to date; returns 0 when the version
 * is newer than this build understands. */
Int32 PrefsUpgrade(FuelPrefs *p, UInt16 storedVersion, UInt16 storedSize) EXPORT_SEC;

#ifdef __palmos__
/* Device only: load (with defaults on first run or a newer version) and save. */
void PrefsLoad(FuelPrefs *p);
void PrefsSave(const FuelPrefs *p);
#endif

#endif

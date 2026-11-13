/* Prefs to and from bytes (settings.h). */
#include "settings.h"
#include "../../src/units.h"
#include "../../src/currency.h"
#include "../../src/records.h"

static void Put32(UInt8 *b, UInt32 v) { b[0] = (UInt8)(v >> 24); b[1] = (UInt8)(v >> 16); b[2] = (UInt8)(v >> 8); b[3] = (UInt8)v; }
static UInt32 Get32(const UInt8 *b) { return ((UInt32)b[0] << 24) | ((UInt32)b[1] << 16) | ((UInt32)b[2] << 8) | b[3]; }

UInt32 SettingsPack(const FuelPrefs *p, UInt8 *buf)
{
    UInt8 i;
    buf[0] = 0; buf[1] = settingsVersion;
    buf[2] = p->distanceUnit;
    buf[3] = p->volumeUnit;
    buf[4] = p->economyUnit;
    buf[5] = p->priceUnit;
    buf[6] = p->priceFormat;
    buf[7] = p->homeCurrency;
    buf[8] = p->defaultCurrency;
    buf[9] = p->lastCurrency;
    buf[10] = p->lastPayment;
    buf[11] = p->currentVehicle;
    for (i = 0; i < prefsRateSlots; i++) Put32(buf + 12 + 4 * i, p->fxRate[i]);
    buf[44] = p->autoBackup;
    buf[45] = p->backupSetsKept;
    buf[46] = (UInt8)(p->lastBackupDate >> 8); buf[47] = (UInt8)p->lastBackupDate;
    return settingsSize;
}

Int32 SettingsUnpack(const UInt8 *buf, UInt32 len, FuelPrefs *p)
{
    UInt8 i;
    if (len < settingsSize || (buf[0] == 0 && buf[1] == 0)) return -1;
    if (buf[2] <= unitDistMiles) p->distanceUnit = buf[2];
    if (buf[3] <= unitVolImpGal) p->volumeUnit = buf[3];
    if (buf[4] <= unitEcoMpgImp) p->economyUnit = buf[4];
    if (buf[5] <= unitPricePerImpGal) p->priceUnit = buf[5];
    if (buf[6] <= unitPriceFormatCents) p->priceFormat = buf[6];
    if (buf[7] < curCount) p->homeCurrency = buf[7];
    if (buf[8] < curCount || buf[8] == curLastUsed) p->defaultCurrency = buf[8];
    if (buf[9] < curCount) p->lastCurrency = buf[9];
    if (buf[10] <= fillPaymentCard) p->lastPayment = buf[10];
    p->currentVehicle = buf[11];            /* the window drops one that no longer exists */
    for (i = 0; i < prefsRateSlots; i++) p->fxRate[i] = Get32(buf + 12 + 4 * i);
    p->autoBackup = buf[44] ? 1 : 0;
    if (buf[45]) p->backupSetsKept = buf[45];
    p->lastBackupDate = (UInt16)((buf[46] << 8) | buf[47]);
    return 0;
}

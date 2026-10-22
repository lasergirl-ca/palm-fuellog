#include "currency.h"
#include "records.h"

static const char *const kCode[curCount] = { "USD", "CAD", "EUR", "AUD" };
static const char *const kSymbol[curCount] = { "US$", "C$", "\200", "A$" };
static const char *const kExcel[curCount] = {
    "[$USD] #,##0.00", "[$CAD] #,##0.00", "[$EUR] #,##0.00", "[$AUD] #,##0.00" };

const char *CurrencyCode(UInt8 c) { return c < curCount ? kCode[c] : "???"; }
const char *CurrencySymbol(UInt8 c) { return c < curCount ? kSymbol[c] : "?"; }
const char *CurrencyExcelFormat(UInt8 c) { return c < curCount ? kExcel[c] : "#,##0.00"; }

UInt8 CurrencyForNewEntry(UInt8 vehicleOverride, UInt8 prefsDefault, UInt8 lastUsed)
{
    if (vehicleOverride < curCount) return vehicleOverride;
    if (prefsDefault < curCount) return prefsDefault;
    return lastUsed < curCount ? lastUsed : curCAD;
}

UInt32 CurrencyDefaultRate(UInt8 currency, UInt8 home, const UInt32 *lastRates)
{
    if (currency == home || currency >= curCount) return fxOne;
    return lastRates[currency] ? lastRates[currency] : fxOne;
}

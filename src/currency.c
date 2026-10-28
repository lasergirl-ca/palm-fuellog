#include "currency.h"
#include "records.h"
#include "calc.h"
#include "rates_default.h"
#if kRatesCount != curCount
#error "src/rates_default.h is out of step with currency.h: run make rates (needs the network)"
#endif

static const char *const kCode[curCount] = { "USD", "CAD", "EUR", "AUD", "NZD", "GBP" };
static const char *const kSymbol[curCount] = { "US$", "C$", "\200", "A$", "NZ$", "\243" };   /* euro 0x80, pound 0xA3: Palm Latin and PDF WinAnsi agree */
static const char *const kExcel[curCount] = {
    "[$USD] #,##0.00", "[$CAD] #,##0.00", "[$EUR] #,##0.00", "[$AUD] #,##0.00", "[$NZD] #,##0.00", "[$GBP] #,##0.00" };
static const UInt32 kPerUsd[curCount] = kRatesPerUsd;   /* units per 1 USD x 1e6 */

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
    return lastRates[currency] ? lastRates[currency] : CurrencyBuiltInRate(currency, home);
}

UInt32 CurrencyBuiltInRate(UInt8 currency, UInt8 home)
{
    if (currency >= curCount || home >= curCount || currency == home) return fxOne;
    /* home per unit = (home per USD) / (currency per USD) */
    return CalcMulDiv(kPerUsd[home], fxOne, kPerUsd[currency]);
}

const char *CurrencyRatesDate(void) { return kRatesDate; }

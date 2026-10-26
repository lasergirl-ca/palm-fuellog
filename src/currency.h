/* Currencies and the rules for which one a new entry gets. Amounts in the
 * fill currency are converted to the home currency with the fill's stored
 * rate (CalcHomeCents), so totals never move when rates change later. */
#ifndef CURRENCY_H
#define CURRENCY_H
#include "ptypes.h"
#include "sections.h"

#define curUSD 0
#define curCAD 1
#define curEUR 2
#define curAUD 3
#define curNZD 4
#define curCount 5
#define curLastUsed 255      /* Prefs: default currency follows the last entry */

#define fxOne 1000000UL      /* a rate of 1.000000 */

const char *CurrencyCode(UInt8 currency) EXPORT_SEC;     /* "CAD" */
const char *CurrencySymbol(UInt8 currency) EXPORT_SEC;   /* "C$"; the euro sign is Palm Latin 0x80 */
const char *CurrencyExcelFormat(UInt8 currency) EXPORT_SEC;

/* The currency for a new fill or visit: the vehicle's override if set,
 * else the Prefs default, else the last one used. */
UInt8 CurrencyForNewEntry(UInt8 vehicleOverride, UInt8 prefsDefault, UInt8 lastUsed) EXPORT_SEC;

/* The rate a new entry starts with: 1.0 when the entry is in the home
 * currency, else the last rate kept for that currency in Prefs, else the
 * built-in rate. */
UInt32 CurrencyDefaultRate(UInt8 currency, UInt8 home, const UInt32 *lastRates) EXPORT_SEC;

/* Home units per one unit of `currency` from the table baked in at build time
 * (tools/fetch_rates.py, src/rates_default.h), and the date of that table. */
UInt32 CurrencyBuiltInRate(UInt8 currency, UInt8 home) EXPORT_SEC;
const char *CurrencyRatesDate(void) EXPORT_SEC;

#endif

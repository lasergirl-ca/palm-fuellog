/* Fixed-point number text: parsing what the user types and formatting what
 * the forms, lists and exports show. Plain C, no Palm headers. Separators
 * are parameters so the device can pass the ones from the Formats panel. */
#ifndef TEXT_H
#define TEXT_H
#include "ptypes.h"
#include "sections.h"

/* Format value / 10^decimals into buf (at least 16 bytes) with the given
 * decimal point and optional thousands separator (0 = none). Returns the
 * length. 8462 with 2 decimals -> "84.62"; 842120 with 1 -> "84,212.0". */
UInt16 TextFixed(char *buf, Int32 value, UInt8 decimals, char point, char thousands) EXPORT_SEC;

/* Parse digits with an optional decimal point into value x 10^decimals,
 * rounding extra digits, ignoring the thousands separator and spaces.
 * "47.3" with 3 decimals -> 47300; "178.9" with 1 -> 1789. Returns 0 when
 * the text is empty or has no digits, 1 on success, -1 on a bad character. */
Int32 TextParseFixed(const char *text, UInt8 decimals, char point, char thousands, UInt32 *value) EXPORT_SEC;

/* Price text per the Prefs format: 17890 ten-thousandths as "1.789" (dollars,
 * three decimals) or "178.9" (cents, one decimal). */
UInt16 TextPrice(char *buf, UInt32 price, UInt8 cents, char point) EXPORT_SEC;
/* The reverse: typed price text to ten-thousandths. */
Int32 TextParsePrice(const char *text, UInt8 cents, char point, char thousands, UInt32 *price) EXPORT_SEC;

#endif

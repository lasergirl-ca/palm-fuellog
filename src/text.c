#include "text.h"

UInt16 TextFixed(char *buf, Int32 value, UInt8 decimals, char point, char thousands)
{
    char tmp[16];
    int i = 15, n = 0, intDigits = 0;
    UInt32 u = value < 0 ? (UInt32)(-value) : (UInt32)value;
    UInt8 d;
    UInt16 len;

    tmp[i] = 0;
    for (d = 0; d < decimals; d++) { tmp[--i] = (char)('0' + u % 10); u /= 10; }
    if (decimals) tmp[--i] = point;
    do {
        if (thousands && intDigits && intDigits % 3 == 0) tmp[--i] = thousands;
        tmp[--i] = (char)('0' + u % 10); u /= 10; intDigits++;
    } while (u);
    if (value < 0) tmp[--i] = '-';
    for (len = 0; tmp[i + len]; len++) buf[n++] = tmp[i + len];
    buf[n] = 0;
    return (UInt16)n;
}

Int32 TextParseFixed(const char *text, UInt8 decimals, char point, char thousands, UInt32 *value)
{
    UInt32 v = 0;
    int seenPoint = 0, digits = 0, frac = 0, roundUp = 0;
    const char *p;

    for (p = text; *p; p++) {
        char c = *p;
        if (c >= '0' && c <= '9') {
            if (!seenPoint) {
                if (v > 429496729UL) return -1;        /* would overflow */
                v = v * 10 + (UInt32)(c - '0');
            } else if (frac < decimals) {
                v = v * 10 + (UInt32)(c - '0');
                frac++;
            } else if (frac == decimals) {
                roundUp = c >= '5';
                frac++;
            }
            digits++;
        } else if (c == point && !seenPoint) {
            seenPoint = 1;
        } else if (c == thousands || c == ' ') {
            continue;
        } else {
            return -1;
        }
    }
    if (!digits) return 0;
    while (frac < decimals) { v *= 10; frac++; }
    if (roundUp) v++;
    *value = v;
    return 1;
}

UInt16 TextPrice(char *buf, UInt32 price, UInt8 cents, char point)
{
    if (cents) return TextFixed(buf, (Int32)((price + 5) / 10), 1, point, 0);   /* 178.9 */
    return TextFixed(buf, (Int32)((price + 5) / 10), 3, point, 0);             /* 1.789 */
}

Int32 TextParsePrice(const char *text, UInt8 cents, char point, char thousands, UInt32 *price)
{
    UInt32 v;
    Int32 r = TextParseFixed(text, cents ? 2 : 4, point, thousands, &v);
    if (r == 1) *price = cents ? v : v;      /* cents x 100 = ten-thousandths of a unit; dollars x 10^4 likewise */
    return r;
}

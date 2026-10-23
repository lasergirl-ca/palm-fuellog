#include <stdio.h>
#include <string.h>
#include "../src/text.h"

static int failures;
static void checkStr(const char *name, const char *got, const char *want)
{
    if (strcmp(got, want)) { printf("FAIL %s: got \"%s\" want \"%s\"\n", name, got, want); failures++; }
    else printf("ok   %s = \"%s\"\n", name, got);
}
static void check(const char *name, long got, long want)
{
    if (got != want) { printf("FAIL %s: got %ld want %ld\n", name, got, want); failures++; }
    else printf("ok   %s = %ld\n", name, got);
}

int main(void)
{
    char b[32];
    UInt32 v;
    TextFixed(b, 8462, 2, '.', 0);        checkStr("84.62", b, "84.62");
    TextFixed(b, 842120, 1, '.', ',');    checkStr("84,212.0", b, "84,212.0");
    TextFixed(b, 5, 2, '.', 0);           checkStr("0.05", b, "0.05");
    TextFixed(b, 0, 0, '.', 0);           checkStr("0", b, "0");
    TextFixed(b, -7, 0, '.', 0);          checkStr("-7", b, "-7");
    TextFixed(b, 1234567, 2, ',', '.');   checkStr("european 12.345,67", b, "12.345,67");
    TextFixed(b, 3399, 0, '.', ',');      checkStr("3,399", b, "3,399");

    check("parse 47.3 -> 47300", TextParseFixed("47.3", 3, '.', ',', &v), 1);   check("  value", v, 47300);
    check("parse 84,212 -> 842120", TextParseFixed("84,212", 1, '.', ',', &v), 1); check("  value", v, 842120);
    check("parse 1.7895 rounds up", TextParseFixed("1.7895", 3, '.', ',', &v), 1); check("  value", v, 1790);
    check("parse 1.7894 rounds down", TextParseFixed("1.7894", 3, '.', ',', &v), 1); check("  value", v, 1789);
    check("parse .5", TextParseFixed(".5", 2, '.', ',', &v), 1);                check("  value", v, 50);
    check("parse empty", TextParseFixed("", 2, '.', ',', &v), 0);
    check("parse junk", TextParseFixed("12a", 2, '.', ',', &v), -1);
    check("parse european 1.234,5", TextParseFixed("1.234,5", 1, ',', '.', &v), 1); check("  value", v, 12345);

    TextPrice(b, 17890, 1, '.');  checkStr("price as cents", b, "178.9");
    TextPrice(b, 17890, 0, '.');  checkStr("price as dollars", b, "1.789");
    TextPrice(b, 1162, 1, '.');   checkStr("hydro rate as cents", b, "11.6");
    check("parse price cents 178.9", TextParsePrice("178.9", 1, '.', ',', &v), 1); check("  value", v, 17890);
    check("parse price dollars 1.789", TextParsePrice("1.789", 0, '.', ',', &v), 1); check("  value", v, 17890);
    check("parse hydro 11.62 c", TextParsePrice("11.62", 1, '.', ',', &v), 1);      check("  value", v, 1162);
    printf("%d failure(s)\n", failures);
    return failures ? 1 : 0;
}
